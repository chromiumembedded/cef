// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#include <string>
#include <utility>

#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/browser_util.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools_frontend.h"
#include "libcef/browser/devtools_manager_delegate.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/browser/image_impl.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/navigate_params.h"
#include "libcef/browser/navigation_entry_impl.h"
#include "libcef/browser/net/chrome_scheme_handler.h"
#include "libcef/browser/net/scheme_handler.h"
#include "libcef/browser/osr/osr_util.h"
#include "libcef/browser/printing/print_view_manager.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/drag_data_impl.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/main_delegate.h"
#include "libcef/common/process_message_impl.h"
#include "libcef/common/request_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "components/zoom/zoom_controller.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/common/view_messages.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "ui/events/base_event_utils.h"

#if defined(OS_MACOSX)
#include "components/spellcheck/browser/spellcheck_platform.h"
#endif

using content::KeyboardEventProcessingResult;

namespace {

// Associates a CefBrowserHostImpl instance with a WebContents. This object will
// be deleted automatically when the WebContents is destroyed.
class WebContentsUserDataAdapter : public base::SupportsUserData::Data {
 public:
  static void Register(CefBrowserHostImpl* browser) {
    new WebContentsUserDataAdapter(browser);
  }

  static CefBrowserHostImpl* Get(const content::WebContents* web_contents) {
    WebContentsUserDataAdapter* adapter =
        static_cast<WebContentsUserDataAdapter*>(
            web_contents->GetUserData(UserDataKey()));
    if (adapter)
      return adapter->browser_;
    return nullptr;
  }

 private:
  WebContentsUserDataAdapter(CefBrowserHostImpl* browser)
        : browser_(browser) {
    browser->web_contents()->SetUserData(UserDataKey(), this);
  }

  static void* UserDataKey() {
    // We just need a unique constant. Use the address of a static that
    // COMDAT folding won't touch in an optimizing linker.
    static int data_key = 0;
    return reinterpret_cast<void*>(&data_key);
  }

  CefBrowserHostImpl* browser_;  // Not owned.
};

class CreateBrowserHelper {
 public:
  CreateBrowserHelper(const CefWindowInfo& windowInfo,
                      CefRefPtr<CefClient> client,
                      const CefString& url,
                      const CefBrowserSettings& settings,
                      CefRefPtr<CefRequestContext> request_context)
                      : window_info_(windowInfo),
                        client_(client),
                        url_(url),
                        settings_(settings),
                        request_context_(request_context) {}

  CefWindowInfo window_info_;
  CefRefPtr<CefClient> client_;
  CefString url_;
  CefBrowserSettings settings_;
  CefRefPtr<CefRequestContext> request_context_;
};

void CreateBrowserWithHelper(CreateBrowserHelper* helper) {
  CefBrowserHost::CreateBrowserSync(helper->window_info_, helper->client_,
      helper->url_, helper->settings_, helper->request_context_);
  delete helper;
}

class ShowDevToolsHelper {
 public:
  ShowDevToolsHelper(CefRefPtr<CefBrowserHostImpl> browser,
                     const CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient> client,
                     const CefBrowserSettings& settings,
                     const CefPoint& inspect_element_at)
                      : browser_(browser),
                        window_info_(windowInfo),
                        client_(client),
                        settings_(settings),
                        inspect_element_at_(inspect_element_at) {}

  CefRefPtr<CefBrowserHostImpl> browser_;
  CefWindowInfo window_info_;
  CefRefPtr<CefClient> client_;
  CefBrowserSettings settings_;
  CefPoint inspect_element_at_;
};

void ShowDevToolsWithHelper(ShowDevToolsHelper* helper) {
  helper->browser_->ShowDevTools(helper->window_info_, helper->client_,
      helper->settings_, helper->inspect_element_at_);
  delete helper;
}

// Callback from CefBrowserHostImpl::DownloadImage.
void OnDownloadImage(uint32 max_image_size,
                     CefRefPtr<CefDownloadImageCallback> callback,
                     int id,
                     int http_status_code,
                     const GURL& image_url,
                     const std::vector<SkBitmap>& bitmaps,
                     const std::vector<gfx::Size>& sizes) {
  CEF_REQUIRE_UIT();

  CefRefPtr<CefImageImpl> image_impl;

  if (!bitmaps.empty()) {
    image_impl = new CefImageImpl();
    image_impl->AddBitmaps(max_image_size, bitmaps);
  }

  callback->OnDownloadImageFinished(image_url.spec(), http_status_code,
                                    image_impl.get());
}

}  // namespace


// CefBrowserHost static methods.
// -----------------------------------------------------------------------------

// static
bool CefBrowserHost::CreateBrowser(
    const CefWindowInfo& windowInfo,
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefRequestContext> request_context) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  // Verify that the settings structure is a valid size.
  if (settings.size != sizeof(cef_browser_settings_t)) {
    NOTREACHED() << "invalid CefBrowserSettings structure size";
    return false;
  }

  // Verify windowless rendering requirements.
  if (windowInfo.windowless_rendering_enabled &&
      !client->GetRenderHandler().get()) {
    NOTREACHED() << "CefRenderHandler implementation is required";
    return false;
  }

  // Create the browser on the UI thread.
  CreateBrowserHelper* helper =
      new CreateBrowserHelper(windowInfo, client, url, settings,
                              request_context);
  CEF_POST_TASK(CEF_UIT, base::Bind(CreateBrowserWithHelper, helper));

  return true;
}

// static
CefRefPtr<CefBrowser> CefBrowserHost::CreateBrowserSync(
    const CefWindowInfo& windowInfo,
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefRequestContext> request_context) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return nullptr;
  }

  // Verify that the settings structure is a valid size.
  if (settings.size != sizeof(cef_browser_settings_t)) {
    NOTREACHED() << "invalid CefBrowserSettings structure size";
    return nullptr;
  }

  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return nullptr;
  }

  // Verify windowless rendering requirements.
  if (windowInfo.windowless_rendering_enabled &&
      !client->GetRenderHandler().get()) {
    NOTREACHED() << "CefRenderHandler implementation is required";
    return nullptr;
  }

  CefBrowserHostImpl::CreateParams create_params;
  create_params.window_info.reset(new CefWindowInfo(windowInfo));
  create_params.client = client;
  create_params.url = url;
  create_params.settings = settings;
  create_params.request_context = request_context;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::Create(create_params);
  return browser.get();
}


