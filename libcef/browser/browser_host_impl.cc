// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#include <string>
#include <utility>

#include "libcef/browser/audio_mirror_destination.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/browser_util.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools/devtools_frontend.h"
#include "libcef/browser/devtools/devtools_manager_delegate.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/browser/extensions/extension_background_host.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/extensions/extension_view_host.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/image_impl.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
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
#include "libcef/common/request_impl.h"
#include "libcef/common/values_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "components/zoom/zoom_controller.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/file_select_listener.h"
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
#include "content/public/common/favicon_url.h"
#include "extensions/browser/process_manager.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"
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
  WebContentsUserDataAdapter(CefBrowserHostImpl* browser) : browser_(browser) {
    browser->web_contents()->SetUserData(UserDataKey(), base::WrapUnique(this));
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
                      CefRefPtr<CefDictionaryValue> extra_info,
                      CefRefPtr<CefRequestContext> request_context)
      : window_info_(windowInfo),
        client_(client),
        url_(url),
        settings_(settings),
        extra_info_(extra_info),
        request_context_(request_context) {}

  CefWindowInfo window_info_;
  CefRefPtr<CefClient> client_;
  CefString url_;
  CefBrowserSettings settings_;
  CefRefPtr<CefDictionaryValue> extra_info_;
  CefRefPtr<CefRequestContext> request_context_;
};

void CreateBrowserWithHelper(CreateBrowserHelper* helper) {
  CefBrowserHost::CreateBrowserSync(
      helper->window_info_, helper->client_, helper->url_, helper->settings_,
      helper->extra_info_, helper->request_context_);
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
                                 helper->settings_,
                                 helper->inspect_element_at_);
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
    CefRefPtr<CefDictionaryValue> extra_info,
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

  if (windowInfo.windowless_rendering_enabled &&
      !CefContext::Get()->settings().windowless_rendering_enabled) {
    LOG(ERROR) << "Creating a windowless browser without setting "
                  "CefSettings.windowless_rendering_enabled may result in "
                  "reduced performance or runtime errors.";
  }

  // Create the browser on the UI thread.
  CreateBrowserHelper* helper = new CreateBrowserHelper(
      windowInfo, client, url, settings, extra_info, request_context);
  CEF_POST_TASK(CEF_UIT, base::BindOnce(CreateBrowserWithHelper, helper));

  return true;
}

// static
CefRefPtr<CefBrowser> CefBrowserHost::CreateBrowserSync(
    const CefWindowInfo& windowInfo,
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
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
  create_params.url = GURL(url.ToString());
  if (!url.empty() && !create_params.url.is_valid() &&
      !create_params.url.has_scheme()) {
    std::string new_url = std::string("http://") + url.ToString();
    create_params.url = GURL(new_url);
  }
  create_params.settings = settings;
  create_params.extra_info = extra_info;
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
          is_devtools_popup, platform_delegate->IsWindowless(),
          create_params.extra_info);

  // Get or create the request context and browser context.
  CefRefPtr<CefRequestContextImpl> request_context_impl =
      CefRequestContextImpl::GetOrCreateForRequestContext(
          create_params.request_context);
  DCHECK(request_context_impl);
  CefBrowserContext* browser_context =
      request_context_impl->GetBrowserContext();
  DCHECK(browser_context);

  if (!create_params.request_context) {
    // Using the global request context.
    create_params.request_context = request_context_impl.get();
  }

  CefRefPtr<CefExtension> cef_extension;
  scoped_refptr<content::SiteInstance> site_instance;
  if (extensions::ExtensionsEnabled() && !create_params.url.is_empty()) {
    if (!create_params.extension) {
      // We might be loading an extension app view where the extension URL is
      // provided by the client.
      create_params.extension =
          extensions::GetExtensionForUrl(browser_context, create_params.url);
    }
    if (create_params.extension) {
      cef_extension = browser_context->extension_system()->GetExtension(
          create_params.extension->id());
      DCHECK(cef_extension);

      if (create_params.extension_host_type == extensions::VIEW_TYPE_INVALID) {
        // Default to dialog behavior.
        create_params.extension_host_type =
            extensions::VIEW_TYPE_EXTENSION_DIALOG;
      }

      // Extension resources will fail to load if we don't use a SiteInstance
      // associated with the extension.
      // (CefContentBrowserClient::SiteInstanceGotProcess won't find the
      // extension to register with InfoMap, and AllowExtensionResourceLoad in
      // ExtensionProtocolHandler::MaybeCreateJob will return false resulting in
      // ERR_BLOCKED_BY_CLIENT).
      site_instance = extensions::ProcessManager::Get(browser_context)
                          ->GetSiteInstanceForURL(create_params.url);
      DCHECK(site_instance);
    }
  }

  content::WebContents::CreateParams wc_create_params(browser_context,
                                                      site_instance);

  if (platform_delegate->IsWindowless()) {
    // Create the OSR view for the WebContents.
    platform_delegate->CreateViewForWebContents(
        &wc_create_params.view, &wc_create_params.delegate_view);
  }

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(wc_create_params);
  DCHECK(web_contents);

  CefRefPtr<CefBrowserHostImpl> browser = CreateInternal(
      create_params.settings, create_params.client, web_contents.release(),
      true, info, create_params.devtools_opener, is_devtools_popup,
      static_cast<CefRequestContextImpl*>(create_params.request_context.get()),
      std::move(platform_delegate), cef_extension);
  if (!browser)
    return nullptr;

  if (create_params.extension) {
    browser->CreateExtensionHost(create_params.extension, browser_context,
                                 browser->web_contents(), create_params.url,
                                 create_params.extension_host_type);
  } else if (!create_params.url.is_empty()) {
    browser->LoadMainFrameURL(create_params.url.spec(), content::Referrer(),
                              ui::PAGE_TRANSITION_TYPED, std::string());
  }

  return browser.get();
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::CreateInternal(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* web_contents,
    bool own_web_contents,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<CefBrowserHostImpl> opener,
    bool is_devtools_popup,
    CefRefPtr<CefRequestContextImpl> request_context,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
    CefRefPtr<CefExtension> extension) {
  CEF_REQUIRE_UIT();
  DCHECK(web_contents);
  DCHECK(browser_info);
  DCHECK(request_context);
  DCHECK(platform_delegate);

  // If |opener| is non-NULL it must be a popup window.
  DCHECK(!opener.get() || browser_info->is_popup());

  if (opener) {
    if (!opener->platform_delegate_) {
      // The opener window is being destroyed. Cancel the popup.
      return nullptr;
    }

    // Give the opener browser's platform delegate an opportunity to modify the
    // new browser's platform delegate.
    opener->platform_delegate_->PopupWebContentsCreated(
        settings, client, web_contents, platform_delegate.get(),
        is_devtools_popup);
  }

  platform_delegate->WebContentsCreated(web_contents);

  CefRefPtr<CefBrowserHostImpl> browser = new CefBrowserHostImpl(
      settings, client, web_contents, browser_info, opener, request_context,
      std::move(platform_delegate), extension);
  if (own_web_contents)
    browser->set_owned_web_contents(web_contents);
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

  if (opener && opener->platform_delegate_) {
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
  content::WebContents* web_contents = content::WebContents::FromRenderViewHost(
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
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForFrameTreeNode(
    int frame_tree_node_id) {
  // Use the thread-safe approach.
  scoped_refptr<CefBrowserInfo> info =
      CefBrowserInfoManager::GetInstance()->GetBrowserInfoForFrameTreeNode(
          frame_tree_node_id);
  if (info.get()) {
    CefRefPtr<CefBrowserHostImpl> browser = info->browser();
    if (!browser.get()) {
      LOG(WARNING) << "Found browser id " << info->browser_id()
                   << " but no browser object matching frame tree node id "
                   << frame_tree_node_id;
    }
    return browser;
  }

  return nullptr;
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForFrameRoute(
    int render_process_id,
    int render_routing_id) {
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
        CefBrowserInfoManager::GetInstance()->GetBrowserInfoForFrameRoute(
            render_process_id, render_routing_id, &is_guest_view);
    if (info.get() && !is_guest_view) {
      CefRefPtr<CefBrowserHostImpl> browser = info->browser();
      if (!browser.get()) {
        LOG(WARNING) << "Found browser id " << info->browser_id()
                     << " but no browser object matching frame process id "
                     << render_process_id << " and routing id "
                     << render_routing_id;
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
class CefBrowserHostImpl::DevToolsWebContentsObserver
    : public content::WebContentsObserver {
 public:
  DevToolsWebContentsObserver(CefBrowserHostImpl* browser,
                              content::WebContents* frontend_web_contents)
      : WebContentsObserver(frontend_web_contents), browser_(browser) {}

  // WebContentsObserver methods:
  void WebContentsDestroyed() override {
    browser_->OnDevToolsWebContentsDestroyed();
  }

 private:
  CefBrowserHostImpl* browser_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsWebContentsObserver);
};

CefBrowserHostImpl::~CefBrowserHostImpl() {}

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
      destruction_state_ = (force_close ? DESTRUCTION_STATE_ACCEPTED
                                        : DESTRUCTION_STATE_PENDING);
    }

    content::WebContents* contents = web_contents();
    if (contents && contents->NeedToFireBeforeUnload()) {
      // Will result in a call to BeforeUnloadFired() and, if the close isn't
      // canceled, CloseContents().
      contents->DispatchBeforeUnload(false /* auto_cancel */);
    } else {
      CloseContents(contents);
    }
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::CloseBrowser,
                                          this, force_close));
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
                  base::BindOnce(&CefBrowserHostImpl::SetFocus, this, focus));
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
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::SetZoomLevel,
                                          this, zoomLevel));
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
                  base::BindOnce(&CefBrowserHostImpl::RunFileDialog, this, mode,
                                 title, default_file_path, accept_filters,
                                 selected_accept_filter, callback));
    return;
  }

  EnsureFileDialogManager();
  file_dialog_manager_->RunFileDialog(mode, title, default_file_path,
                                      accept_filters, selected_accept_filter,
                                      callback);
}

void CefBrowserHostImpl::StartDownload(const CefString& url) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefBrowserHostImpl::StartDownload, this, url));
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

  std::unique_ptr<download::DownloadUrlParameters> params(
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents(), gurl, NO_TRAFFIC_ANNOTATION_YET));
  manager->DownloadUrl(std::move(params));
}

void CefBrowserHostImpl::DownloadImage(
    const CefString& image_url,
    bool is_favicon,
    uint32 max_image_size,
    bool bypass_cache,
    CefRefPtr<CefDownloadImageCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::DownloadImage, this, image_url,
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
      bypass_cache, base::BindOnce(OnDownloadImage, max_image_size, callback));
}

void CefBrowserHostImpl::Print() {
  if (CEF_CURRENTLY_ON_UIT()) {
    content::WebContents* actionable_contents = GetActionableWebContents();
    if (!actionable_contents)
      return;
    printing::CefPrintViewManager::FromWebContents(actionable_contents)
        ->PrintNow(actionable_contents->GetRenderViewHost()->GetMainFrame());
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::Print, this));
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
    printing::CefPrintViewManager::FromWebContents(actionable_contents)
        ->PrintToPDF(actionable_contents->GetMainFrame(), base::FilePath(path),
                     settings, pdf_callback);
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::PrintToPDF, this,
                                          path, settings, callback));
  }
}

void CefBrowserHostImpl::Find(int identifier,
                              const CefString& searchText,
                              bool forward,
                              bool matchCase,
                              bool findNext) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (!web_contents())
      return;

    // Every find request must have a unique ID and these IDs must strictly
    // increase so that newer requests always have greater IDs than older
    // requests.
    if (identifier <= find_request_id_counter_)
      identifier = ++find_request_id_counter_;
    else
      find_request_id_counter_ = identifier;

    auto options = blink::mojom::FindOptions::New();
    options->forward = forward;
    options->match_case = matchCase;
    options->find_next = findNext;
    web_contents()->Find(identifier, searchText, std::move(options));
  } else {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::Find, this, identifier,
                                 searchText, forward, matchCase, findNext));
  }
}

void CefBrowserHostImpl::StopFinding(bool clearSelection) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (!web_contents())
      return;

    content::StopFindAction action =
        clearSelection ? content::STOP_FIND_ACTION_CLEAR_SELECTION
                       : content::STOP_FIND_ACTION_KEEP_SELECTION;
    web_contents()->StopFinding(action);
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::StopFinding,
                                          this, clearSelection));
  }
}

void CefBrowserHostImpl::ShowDevTools(const CefWindowInfo& windowInfo,
                                      CefRefPtr<CefClient> client,
                                      const CefBrowserSettings& settings,
                                      const CefPoint& inspect_element_at) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (!web_contents())
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
    ShowDevToolsHelper* helper = new ShowDevToolsHelper(
        this, windowInfo, client, settings, inspect_element_at);
    CEF_POST_TASK(CEF_UIT, base::BindOnce(ShowDevToolsWithHelper, helper));
  }
}