// CefBrowserHostImpl static methods.
// -----------------------------------------------------------------------------

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::Create(
    CreateParams& create_params) {
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate =
      CefBrowserPlatformDelegate::Create(create_params);
  CHECK(platform_delegate);

  const bool is_devtools_popup = !!create_params.devtools_opener;

  scoped_refptr<CefBrowserInfo> info =
      CefBrowserInfoManager::GetInstance()->CreateBrowserInfo(
          is_devtools_popup,
          platform_delegate->IsWindowless());

  // Get or create the request context and browser context.
  CefRefPtr<CefRequestContextImpl> request_context_impl =
      CefRequestContextImpl::GetOrCreateForRequestContext(
          create_params.request_context);
  DCHECK(request_context_impl);
  CefBrowserContext* browser_context =
      request_context_impl->GetBrowserContext();
  DCHECK(browser_context);

  // A StoragePartitionImplMap must already exist for the BrowserContext. See
  // additional comments in CefBrowserContextImpl::Initialize().
  DCHECK(browser_context->GetUserData(
      content::BrowserContext::GetStoragePartitionMapUserDataKey()));

  if (!create_params.request_context) {
    // Using the global request context.
    create_params.request_context = request_context_impl.get();
  }

  content::WebContents::CreateParams wc_create_params(browser_context);

  if (platform_delegate->IsWindowless()) {
    // Create the OSR view for the WebContents.
    platform_delegate->CreateViewForWebContents(
        &wc_create_params.view,
        &wc_create_params.delegate_view);
  }

  content::WebContents* web_contents =
      content::WebContents::Create(wc_create_params);
  DCHECK(web_contents);

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::CreateInternal(
          create_params.settings,
          create_params.client,
          web_contents,
          info,
          create_params.devtools_opener,
          is_devtools_popup,
          create_params.request_context,
          std::move(platform_delegate));
  if (browser.get() && !create_params.url.empty()) {
    browser->LoadURL(CefFrameHostImpl::kMainFrameId, create_params.url,
                     content::Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  }
  return browser.get();
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::CreateInternal(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* web_contents,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<CefBrowserHostImpl> opener,
    bool is_devtools_popup,
    CefRefPtr<CefRequestContext> request_context,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate) {
  CEF_REQUIRE_UIT();
  DCHECK(web_contents);
  DCHECK(browser_info);
  DCHECK(request_context);
  DCHECK(platform_delegate);

  // If |opener| is non-NULL it must be a popup window.
  DCHECK(!opener.get() || browser_info->is_popup());

  if (opener) {
    // Give the opener browser's platform delegate an opportunity to modify the
    // new browser's platform delegate.
    opener->platform_delegate_->PopupWebContentsCreated(
        settings, client, web_contents, platform_delegate.get(),
        is_devtools_popup);
  }

  platform_delegate->WebContentsCreated(web_contents);

  CefRefPtr<CefBrowserHostImpl> browser =
      new CefBrowserHostImpl(settings, client, web_contents, browser_info,
                             opener, request_context,
                             std::move(platform_delegate));
  if (!browser->CreateHostWindow())
    return nullptr;

  // Notify that the browser has been created. These must be delivered in the
  // expected order.

  // 1. Notify the browser's LifeSpanHandler. This must always be the first
  // notification for the browser.
  if (client.get()) {
    CefRefPtr<CefLifeSpanHandler> handler = client->GetLifeSpanHandler();
    if (handler.get())
      handler->OnAfterCreated(browser.get());
  }

  // 2. Notify the platform delegate. With Views this will result in a call to
  // CefBrowserViewDelegate::OnBrowserCreated().
  browser->platform_delegate_->NotifyBrowserCreated();

  if (opener) {
    // 3. Notify the opener browser's platform delegate. With Views this will
    // result in a call to CefBrowserViewDelegate::OnPopupBrowserViewCreated().
    opener->platform_delegate_->PopupBrowserCreated(browser.get(),
                                                    is_devtools_popup);
  }

  return browser;
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForHost(
    const content::RenderViewHost* host) {
  DCHECK(host);
  CEF_REQUIRE_UIT();
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(
          const_cast<content::RenderViewHost*>(host));
  if (web_contents)
    return GetBrowserForContents(web_contents);
  return nullptr;
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForHost(
    const content::RenderFrameHost* host) {
  DCHECK(host);
  CEF_REQUIRE_UIT();
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          const_cast<content::RenderFrameHost*>(host));
  if (web_contents)
    return GetBrowserForContents(web_contents);
  return nullptr;
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForContents(
    const content::WebContents* contents) {
  DCHECK(contents);
  CEF_REQUIRE_UIT();
  return WebContentsUserDataAdapter::Get(contents);
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForRequest(
    const net::URLRequest* request) {
  DCHECK(request);
  CEF_REQUIRE_IOT();
  int render_process_id = -1;
  int render_frame_id = MSG_ROUTING_NONE;

  if (!content::ResourceRequestInfo::GetRenderFrameForRequest(
          request, &render_process_id, &render_frame_id) ||
      render_process_id == -1 ||
      render_frame_id == MSG_ROUTING_NONE) {
    return nullptr;
  }

  return GetBrowserForFrame(render_process_id, render_frame_id);
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForView(
    int render_process_id, int render_routing_id) {
  if (render_process_id == -1 || render_routing_id == MSG_ROUTING_NONE)
    return nullptr;

  if (CEF_CURRENTLY_ON_UIT()) {
    // Use the non-thread-safe but potentially faster approach.
    content::RenderViewHost* render_view_host =
        content::RenderViewHost::FromID(render_process_id, render_routing_id);
    if (!render_view_host)
      return nullptr;
    return GetBrowserForHost(render_view_host);
  } else {
    // Use the thread-safe approach.
    bool is_guest_view = false;
    scoped_refptr<CefBrowserInfo> info =
        CefBrowserInfoManager::GetInstance()->GetBrowserInfoForView(
            render_process_id, render_routing_id, &is_guest_view);
    if (info.get() && !is_guest_view) {
      CefRefPtr<CefBrowserHostImpl> browser = info->browser();
      if (!browser.get()) {
        LOG(WARNING) << "Found browser id " << info->browser_id() <<
                        " but no browser object matching view process id " <<
                        render_process_id << " and routing id " <<
                        render_routing_id;
      }
      return browser;
    }
    return nullptr;
  }
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForFrame(
    int render_process_id, int render_routing_id) {
  if (render_process_id == -1 || render_routing_id == MSG_ROUTING_NONE)
    return nullptr;

  if (CEF_CURRENTLY_ON_UIT()) {
    // Use the non-thread-safe but potentially faster approach.
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(render_process_id, render_routing_id);
    if (!render_frame_host)
      return nullptr;
    return GetBrowserForHost(render_frame_host);
  } else {
    // Use the thread-safe approach.
    bool is_guest_view = false;
    scoped_refptr<CefBrowserInfo> info =
        CefBrowserInfoManager::GetInstance()->GetBrowserInfoForFrame(
            render_process_id, render_routing_id, &is_guest_view);
    if (info.get() && !is_guest_view) {
      CefRefPtr<CefBrowserHostImpl> browser = info->browser();
      if (!browser.get()) {
        LOG(WARNING) << "Found browser id " << info->browser_id() <<
                        " but no browser object matching frame process id " <<
                        render_process_id << " and routing id " <<
                        render_routing_id;
      }
      return browser;
    }
    return nullptr;
  }
}


// CefBrowserHostImpl methods.
// -----------------------------------------------------------------------------

// WebContentsObserver that will be notified when the frontend WebContents is
// destroyed so that the inspected browser can clear its DevTools references.
class CefBrowserHostImpl::DevToolsWebContentsObserver :
    public content::WebContentsObserver {
 public:
  DevToolsWebContentsObserver(CefBrowserHostImpl* browser,
                              content::WebContents* frontend_web_contents)
      : WebContentsObserver(frontend_web_contents),
        browser_(browser) {
  }

  // WebContentsObserver methods:
  void WebContentsDestroyed() override {
    browser_->OnDevToolsWebContentsDestroyed();
  }

 private:
  CefBrowserHostImpl* browser_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsWebContentsObserver);
};

CefBrowserHostImpl::~CefBrowserHostImpl() {
}

CefRefPtr<CefBrowser> CefBrowserHostImpl::GetBrowser() {
  return this;
}

void CefBrowserHostImpl::CloseBrowser(bool force_close) {
  if (CEF_CURRENTLY_ON_UIT()) {
    // Exit early if a close attempt is already pending and this method is
    // called again from somewhere other than WindowDestroyed().
    if (destruction_state_ >= DESTRUCTION_STATE_PENDING &&
        (IsWindowless() || !window_destroyed_)) {
      if (force_close && destruction_state_ == DESTRUCTION_STATE_PENDING) {
        // Upgrade the destruction state.
        destruction_state_ = DESTRUCTION_STATE_ACCEPTED;
      }
      return;
    }

    if (destruction_state_ < DESTRUCTION_STATE_ACCEPTED) {
      destruction_state_ = (force_close ? DESTRUCTION_STATE_ACCEPTED :
                                          DESTRUCTION_STATE_PENDING);
    }

    content::WebContents* contents = web_contents();
    if (contents && contents->NeedToFireBeforeUnload()) {
      // Will result in a call to BeforeUnloadFired() and, if the close isn't
      // canceled, CloseContents().
      contents->DispatchBeforeUnload();
    } else {
      CloseContents(contents);
    }
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::CloseBrowser, this, force_close));
  }
}

bool CefBrowserHostImpl::TryCloseBrowser() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  // Protect against multiple requests to close while the close is pending.
  if (destruction_state_ <= DESTRUCTION_STATE_PENDING) {
    if (destruction_state_ == DESTRUCTION_STATE_NONE) {
      // Request that the browser close.
      CloseBrowser(false);
    }

    // Cancel the close.
    return false;
  }

  // Allow the close.
  return true;
}

void CefBrowserHostImpl::SetFocus(bool focus) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SetFocus, this, focus));
    return;
  }

  if (focus)
    OnSetFocus(FOCUS_SOURCE_SYSTEM);
  else if (platform_delegate_)
    platform_delegate_->SendFocusEvent(false);
}

CefWindowHandle CefBrowserHostImpl::GetWindowHandle() {
  if (IsViewsHosted() && CEF_CURRENTLY_ON_UIT()) {
    // Always return the most up-to-date window handle for a views-hosted
    // browser since it may change if the view is re-parented.
    if (platform_delegate_)
      return platform_delegate_->GetHostWindowHandle();
  }
  return host_window_handle_;
}

CefWindowHandle CefBrowserHostImpl::GetOpenerWindowHandle() {
  return opener_;
}

bool CefBrowserHostImpl::HasView() {
  return IsViewsHosted();
}

CefRefPtr<CefClient> CefBrowserHostImpl::GetClient() {
  return client_;
}

CefRefPtr<CefRequestContext> CefBrowserHostImpl::GetRequestContext() {
  return request_context_;
}

double CefBrowserHostImpl::GetZoomLevel() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return 0;
  }

  if (web_contents())
    return content::HostZoomMap::GetZoomLevel(web_contents());

  return 0;
}

void CefBrowserHostImpl::SetZoomLevel(double zoomLevel) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (web_contents())
      content::HostZoomMap::SetZoomLevel(web_contents(), zoomLevel);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SetZoomLevel, this, zoomLevel));
  }
}

void CefBrowserHostImpl::RunFileDialog(
      FileDialogMode mode,
      const CefString& title,
      const CefString& default_file_path,
      const std::vector<CefString>& accept_filters,
      int selected_accept_filter,
      CefRefPtr<CefRunFileDialogCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::RunFileDialog, this, mode, title,
                   default_file_path, accept_filters, selected_accept_filter,
                   callback));
    return;
  }

  EnsureFileDialogManager();
  file_dialog_manager_->RunFileDialog(mode, title, default_file_path,
                                      accept_filters, selected_accept_filter,
                                      callback);
}

void CefBrowserHostImpl::StartDownload(const CefString& url) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::StartDownload, this, url));
    return;
  }

  GURL gurl = GURL(url.ToString());
  if (gurl.is_empty() || !gurl.is_valid())
    return;

  if (!web_contents())
    return;

  CefBrowserContext* context =
      static_cast<CefBrowserContext*>(web_contents()->GetBrowserContext());
  if (!context)
    return;

  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(context);
  if (!manager)
    return;

  std::unique_ptr<content::DownloadUrlParameters> params(
      content::DownloadUrlParameters::CreateForWebContentsMainFrame(
          web_contents(), gurl));
  manager->DownloadUrl(std::move(params));
}

void CefBrowserHostImpl::DownloadImage(
    const CefString& image_url,
    bool is_favicon,
    uint32 max_image_size,
    bool bypass_cache,
    CefRefPtr<CefDownloadImageCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::DownloadImage, this, image_url,
            is_favicon, max_image_size, bypass_cache, callback));
    return;
  }

  if (!callback)
    return;

  GURL gurl = GURL(image_url.ToString());
  if (gurl.is_empty() || !gurl.is_valid())
    return;

  if (!web_contents())
    return;

  web_contents()->DownloadImage(
      gurl, is_favicon, max_image_size * gfx::ImageSkia::GetMaxSupportedScale(),
      bypass_cache, base::Bind(OnDownloadImage, max_image_size, callback));
}

void CefBrowserHostImpl::Print() {
  if (CEF_CURRENTLY_ON_UIT()) {
    content::WebContents* actionable_contents = GetActionableWebContents();
    if (!actionable_contents)
      return;
    printing::CefPrintViewManager::FromWebContents(
        actionable_contents)->PrintNow(
            actionable_contents->GetRenderViewHost()->GetMainFrame());
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::Print, this));
  }
}

void CefBrowserHostImpl::PrintToPDF(const CefString& path,
                                    const CefPdfPrintSettings& settings,
                                    CefRefPtr<CefPdfPrintCallback> callback) {
  if (CEF_CURRENTLY_ON_UIT()) {
    content::WebContents* actionable_contents = GetActionableWebContents();
    if (!actionable_contents)
      return;

    printing::CefPrintViewManager::PdfPrintCallback pdf_callback;
    if (callback.get()) {
      pdf_callback = base::Bind(&CefPdfPrintCallback::OnPdfPrintFinished,
                                callback.get(), path);
    }
    printing::CefPrintViewManager::FromWebContents(actionable_contents)->
        PrintToPDF(actionable_contents->GetMainFrame(), base::FilePath(path),
                   settings, pdf_callback);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::PrintToPDF, this, path, settings,
                   callback));
  }
}

void CefBrowserHostImpl::Find(int identifier, const CefString& searchText,
                              bool forward, bool matchCase, bool findNext) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (!web_contents_)
      return;

    blink::WebFindOptions options;
    options.forward = forward;
    options.match_case = matchCase;
    options.find_next = findNext;
    web_contents()->Find(identifier, searchText, options);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::Find, this, identifier, searchText,
                   forward, matchCase, findNext));
  }
}

void CefBrowserHostImpl::StopFinding(bool clearSelection) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (!web_contents_)
      return;

    content::StopFindAction action = clearSelection ?
        content::STOP_FIND_ACTION_CLEAR_SELECTION :
        content::STOP_FIND_ACTION_KEEP_SELECTION;
    web_contents()->StopFinding(action);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::StopFinding, this, clearSelection));
  }
}

void CefBrowserHostImpl::ShowDevTools(
    const CefWindowInfo& windowInfo,
    CefRefPtr<CefClient> client,
    const CefBrowserSettings& settings,
    const CefPoint& inspect_element_at) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (!web_contents_)
      return;

    if (devtools_frontend_) {
      if (!inspect_element_at.IsEmpty()) {
        devtools_frontend_->InspectElementAt(inspect_element_at.x,
                                             inspect_element_at.y);
      }
      devtools_frontend_->Focus();
      return;
    }

    devtools_frontend_ = CefDevToolsFrontend::Show(
        this, windowInfo, client, settings, inspect_element_at);
    devtools_observer_.reset(new DevToolsWebContentsObserver(
        this, devtools_frontend_->frontend_browser()->web_contents()));
  } else {
    ShowDevToolsHelper* helper =
        new ShowDevToolsHelper(this, windowInfo, client, settings,
                               inspect_element_at);
    CEF_POST_TASK(CEF_UIT, base::Bind(ShowDevToolsWithHelper, helper));
  }
}

void CefBrowserHostImpl::CloseDevTools() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (!devtools_frontend_)
      return;
    devtools_frontend_->Close();
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::CloseDevTools, this));
  }
}

bool CefBrowserHostImpl::HasDevTools() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  return (devtools_frontend_ != nullptr);
}

void CefBrowserHostImpl::GetNavigationEntries(
    CefRefPtr<CefNavigationEntryVisitor> visitor,
    bool current_only) {
  DCHECK(visitor.get());
  if (!visitor.get())
    return;

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::GetNavigationEntries, this, visitor,
                   current_only));
    return;
  }

  if (!web_contents())
    return;

  const content::NavigationController& controller =
      web_contents()->GetController();
  const int total = controller.GetEntryCount();
  const int current = controller.GetCurrentEntryIndex();

  if (current_only) {
    // Visit only the current entry.
    CefRefPtr<CefNavigationEntryImpl> entry =
        new CefNavigationEntryImpl(controller.GetEntryAtIndex(current));
    visitor->Visit(entry.get(), true, current, total);
    entry->Detach(NULL);
  } else {
    // Visit all entries.
    bool cont = true;
    for (int i = 0; i < total && cont; ++i) {
      CefRefPtr<CefNavigationEntryImpl> entry =
          new CefNavigationEntryImpl(controller.GetEntryAtIndex(i));
      cont = visitor->Visit(entry.get(), (i == current), i, total);
      entry->Detach(NULL);
    }
  }
}

CefRefPtr<CefNavigationEntry> CefBrowserHostImpl::GetVisibleNavigationEntry() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return nullptr;
  }

  content::NavigationEntry* entry = nullptr;
  if (web_contents())
      entry = web_contents()->GetController().GetVisibleEntry();

  if (!entry)
    return nullptr;

  return new CefNavigationEntryImpl(entry);
}

void CefBrowserHostImpl::SetMouseCursorChangeDisabled(bool disabled) {
  base::AutoLock lock_scope(state_lock_);
  mouse_cursor_change_disabled_ = disabled;
}

bool CefBrowserHostImpl::IsMouseCursorChangeDisabled() {
  base::AutoLock lock_scope(state_lock_);
  return mouse_cursor_change_disabled_;
}

bool CefBrowserHostImpl::IsWindowRenderingDisabled() {
  return IsWindowless();
}

void CefBrowserHostImpl::ReplaceMisspelling(const CefString& word) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::ReplaceMisspelling, this, word));
    return;
  }

  if(web_contents())
    web_contents()->ReplaceMisspelling(word);
}

void CefBrowserHostImpl::AddWordToDictionary(const CefString& word) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::AddWordToDictionary, this, word));
    return;
  }

  if (!web_contents())
    return;

  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();
  if (browser_context) {
    SpellcheckService* spellcheck =
        SpellcheckServiceFactory::GetForContext(browser_context);
    if (spellcheck)
      spellcheck->GetCustomDictionary()->AddWord(word);
  }
#if defined(OS_MACOSX)
  spellcheck_platform::AddWord(word);
#endif
}

void CefBrowserHostImpl::WasResized() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::Bind(&CefBrowserHostImpl::WasResized, this));
    return;
  }

if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->WasResized();
}

void CefBrowserHostImpl::WasHidden(bool hidden) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHost::WasHidden, this, hidden));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->WasHidden(hidden);
}

void CefBrowserHostImpl::NotifyScreenInfoChanged() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::NotifyScreenInfoChanged, this));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->NotifyScreenInfoChanged();
}

void CefBrowserHostImpl::Invalidate(PaintElementType type) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::Invalidate, this, type));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->Invalidate(type);
}

void CefBrowserHostImpl::SendKeyEvent(const CefKeyEvent& event) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendKeyEvent, this, event));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  content::NativeWebKeyboardEvent web_event(
      blink::WebInputEvent::kUndefined,
      blink::WebInputEvent::kNoModifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  platform_delegate_->TranslateKeyEvent(web_event, event);
  platform_delegate_->SendKeyEvent(web_event);
}

void CefBrowserHostImpl::SendMouseClickEvent(const CefMouseEvent& event,
    MouseButtonType type, bool mouseUp, int clickCount) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendMouseClickEvent, this, event, type,
                   mouseUp, clickCount));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  blink::WebMouseEvent web_event;
  platform_delegate_->TranslateClickEvent(web_event, event, type, mouseUp,
                                          clickCount);
  platform_delegate_->SendMouseEvent(web_event);
}

void CefBrowserHostImpl::SendMouseMoveEvent(const CefMouseEvent& event,
                                            bool mouseLeave) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendMouseMoveEvent, this, event,
                   mouseLeave));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  blink::WebMouseEvent web_event;
  platform_delegate_->TranslateMoveEvent(web_event, event, mouseLeave);
  platform_delegate_->SendMouseEvent(web_event);
}

void CefBrowserHostImpl::SendMouseWheelEvent(const CefMouseEvent& event,
                                             int deltaX, int deltaY) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendMouseWheelEvent, this, event,
                   deltaX, deltaY));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  blink::WebMouseWheelEvent web_event;
  platform_delegate_->TranslateWheelEvent(web_event, event, deltaX, deltaY);
  platform_delegate_->SendMouseWheelEvent(web_event);
}

void CefBrowserHostImpl::SendFocusEvent(bool setFocus) {
  SetFocus(setFocus);
}

void CefBrowserHostImpl::SendCaptureLostEvent() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendCaptureLostEvent, this));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->SendCaptureLostEvent();
}

void CefBrowserHostImpl::NotifyMoveOrResizeStarted() {
#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::NotifyMoveOrResizeStarted, this));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->NotifyMoveOrResizeStarted();
#endif
}

int CefBrowserHostImpl::GetWindowlessFrameRate() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return 0;
  }

  return osr_util::ClampFrameRate(settings_.windowless_frame_rate);
}

void CefBrowserHostImpl::SetWindowlessFrameRate(int frame_rate) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SetWindowlessFrameRate, this,
                   frame_rate));
    return;
  }

  settings_.windowless_frame_rate = frame_rate;

  if (platform_delegate_)
    platform_delegate_->SetWindowlessFrameRate(frame_rate);
}

// CefBrowser methods.
// -----------------------------------------------------------------------------

CefRefPtr<CefBrowserHost> CefBrowserHostImpl::GetHost() {
  return this;
}

bool CefBrowserHostImpl::CanGoBack() {
  base::AutoLock lock_scope(state_lock_);
  return can_go_back_;
}

void CefBrowserHostImpl::GoBack() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (frame_destruction_pending_) {
      // Try again after frame destruction has completed.
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefBrowserHostImpl::GoBack, this));
      return;
    }

    if (web_contents_.get() && web_contents_->GetController().CanGoBack())
      web_contents_->GetController().GoBack();
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::GoBack, this));
  }
}

bool CefBrowserHostImpl::CanGoForward() {
  base::AutoLock lock_scope(state_lock_);
  return can_go_forward_;
}

void CefBrowserHostImpl::GoForward() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (frame_destruction_pending_) {
      // Try again after frame destruction has completed.
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefBrowserHostImpl::GoForward, this));
      return;
    }

    if (web_contents_.get() && web_contents_->GetController().CanGoForward())
      web_contents_->GetController().GoForward();
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::GoForward, this));
  }
}

bool CefBrowserHostImpl::IsLoading() {
  base::AutoLock lock_scope(state_lock_);
  return is_loading_;
}

void CefBrowserHostImpl::Reload() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (frame_destruction_pending_) {
      // Try again after frame destruction has completed.
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefBrowserHostImpl::Reload, this));
      return;
    }

    if (web_contents_.get())
      web_contents_->GetController().Reload(content::ReloadType::NORMAL, true);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::Reload, this));
  }
}

void CefBrowserHostImpl::ReloadIgnoreCache() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (frame_destruction_pending_) {
      // Try again after frame destruction has completed.
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefBrowserHostImpl::ReloadIgnoreCache, this));
      return;
    }

    if (web_contents_.get()) {
      web_contents_->GetController().Reload(
          content::ReloadType::BYPASSING_CACHE, true);
    }
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::ReloadIgnoreCache, this));
  }
}

void CefBrowserHostImpl::StopLoad() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (frame_destruction_pending_) {
      // Try again after frame destruction has completed.
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefBrowserHostImpl::StopLoad, this));
      return;
    }

    if (web_contents_.get())
      web_contents_->Stop();
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::StopLoad, this));
  }
}

int CefBrowserHostImpl::GetIdentifier() {
  return browser_id();
}

bool CefBrowserHostImpl::IsSame(CefRefPtr<CefBrowser> that) {
  CefBrowserHostImpl* impl = static_cast<CefBrowserHostImpl*>(that.get());
  return (impl == this);
}

bool CefBrowserHostImpl::IsPopup() {
  return browser_info_->is_popup();
}

bool CefBrowserHostImpl::HasDocument() {
  base::AutoLock lock_scope(state_lock_);
  return has_document_;
}

CefRefPtr<CefFrame> CefBrowserHostImpl::GetMainFrame() {
  return GetFrame(CefFrameHostImpl::kMainFrameId);
}

CefRefPtr<CefFrame> CefBrowserHostImpl::GetFocusedFrame() {
  return GetFrame(CefFrameHostImpl::kFocusedFrameId);
}

CefRefPtr<CefFrame> CefBrowserHostImpl::GetFrame(int64 identifier) {
  base::AutoLock lock_scope(state_lock_);

  if (main_frame_id_ == CefFrameHostImpl::kInvalidFrameId) {
    // A main frame does not exist yet. Return the placeholder frame that
    // provides limited functionality.
    return placeholder_frame_.get();
  }

  if (identifier == CefFrameHostImpl::kMainFrameId) {
    identifier = main_frame_id_;
  } else if (identifier == CefFrameHostImpl::kFocusedFrameId) {
    // Return the main frame if no focused frame is currently identified.
    if (focused_frame_id_ == CefFrameHostImpl::kInvalidFrameId)
      identifier = main_frame_id_;
    else
      identifier = focused_frame_id_;
  }

  if (identifier == CefFrameHostImpl::kInvalidFrameId)
    return nullptr;

  FrameMap::const_iterator it = frames_.find(identifier);
  if (it != frames_.end())
    return it->second.get();

  return nullptr;
}