void CefBrowserHostImpl::CloseDevTools() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (!devtools_frontend_)
      return;
    devtools_frontend_->Close();
  } else {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::CloseDevTools, this));
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
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefBrowserHostImpl::GetNavigationEntries, this,
                                visitor, current_only));
    return;
  }

  if (!web_contents())
    return;

  content::NavigationController& controller = web_contents()->GetController();
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

void CefBrowserHostImpl::SetAccessibilityState(
    cef_state_t accessibility_state) {
  // Do nothing if state is set to default. It'll be disabled by default and
  // controlled by the commmand-line flags "force-renderer-accessibility" and
  // "disable-renderer-accessibility".
  if (accessibility_state == STATE_DEFAULT)
    return;

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::SetAccessibilityState,
                                 this, accessibility_state));
    return;
  }

  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents());

  if (!web_contents_impl)
    return;

  ui::AXMode accMode;
  // In windowless mode set accessibility to TreeOnly mode. Else native
  // accessibility APIs, specific to each platform, are also created.
  if (accessibility_state == STATE_ENABLED) {
    accMode = IsWindowless() ? ui::kAXModeWebContentsOnly : ui::kAXModeComplete;
  }
  web_contents_impl->SetAccessibilityMode(accMode);
}

void CefBrowserHostImpl::SetAutoResizeEnabled(bool enabled,
                                              const CefSize& min_size,
                                              const CefSize& max_size) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefBrowserHostImpl::SetAutoResizeEnabled, this,
                                enabled, min_size, max_size));
    return;
  }

  if (enabled == auto_resize_enabled_)
    return;

  auto_resize_enabled_ = enabled;
  if (enabled) {
    auto_resize_min_ = gfx::Size(min_size.width, min_size.height);
    auto_resize_max_ = gfx::Size(max_size.width, max_size.height);
  } else {
    auto_resize_min_ = auto_resize_max_ = gfx::Size();
  }
  ConfigureAutoResize();
}

CefRefPtr<CefExtension> CefBrowserHostImpl::GetExtension() {
  return extension_;
}

bool CefBrowserHostImpl::IsBackgroundHost() {
  return is_background_host_;
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
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::ReplaceMisspelling, this, word));
    return;
  }

  if (web_contents())
    web_contents()->ReplaceMisspelling(word);
}

void CefBrowserHostImpl::AddWordToDictionary(const CefString& word) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::AddWordToDictionary, this, word));
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
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::WasResized, this));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->SynchronizeVisualProperties();
}

void CefBrowserHostImpl::WasHidden(bool hidden) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHost::WasHidden, this, hidden));
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
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::NotifyScreenInfoChanged, this));
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
                  base::BindOnce(&CefBrowserHostImpl::Invalidate, this, type));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->Invalidate(type);
}

void CefBrowserHostImpl::SendExternalBeginFrame() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::Bind(&CefBrowserHostImpl::SendExternalBeginFrame, this));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->SendExternalBeginFrame();
}

void CefBrowserHostImpl::SendKeyEvent(const CefKeyEvent& event) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::SendKeyEvent,
                                          this, event));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  content::NativeWebKeyboardEvent web_event(blink::WebInputEvent::kUndefined,
                                            blink::WebInputEvent::kNoModifiers,
                                            ui::EventTimeForNow());
  platform_delegate_->TranslateKeyEvent(web_event, event);
  platform_delegate_->SendKeyEvent(web_event);
}

void CefBrowserHostImpl::SendMouseClickEvent(const CefMouseEvent& event,
                                             MouseButtonType type,
                                             bool mouseUp,
                                             int clickCount) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::SendMouseClickEvent, this,
                                 event, type, mouseUp, clickCount));
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
                  base::BindOnce(&CefBrowserHostImpl::SendMouseMoveEvent, this,
                                 event, mouseLeave));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  blink::WebMouseEvent web_event;
  platform_delegate_->TranslateMoveEvent(web_event, event, mouseLeave);
  platform_delegate_->SendMouseEvent(web_event);
}

void CefBrowserHostImpl::SendMouseWheelEvent(const CefMouseEvent& event,
                                             int deltaX,
                                             int deltaY) {
  if (deltaX == 0 && deltaY == 0) {
    // Nothing to do.
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::SendMouseWheelEvent, this,
                                 event, deltaX, deltaY));
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
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::SendCaptureLostEvent, this));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->SendCaptureLostEvent();
}

void CefBrowserHostImpl::NotifyMoveOrResizeStarted() {
#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::NotifyMoveOrResizeStarted, this));
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
                  base::BindOnce(&CefBrowserHostImpl::SetWindowlessFrameRate,
                                 this, frame_rate));
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
    if (navigation_locked()) {
      // Try again after the lock has been released.
      set_pending_navigation_action(
          base::BindOnce(&CefBrowserHostImpl::GoBack, this));
      return;
    }

    if (web_contents() && web_contents()->GetController().CanGoBack())
      web_contents()->GetController().GoBack();
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::GoBack, this));
  }
}

bool CefBrowserHostImpl::CanGoForward() {
  base::AutoLock lock_scope(state_lock_);
  return can_go_forward_;
}

void CefBrowserHostImpl::GoForward() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (navigation_locked()) {
      // Try again after the lock has been released.
      set_pending_navigation_action(
          base::BindOnce(&CefBrowserHostImpl::GoForward, this));
      return;
    }

    if (web_contents() && web_contents()->GetController().CanGoForward())
      web_contents()->GetController().GoForward();
  } else {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::GoForward, this));
  }
}

bool CefBrowserHostImpl::IsLoading() {
  base::AutoLock lock_scope(state_lock_);
  return is_loading_;
}

void CefBrowserHostImpl::Reload() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (navigation_locked()) {
      // Try again after the lock has been released.
      set_pending_navigation_action(
          base::BindOnce(&CefBrowserHostImpl::Reload, this));
      return;
    }

    if (web_contents())
      web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::Reload, this));
  }
}

void CefBrowserHostImpl::ReloadIgnoreCache() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (navigation_locked()) {
      // Try again after the lock has been released.
      set_pending_navigation_action(
          base::BindOnce(&CefBrowserHostImpl::ReloadIgnoreCache, this));
      return;
    }

    if (web_contents()) {
      web_contents()->GetController().Reload(
          content::ReloadType::BYPASSING_CACHE, true);
    }
  } else {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::ReloadIgnoreCache, this));
  }
}

void CefBrowserHostImpl::StopLoad() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (navigation_locked()) {
      // Try again after the lock has been released.
      set_pending_navigation_action(
          base::BindOnce(&CefBrowserHostImpl::StopLoad, this));
      return;
    }

    if (web_contents())
      web_contents()->Stop();
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::StopLoad, this));
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
  if (identifier == CefFrameHostImpl::kInvalidFrameId) {
    return nullptr;
  } else if (identifier == CefFrameHostImpl::kMainFrameId) {
    return browser_info_->GetMainFrame();
  } else if (identifier == CefFrameHostImpl::kFocusedFrameId) {
    base::AutoLock lock_scope(state_lock_);
    if (!focused_frame_) {
      // The main frame is focused by default.
      return browser_info_->GetMainFrame();
    }
    return focused_frame_;
  }

  return browser_info_->GetFrameForId(identifier);
}

CefRefPtr<CefFrame> CefBrowserHostImpl::GetFrame(const CefString& name) {
  for (const auto& frame : browser_info_->GetAllFrames()) {
    if (frame->GetName() == name)
      return frame;
  }
  return nullptr;
}

size_t CefBrowserHostImpl::GetFrameCount() {
  return browser_info_->GetAllFrames().size();
}

void CefBrowserHostImpl::GetFrameIdentifiers(std::vector<int64>& identifiers) {
  if (identifiers.size() > 0)
    identifiers.clear();

  const auto frames = browser_info_->GetAllFrames();
  if (frames.empty())
    return;

  identifiers.reserve(frames.size());
  for (const auto& frame : frames) {
    identifiers.push_back(frame->GetIdentifier());
  }
}

void CefBrowserHostImpl::GetFrameNames(std::vector<CefString>& names) {
  if (names.size() > 0)
    names.clear();

  const auto frames = browser_info_->GetAllFrames();
  if (frames.empty())
    return;

  names.reserve(frames.size());
  for (const auto& frame : frames) {
    names.push_back(frame->GetName());
  }
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
  DestroyExtensionHost();

  StopAudioMirroring();

  // Notify any observers that may have state associated with this browser.
  for (auto& observer : observers_)
    observer.OnBrowserDestroyed(this);

  // Disassociate the platform delegate from this browser.
  platform_delegate_->BrowserDestroyed(this);

  registrar_.reset(NULL);
  content::WebContentsObserver::Observe(NULL);
  if (owned_web_contents_)
    owned_web_contents_.reset(NULL);

  // Delete objects created by the platform delegate that may be referenced by
  // the WebContents.
  file_dialog_manager_.reset(NULL);
  javascript_dialog_manager_.reset(NULL);
  menu_manager_.reset(NULL);

  // Delete the platform delegate.
  platform_delegate_.reset(NULL);

  CefBrowserInfoManager::GetInstance()->RemoveBrowserInfo(browser_info_);
  browser_info_->SetBrowser(nullptr);
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

CefRefPtr<CefFrame> CefBrowserHostImpl::GetFrameForHost(
    const content::RenderFrameHost* host) {
  CEF_REQUIRE_UIT();
  if (!host)
    return nullptr;

  return browser_info_->GetFrameForHost(host);
}

CefRefPtr<CefFrame> CefBrowserHostImpl::GetFrameForFrameTreeNode(
    int frame_tree_node_id) {
  return browser_info_->GetFrameForFrameTreeNode(frame_tree_node_id, nullptr);
}

void CefBrowserHostImpl::LoadMainFrameURL(const std::string& url,
                                          const content::Referrer& referrer,
                                          ui::PageTransition transition,
                                          const std::string& extra_headers) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::LoadMainFrameURL, this,
                                 url, referrer, transition, extra_headers));
    return;
  }

  // Go through the navigation controller.
  if (navigation_locked()) {
    // Try again after the lock has been released.
    set_pending_navigation_action(
        base::BindOnce(&CefBrowserHostImpl::LoadMainFrameURL, this, url,
                       referrer, transition, extra_headers));
    return;
  }

  if (web_contents()) {
    GURL gurl = GURL(url);

    if (!gurl.is_valid() && !gurl.has_scheme()) {
      // Try to add "http://" at the beginning
      std::string new_url = std::string("http://") + url;
      gurl = GURL(new_url);
    }

    if (!gurl.is_valid()) {
      LOG(ERROR)
          << "Invalid URL passed to CefBrowserHostImpl::LoadMainFrameURL: "
          << url;
      return;
    }

    web_contents()->GetController().LoadURL(gurl, referrer, transition,
                                            extra_headers);
    OnSetFocus(FOCUS_SOURCE_NAVIGATION);
  }
}

void CefBrowserHostImpl::OnFrameFocused(CefRefPtr<CefFrameHostImpl> frame) {
  DCHECK(frame);

  CefRefPtr<CefFrameHostImpl> previous_frame;
  {
    base::AutoLock lock_scope(state_lock_);
    previous_frame = focused_frame_;
    if (frame->IsMain())
      focused_frame_ = nullptr;
    else
      focused_frame_ = frame;
  }

  if (!previous_frame) {
    // The main frame is focused by default.
    previous_frame = browser_info_->GetMainFrame();
  }

  if (previous_frame->GetIdentifier() != frame->GetIdentifier()) {
    previous_frame->SetFocused(false);
    frame->SetFocused(true);
  }
}

void CefBrowserHostImpl::OnDidFinishLoad(CefRefPtr<CefFrameHostImpl> frame,
                                         const GURL& validated_url,
                                         int http_status_code) {
  frame->RefreshAttributes();

  // Give internal scheme handlers an opportunity to update content.
  scheme::DidFinishLoad(frame, validated_url);

  OnLoadEnd(frame, validated_url, http_status_code);
}

void CefBrowserHostImpl::ViewText(const std::string& text) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::ViewText, this, text));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->ViewText(text);
}

SkColor CefBrowserHostImpl::GetBackgroundColor() const {
  // Don't use |platform_delegate_| because it's not thread-safe.
  return CefContext::Get()->GetBackgroundColor(
      &settings_, is_windowless_ ? STATE_ENABLED : STATE_DISABLED);
}

int CefBrowserHostImpl::browser_id() const {
  return browser_info_->browser_id();
}