CefRefPtr<CefFrame> CefBrowserHostImpl::GetFrame(const CefString& name) {
  base::AutoLock lock_scope(state_lock_);

  FrameMap::const_iterator it = frames_.begin();
  for (; it != frames_.end(); ++it) {
    if (it->second->GetName() == name)
      return it->second.get();
  }

  return nullptr;
}

size_t CefBrowserHostImpl::GetFrameCount() {
  base::AutoLock lock_scope(state_lock_);
  return frames_.size();
}

void CefBrowserHostImpl::GetFrameIdentifiers(std::vector<int64>& identifiers) {
  base::AutoLock lock_scope(state_lock_);

  if (identifiers.size() > 0)
    identifiers.clear();

  FrameMap::const_iterator it = frames_.begin();
  for (; it != frames_.end(); ++it)
    identifiers.push_back(it->first);
}

void CefBrowserHostImpl::GetFrameNames(std::vector<CefString>& names) {
  base::AutoLock lock_scope(state_lock_);

  if (names.size() > 0)
    names.clear();

  FrameMap::const_iterator it = frames_.begin();
  for (; it != frames_.end(); ++it)
    names.push_back(it->second->GetName());
}

bool CefBrowserHostImpl::SendProcessMessage(
    CefProcessId target_process,
    CefRefPtr<CefProcessMessage> message) {
  DCHECK(message.get());

  Cef_Request_Params params;
  CefProcessMessageImpl* impl =
      static_cast<CefProcessMessageImpl*>(message.get());
  if (impl->CopyTo(params)) {
    return SendProcessMessage(target_process, params.name, &params.arguments,
                              true);
  }

  return false;
}


// CefBrowserHostImpl public methods.
// -----------------------------------------------------------------------------

bool CefBrowserHostImpl::IsWindowless() const {
  return is_windowless_;
}

bool CefBrowserHostImpl::IsViewsHosted() const {
  return is_views_hosted_;
}

void CefBrowserHostImpl::WindowDestroyed() {
  CEF_REQUIRE_UIT();
  DCHECK(!window_destroyed_);
  window_destroyed_ = true;
  CloseBrowser(true);
}

void CefBrowserHostImpl::DestroyBrowser() {
  CEF_REQUIRE_UIT();

  destruction_state_ = DESTRUCTION_STATE_COMPLETED;

  // Notify that this browser has been destroyed. These must be delivered in the
  // expected order.

  // 1. Notify the platform delegate. With Views this will result in a call to
  // CefBrowserViewDelegate::OnBrowserDestroyed().
  platform_delegate_->NotifyBrowserDestroyed();

  // 2. Notify the browser's LifeSpanHandler. This must always be the last
  // notification for this browser.
  if (client_.get()) {
    CefRefPtr<CefLifeSpanHandler> handler = client_->GetLifeSpanHandler();
    if (handler.get()) {
      // Notify the handler that the window is about to be closed.
      handler->OnBeforeClose(this);
    }
  }

  // Destroy any platform constructs first.
  if (file_dialog_manager_.get())
    file_dialog_manager_->Destroy();
  if (javascript_dialog_manager_.get())
    javascript_dialog_manager_->Destroy();
  if (menu_manager_.get())
    menu_manager_->Destroy();

  // Notify any observers that may have state associated with this browser.
  for (auto& observer : observers_)
    observer.OnBrowserDestroyed(this);

  // Disassociate the platform delegate from this browser.
  platform_delegate_->BrowserDestroyed(this);

  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

  registrar_.reset(NULL);
  response_manager_.reset(NULL);
  content::WebContentsObserver::Observe(NULL);
  web_contents_.reset(NULL);

  // Delete objects created by the platform delegate that may be referenced by
  // the WebContents.
  file_dialog_manager_.reset(NULL);
  javascript_dialog_manager_.reset(NULL);
  menu_manager_.reset(NULL);

  // Delete the platform delegate.
  platform_delegate_.reset(NULL);

  DetachAllFrames();

  CefBrowserInfoManager::GetInstance()->RemoveBrowserInfo(browser_info_);
  browser_info_->set_browser(NULL);
}

#if defined(USE_AURA)
views::Widget* CefBrowserHostImpl::GetWindowWidget() const {
  CEF_REQUIRE_UIT();
  if (!platform_delegate_)
    return nullptr;
  return platform_delegate_->GetWindowWidget();
}

CefRefPtr<CefBrowserView> CefBrowserHostImpl::GetBrowserView() const {
  CEF_REQUIRE_UIT();
  if (IsViewsHosted() && platform_delegate_)
    return platform_delegate_->GetBrowserView();
  return nullptr;
}
#endif  // defined(USE_AURA)

void CefBrowserHostImpl::CancelContextMenu() {
  CEF_REQUIRE_UIT();
  if (menu_manager_)
    menu_manager_->CancelContextMenu();
}

CefRefPtr<CefFrame> CefBrowserHostImpl::GetFrameForRequest(
    net::URLRequest* request) {
  CEF_REQUIRE_IOT();
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info)
    return nullptr;
  return GetOrCreateFrame(info->GetRenderFrameID(),
                          CefFrameHostImpl::kUnspecifiedFrameId,
                          info->IsMainFrame(),
                          base::string16(),
                          GURL());
}

void CefBrowserHostImpl::Navigate(const CefNavigateParams& params) {
  // Only known frame ids and kMainFrameId are supported.
  DCHECK(params.frame_id >= CefFrameHostImpl::kMainFrameId);

  CefMsg_LoadRequest_Params request;
  request.url = params.url;
  if (!request.url.is_valid()) {
    LOG(ERROR) << "Invalid URL passed to CefBrowserHostImpl::Navigate: " <<
        params.url;
    return;
  }

  request.method = params.method;
  request.referrer = params.referrer.url;
  request.referrer_policy = params.referrer.policy;
  request.frame_id = params.frame_id;
  request.first_party_for_cookies = params.first_party_for_cookies;
  request.headers = params.headers;
  request.load_flags = params.load_flags;
  request.upload_data = params.upload_data;

  Send(new CefMsg_LoadRequest(routing_id(), request));

  OnSetFocus(FOCUS_SOURCE_NAVIGATION);
}

void CefBrowserHostImpl::LoadRequest(int64 frame_id,
                                     CefRefPtr<CefRequest> request) {
  CefNavigateParams params(GURL(), ui::PAGE_TRANSITION_TYPED);
  params.frame_id = frame_id;

  static_cast<CefRequestImpl*>(request.get())->Get(params);

  Navigate(params);
}

void CefBrowserHostImpl::LoadURL(
    int64 frame_id,
    const std::string& url,
    const content::Referrer& referrer,
    ui::PageTransition transition,
    const std::string& extra_headers) {
  if (frame_id == CefFrameHostImpl::kMainFrameId) {
    // Go through the navigation controller.
    if (CEF_CURRENTLY_ON_UIT()) {
      if (frame_destruction_pending_) {
        // Try again after frame destruction has completed.
        CEF_POST_TASK(CEF_UIT,
            base::Bind(&CefBrowserHostImpl::LoadURL, this, frame_id, url,
                       referrer, transition, extra_headers));
        return;
      }

      if (web_contents_.get()) {
        GURL gurl = GURL(url);

        if (!gurl.is_valid() && !gurl.has_scheme()) {
          // Try to add "http://" at the beginning
          std::string new_url = std::string("http://") + url;
          gurl = GURL(new_url);
        }

        if (!gurl.is_valid()) {
          LOG(ERROR) <<
              "Invalid URL passed to CefBrowserHostImpl::LoadURL: " << url;
          return;
        }

        web_contents_->GetController().LoadURL(
            gurl,
            referrer,
            transition,
            extra_headers);
        OnSetFocus(FOCUS_SOURCE_NAVIGATION);
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefBrowserHostImpl::LoadURL, this, frame_id, url,
                     referrer, transition, extra_headers));
    }
  } else {
    CefNavigateParams params(GURL(url), transition);
    params.frame_id = frame_id;
    params.referrer = referrer;
    params.headers = extra_headers;
    Navigate(params);
  }
}

void CefBrowserHostImpl::LoadString(int64 frame_id, const std::string& string,
                                    const std::string& url) {
  // Only known frame ids or kMainFrameId are supported.
  DCHECK(frame_id >= CefFrameHostImpl::kMainFrameId);

  Cef_Request_Params params;
  params.name = "load-string";
  params.frame_id = frame_id;
  params.user_initiated = false;
  params.request_id = -1;
  params.expect_response = false;

  params.arguments.AppendString(string);
  params.arguments.AppendString(url);

  Send(new CefMsg_Request(routing_id(), params));
}

void CefBrowserHostImpl::SendCommand(
    int64 frame_id,
    const std::string& command,
    CefRefPtr<CefResponseManager::Handler> responseHandler) {
  // Only known frame ids are supported.
  DCHECK(frame_id > CefFrameHostImpl::kMainFrameId);
  DCHECK(!command.empty());

  // Execute on the UI thread because CefResponseManager is not thread safe.
  if (CEF_CURRENTLY_ON_UIT()) {
    TRACE_EVENT2("libcef", "CefBrowserHostImpl::SendCommand",
                 "frame_id", frame_id,
                 "needsResponse", responseHandler.get() ? 1 : 0);
    Cef_Request_Params params;
    params.name = "execute-command";
    params.frame_id = frame_id;
    params.user_initiated = false;

    if (responseHandler.get()) {
      params.request_id = response_manager_->RegisterHandler(responseHandler);
      params.expect_response = true;
    } else {
      params.request_id = -1;
      params.expect_response = false;
    }

    params.arguments.AppendString(command);

    Send(new CefMsg_Request(routing_id(), params));
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendCommand, this, frame_id, command,
                   responseHandler));
  }
}