content::BrowserContext* CefBrowserHostImpl::GetBrowserContext() {
  CEF_REQUIRE_UIT();
  if (web_contents())
    return web_contents()->GetBrowserContext();
  return nullptr;
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
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefBrowserHostImpl::OnSetFocus, this, source));
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
    const GURL& origin,
    const blink::WebFullscreenOptions& options) {
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
  return is_fullscreen_ ? blink::kWebDisplayModeFullscreen
                        : blink::kWebDisplayModeBrowser;
}

void CefBrowserHostImpl::FindReply(content::WebContents* web_contents,
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
      handler->OnFindResult(this, request_id, number_of_matches, rect,
                            active_match_ordinal, final_update);
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
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::ImeSetComposition, this, text,
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
                  base::BindOnce(&CefBrowserHostImpl::ImeCommitText, this, text,
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
                  base::BindOnce(&CefBrowserHostImpl::ImeFinishComposingText,
                                 this, keep_selection));
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
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::ImeCancelComposition, this));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->ImeCancelComposition();
}

void CefBrowserHostImpl::DragTargetDragEnter(
    CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::DragTargetDragEnter, this,
                                 drag_data, event, allowed_ops));
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

void CefBrowserHostImpl::DragTargetDragOver(
    const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::DragTargetDragOver, this,
                                 event, allowed_ops));
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
    CEF_POST_TASK(CEF_UIT, base::BindOnce(
                               &CefBrowserHostImpl::DragTargetDragLeave, this));
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
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::DragTargetDrop,
                                          this, event));
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
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::DragSourceSystemDragEnded, this));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->DragSourceSystemDragEnded();
}

void CefBrowserHostImpl::DragSourceEndedAt(
    int x,
    int y,
    CefBrowserHost::DragOperationsMask op) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::DragSourceEndedAt, this, x, y, op));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->DragSourceEndedAt(x, y, op);
}

void CefBrowserHostImpl::SetAudioMuted(bool mute) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::Bind(&CefBrowserHostImpl::SetAudioMuted, this, mute));
    return;
  }
  if (!web_contents())
    return;
  web_contents()->SetAudioMuted(mute);
}

bool CefBrowserHostImpl::IsAudioMuted() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }
  if (!web_contents())
    return false;
  return web_contents()->IsAudioMuted();
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
          this, GetFrame(params.frame_tree_node_id), params.url.spec(),
          static_cast<cef_window_open_disposition_t>(params.disposition),
          params.user_gesture);
    }
  }

  if (!cancel) {
    // Start a navigation in the current browser that will result in the
    // creation of a new render process.
    LoadMainFrameURL(params.url.spec(), params.referrer, params.transition,
                     params.extra_headers);
    return source;
  }

  // We don't know where the navigation, if any, will occur.
  return nullptr;
}

bool CefBrowserHostImpl::ShouldTransferNavigation(
    bool is_main_frame_navigation) {
  if (extension_host_) {
    return extension_host_->ShouldTransferNavigation(is_main_frame_navigation);
  }
  return true;
}

void CefBrowserHostImpl::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  CefRefPtr<CefBrowserHostImpl> owner =
      GetBrowserForContents(new_contents.get());
  if (owner) {
    // Taking ownership of |new_contents|.
    owner->set_owned_web_contents(new_contents.release());
    return;
  }

  if (extension_host_) {
    extension_host_->AddNewContents(source, std::move(new_contents),
                                    disposition, initial_rect, user_gesture,
                                    was_blocked);
  }
}

void CefBrowserHostImpl::LoadingStateChanged(content::WebContents* source,
                                             bool to_different_document) {
  const int current_index =
      source->GetController().GetLastCommittedEntryIndex();
  const int max_index = source->GetController().GetEntryCount() - 1;

  const bool is_loading = source->IsLoading();
  const bool can_go_back = (current_index > 0);
  const bool can_go_forward = (current_index < max_index);

  {
    base::AutoLock lock_scope(state_lock_);

    // This method may be called multiple times in a row with |is_loading| true
    // as a result of https://crrev.com/5e750ad0. Ignore the 2nd+ times.
    if (is_loading_ == is_loading && can_go_back_ == can_go_back &&
        can_go_forward_ == can_go_forward) {
      return;
    }

    is_loading_ = is_loading;
    can_go_back_ = can_go_back;
    can_go_forward_ = can_go_forward;
  }

  if (client_.get()) {
    CefRefPtr<CefLoadHandler> handler = client_->GetLoadHandler();
    if (handler.get()) {
      handler->OnLoadingStateChange(this, is_loading, can_go_back,
                                    can_go_forward);
    }
  }
}

void CefBrowserHostImpl::LoadProgressChanged(content::WebContents* source,
                                             double progress) {
  if (client_.get()) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler.get()) {
      handler->OnLoadingProgressChange(this, progress);
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
    CefRefPtr<CefLifeSpanHandler> handler = client_->GetLifeSpanHandler();
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
    blink::mojom::ConsoleMessageLevel level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  if (client_.get()) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler.get()) {
      // Use LOGSEVERITY_DEBUG for unrecognized |level| values.
      cef_log_severity_t log_level = LOGSEVERITY_DEBUG;
      switch (level) {
        case blink::mojom::ConsoleMessageLevel::kVerbose:
          log_level = LOGSEVERITY_DEBUG;
          break;
        case blink::mojom::ConsoleMessageLevel::kInfo:
          log_level = LOGSEVERITY_INFO;
          break;
        case blink::mojom::ConsoleMessageLevel::kWarning:
          log_level = LOGSEVERITY_WARNING;
          break;
        case blink::mojom::ConsoleMessageLevel::kError:
          log_level = LOGSEVERITY_ERROR;
          break;
      }

      return handler->OnConsoleMessage(this, log_level, message, source_id,
                                       line_no);
    }
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

bool CefBrowserHostImpl::TakeFocus(content::WebContents* source, bool reverse) {
  if (client_.get()) {
    CefRefPtr<CefFocusHandler> handler = client_->GetFocusHandler();
    if (handler.get())
      handler->OnTakeFocus(this, !reverse);
  }

  return false;
}

bool CefBrowserHostImpl::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
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

bool CefBrowserHostImpl::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // Check to see if event should be ignored.
  if (event.skip_in_browser)
    return false;

  if (!platform_delegate_)
    return false;

  if (client_.get()) {
    CefRefPtr<CefKeyboardHandler> handler = client_->GetKeyboardHandler();
    if (handler.get()) {
      CefKeyEvent cef_event;
      if (browser_util::GetCefKeyEvent(event, cef_event)) {
        cef_event.focus_on_editable_field = focus_on_editable_field_;

        CefEventHandle event_handle = platform_delegate_->GetEventHandle(event);
        if (handler->OnKeyEvent(this, cef_event, event_handle))
          return true;
      }
    }
  }

  return platform_delegate_->HandleKeyboardEvent(event);
}

bool CefBrowserHostImpl::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  if (extension_host_)
    return extension_host_->PreHandleGestureEvent(source, event);
  return false;
}

bool CefBrowserHostImpl::CanDragEnter(content::WebContents* source,
                                      const content::DropData& data,
                                      blink::WebDragOperationsMask mask) {
  CefRefPtr<CefDragHandler> handler = client_->GetDragHandler();
  if (handler.get()) {
    CefRefPtr<CefDragDataImpl> drag_data(new CefDragDataImpl(data));
    drag_data->SetReadOnly(true);
    if (handler->OnDragEnter(
            this, drag_data.get(),
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
  CefRefPtr<CefDictionaryValue> extra_info;

  CefBrowserInfoManager::GetInstance()->WebContentsCreated(
      target_url, opener_render_process_id, opener_render_frame_id, settings,
      client, platform_delegate, extra_info);

  scoped_refptr<CefBrowserInfo> info =
      CefBrowserInfoManager::GetInstance()->CreatePopupBrowserInfo(
          new_contents, platform_delegate->IsWindowless(), extra_info);
  DCHECK(info.get());
  DCHECK(info->is_popup());

  CefRefPtr<CefBrowserHostImpl> opener = GetBrowserForContents(source_contents);
  if (!opener.get())
    return;

  // Popups must share the same RequestContext as the parent.
  CefRefPtr<CefRequestContextImpl> request_context = opener->request_context();
  DCHECK(request_context);

  // We don't officially own |new_contents| until AddNewContents() is called.
  // However, we need to install observers/delegates here.
  CefRefPtr<CefBrowserHostImpl> browser =
      CreateInternal(settings, client, new_contents, false, info, opener, false,
                     request_context, std::move(platform_delegate), nullptr);
}

void CefBrowserHostImpl::DidNavigateMainFramePostCommit(
    content::WebContents* web_contents) {
  base::AutoLock lock_scope(state_lock_);
  has_document_ = false;
}

content::JavaScriptDialogManager*
CefBrowserHostImpl::GetJavaScriptDialogManager(content::WebContents* source) {
  if (!javascript_dialog_manager_.get() && platform_delegate_) {
    javascript_dialog_manager_.reset(new CefJavaScriptDialogManager(
        this, platform_delegate_->CreateJavaScriptDialogRunner()));
  }
  return javascript_dialog_manager_.get();
}

void CefBrowserHostImpl::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  EnsureFileDialogManager();
  file_dialog_manager_->RunFileChooser(std::move(listener), params);
}

bool CefBrowserHostImpl::HandleContextMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  CEF_REQUIRE_UIT();
  if (!menu_manager_.get() && platform_delegate_) {
    menu_manager_.reset(
        new CefMenuManager(this, platform_delegate_->CreateMenuRunner()));
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

void CefBrowserHostImpl::ResizeDueToAutoResize(content::WebContents* source,
                                               const gfx::Size& new_size) {
  CEF_REQUIRE_UIT();

  if (client_) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler && handler->OnAutoResize(
                       this, CefSize(new_size.width(), new_size.height()))) {
      return;
    }
  }

  UpdatePreferredSize(source, new_size);
}

void CefBrowserHostImpl::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  CEF_REQUIRE_UIT();

  blink::MediaStreamDevices devices;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableMediaStream)) {
    // Cancel the request.
    std::move(callback).Run(devices, blink::MEDIA_DEVICE_PERMISSION_DENIED,
                            std::unique_ptr<content::MediaStreamUI>());
    return;
  }

  // Based on chrome/browser/media/media_stream_devices_controller.cc
  bool microphone_requested =
      (request.audio_type == blink::MEDIA_DEVICE_AUDIO_CAPTURE);
  bool webcam_requested =
      (request.video_type == blink::MEDIA_DEVICE_VIDEO_CAPTURE);
  bool screen_requested =
      (request.video_type == blink::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE);
  if (microphone_requested || webcam_requested || screen_requested) {
    // Pick the desired device or fall back to the first available of the
    // given type.
    if (microphone_requested) {
      CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request.requested_audio_device_id, true, false, &devices);
    }
    if (webcam_requested) {
      CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request.requested_video_device_id, false, true, &devices);
    }
    if (screen_requested) {
      content::DesktopMediaID media_id;
      if (request.requested_video_device_id.empty()) {
        media_id =
            content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                    -1 /* webrtc::kFullDesktopScreenId */);
      } else {
        media_id =
            content::DesktopMediaID::Parse(request.requested_video_device_id);
      }
      devices.push_back(
          blink::MediaStreamDevice(blink::MEDIA_GUM_DESKTOP_VIDEO_CAPTURE,
                                   media_id.ToString(), "Screen"));
    }
  }

  std::move(callback).Run(devices, blink::MEDIA_DEVICE_OK,
                          std::unique_ptr<content::MediaStreamUI>());
}

bool CefBrowserHostImpl::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::MediaStreamType type) {
  // Check media access permission without prompting the user. This is called
  // when loading the Pepper Flash plugin.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableMediaStream);
}

bool CefBrowserHostImpl::IsNeverVisible(content::WebContents* web_contents) {
  if (extension_host_)
    return extension_host_->IsNeverVisible(web_contents);
  return false;
}

// content::WebContentsObserver methods.
// -----------------------------------------------------------------------------

void CefBrowserHostImpl::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  browser_info_->MaybeCreateFrame(render_frame_host, false /* is_guest_view */);
}

void CefBrowserHostImpl::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // Just in case RenderFrameCreated wasn't called for some reason.
  RenderFrameCreated(new_host);
}

void CefBrowserHostImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  const auto frame_id = CefFrameHostImpl::MakeFrameId(render_frame_host);
  browser_info_->RemoveFrame(render_frame_host);

  base::AutoLock lock_scope(state_lock_);

  if (focused_frame_ && focused_frame_->GetIdentifier() == frame_id) {
    focused_frame_ = nullptr;
  }
}