void CefBrowserHostImpl::SendCode(
    int64 frame_id,
    bool is_javascript,
    const std::string& code,
    const std::string& script_url,
    int script_start_line,
    CefRefPtr<CefResponseManager::Handler> responseHandler) {
  // Only known frame ids are supported.
  DCHECK(frame_id >= CefFrameHostImpl::kMainFrameId);
  DCHECK(!code.empty());
  DCHECK_GE(script_start_line, 0);

  // Execute on the UI thread because CefResponseManager is not thread safe.
  if (CEF_CURRENTLY_ON_UIT()) {
    TRACE_EVENT2("libcef", "CefBrowserHostImpl::SendCommand",
                 "frame_id", frame_id,
                 "needsResponse", responseHandler.get() ? 1 : 0);
    Cef_Request_Params params;
    params.name = "execute-code";
    params.frame_id = frame_id;
    params.user_initiated = false;

    if (responseHandler.get()) {
      params.request_id = response_manager_->RegisterHandler(responseHandler);
      params.expect_response = true;
    } else {
      params.request_id = -1;
      params.expect_response = false;
    }

    params.arguments.AppendBoolean(is_javascript);
    params.arguments.AppendString(code);
    params.arguments.AppendString(script_url);
    params.arguments.AppendInteger(script_start_line);

    Send(new CefMsg_Request(routing_id(), params));
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendCode, this, frame_id, is_javascript,
                   code, script_url, script_start_line, responseHandler));
  }
}

bool CefBrowserHostImpl::SendProcessMessage(CefProcessId target_process,
                                            const std::string& name,
                                            base::ListValue* arguments,
                                            bool user_initiated) {
  DCHECK_EQ(PID_RENDERER, target_process);
  DCHECK(!name.empty());

  Cef_Request_Params params;
  params.name = name;
  if (arguments)
    params.arguments.Swap(arguments);
  params.frame_id = -1;
  params.user_initiated = user_initiated;
  params.request_id = -1;
  params.expect_response = false;

  return Send(new CefMsg_Request(routing_id(), params));
}

void CefBrowserHostImpl::ViewText(const std::string& text) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::ViewText,
                   this, text));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->ViewText(text);
}

void CefBrowserHostImpl::HandleExternalProtocol(const GURL& url) {
  if (CEF_CURRENTLY_ON_UIT()) {
    bool allow_os_execution = false;

    if (client_.get()) {
      CefRefPtr<CefRequestHandler> handler = client_->GetRequestHandler();
      if (handler.get())
        handler->OnProtocolExecution(this, url.spec(), allow_os_execution);
    }

    if (allow_os_execution && platform_delegate_)
      platform_delegate_->HandleExternalProtocol(url);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::HandleExternalProtocol, this, url));
  }
}

SkColor CefBrowserHostImpl::GetBackgroundColor() const {
  // Don't use |platform_delegate_| because it's not thread-safe.
  return CefContext::Get()->GetBackgroundColor(&settings_,
      is_windowless_ ? STATE_ENABLED : STATE_DISABLED);
}

int CefBrowserHostImpl::browser_id() const {
  return browser_info_->browser_id();
}

void CefBrowserHostImpl::OnSetFocus(cef_focus_source_t source) {
  if (CEF_CURRENTLY_ON_UIT()) {
    // SetFocus() might be called while inside the OnSetFocus() callback. If so,
    // don't re-enter the callback.
    if (!is_in_onsetfocus_) {
      if (client_.get()) {
        CefRefPtr<CefFocusHandler> handler = client_->GetFocusHandler();
        if (handler.get()) {
          is_in_onsetfocus_ = true;
          bool handled = handler->OnSetFocus(this, source);
          is_in_onsetfocus_ = false;

          if (handled)
            return;
        }
      }
    }

    if (platform_delegate_)
      platform_delegate_->SendFocusEvent(true);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::OnSetFocus, this, source));
  }
}

void CefBrowserHostImpl::RunFileChooser(
    const CefFileDialogRunner::FileChooserParams& params,
    const CefFileDialogRunner::RunFileChooserCallback& callback) {
  EnsureFileDialogManager();
  file_dialog_manager_->RunFileChooser(params, callback);
}

bool CefBrowserHostImpl::EmbedsFullscreenWidget() const {
  // When using windowless rendering do not allow Flash to create its own full-
  // screen widget.
  return IsWindowless();
}

void CefBrowserHostImpl::EnterFullscreenModeForTab(
    content::WebContents* web_contents,
    const GURL& origin) {
  OnFullscreenModeChange(true);
}

void CefBrowserHostImpl::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  OnFullscreenModeChange(false);
}

bool CefBrowserHostImpl::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) const {
  return is_fullscreen_;
}

blink::WebDisplayMode CefBrowserHostImpl::GetDisplayMode(
    const content::WebContents* web_contents) const {
  return is_fullscreen_ ? blink::kWebDisplayModeFullscreen :
                          blink::kWebDisplayModeBrowser;
}

void CefBrowserHostImpl::FindReply(
    content::WebContents* web_contents,
    int request_id,
    int number_of_matches,
    const gfx::Rect& selection_rect,
    int active_match_ordinal,
    bool final_update) {
  if (client_.get()) {
    CefRefPtr<CefFindHandler> handler = client_->GetFindHandler();
    if (handler.get()) {
      CefRect rect(selection_rect.x(), selection_rect.y(),
          selection_rect.width(), selection_rect.height());
      handler->OnFindResult(this, request_id, number_of_matches,
          rect, active_match_ordinal, final_update);
    }
  }
}

void CefBrowserHostImpl::ImeSetComposition(
    const CefString& text,
    const std::vector<CefCompositionUnderline>& underlines,
    const CefRange& replacement_range,
    const CefRange& selection_range) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::ImeSetComposition, this, text,
                   underlines, replacement_range, selection_range));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->ImeSetComposition(text, underlines, replacement_range,
                                        selection_range);
}

void CefBrowserHostImpl::ImeCommitText(const CefString& text,
                                       const CefRange& replacement_range,
                                       int relative_cursor_pos) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::ImeCommitText, this, text,
                   replacement_range, relative_cursor_pos));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->ImeCommitText(text, replacement_range,
                                    relative_cursor_pos);
}

void CefBrowserHostImpl::ImeFinishComposingText(bool keep_selection) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::ImeFinishComposingText, this,
                   keep_selection));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->ImeFinishComposingText(keep_selection);
}

void CefBrowserHostImpl::ImeCancelComposition() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::ImeCancelComposition, this));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->ImeCancelComposition();
}

void CefBrowserHostImpl::DragTargetDragEnter(CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::DragTargetDragEnter, this, drag_data,
                   event, allowed_ops));
    return;
  }

  if (!drag_data.get()) {
    NOTREACHED();
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->DragTargetDragEnter(drag_data, event, allowed_ops);
}

void CefBrowserHostImpl::DragTargetDragOver(const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::DragTargetDragOver, this, event,
                   allowed_ops));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->DragTargetDragOver(event, allowed_ops);
}

void CefBrowserHostImpl::DragTargetDragLeave() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::DragTargetDragLeave, this));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->DragTargetDragLeave();
}

void CefBrowserHostImpl::DragTargetDrop(const CefMouseEvent& event) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::DragTargetDrop, this, event));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->DragTargetDrop(event);
}

void CefBrowserHostImpl::DragSourceSystemDragEnded() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::DragSourceSystemDragEnded, this));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->DragSourceSystemDragEnded();
}

void CefBrowserHostImpl::DragSourceEndedAt(
    int x, int y, CefBrowserHost::DragOperationsMask op) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::DragSourceEndedAt, this, x, y, op));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->DragSourceEndedAt(x, y, op);
}


// content::WebContentsDelegate methods.
// -----------------------------------------------------------------------------

// |source| may be NULL if the navigation originates from a guest view via
// CefContentBrowserClient::CanCreateWindow.
content::WebContents* CefBrowserHostImpl::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  bool cancel = false;

  if (client_.get()) {
    CefRefPtr<CefRequestHandler> handler = client_->GetRequestHandler();
    if (handler.get()) {
      cancel = handler->OnOpenURLFromTab(
          this,
          GetFrame(params.frame_tree_node_id),
          params.url.spec(),
          static_cast<cef_window_open_disposition_t>(params.disposition),
          params.user_gesture);
    }
  }

  if (!cancel) {
    // Start a navigation in the current browser that will result in the
    // creation of a new render process.
    LoadURL(CefFrameHostImpl::kMainFrameId, params.url.spec(), params.referrer,
            params.transition, params.extra_headers);
    return source;
  }

  // We don't know where the navigation, if any, will occur.
  return nullptr;
}

void CefBrowserHostImpl::LoadingStateChanged(content::WebContents* source,
                                             bool to_different_document) {
  int current_index =
      web_contents_->GetController().GetLastCommittedEntryIndex();
  int max_index = web_contents_->GetController().GetEntryCount() - 1;

  bool is_loading, can_go_back, can_go_forward;

  {
    base::AutoLock lock_scope(state_lock_);
    is_loading = is_loading_ = web_contents_->IsLoading();
    can_go_back = can_go_back_ = (current_index > 0);
    can_go_forward = can_go_forward_ = (current_index < max_index);
  }

  if (client_.get()) {
    CefRefPtr<CefLoadHandler> handler = client_->GetLoadHandler();
    if (handler.get()) {
      handler->OnLoadingStateChange(this, is_loading, can_go_back,
                                    can_go_forward);
    }
  }
}

void CefBrowserHostImpl::CloseContents(content::WebContents* source) {
  CEF_REQUIRE_UIT();

  if (destruction_state_ == DESTRUCTION_STATE_COMPLETED)
    return;

  bool close_browser = true;

  // If this method is called in response to something other than
  // WindowDestroyed() ask the user if the browser should close.
  if (client_.get() && (IsWindowless() || !window_destroyed_)) {
    CefRefPtr<CefLifeSpanHandler> handler =
        client_->GetLifeSpanHandler();
    if (handler.get()) {
      close_browser = !handler->DoClose(this);
    }
  }

  if (close_browser) {
    if (destruction_state_ != DESTRUCTION_STATE_ACCEPTED)
      destruction_state_ = DESTRUCTION_STATE_ACCEPTED;

    if (!IsWindowless() && !window_destroyed_) {
      // A window exists so try to close it using the platform method. Will
      // result in a call to WindowDestroyed() if/when the window is destroyed
      // via the platform window destruction mechanism.
      platform_delegate_->CloseHostWindow();
    } else {
      // Keep a reference to the browser while it's in the process of being
      // destroyed.
      CefRefPtr<CefBrowserHostImpl> browser(this);

      // No window exists. Destroy the browser immediately. Don't call other
      // browser methods after calling DestroyBrowser().
      DestroyBrowser();
    }
  } else if (destruction_state_ != DESTRUCTION_STATE_NONE) {
    destruction_state_ = DESTRUCTION_STATE_NONE;
  }
}

void CefBrowserHostImpl::UpdateTargetURL(content::WebContents* source,
                                         const GURL& url) {
  if (client_.get()) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler.get())
      handler->OnStatusMessage(this, url.spec());
  }
}

bool CefBrowserHostImpl::DidAddMessageToConsole(
    content::WebContents* source,
    int32_t level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  if (client_.get()) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler.get())
      return handler->OnConsoleMessage(this, message, source_id, line_no);
  }

  return false;
}