void CefBrowserHostImpl::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  // May be already registered if the renderer crashed previously.
  if (!registrar_->IsRegistered(
          this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
          content::Source<content::RenderViewHost>(render_view_host))) {
    registrar_->Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                    content::Source<content::RenderViewHost>(render_view_host));
  }

  // RenderFrameCreated is otherwise not called for new popup browsers.
  RenderFrameCreated(render_view_host->GetMainFrame());

  platform_delegate_->RenderViewCreated(render_view_host);
}

void CefBrowserHostImpl::RenderViewDeleted(
    content::RenderViewHost* render_view_host) {
  if (registrar_->IsRegistered(
          this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
          content::Source<content::RenderViewHost>(render_view_host))) {
    registrar_->Remove(
        this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
        content::Source<content::RenderViewHost>(render_view_host));
  }
}

void CefBrowserHostImpl::RenderViewReady() {
  ConfigureAutoResize();

  if (client_.get()) {
    CefRefPtr<CefRequestHandler> handler = client_->GetRequestHandler();
    if (handler.get())
      handler->OnRenderViewReady(this);
  }
}

void CefBrowserHostImpl::RenderProcessGone(base::TerminationStatus status) {
  cef_termination_status_t ts = TS_ABNORMAL_TERMINATION;
  if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED)
    ts = TS_PROCESS_WAS_KILLED;
  else if (status == base::TERMINATION_STATUS_PROCESS_CRASHED)
    ts = TS_PROCESS_CRASHED;
  else if (status == base::TERMINATION_STATUS_OOM)
    ts = TS_PROCESS_OOM;
  else if (status != base::TERMINATION_STATUS_ABNORMAL_TERMINATION)
    return;

  if (client_.get()) {
    CefRefPtr<CefRequestHandler> handler = client_->GetRequestHandler();
    if (handler.get()) {
      std::unique_ptr<NavigationLock> navigation_lock = CreateNavigationLock();
      handler->OnRenderProcessTerminated(this, ts);
    }
  }
}

void CefBrowserHostImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  const net::Error error_code = navigation_handle->GetNetErrorCode();

  // Skip calls where the navigation has not yet committed and there is no
  // error code. For example, when creating a browser without loading a URL.
  if (!navigation_handle->HasCommitted() && error_code == net::OK)
    return;

  const bool is_main_frame = navigation_handle->IsInMainFrame();
  const GURL& url =
      (error_code == net::OK ? navigation_handle->GetURL() : GURL());

  CefRefPtr<CefFrameHostImpl> frame = browser_info_->GetFrameForFrameTreeNode(
      navigation_handle->GetFrameTreeNodeId());
  frame->RefreshAttributes();

  if (error_code == net::OK) {
    // The navigation has been committed and there is no error.
    DCHECK(navigation_handle->HasCommitted());

    // Don't call OnLoadStart for same page navigations (fragments,
    // history state).
    if (!navigation_handle->IsSameDocument())
      OnLoadStart(frame.get(), navigation_handle->GetPageTransition());

    if (is_main_frame)
      OnAddressChange(url);
  } else {
    // The navigation failed with an error. This may happen before commit
    // (e.g. network error) or after commit (e.g. response filter error).
    // If the error happened before commit then this call will originate from
    // RenderFrameHostImpl::OnDidFailProvisionalLoadWithError.
    // OnLoadStart/OnLoadEnd will not be called.
    OnLoadError(frame.get(), navigation_handle->GetURL(), error_code);
  }

  if (web_contents()) {
    CefBrowserContext* context =
        static_cast<CefBrowserContext*>(web_contents()->GetBrowserContext());
    if (context) {
      context->AddVisitedURLs(navigation_handle->GetRedirectChain());
    }
  }
}

void CefBrowserHostImpl::DidStopLoading() {
  // Notify all renderers that loading has stopped. We used to use
  // RenderFrameObserver::DidStopLoading which was removed in
  // https://crrev.com/3e37dd0ead. However, that callback wasn't necessarily
  // accurate because it wasn't called in all of the cases where
  // RenderFrameImpl sends the FrameHostMsg_DidStopLoading message. This adds
  // an additional round trip but should provide the same or improved
  // functionality.
  for (const auto& frame : browser_info_->GetAllFrames()) {
    frame->MaybeSendDidStopLoading();
  }
}

void CefBrowserHostImpl::DocumentAvailableInMainFrame() {
  base::AutoLock lock_scope(state_lock_);
  has_document_ = true;
}

void CefBrowserHostImpl::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  // The navigation failed after commit. OnLoadStart was called so we also call
  // OnLoadEnd.
  auto frame = browser_info_->GetFrameForHost(render_frame_host);
  frame->RefreshAttributes();
  OnLoadError(frame, validated_url, error_code);
  OnLoadEnd(frame, validated_url, error_code);
}

void CefBrowserHostImpl::TitleWasSet(content::NavigationEntry* entry) {
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
      std::vector<content::FaviconURL>::const_iterator it = candidates.begin();
      for (; it != candidates.end(); ++it) {
        if (it->icon_type == content::FaviconURL::IconType::kFavicon)
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
  if (message.type() == WidgetHostMsg_SetCursor::ID)
    return IsMouseCursorChangeDisabled();

  return false;
}

bool CefBrowserHostImpl::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  // Messages may arrive after a frame is detached. Ignore those messages.
  auto frame = GetFrameForHost(render_frame_host);
  if (frame) {
    return static_cast<CefFrameHostImpl*>(frame.get())
        ->OnMessageReceived(message);
  }
  return false;
}

void CefBrowserHostImpl::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& content_event_bundle) {
  // Only needed in windowless mode.
  if (IsWindowless()) {
    if (!web_contents() || !platform_delegate_)
      return;

    platform_delegate_->AccessibilityEventReceived(content_event_bundle);
  }
}

void CefBrowserHostImpl::AccessibilityLocationChangesReceived(
    const std::vector<content::AXLocationChangeNotificationDetails>& locData) {
  // Only needed in windowless mode.
  if (IsWindowless()) {
    if (!web_contents() || !platform_delegate_)
      return;

    platform_delegate_->AccessibilityLocationChangesReceived(locData);
  }
}

void CefBrowserHostImpl::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  if (client_.get()) {
    CefRefPtr<CefFocusHandler> handler = client_->GetFocusHandler();
    if (handler.get())
      handler->OnGotFocus(this);
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

bool CefBrowserHostImpl::StartAudioMirroring() {
  if (client_.get()) {
    CefRefPtr<CefAudioHandler> audio_handler = client_->GetAudioHandler();
    if (audio_handler.get()) {
      audio_mirror_destination_ = new CefAudioMirrorDestination(
          this, audio_handler, content::AudioMirroringManager::GetInstance());
      audio_mirror_destination_->Start();
      return true;
    }
  }
  return false;
}

bool CefBrowserHostImpl::StopAudioMirroring() {
  if (audio_mirror_destination_.get()) {
    audio_mirror_destination_->Stop();
    audio_mirror_destination_.reset();
    return true;
  }
  return false;
}

CefBrowserHostImpl::NavigationLock::NavigationLock(
    CefRefPtr<CefBrowserHostImpl> browser)
    : browser_(browser) {
  CEF_REQUIRE_UIT();
  browser_->navigation_lock_count_++;
}

CefBrowserHostImpl::NavigationLock::~NavigationLock() {
  CEF_REQUIRE_UIT();
  if (--browser_->navigation_lock_count_ == 0) {
    if (!browser_->pending_navigation_action_.is_null()) {
      CEF_POST_TASK(CEF_UIT, std::move(browser_->pending_navigation_action_));
    }
  }
}

std::unique_ptr<CefBrowserHostImpl::NavigationLock>
CefBrowserHostImpl::CreateNavigationLock() {
  return base::WrapUnique(new NavigationLock(this));
}

bool CefBrowserHostImpl::navigation_locked() const {
  CEF_REQUIRE_UIT();
  return navigation_lock_count_ > 0;
}

void CefBrowserHostImpl::set_pending_navigation_action(
    base::OnceClosure action) {
  CEF_REQUIRE_UIT();
  pending_navigation_action_ = std::move(action);
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
    CefRefPtr<CefRequestContextImpl> request_context,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
    CefRefPtr<CefExtension> extension)
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
      destruction_state_(DESTRUCTION_STATE_NONE),
      window_destroyed_(false),
      is_in_onsetfocus_(false),
      focus_on_editable_field_(false),
      mouse_cursor_change_disabled_(false),
      devtools_frontend_(NULL),
      extension_(extension) {
  if (opener.get() && !platform_delegate_->IsViewsHosted()) {
    // GetOpenerWindowHandle() only returns a value for non-views-hosted
    // popup browsers.
    opener_ = opener->GetWindowHandle();
  }

  DCHECK(!browser_info_->browser().get());
  browser_info_->SetBrowser(this);

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

  PrefsTabHelper::CreateForWebContents(web_contents);
  printing::CefPrintViewManager::CreateForWebContents(web_contents);

  if (extensions::ExtensionsEnabled()) {
    extensions::CefExtensionWebContentsObserver::CreateForWebContents(
        web_contents);

    // Used by the tabs extension API.
    zoom::ZoomController::CreateForWebContents(web_contents);
  }

  // Make sure RenderViewCreated is called at least one time.
  RenderViewCreated(web_contents->GetRenderViewHost());

  StartAudioMirroring();

  // Associate the platform delegate with this browser.
  platform_delegate_->BrowserCreated(this);
}

void CefBrowserHostImpl::set_owned_web_contents(
    content::WebContents* owned_contents) {
  // Should not currently own a WebContents.
  CHECK(!owned_web_contents_);
  // Should already be associated with |owned_contents|.
  CHECK(web_contents() == owned_contents);
  owned_web_contents_.reset(owned_contents);
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

void CefBrowserHostImpl::CreateExtensionHost(
    const extensions::Extension* extension,
    content::BrowserContext* browser_context,
    content::WebContents* host_contents,
    const GURL& url,
    extensions::ViewType host_type) {
  DCHECK(!extension_host_);

  if (host_type == extensions::VIEW_TYPE_EXTENSION_DIALOG ||
      host_type == extensions::VIEW_TYPE_EXTENSION_POPUP) {
    // Create an extension host that we own.
    extension_host_ = new extensions::CefExtensionViewHost(
        this, extension, browser_context, host_contents, url, host_type);
    // Trigger load of the extension URL.
    extension_host_->CreateRenderViewSoon();
  } else if (host_type == extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    is_background_host_ = true;
    // Create an extension host that will be owned by ProcessManager.
    extension_host_ = new extensions::CefExtensionBackgroundHost(
        this, base::BindOnce(&CefBrowserHostImpl::OnExtensionHostDeleted, this),
        extension, browser_context, host_contents, url, host_type);
    // Load will be triggered by ProcessManager::CreateBackgroundHost.
  } else {
    NOTREACHED() << " Unsupported extension host type: " << host_type;
  }
}

void CefBrowserHostImpl::DestroyExtensionHost() {
  if (!extension_host_)
    return;
  if (extension_host_->extension_host_type() ==
      extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    DCHECK(is_background_host_);
    // Close notification for background pages arrives via CloseContents.
    // The extension host will be deleted by
    // ProcessManager::CloseBackgroundHost and OnExtensionHostDeleted will be
    // called to notify us.
    extension_host_->Close();
  } else {
    DCHECK(!is_background_host_);
    // We own the extension host and must delete it.
    delete extension_host_;
    extension_host_ = nullptr;
  }
}

void CefBrowserHostImpl::OnExtensionHostDeleted() {
  DCHECK(is_background_host_);
  DCHECK(extension_host_);
  extension_host_ = nullptr;
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

void CefBrowserHostImpl::OnAddressChange(const GURL& url) {
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
      std::unique_ptr<NavigationLock> navigation_lock = CreateNavigationLock();
      // Notify the handler that loading has failed.
      handler->OnLoadError(this, frame,
                           static_cast<cef_errorcode_t>(error_code),
                           net::ErrorToShortString(error_code), url.spec());
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
    file_dialog_manager_.reset(new CefFileDialogManager(
        this, platform_delegate_->CreateFileDialogRunner()));
  }
}

void CefBrowserHostImpl::ConfigureAutoResize() {
  CEF_REQUIRE_UIT();
  if (!web_contents() || !web_contents()->GetRenderWidgetHostView()) {
    return;
  }

  if (auto_resize_enabled_) {
    web_contents()->GetRenderWidgetHostView()->EnableAutoResize(
        auto_resize_min_, auto_resize_max_);
  } else {
    web_contents()->GetRenderWidgetHostView()->DisableAutoResize(gfx::Size());
  }
}

void CefBrowserHostImpl::SendTouchEvent(const CefTouchEvent& event) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::Bind(&CefBrowserHostImpl::SendTouchEvent, this, event));
    return;
  }

  if (!web_contents() || !platform_delegate_)
    return;

  platform_delegate_->SendTouchEvent(event);
}