void CefBrowserHostImpl::BeforeUnloadFired(content::WebContents* source,
                                           bool proceed,
                                           bool* proceed_to_fire_unload) {
  if (destruction_state_ == DESTRUCTION_STATE_ACCEPTED || proceed) {
    *proceed_to_fire_unload = true;
  } else if (!proceed) {
    *proceed_to_fire_unload = false;
    destruction_state_ = DESTRUCTION_STATE_NONE;
  }
}

bool CefBrowserHostImpl::TakeFocus(content::WebContents* source,
                                   bool reverse) {
  if (client_.get()) {
    CefRefPtr<CefFocusHandler> handler = client_->GetFocusHandler();
    if (handler.get())
      handler->OnTakeFocus(this, !reverse);
  }

  return false;
}

bool CefBrowserHostImpl::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return HandleContextMenu(web_contents(), params);
}

content::WebContents* CefBrowserHostImpl::GetActionableWebContents() {
  if (web_contents() && extensions::ExtensionsEnabled()) {
    content::WebContents* guest_contents =
        extensions::GetFullPageGuestForOwnerContents(web_contents());
    if (guest_contents)
      return guest_contents;
  }
  return web_contents();
}

KeyboardEventProcessingResult CefBrowserHostImpl::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (platform_delegate_ && client_.get()) {
    CefRefPtr<CefKeyboardHandler> handler = client_->GetKeyboardHandler();
    if (handler.get()) {
      CefKeyEvent cef_event;
      if (browser_util::GetCefKeyEvent(event, cef_event)) {
        cef_event.focus_on_editable_field = focus_on_editable_field_;

        CefEventHandle event_handle = platform_delegate_->GetEventHandle(event);
        bool is_keyboard_shortcut = false;
        bool result = handler->OnPreKeyEvent(this, cef_event, event_handle,
                                             &is_keyboard_shortcut);
        if (result)
          return KeyboardEventProcessingResult::HANDLED;
        else if (is_keyboard_shortcut)
          return KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT;
      }
    }
  }

  return KeyboardEventProcessingResult::NOT_HANDLED;
}

void CefBrowserHostImpl::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // Check to see if event should be ignored.
  if (event.skip_in_browser)
    return;

  if (!platform_delegate_)
    return;

  if (client_.get()) {
    CefRefPtr<CefKeyboardHandler> handler = client_->GetKeyboardHandler();
    if (handler.get()) {
      CefKeyEvent cef_event;
      if (browser_util::GetCefKeyEvent(event, cef_event)) {
        cef_event.focus_on_editable_field = focus_on_editable_field_;

        CefEventHandle event_handle = platform_delegate_->GetEventHandle(event);
        if (handler->OnKeyEvent(this, cef_event, event_handle))
          return;
      }
    }
  }

  platform_delegate_->HandleKeyboardEvent(event);
}

bool CefBrowserHostImpl::CanDragEnter(
    content::WebContents* source,
    const content::DropData& data,
    blink::WebDragOperationsMask mask) {
  CefRefPtr<CefDragHandler> handler = client_->GetDragHandler();
  if (handler.get()) {
    CefRefPtr<CefDragDataImpl> drag_data(new CefDragDataImpl(data));
    drag_data->SetReadOnly(true);
    if (handler->OnDragEnter(
        this,
        drag_data.get(),
        static_cast<CefDragHandler::DragOperationsMask>(mask))) {
      return false;
    }
  }
  return true;
}

void CefBrowserHostImpl::GetCustomWebContentsView(
    content::WebContents* web_contents,
    const GURL& target_url,
    int opener_render_process_id,
    int opener_render_frame_id,
    content::WebContentsView** view,
    content::RenderViewHostDelegateView** delegate_view) {
  CefBrowserInfoManager::GetInstance()->GetCustomWebContentsView(
      target_url, opener_render_process_id, opener_render_frame_id, view,
      delegate_view);
}

void CefBrowserHostImpl::WebContentsCreated(
    content::WebContents* source_contents,
    int opener_render_process_id,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  CefBrowserSettings settings;
  CefRefPtr<CefClient> client;
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate;

  CefBrowserInfoManager::GetInstance()->WebContentsCreated(
      target_url, opener_render_process_id, opener_render_frame_id, settings,
      client, platform_delegate);

  scoped_refptr<CefBrowserInfo> info =
      CefBrowserInfoManager::GetInstance()->CreatePopupBrowserInfo(
          new_contents, platform_delegate->IsWindowless());
  DCHECK(info.get());
  DCHECK(info->is_popup());

  CefRefPtr<CefBrowserHostImpl> opener = GetBrowserForContents(source_contents);
  DCHECK(opener.get());

  CefBrowserContext* browser_context =
      static_cast<CefBrowserContext*>(new_contents->GetBrowserContext());
  DCHECK(browser_context);
  CefRefPtr<CefRequestContext> request_context =
      CefRequestContextImpl::CreateForBrowserContext(browser_context).get();
  DCHECK(request_context.get());

  CefRefPtr<CefBrowserHostImpl> browser = CefBrowserHostImpl::CreateInternal(
      settings, client, new_contents, info, opener, false, request_context,
      std::move(platform_delegate));
}

void CefBrowserHostImpl::DidNavigateMainFramePostCommit(
    content::WebContents* web_contents) {
  base::AutoLock lock_scope(state_lock_);
  has_document_ = false;
}

content::JavaScriptDialogManager*
    CefBrowserHostImpl::GetJavaScriptDialogManager(
        content::WebContents* source) {
  if (!javascript_dialog_manager_.get() && platform_delegate_) {
    javascript_dialog_manager_.reset(
        new CefJavaScriptDialogManager(this,
              platform_delegate_->CreateJavaScriptDialogRunner()));
  }
  return javascript_dialog_manager_.get();
}

void CefBrowserHostImpl::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    const content::FileChooserParams& params) {
  EnsureFileDialogManager();
  file_dialog_manager_->RunFileChooser(render_frame_host, params);
}

bool CefBrowserHostImpl::HandleContextMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  CEF_REQUIRE_UIT();
  if (!menu_manager_.get() && platform_delegate_) {
    menu_manager_.reset(
        new CefMenuManager(this,
            platform_delegate_->CreateMenuRunner()));
  }
  return menu_manager_->CreateContextMenu(params);
}

void CefBrowserHostImpl::UpdatePreferredSize(content::WebContents* source,
                                             const gfx::Size& pref_size) {
#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))
  CEF_REQUIRE_UIT();
  if (platform_delegate_)
    platform_delegate_->SizeTo(pref_size.width(), pref_size.height());
#endif
}

void CefBrowserHostImpl::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  CEF_REQUIRE_UIT();

  content::MediaStreamDevices devices;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableMediaStream)) {
    // Cancel the request.
    callback.Run(devices, content::MEDIA_DEVICE_PERMISSION_DENIED,
                 std::unique_ptr<content::MediaStreamUI>());
    return;
  }

  // Based on chrome/browser/media/media_stream_devices_controller.cc
  bool microphone_requested =
      (request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE);
  bool webcam_requested =
      (request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE);
  bool screen_requested =
      (request.video_type == content::MEDIA_DESKTOP_VIDEO_CAPTURE);
  if (microphone_requested || webcam_requested || screen_requested) {
    // Pick the desired device or fall back to the first available of the
    // given type.
    if (microphone_requested) {
      CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request.requested_audio_device_id,
          true,
          false,
          &devices);
    }
    if (webcam_requested) {
      CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request.requested_video_device_id,
          false,
          true,
          &devices);
    }
    if (screen_requested) {
      content::DesktopMediaID media_id;
      if (request.requested_video_device_id.empty()) {
        media_id = content::DesktopMediaID(
            content::DesktopMediaID::TYPE_SCREEN,
            -1  /* webrtc::kFullDesktopScreenId */);
      } else {
        media_id =
            content::DesktopMediaID::Parse(request.requested_video_device_id);
      }
      devices.push_back(content::MediaStreamDevice(
          content::MEDIA_DESKTOP_VIDEO_CAPTURE, media_id.ToString(), "Screen"));
    }
  }

  callback.Run(devices, content::MEDIA_DEVICE_OK,
               std::unique_ptr<content::MediaStreamUI>());
}

bool CefBrowserHostImpl::CheckMediaAccessPermission(
   content::WebContents* web_contents,
   const GURL& security_origin,
   content::MediaStreamType type) {
  // Check media access permission without prompting the user. This is called
  // when loading the Pepper Flash plugin.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableMediaStream);
}


// content::WebContentsObserver methods.
// -----------------------------------------------------------------------------

void CefBrowserHostImpl::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  const int render_process_id = render_frame_host->GetProcess()->GetID();
  const int render_routing_id = render_frame_host->GetRoutingID();
  browser_info_->render_id_manager()->add_render_frame_id(
      render_process_id, render_routing_id);
}

void CefBrowserHostImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  const int render_process_id = render_frame_host->GetProcess()->GetID();
  const int render_routing_id = render_frame_host->GetRoutingID();
  browser_info_->render_id_manager()->remove_render_frame_id(
      render_process_id, render_routing_id);

  if (web_contents()) {
    const bool is_main_frame = (render_frame_host->GetParent() == nullptr);
    CefBrowserContext* context =
        static_cast<CefBrowserContext*>(web_contents()->GetBrowserContext());
    if (context) {
      context->OnRenderFrameDeleted(render_process_id, render_routing_id,
                                    is_main_frame, false);
    }
  }
}

void CefBrowserHostImpl::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  // The swapped out state of a RVH is determined by its main frame since
  // subframes should have their own widgets. We should never recieve creation
  // notifications for a RVH where the main frame is swapped out.
  content::RenderViewHostImpl* render_view_host_impl =
      static_cast<content::RenderViewHostImpl*>(render_view_host);
  DCHECK(!render_view_host_impl->is_swapped_out());

  const int render_process_id = render_view_host->GetProcess()->GetID();
  const int render_routing_id = render_view_host->GetRoutingID();
  browser_info_->render_id_manager()->add_render_view_id(
      render_process_id, render_routing_id);

  // May be already registered if the renderer crashed previously.
  if (!registrar_->IsRegistered(
      this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
      content::Source<content::RenderViewHost>(render_view_host))) {
    registrar_->Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                    content::Source<content::RenderViewHost>(render_view_host));
  }

  platform_delegate_->RenderViewCreated(render_view_host);
}

void CefBrowserHostImpl::RenderViewDeleted(
    content::RenderViewHost* render_view_host) {
  // The swapped out state of a RVH is determined by its main frame since
  // subframes should have their own widgets. Ignore deletion notification for
  // a RVH where the main frame host is swapped out. We probably shouldn't be
  // getting these notifications to begin with.
  content::RenderViewHostImpl* render_view_host_impl =
      static_cast<content::RenderViewHostImpl*>(render_view_host);
  if (render_view_host_impl->is_swapped_out())
    return;

  const int render_process_id = render_view_host->GetProcess()->GetID();
  const int render_routing_id = render_view_host->GetRoutingID();
  browser_info_->render_id_manager()->remove_render_view_id(
      render_process_id, render_routing_id);

  if (registrar_->IsRegistered(
      this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
      content::Source<content::RenderViewHost>(render_view_host))) {
    registrar_->Remove(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
        content::Source<content::RenderViewHost>(render_view_host));
  }
}

void CefBrowserHostImpl::RenderViewReady() {
  // Send the queued messages.
  queue_messages_ = false;
  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }

  if (client_.get()) {
    CefRefPtr<CefRequestHandler> handler = client_->GetRequestHandler();
    if (handler.get())
      handler->OnRenderViewReady(this);
  }
}

void CefBrowserHostImpl::RenderProcessGone(base::TerminationStatus status) {
  queue_messages_ = true;

  cef_termination_status_t ts = TS_ABNORMAL_TERMINATION;
  if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED)
    ts = TS_PROCESS_WAS_KILLED;
  else if (status == base::TERMINATION_STATUS_PROCESS_CRASHED)
    ts = TS_PROCESS_CRASHED;
  else if (status != base::TERMINATION_STATUS_ABNORMAL_TERMINATION)
    return;

  if (client_.get()) {
    CefRefPtr<CefRequestHandler> handler = client_->GetRequestHandler();
    if (handler.get()) {
      frame_destruction_pending_ = true;
      handler->OnRenderProcessTerminated(this, ts);
      frame_destruction_pending_ = false;
    }
  }
}

void CefBrowserHostImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // This method may be called with a nullptr RenderFrameHost (RFH) when a
  // provisional load is started. It should be called again with a non-nullptr
  // RFH once the provisional load is committed or if the provisional load
  // fails.
  if (!navigation_handle->GetRenderFrameHost())
    return;

  const net::Error error_code = navigation_handle->GetNetErrorCode();
  if (error_code == net::OK) {
    // The navigation has been committed.
    const bool is_main_frame = navigation_handle->IsInMainFrame();
    const GURL& url = navigation_handle->GetURL();

    // This also updates the URL associated with the frame.
    CefRefPtr<CefFrame> frame = GetOrCreateFrame(
        navigation_handle->GetRenderFrameHost()->GetRoutingID(),
        CefFrameHostImpl::kUnspecifiedFrameId,
        is_main_frame, base::string16(), url);

    // Don't call OnLoadStart for same page navigations (fragments,
    // history state).
    if (!navigation_handle->IsSameDocument())
      OnLoadStart(frame, navigation_handle->GetPageTransition());

    if (is_main_frame)
      OnAddressChange(frame, url);
  } else {
    // The navigation failed before commit. Originates from
    // RenderFrameHostImpl::OnDidFailProvisionalLoadWithError.
    CefRefPtr<CefFrame> frame = GetOrCreateFrame(
        navigation_handle->GetRenderFrameHost()->GetRoutingID(),
        CefFrameHostImpl::kUnspecifiedFrameId,
        navigation_handle->IsInMainFrame(), base::string16(), GURL());

    // OnLoadStart/OnLoadEnd will not be called.
    OnLoadError(frame, navigation_handle->GetURL(), error_code);
  }

  if (!web_contents())
    return;

  CefBrowserContext* context =
      static_cast<CefBrowserContext*>(web_contents()->GetBrowserContext());
  if (!context)
    return;

  context->AddVisitedURLs(navigation_handle->GetRedirectChain());
}

void CefBrowserHostImpl::DocumentAvailableInMainFrame() {
  base::AutoLock lock_scope(state_lock_);
  has_document_ = true;
}

void CefBrowserHostImpl::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  // The navigation failed after commit. OnLoadStart was called so we also call
  // OnLoadEnd.
  const bool is_main_frame = !render_frame_host->GetParent();
  CefRefPtr<CefFrame> frame = GetOrCreateFrame(
      render_frame_host->GetRoutingID(),
      CefFrameHostImpl::kUnspecifiedFrameId,
      is_main_frame,
      base::string16(),
      validated_url);
  OnLoadError(frame, validated_url, error_code);
  OnLoadEnd(frame, validated_url, error_code);
}

void CefBrowserHostImpl::FrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  base::AutoLock lock_scope(state_lock_);

  const int64 frame_id = render_frame_host->GetRoutingID();
  FrameMap::iterator it = frames_.find(frame_id);
  if (it != frames_.end()) {
    it->second->Detach();
    frames_.erase(it);
  }

  if (main_frame_id_ == frame_id)
    main_frame_id_ = CefFrameHostImpl::kInvalidFrameId;
  if (focused_frame_id_ ==  frame_id)
    focused_frame_id_ = CefFrameHostImpl::kInvalidFrameId;
}

void CefBrowserHostImpl::TitleWasSet(content::NavigationEntry* entry,
                                     bool explicit_set) {
  // |entry| may be NULL if a popup is created via window.open and never
  // navigated.
  if (entry)
    OnTitleChange(entry->GetTitle());
  else if (web_contents())
    OnTitleChange(web_contents()->GetTitle());
}

void CefBrowserHostImpl::PluginCrashed(const base::FilePath& plugin_path,
                                       base::ProcessId plugin_pid) {
  if (client_.get()) {
    CefRefPtr<CefRequestHandler> handler = client_->GetRequestHandler();
    if (handler.get())
      handler->OnPluginCrashed(this, plugin_path.value());
  }
}

void CefBrowserHostImpl::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  if (client_.get()) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler.get()) {
      std::vector<CefString> icon_urls;
      std::vector<content::FaviconURL>::const_iterator it =
          candidates.begin();
      for (; it != candidates.end(); ++it) {
        if (it->icon_type == content::FaviconURL::FAVICON)
          icon_urls.push_back(it->icon_url.spec());
      }
      if (!icon_urls.empty())
        handler->OnFaviconURLChange(this, icon_urls);
    }
  }
}

bool CefBrowserHostImpl::OnMessageReceived(const IPC::Message& message) {
  // Handle the cursor message here if mouse cursor change is disabled instead
  // of propegating the message to the normal handler.
  if (message.type() == ViewHostMsg_SetCursor::ID)
    return IsMouseCursorChangeDisabled();

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefBrowserHostImpl, message)
    IPC_MESSAGE_HANDLER(CefHostMsg_FrameIdentified, OnFrameIdentified)
    IPC_MESSAGE_HANDLER(CefHostMsg_DidFinishLoad, OnDidFinishLoad)
    IPC_MESSAGE_HANDLER(CefHostMsg_UpdateDraggableRegions,
                        OnUpdateDraggableRegions)
    IPC_MESSAGE_HANDLER(CefHostMsg_Request, OnRequest)
    IPC_MESSAGE_HANDLER(CefHostMsg_Response, OnResponse)
    IPC_MESSAGE_HANDLER(CefHostMsg_ResponseAck, OnResponseAck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CefBrowserHostImpl::OnWebContentsFocused() {
  if (client_.get()) {
    CefRefPtr<CefFocusHandler> handler = client_->GetFocusHandler();
    if (handler.get())
      handler->OnGotFocus(this);
  }
}

bool CefBrowserHostImpl::Send(IPC::Message* message) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (queue_messages_) {
      queued_messages_.push(message);
      return true;
    } else {
      return content::WebContentsObserver::Send(message);
    }
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(base::IgnoreResult(&CefBrowserHostImpl::Send), this,
                   message));
    return true;
  }
}

void CefBrowserHostImpl::AddObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.AddObserver(observer);
}

void CefBrowserHostImpl::RemoveObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.RemoveObserver(observer);
}

bool CefBrowserHostImpl::HasObserver(Observer* observer) const {
  CEF_REQUIRE_UIT();
  return observers_.HasObserver(observer);
}


// content::WebContentsObserver::OnMessageReceived() message handlers.
// -----------------------------------------------------------------------------

void CefBrowserHostImpl::OnFrameIdentified(int64 frame_id,
                                           int64 parent_frame_id,
                                           base::string16 name) {
  bool is_main_frame = (parent_frame_id == CefFrameHostImpl::kMainFrameId);
  GetOrCreateFrame(frame_id, parent_frame_id, is_main_frame, name, GURL());
}

void CefBrowserHostImpl::OnDidFinishLoad(int64 frame_id,
                                         const GURL& validated_url,
                                         bool is_main_frame,
                                         int http_status_code) {
  CefRefPtr<CefFrame> frame = GetOrCreateFrame(frame_id,
      CefFrameHostImpl::kUnspecifiedFrameId, is_main_frame, base::string16(),
      validated_url);

  // Give internal scheme handlers an opportunity to update content.
  scheme::DidFinishLoad(frame, validated_url);

  OnLoadEnd(frame, validated_url, http_status_code);
}

void CefBrowserHostImpl::OnUpdateDraggableRegions(
    const std::vector<Cef_DraggableRegion_Params>& regions) {
  std::vector<CefDraggableRegion> draggable_regions;
  draggable_regions.reserve(regions.size());

  std::vector<Cef_DraggableRegion_Params>::const_iterator it = regions.begin();
  for (; it != regions.end(); ++it) {
    const gfx::Rect& rect(it->bounds);
    const CefRect bounds(rect.x(), rect.y(), rect.width(), rect.height());
    draggable_regions.push_back(CefDraggableRegion(bounds, it->draggable));
  }

  if (client_.get()) {
    CefRefPtr<CefDragHandler> handler = client_->GetDragHandler();
    if (handler.get()) {
      handler->OnDraggableRegionsChanged(this, draggable_regions);
    }
  }
}

void CefBrowserHostImpl::OnRequest(const Cef_Request_Params& params) {
  bool success = false;
  std::string response;
  bool expect_response_ack = false;

  if (params.user_initiated) {
    // Give the user a chance to handle the request.
    if (client_.get()) {
      CefRefPtr<CefProcessMessageImpl> message(
          new CefProcessMessageImpl(const_cast<Cef_Request_Params*>(&params),
                                    false, true));
      success = client_->OnProcessMessageReceived(this, PID_RENDERER,
                                                  message.get());
      message->Detach(NULL);
    }
  } else {
    // Invalid request.
    NOTREACHED();
  }

  if (params.expect_response) {
    DCHECK_GE(params.request_id, 0);

    // Send a response to the renderer.
    Cef_Response_Params response_params;
    response_params.request_id = params.request_id;
    response_params.success = success;
    response_params.response = response;
    response_params.expect_response_ack = expect_response_ack;
    Send(new CefMsg_Response(routing_id(), response_params));
  }
}

void CefBrowserHostImpl::OnResponse(const Cef_Response_Params& params) {
  response_manager_->RunHandler(params);
  if (params.expect_response_ack)
    Send(new CefMsg_ResponseAck(routing_id(), params.request_id));
}

void CefBrowserHostImpl::OnResponseAck(int request_id) {
  response_manager_->RunAckHandler(request_id);
}


// content::NotificationObserver methods.
// -----------------------------------------------------------------------------

void CefBrowserHostImpl::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_LOAD_STOP ||
         type == content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE);

   if (type == content::NOTIFICATION_LOAD_STOP) {
    content::NavigationController* controller =
        content::Source<content::NavigationController>(source).ptr();
    OnTitleChange(controller->GetWebContents()->GetTitle());
  } else if (type == content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE) {
    focus_on_editable_field_ = *content::Details<bool>(details).ptr();
  }
}


// CefBrowserHostImpl private methods.
// -----------------------------------------------------------------------------

CefBrowserHostImpl::CefBrowserHostImpl(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* web_contents,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<CefBrowserHostImpl> opener,
    CefRefPtr<CefRequestContext> request_context,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate)
    : content::WebContentsObserver(web_contents),
      settings_(settings),
      client_(client),
      browser_info_(browser_info),
      opener_(kNullWindowHandle),
      request_context_(request_context),
      platform_delegate_(std::move(platform_delegate)),
      is_windowless_(platform_delegate_->IsWindowless()),
      is_views_hosted_(platform_delegate_->IsViewsHosted()),
      host_window_handle_(kNullWindowHandle),
      is_loading_(false),
      can_go_back_(false),
      can_go_forward_(false),
      has_document_(false),
      is_fullscreen_(false),
      queue_messages_(true),
      main_frame_id_(CefFrameHostImpl::kInvalidFrameId),
      focused_frame_id_(CefFrameHostImpl::kInvalidFrameId),
      destruction_state_(DESTRUCTION_STATE_NONE),
      frame_destruction_pending_(false),
      window_destroyed_(false),
      is_in_onsetfocus_(false),
      focus_on_editable_field_(false),
      mouse_cursor_change_disabled_(false),
      devtools_frontend_(NULL) {
  if (opener.get() && !platform_delegate_->IsViewsHosted()) {
    // GetOpenerWindowHandle() only returns a value for non-views-hosted
    // popup browsers.
    opener_ = opener->GetWindowHandle();
  }

  DCHECK(!browser_info_->browser().get());
  browser_info_->set_browser(this);

  web_contents_.reset(web_contents);
  web_contents->SetDelegate(this);

  // Associate the WebContents with this browser object.
  WebContentsUserDataAdapter::Register(this);

  registrar_.reset(new content::NotificationRegistrar);

  // When navigating through the history, the restored NavigationEntry's title
  // will be used. If the entry ends up having the same title after we return
  // to it, as will usually be the case, the
  // NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED will then be suppressed, since the
  // NavigationEntry's title hasn't changed.
  registrar_->Add(this, content::NOTIFICATION_LOAD_STOP,
                  content::Source<content::NavigationController>(
                      &web_contents->GetController()));

  response_manager_.reset(new CefResponseManager);

  placeholder_frame_ =
      new CefFrameHostImpl(this, CefFrameHostImpl::kInvalidFrameId, true,
                           CefString(), CefString(),
                           CefFrameHostImpl::kInvalidFrameId);

  PrefsTabHelper::CreateForWebContents(web_contents_.get());
  printing::CefPrintViewManager::CreateForWebContents(web_contents_.get());

  if (extensions::ExtensionsEnabled()) {
    // Used by the tabs extension API.
    SessionTabHelper::CreateForWebContents(web_contents_.get());
    zoom::ZoomController::CreateForWebContents(web_contents_.get());
  }

  // Make sure RenderViewCreated is called at least one time.
  RenderViewCreated(web_contents->GetRenderViewHost());

  // Associate the platform delegate with this browser.
  platform_delegate_->BrowserCreated(this);
}

bool CefBrowserHostImpl::CreateHostWindow() {
  // |host_window_handle_| will not change after initial host creation for
  // non-views-hosted browsers.
  bool success = true;
  if (!IsWindowless())
    success = platform_delegate_->CreateHostWindow();
  if (success && !IsViewsHosted())
    host_window_handle_ = platform_delegate_->GetHostWindowHandle();
  return success;
}

CefRefPtr<CefFrame> CefBrowserHostImpl::GetOrCreateFrame(
    int64 frame_id, int64 parent_frame_id, bool is_main_frame,
    base::string16 frame_name, const GURL& frame_url) {
  DCHECK(frame_id > CefFrameHostImpl::kInvalidFrameId);
  if (frame_id <= CefFrameHostImpl::kInvalidFrameId)
    return nullptr;

  CefString url;
  if (frame_url.is_valid())
    url = frame_url.spec();

  CefString name;
  if (!frame_name.empty())
    name = frame_name;

  CefRefPtr<CefFrameHostImpl> frame;
  bool frame_created = false;

  {
    base::AutoLock lock_scope(state_lock_);

    if (is_main_frame && main_frame_id_ != frame_id) {
      if (main_frame_id_ != CefFrameHostImpl::kInvalidFrameId) {
        // Remove the old main frame object before adding the new one.
        FrameMap::iterator it = frames_.find(main_frame_id_);
        if (it != frames_.end()) {
          // Persist URL and name to the new main frame.
          if (url.empty())
            url = it->second->GetURL();
          if (name.empty())
            name = it->second->GetName();

          it->second->Detach();
          frames_.erase(it);
        }

        if (focused_frame_id_ ==  main_frame_id_)
          focused_frame_id_ = frame_id;
      }
      main_frame_id_ = frame_id;
    }

    // Check if a frame object already exists.
    FrameMap::const_iterator it = frames_.find(frame_id);
    if (it != frames_.end())
      frame = it->second.get();

    if (!frame.get()) {
      frame = new CefFrameHostImpl(this, frame_id, is_main_frame, url, name,
                                   parent_frame_id);
      frame_created = true;
      frames_.insert(std::make_pair(frame_id, frame));
    }
  }

  if (!frame_created)
    frame->SetAttributes(url, name, parent_frame_id);

  return frame.get();
}

void CefBrowserHostImpl::DetachAllFrames() {
  FrameMap frames;

  {
    base::AutoLock lock_scope(state_lock_);

    frames = frames_;
    frames_.clear();

    if (main_frame_id_ != CefFrameHostImpl::kInvalidFrameId)
      main_frame_id_ = CefFrameHostImpl::kInvalidFrameId;
    if (focused_frame_id_ != CefFrameHostImpl::kInvalidFrameId)
      focused_frame_id_ = CefFrameHostImpl::kInvalidFrameId;
  }

  FrameMap::const_iterator it = frames.begin();
  for (; it != frames.end(); ++it)
    it->second->Detach();
}

void CefBrowserHostImpl::SetFocusedFrame(int64 frame_id) {
  CefRefPtr<CefFrameHostImpl> unfocused_frame;
  CefRefPtr<CefFrameHostImpl> focused_frame;

  {
    base::AutoLock lock_scope(state_lock_);

    if (focused_frame_id_ != CefFrameHostImpl::kInvalidFrameId) {
      // Unfocus the previously focused frame.
      FrameMap::const_iterator it = frames_.find(frame_id);
      if (it != frames_.end())
        unfocused_frame = it->second;
    }

    if (frame_id != CefFrameHostImpl::kInvalidFrameId) {
      // Focus the newly focused frame.
      FrameMap::iterator it = frames_.find(frame_id);
      if (it != frames_.end())
        focused_frame = it->second;
    }

    focused_frame_id_ =
        focused_frame.get() ? frame_id : CefFrameHostImpl::kInvalidFrameId;
  }

  if (unfocused_frame.get())
    unfocused_frame->SetFocused(false);
  if (focused_frame.get())
    focused_frame->SetFocused(true);
}

gfx::Point CefBrowserHostImpl::GetScreenPoint(const gfx::Point& view) const {
  CEF_REQUIRE_UIT();
  if (platform_delegate_)
    return platform_delegate_->GetScreenPoint(view);
  return gfx::Point();
}

void CefBrowserHostImpl::StartDragging(
    const content::DropData& drop_data,
    blink::WebDragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const content::DragEventSourceInfo& event_info,
    content::RenderWidgetHostImpl* source_rwh) {
  if (platform_delegate_) {
    platform_delegate_->StartDragging(drop_data, allowed_ops, image,
                                      image_offset, event_info, source_rwh);
  }
}

void CefBrowserHostImpl::UpdateDragCursor(blink::WebDragOperation operation) {
  if (platform_delegate_)
    platform_delegate_->UpdateDragCursor(operation);
}

void CefBrowserHostImpl::OnAddressChange(CefRefPtr<CefFrame> frame,
                                         const GURL& url) {
  if (client_.get()) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler.get()) {
      // Notify the handler of an address change.
      handler->OnAddressChange(this, GetMainFrame(), url.spec());
    }
  }
}

void CefBrowserHostImpl::OnLoadStart(CefRefPtr<CefFrame> frame,
                                     ui::PageTransition transition_type) {
  if (client_.get()) {
    CefRefPtr<CefLoadHandler> handler = client_->GetLoadHandler();
    if (handler.get()) {
      // Notify the handler that loading has started.
      handler->OnLoadStart(this, frame,
                           static_cast<cef_transition_type_t>(transition_type));
    }
  }
}

void CefBrowserHostImpl::OnLoadError(CefRefPtr<CefFrame> frame,
                                     const GURL& url,
                                     int error_code) {
  if (client_.get()) {
    CefRefPtr<CefLoadHandler> handler = client_->GetLoadHandler();
    if (handler.get()) {
      frame_destruction_pending_ = true;
      // Notify the handler that loading has failed.
      handler->OnLoadError(this, frame,
          static_cast<cef_errorcode_t>(error_code),
          net::ErrorToShortString(error_code),
          url.spec());
      frame_destruction_pending_ = false;
    }
  }
}

void CefBrowserHostImpl::OnLoadEnd(CefRefPtr<CefFrame> frame,
                                   const GURL& url,
                                   int http_status_code) {
  if (client_.get()) {
    CefRefPtr<CefLoadHandler> handler = client_->GetLoadHandler();
    if (handler.get())
      handler->OnLoadEnd(this, frame, http_status_code);
  }
}

void CefBrowserHostImpl::OnFullscreenModeChange(bool fullscreen) {
  if (is_fullscreen_ == fullscreen)
    return;

  is_fullscreen_ = fullscreen;
  WasResized();

  if (client_.get()) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler.get())
      handler->OnFullscreenModeChange(this, fullscreen);
  }
}

void CefBrowserHostImpl::OnTitleChange(const base::string16& title) {
  if (client_.get()) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler.get())
      handler->OnTitleChange(this, title);
  }
}

void CefBrowserHostImpl::OnDevToolsWebContentsDestroyed() {
  devtools_observer_.reset();
  devtools_frontend_ = NULL;
}

void CefBrowserHostImpl::EnsureFileDialogManager() {
  CEF_REQUIRE_UIT();
  if (!file_dialog_manager_.get() && platform_delegate_) {
    file_dialog_manager_.reset(
        new CefFileDialogManager(this,
            platform_delegate_->CreateFileDialogRunner()));
  }
}
