// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#include <string>
#include <utility>

#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_pref_store.h"
#include "libcef/browser/chrome_scheme_handler.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools_delegate.h"
#include "libcef/browser/devtools_frontend.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/navigate_params.h"
#include "libcef/browser/navigation_entry_impl.h"
#include "libcef/browser/printing/print_view_manager.h"
#include "libcef/browser/render_widget_host_view_osr.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/scheme_handler.h"
#include "libcef/browser/web_contents_view_osr.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/drag_data_impl.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/http_header_utils.h"
#include "libcef/common/main_delegate.h"
#include "libcef/common/process_message_impl.h"
#include "libcef/common/request_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/file_chooser_params.h"
#include "net/base/directory_lister.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "ui/gfx/font_render_params.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/spellchecker/spellcheck_platform.h"
#endif

#if defined(USE_AURA)
#include "ui/views/widget/widget.h"
#endif

namespace {

// Manages the global list of Impl instances.
class ImplManager {
 public:
  typedef std::vector<CefBrowserHostImpl*> Vector;

  ImplManager() {}
  ~ImplManager() {
    DCHECK(all_.empty());
  }

  void AddImpl(CefBrowserHostImpl* impl) {
    CEF_REQUIRE_UIT();
    DCHECK(!IsValidImpl(impl));
    all_.push_back(impl);
  }

  void RemoveImpl(CefBrowserHostImpl* impl) {
    CEF_REQUIRE_UIT();
    Vector::iterator it = GetImplPos(impl);
    DCHECK(it != all_.end());
    all_.erase(it);
  }

  bool IsValidImpl(const CefBrowserHostImpl* impl) {
    CEF_REQUIRE_UIT();
    return GetImplPos(impl) != all_.end();
  }

  CefBrowserHostImpl* GetImplForWebContents(
      const content::WebContents* web_contents) {
    CEF_REQUIRE_UIT();
    Vector::const_iterator it = all_.begin();
    for (; it != all_.end(); ++it) {
      if ((*it)->web_contents() == web_contents)
        return *it;
    }
    return NULL;
  }

 private:
  Vector::iterator GetImplPos(const CefBrowserHostImpl* impl) {
    Vector::iterator it = all_.begin();
    for (; it != all_.end(); ++it) {
      if (*it == impl)
        return it;
    }
    return all_.end();
  }

  Vector all_;

  DISALLOW_COPY_AND_ASSIGN(ImplManager);
};

base::LazyInstance<ImplManager> g_manager = LAZY_INSTANCE_INITIALIZER;


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

// Convert a NativeWebKeyboardEvent to a CefKeyEvent.
bool GetCefKeyEvent(const content::NativeWebKeyboardEvent& event,
                    CefKeyEvent& cef_event) {
  switch (event.type) {
    case blink::WebKeyboardEvent::RawKeyDown:
      cef_event.type = KEYEVENT_RAWKEYDOWN;
      break;
    case blink::WebKeyboardEvent::KeyDown:
      cef_event.type = KEYEVENT_KEYDOWN;
      break;
    case blink::WebKeyboardEvent::KeyUp:
      cef_event.type = KEYEVENT_KEYUP;
      break;
    case blink::WebKeyboardEvent::Char:
      cef_event.type = KEYEVENT_CHAR;
      break;
    default:
      return false;
  }

  cef_event.modifiers = 0;
  if (event.modifiers & blink::WebKeyboardEvent::ShiftKey)
    cef_event.modifiers |= EVENTFLAG_SHIFT_DOWN;
  if (event.modifiers & blink::WebKeyboardEvent::ControlKey)
    cef_event.modifiers |= EVENTFLAG_CONTROL_DOWN;
  if (event.modifiers & blink::WebKeyboardEvent::AltKey)
    cef_event.modifiers |= EVENTFLAG_ALT_DOWN;
  if (event.modifiers & blink::WebKeyboardEvent::MetaKey)
    cef_event.modifiers |= EVENTFLAG_COMMAND_DOWN;
  if (event.modifiers & blink::WebKeyboardEvent::IsKeyPad)
    cef_event.modifiers |= EVENTFLAG_IS_KEY_PAD;

  cef_event.windows_key_code = event.windowsKeyCode;
  cef_event.native_key_code = event.nativeKeyCode;
  cef_event.is_system_key = event.isSystemKey;
  cef_event.character = event.text[0];
  cef_event.unmodified_character = event.unmodifiedText[0];

  return true;
}

// Returns the OS event handle, if any, associated with |event|.
CefEventHandle GetCefEventHandle(const content::NativeWebKeyboardEvent& event) {
#if defined(USE_AURA)
  if (!event.os_event)
    return NULL;
#if defined(OS_WIN)
  return const_cast<CefEventHandle>(&event.os_event->native_event());
#else
  return const_cast<CefEventHandle>(event.os_event->native_event());
#endif
#else  // !defined(USE_AURA)
  return event.os_event;
#endif  // !defined(USE_AURA)
}

class CefFileDialogCallbackImpl : public CefFileDialogCallback {
 public:
  explicit CefFileDialogCallbackImpl(
      const CefBrowserHostImpl::RunFileChooserCallback& callback)
      : callback_(callback) {
  }
  ~CefFileDialogCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        CancelNow(callback_);
      } else {
        CEF_POST_TASK(CEF_UIT,
            base::Bind(&CefFileDialogCallbackImpl::CancelNow, callback_));
      }
    }
  }

  void Continue(int selected_accept_filter,
                const std::vector<CefString>& file_paths) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        std::vector<base::FilePath> vec;
        if (!file_paths.empty()) {
          std::vector<CefString>::const_iterator it = file_paths.begin();
          for (; it != file_paths.end(); ++it)
            vec.push_back(base::FilePath(*it));
        }
        callback_.Run(selected_accept_filter, vec);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefFileDialogCallbackImpl::Continue, this,
                     selected_accept_filter, file_paths));
    }
  }

  void Cancel() override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        CancelNow(callback_);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefFileDialogCallbackImpl::Cancel, this));
    }
  }

  bool IsConnected() {
    return !callback_.is_null();
  }

  void Disconnect() {
    callback_.Reset();
  }

 private:
  static void CancelNow(
      const CefBrowserHostImpl::RunFileChooserCallback& callback) {
    CEF_REQUIRE_UIT();
    std::vector<base::FilePath> file_paths;
    callback.Run(0, file_paths);
  }

  CefBrowserHostImpl::RunFileChooserCallback callback_;

  IMPLEMENT_REFCOUNTING(CefFileDialogCallbackImpl);
};

void RunFileDialogDismissed(
    CefRefPtr<CefRunFileDialogCallback> callback,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  std::vector<CefString> paths;
  if (file_paths.size() > 0) {
    for (size_t i = 0; i < file_paths.size(); ++i)
      paths.push_back(file_paths[i].value());
  }
  callback->OnFileDialogDismissed(selected_accept_filter, paths);
}

class UploadFolderHelper :
    public net::DirectoryLister::DirectoryListerDelegate {
 public:
  explicit UploadFolderHelper(
      const CefBrowserHostImpl::RunFileChooserCallback& callback)
      : callback_(callback) {
  }

  ~UploadFolderHelper() override {
    if (!callback_.is_null()) {
      if (CEF_CURRENTLY_ON_UIT()) {
        CancelNow(callback_);
      } else {
        CEF_POST_TASK(CEF_UIT,
            base::Bind(&UploadFolderHelper::CancelNow, callback_));
      }
    }
  }

  void OnListFile(
      const net::DirectoryLister::DirectoryListerData& data) override {
    CEF_REQUIRE_UIT();
    if (!data.info.IsDirectory())
      select_files_.push_back(data.path);
  }

  void OnListDone(int error) override {
    CEF_REQUIRE_UIT();
    if (!callback_.is_null()) {
      callback_.Run(0, select_files_);
      callback_.Reset();
    }
  }

 private:
  static void CancelNow(
      const CefBrowserHostImpl::RunFileChooserCallback& callback) {
    CEF_REQUIRE_UIT();
    std::vector<base::FilePath> file_paths;
    callback.Run(0, file_paths);
  }

  CefBrowserHostImpl::RunFileChooserCallback callback_;
  std::vector<base::FilePath> select_files_;

  DISALLOW_COPY_AND_ASSIGN(UploadFolderHelper);
};

// Returns the primary OSR host view for the specified |web_contents|. If a
// full-screen host view currently exists then it will be returned. Otherwise,
// the main host view will be returned.
CefRenderWidgetHostViewOSR* GetOSRHostView(content::WebContents* web_contents) {
  CefRenderWidgetHostViewOSR* fs_view =
      static_cast<CefRenderWidgetHostViewOSR*>(
          web_contents->GetFullscreenRenderWidgetHostView());
  if (fs_view)
    return fs_view;

  content::RenderViewHost* host = web_contents->GetRenderViewHost();
  if (host)
    return static_cast<CefRenderWidgetHostViewOSR*>(host->GetView());

  return NULL;
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
  if (windowInfo.windowless_rendering_enabled) {
    if (!client->GetRenderHandler().get()) {
      NOTREACHED() << "CefRenderHandler implementation is required";
      return false;
    }
    if (!content::IsDelegatedRendererEnabled()) {
      NOTREACHED() << "Delegated renderer must be enabled";
      return false;
    }
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
    return NULL;
  }

  // Verify that the settings structure is a valid size.
  if (settings.size != sizeof(cef_browser_settings_t)) {
    NOTREACHED() << "invalid CefBrowserSettings structure size";
    return NULL;
  }

  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return NULL;
  }

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::Create(windowInfo, client, url, settings,
                                 kNullWindowHandle, false, request_context);
  return browser.get();
}


// CefBrowserHostImpl static methods.
// -----------------------------------------------------------------------------

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::Create(
    const CefWindowInfo& windowInfo,
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefWindowHandle opener,
    bool is_popup,
    CefRefPtr<CefRequestContext> request_context) {
  // Verify windowless rendering requirements.
  if (windowInfo.windowless_rendering_enabled) {
    if (!client->GetRenderHandler().get()) {
      NOTREACHED() << "CefRenderHandler implementation is required";
      return NULL;
    }
    if (!content::IsDelegatedRendererEnabled()) {
      NOTREACHED() << "Delegated renderer must be enabled";
      return NULL;
    }
  }

  scoped_refptr<CefBrowserInfo> info =
      CefContentBrowserClient::Get()->CreateBrowserInfo(is_popup);
  info->set_windowless(windowInfo.windowless_rendering_enabled ? true : false);

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::CreateInternal(windowInfo, settings, client, NULL,
                                         info, opener, request_context);
  if (browser.get() && !url.empty()) {
    browser->LoadURL(CefFrameHostImpl::kMainFrameId, url, content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
  }
  return browser.get();
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::CreateInternal(
    const CefWindowInfo& window_info,
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* web_contents,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefWindowHandle opener,
    CefRefPtr<CefRequestContext> request_context) {
  CEF_REQUIRE_UIT();
  DCHECK(browser_info.get());

  // If |opener| is non-NULL it must be a popup window.
  DCHECK(opener == kNullWindowHandle || browser_info->is_popup());

  if (!web_contents) {
    // Get or create the request context and browser context.
    CefRefPtr<CefRequestContextImpl> request_context_impl =
        CefRequestContextImpl::GetForRequestContext(request_context);
    DCHECK(request_context_impl.get());
    scoped_refptr<CefBrowserContext> browser_context =
        request_context_impl->GetBrowserContext();
    DCHECK(browser_context.get());

    if (!request_context.get())
      request_context = request_context_impl.get();

    content::WebContents::CreateParams create_params(
        browser_context.get());

    CefWebContentsViewOSR* view_or = NULL;
    if (window_info.windowless_rendering_enabled) {
      // Use the OSR view instead of the default view.
      view_or = new CefWebContentsViewOSR();
      create_params.view = view_or;
      create_params.delegate_view = view_or;
    }
    web_contents = content::WebContents::Create(create_params);
    if (view_or)
      view_or->set_web_contents(web_contents);
  }

  CefRefPtr<CefBrowserHostImpl> browser =
      new CefBrowserHostImpl(window_info, settings, client, web_contents,
                             browser_info, opener, request_context);
  if (!browser->IsWindowless() && !browser->PlatformCreateWindow())
    return NULL;

#if defined(OS_LINUX) || defined(OS_ANDROID)
  content::RendererPreferences* prefs = web_contents->GetMutableRendererPrefs();
  CR_DEFINE_STATIC_LOCAL(const gfx::FontRenderParams, params,
      (gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), NULL)));
  prefs->should_antialias_text = params.antialiasing;
  prefs->use_subpixel_positioning = params.subpixel_positioning;
  prefs->hinting = params.hinting;
  prefs->use_autohinter = params.autohinter;
  prefs->use_bitmaps = params.use_bitmaps;
  prefs->subpixel_rendering = params.subpixel_rendering;
  web_contents->GetRenderViewHost()->SyncRendererPrefs();
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

  if (client.get()) {
    CefRefPtr<CefLifeSpanHandler> handler = client->GetLifeSpanHandler();
    if (handler.get())
      handler->OnAfterCreated(browser.get());
  }

  return browser;
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForHost(
    const content::RenderViewHost* host) {
  DCHECK(host);
  CEF_REQUIRE_UIT();
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(host);
  if (web_contents)
    return GetBrowserForContents(web_contents);
  return NULL;
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
  return NULL;
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForContents(
    const content::WebContents* contents) {
  DCHECK(contents);
  return g_manager.Get().GetImplForWebContents(contents);
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
    return NULL;
  }

  return GetBrowserForFrame(render_process_id, render_frame_id);
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForView(
    int render_process_id, int render_routing_id) {
  if (render_process_id == -1 || render_routing_id == MSG_ROUTING_NONE)
    return NULL;

  if (CEF_CURRENTLY_ON_UIT()) {
    // Use the non-thread-safe but potentially faster approach.
    content::RenderViewHost* render_view_host =
        content::RenderViewHost::FromID(render_process_id, render_routing_id);
    if (!render_view_host)
      return NULL;
    return GetBrowserForHost(render_view_host);
  } else {
    // Use the thread-safe approach.
    scoped_refptr<CefBrowserInfo> info =
        CefContentBrowserClient::Get()->GetBrowserInfoForView(
            render_process_id,
            render_routing_id);
    if (info.get()) {
      CefRefPtr<CefBrowserHostImpl> browser = info->browser();
      if (!browser.get() && !info->is_mime_handler_view()) {
        LOG(WARNING) << "Found browser id " << info->browser_id() <<
                        " but no browser object matching view process id " <<
                        render_process_id << " and routing id " <<
                        render_routing_id;
      }
      return browser;
    }
    return NULL;
  }
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForFrame(
    int render_process_id, int render_routing_id) {
  if (render_process_id == -1 || render_routing_id == MSG_ROUTING_NONE)
    return NULL;

  if (CEF_CURRENTLY_ON_UIT()) {
    // Use the non-thread-safe but potentially faster approach.
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(render_process_id, render_routing_id);
    if (!render_frame_host)
      return NULL;
    return GetBrowserForHost(render_frame_host);
  } else {
    // Use the thread-safe approach.
    scoped_refptr<CefBrowserInfo> info =
        CefContentBrowserClient::Get()->GetBrowserInfoForFrame(
            render_process_id,
            render_routing_id);
    if (info.get()) {
      CefRefPtr<CefBrowserHostImpl> browser = info->browser();
      if (!browser.get() && !info->is_mime_handler_view()) {
        LOG(WARNING) << "Found browser id " << info->browser_id() <<
                        " but no browser object matching frame process id " <<
                        render_process_id << " and routing id " <<
                        render_routing_id;
      }
      return browser;
    }
    return NULL;
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
      contents->DispatchBeforeUnload(false);
    } else {
      CloseContents(contents);
    }
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::CloseBrowser, this, force_close));
  }
}

void CefBrowserHostImpl::SetFocus(bool focus) {
  if (focus) {
    OnSetFocus(FOCUS_SOURCE_SYSTEM);
  } else {
    if (CEF_CURRENTLY_ON_UIT()) {
      PlatformSetFocus(false);
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefBrowserHostImpl::PlatformSetFocus, this, false));
    }
  }
}

void CefBrowserHostImpl::SetWindowVisibility(bool visible) {
#if defined(OS_MACOSX)
  if (CEF_CURRENTLY_ON_UIT()) {
    PlatformSetWindowVisibility(visible);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::PlatformSetWindowVisibility,
                   this, visible));
  }
#endif
}

CefWindowHandle CefBrowserHostImpl::GetWindowHandle() {
  return PlatformGetWindowHandle();
}

CefWindowHandle CefBrowserHostImpl::GetOpenerWindowHandle() {
  return opener_;
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
  DCHECK(callback.get());
  if (!callback.get())
    return;

  FileChooserParams params;
  switch (mode & FILE_DIALOG_TYPE_MASK) {
    case FILE_DIALOG_OPEN:
      params.mode = content::FileChooserParams::Open;
      break;
    case FILE_DIALOG_OPEN_MULTIPLE:
      params.mode = content::FileChooserParams::OpenMultiple;
      break;
    case FILE_DIALOG_OPEN_FOLDER:
      params.mode = content::FileChooserParams::UploadFolder;
      break;
    case FILE_DIALOG_SAVE:
      params.mode = content::FileChooserParams::Save;
      break;
  }

  DCHECK_GE(selected_accept_filter, 0);
  params.selected_accept_filter = selected_accept_filter;

  params.overwriteprompt = !!(mode & FILE_DIALOG_OVERWRITEPROMPT_FLAG);
  params.hidereadonly = !!(mode & FILE_DIALOG_HIDEREADONLY_FLAG);

  params.title = title;
  if (!default_file_path.empty())
    params.default_file_name = base::FilePath(default_file_path);

  if (!accept_filters.empty()) {
    std::vector<CefString>::const_iterator it = accept_filters.begin();
    for (; it != accept_filters.end(); ++it)
      params.accept_types.push_back(*it);
  }

  RunFileChooser(params, base::Bind(RunFileDialogDismissed, callback));
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

  scoped_refptr<CefBrowserContext> context =
      static_cast<CefBrowserContext*>(web_contents()->GetBrowserContext());
  if (!context.get())
    return;

  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(context.get());
  if (!manager)
    return;

  scoped_ptr<content::DownloadUrlParameters> params(
      content::DownloadUrlParameters::FromWebContents(web_contents(), gurl));
  manager->DownloadUrl(params.Pass());
}

void CefBrowserHostImpl::Print() {
  if (CEF_CURRENTLY_ON_UIT()) {
    content::WebContents* actionable_contents = GetActionableWebContents();
    if (!actionable_contents)
      return;
    printing::PrintViewManager::FromWebContents(
        actionable_contents)->PrintNow();
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

    printing::PrintViewManager::PdfPrintCallback pdf_callback;
    if (callback.get()) {
      pdf_callback = base::Bind(&CefPdfPrintCallback::OnPdfPrintFinished,
                                callback.get(), path);
    }
    printing::PrintViewManager::FromWebContents(actionable_contents)->
        PrintToPDF(base::FilePath(path), settings, pdf_callback);
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
    options.matchCase = matchCase;
    options.findNext = findNext;
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

  if (!web_contents())
    return;

  if (!IsWindowless()) {
    content::RenderViewHost* host = web_contents()->GetRenderViewHost();
    if (host)
      host->WasResized();
  } else {
    CefRenderWidgetHostViewOSR* view = GetOSRHostView(web_contents());
    if (view)
      view->WasResized();
  }
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

  if (!web_contents())
    return;

  CefRenderWidgetHostViewOSR* view = GetOSRHostView(web_contents());
  if (view) {
    if (hidden)
      view->Hide();
    else
      view->Show();
  }
}

void CefBrowserHostImpl::NotifyScreenInfoChanged() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::NotifyScreenInfoChanged, this));
    return;
  }

  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!web_contents())
    return;

  CefRenderWidgetHostViewOSR* view = GetOSRHostView(web_contents());
  if (view)
    view->OnScreenInfoChanged();
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

  if (!web_contents())
    return;

  CefRenderWidgetHostViewOSR* view = GetOSRHostView(web_contents());
  if (view)
    view->Invalidate(type);
}

void CefBrowserHostImpl::SendKeyEvent(const CefKeyEvent& event) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendKeyEvent, this, event));
    return;
  }

  if (!web_contents())
    return;

  content::NativeWebKeyboardEvent web_event;
  PlatformTranslateKeyEvent(web_event, event);

  if (!IsWindowless()) {
    content::RenderViewHost* host = web_contents()->GetRenderViewHost();
    if (host)
      host->ForwardKeyboardEvent(web_event);
  } else {
    CefRenderWidgetHostViewOSR* view = GetOSRHostView(web_contents());
    if (view)
      view->SendKeyEvent(web_event);
  }
}

void CefBrowserHostImpl::SendMouseClickEvent(const CefMouseEvent& event,
    MouseButtonType type, bool mouseUp, int clickCount) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendMouseClickEvent, this, event, type,
                   mouseUp, clickCount));
    return;
  }

  blink::WebMouseEvent web_event;
  PlatformTranslateClickEvent(web_event, event, type, mouseUp, clickCount);

  SendMouseEvent(web_event);
}

void CefBrowserHostImpl::SendMouseMoveEvent(const CefMouseEvent& event,
                                            bool mouseLeave) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendMouseMoveEvent, this, event,
                   mouseLeave));
    return;
  }

  blink::WebMouseEvent web_event;
  PlatformTranslateMoveEvent(web_event, event, mouseLeave);

  SendMouseEvent(web_event);
}

void CefBrowserHostImpl::SendMouseWheelEvent(const CefMouseEvent& event,
                                             int deltaX, int deltaY) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendMouseWheelEvent, this, event,
                   deltaX, deltaY));
    return;
  }

  if (!web_contents())
    return;

  blink::WebMouseWheelEvent web_event;
  PlatformTranslateWheelEvent(web_event, event, deltaX, deltaY);

  if (!IsWindowless()) {
    content::RenderViewHost* host = web_contents()->GetRenderViewHost();
    if (host)
      host->ForwardWheelEvent(web_event);
  } else {
    CefRenderWidgetHostViewOSR* view = GetOSRHostView(web_contents());
    if (view)
      view->SendMouseWheelEvent(web_event);
  }
}

int CefBrowserHostImpl::TranslateModifiers(uint32 cef_modifiers) {
  int webkit_modifiers = 0;
  // Set modifiers based on key state.
  if (cef_modifiers & EVENTFLAG_SHIFT_DOWN)
    webkit_modifiers |= blink::WebInputEvent::ShiftKey;
  if (cef_modifiers & EVENTFLAG_CONTROL_DOWN)
    webkit_modifiers |= blink::WebInputEvent::ControlKey;
  if (cef_modifiers & EVENTFLAG_ALT_DOWN)
    webkit_modifiers |= blink::WebInputEvent::AltKey;
  if (cef_modifiers & EVENTFLAG_COMMAND_DOWN)
    webkit_modifiers |= blink::WebInputEvent::MetaKey;
  if (cef_modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    webkit_modifiers |= blink::WebInputEvent::LeftButtonDown;
  if (cef_modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    webkit_modifiers |= blink::WebInputEvent::MiddleButtonDown;
  if (cef_modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    webkit_modifiers |= blink::WebInputEvent::RightButtonDown;
  if (cef_modifiers & EVENTFLAG_CAPS_LOCK_ON)
    webkit_modifiers |= blink::WebInputEvent::CapsLockOn;
  if (cef_modifiers & EVENTFLAG_NUM_LOCK_ON)
    webkit_modifiers |= blink::WebInputEvent::NumLockOn;
  if (cef_modifiers & EVENTFLAG_IS_LEFT)
    webkit_modifiers |= blink::WebInputEvent::IsLeft;
  if (cef_modifiers & EVENTFLAG_IS_RIGHT)
    webkit_modifiers |= blink::WebInputEvent::IsRight;
  if (cef_modifiers & EVENTFLAG_IS_KEY_PAD)
    webkit_modifiers |= blink::WebInputEvent::IsKeyPad;
  return webkit_modifiers;
}

void CefBrowserHostImpl::SendMouseEvent(const blink::WebMouseEvent& event) {
  if (!web_contents())
    return;

  if (!IsWindowless()) {
    content::RenderViewHost* host = web_contents()->GetRenderViewHost();
    if (host)
      host->ForwardMouseEvent(event);
  } else {
    CefRenderWidgetHostViewOSR* view = GetOSRHostView(web_contents());
    if (view)
      view->SendMouseEvent(event);
  }
}

void CefBrowserHostImpl::SendFocusEvent(bool setFocus) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendFocusEvent, this, setFocus));
    return;
  }

  if (!IsWindowless()) {
    SetFocus(setFocus);
  } else {
    if (!web_contents())
      return;

    CefRenderWidgetHostViewOSR* view = GetOSRHostView(web_contents());
    if (view)
      view->SendFocusEvent(setFocus);
  }
}

void CefBrowserHostImpl::SendCaptureLostEvent() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendCaptureLostEvent, this));
    return;
  }

  if (!web_contents())
    return;

  content::RenderWidgetHostImpl* widget =
      content::RenderWidgetHostImpl::From(web_contents()->GetRenderViewHost());
  if (widget)
    widget->LostCapture();
}

void CefBrowserHostImpl::NotifyMoveOrResizeStarted() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::NotifyMoveOrResizeStarted, this));
    return;
  }

  if (!web_contents())
    return;

  // Dismiss any existing popups.
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  if (host)
    host->NotifyMoveOrResizeStarted();

  PlatformNotifyMoveOrResizeStarted();
}

int CefBrowserHostImpl::GetWindowlessFrameRate() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return 0;
  }

  return CefRenderWidgetHostViewOSR::ClampFrameRate(
      settings_.windowless_frame_rate);
}

void CefBrowserHostImpl::SetWindowlessFrameRate(int frame_rate) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SetWindowlessFrameRate, this,
                   frame_rate));
    return;
  }

  if (!IsWindowless())
    return;

  settings_.windowless_frame_rate = frame_rate;

  if (!web_contents())
    return;

  CefRenderWidgetHostViewOSR* view = GetOSRHostView(web_contents());
  if (view)
    view->UpdateFrameRate();
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
      web_contents_->GetController().Reload(true);
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

    if (web_contents_.get())
      web_contents_->GetController().ReloadIgnoringCache(true);
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
    return NULL;

  FrameMap::const_iterator it = frames_.find(identifier);
  if (it != frames_.end())
    return it->second.get();

  return NULL;
}

CefRefPtr<CefFrame> CefBrowserHostImpl::GetFrame(const CefString& name) {
  base::AutoLock lock_scope(state_lock_);

  FrameMap::const_iterator it = frames_.begin();
  for (; it != frames_.end(); ++it) {
    if (it->second->GetName() == name)
      return it->second.get();
  }

  return NULL;
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
  return window_info_.windowless_rendering_enabled ? true : false;
}

bool CefBrowserHostImpl::IsTransparent() const {
  return window_info_.transparent_painting_enabled ? true : false;
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

  if (client_.get()) {
    CefRefPtr<CefLifeSpanHandler> handler = client_->GetLifeSpanHandler();
    if (handler.get()) {
      // Notify the handler that the window is about to be closed.
      handler->OnBeforeClose(this);
    }
  }

  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

  g_manager.Get().RemoveImpl(this);

  registrar_.reset(NULL);
  response_manager_.reset(NULL);
  content::WebContentsObserver::Observe(NULL);
  web_contents_.reset(NULL);
  menu_creator_.reset(NULL);

#if defined(USE_AURA)
  window_widget_ = NULL;
#endif

  DetachAllFrames();

  CefContentBrowserClient::Get()->RemoveBrowserInfo(browser_info_);
  browser_info_->set_browser(NULL);
}

gfx::NativeView CefBrowserHostImpl::GetContentView() const {
  CEF_REQUIRE_UIT();
#if defined(USE_AURA)
  if (!window_widget_)
    return NULL;
  return window_widget_->GetNativeView();
#else
  if (!web_contents_.get())
    return NULL;
  return web_contents_->GetNativeView();
#endif
}

void CefBrowserHostImpl::CancelContextMenu() {
#if defined(OS_LINUX) && defined(USE_AURA)
  CEF_REQUIRE_UIT();
  // Special case for dismissing views-based context menus on Linux Aura
  // when using windowless rendering.
  if (IsWindowless() && menu_creator_)
    menu_creator_->CancelContextMenu();
#endif
}

content::WebContents* CefBrowserHostImpl::GetWebContents() const {
  CEF_REQUIRE_UIT();
  return web_contents_.get();
}

CefRefPtr<CefFrame> CefBrowserHostImpl::GetFrameForRequest(
    net::URLRequest* request) {
  CEF_REQUIRE_IOT();
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info)
    return NULL;
  return GetOrCreateFrame(info->GetRenderFrameID(),
                          info->GetParentRenderFrameID(),
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
  CefNavigateParams params(GURL(std::string(request->GetURL())),
                           ui::PAGE_TRANSITION_TYPED);
  params.method = request->GetMethod();
  params.frame_id = frame_id;
  params.first_party_for_cookies =
    GURL(std::string(request->GetFirstPartyForCookies()));

  CefRequest::HeaderMap headerMap;
  request->GetHeaderMap(headerMap);
  if (!headerMap.empty())
    params.headers = HttpHeaderUtils::GenerateHeaders(headerMap);

  CefRefPtr<CefPostData> postData = request->GetPostData();
  if (postData.get()) {
    CefPostDataImpl* impl = static_cast<CefPostDataImpl*>(postData.get());
    params.upload_data = new net::UploadData();
    impl->Get(*params.upload_data.get());
  }

  params.load_flags = request->GetFlags();

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

bool CefBrowserHostImpl::ViewText(const std::string& text) {
  return PlatformViewText(text);
}

void CefBrowserHostImpl::HandleExternalProtocol(const GURL& url) {
  if (CEF_CURRENTLY_ON_UIT()) {
    bool allow_os_execution = false;

    if (client_.get()) {
      CefRefPtr<CefRequestHandler> handler = client_->GetRequestHandler();
      if (handler.get())
        handler->OnProtocolExecution(this, url.spec(), allow_os_execution);
    }

    if (allow_os_execution)
      PlatformHandleExternalProtocol(url);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::HandleExternalProtocol, this, url));
  }
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

    PlatformSetFocus(true);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::OnSetFocus, this, source));
  }
}

void CefBrowserHostImpl::RunFileChooser(
    const FileChooserParams& params,
    const RunFileChooserCallback& callback) {
  CEF_POST_TASK(CEF_UIT,
      base::Bind(&CefBrowserHostImpl::RunFileChooserOnUIThread, this, params,
                 callback));
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
  return is_fullscreen_ ? blink::WebDisplayModeFullscreen :
                          blink::WebDisplayModeBrowser;
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

#if !defined(OS_MACOSX)
CefTextInputContext CefBrowserHostImpl::GetNSTextInputContext() {
  NOTREACHED();
  return NULL;
}

void CefBrowserHostImpl::HandleKeyEventBeforeTextInputClient(
    CefEventHandle keyEvent) {
  NOTREACHED();
}

void CefBrowserHostImpl::HandleKeyEventAfterTextInputClient(
    CefEventHandle keyEvent) {
  NOTREACHED();
}
#endif  // !defined(OS_MACOSX)

void CefBrowserHostImpl::DragTargetDragEnter(CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
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

  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  content::RenderViewHost* rvh =
      web_contents() ? web_contents()->GetRenderViewHost() : NULL;
  if (!rvh)
    return;

  int screenX, screenY;

  CefRefPtr<CefRenderHandler> handler = client_->GetRenderHandler();
  if (!handler.get() || !handler->GetScreenPoint(this, event.x, event.y,
                                                 screenX, screenY)) {
    screenX = event.x;
    screenY = event.y;
  }

  CefDragDataImpl* data_impl = static_cast<CefDragDataImpl*>(drag_data.get());
  base::AutoLock lock_scope(data_impl->lock());
  const content::DropData& drop_data = data_impl->drop_data();
  gfx::Point client_pt(event.x, event.y);
  gfx::Point screen_pt(screenX, screenY);
  blink::WebDragOperationsMask ops =
      static_cast<blink::WebDragOperationsMask>(allowed_ops);
  int modifiers = CefBrowserHostImpl::TranslateModifiers(event.modifiers);

  rvh->DragTargetDragEnter(drop_data, client_pt, screen_pt, ops, modifiers);
}

void CefBrowserHostImpl::DragTargetDragOver(const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::DragTargetDragOver, this, event,
                   allowed_ops));
    return;
  }

  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  content::RenderViewHost* rvh =
      web_contents() ? web_contents()->GetRenderViewHost() : NULL;
  if (!rvh)
    return;

  int screenX, screenY;

  CefRefPtr<CefRenderHandler> handler = client_->GetRenderHandler();
  if (!handler.get() || !handler->GetScreenPoint(this, event.x, event.y,
                                                 screenX, screenY)) {
    screenX = event.x;
    screenY = event.y;
  }

  gfx::Point client_pt(event.x, event.y);
  gfx::Point screen_pt(screenX, screenY);
  blink::WebDragOperationsMask ops =
      static_cast<blink::WebDragOperationsMask>(allowed_ops);
  int modifiers = CefBrowserHostImpl::TranslateModifiers(event.modifiers);

  rvh->DragTargetDragOver(client_pt, screen_pt, ops, modifiers);
}

void CefBrowserHostImpl::DragTargetDragLeave() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::DragTargetDragLeave, this));
    return;
  }

  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  content::RenderViewHost* rvh =
      web_contents() ? web_contents()->GetRenderViewHost() : NULL;
  if (!rvh)
    return;

  rvh->DragTargetDragLeave();
}

void CefBrowserHostImpl::DragTargetDrop(const CefMouseEvent& event) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::DragTargetDrop, this, event));
    return;
  }

  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  content::RenderViewHost* rvh =
      web_contents() ? web_contents()->GetRenderViewHost() : NULL;
  if (!rvh)
    return;

  int screenX, screenY;

  CefRefPtr<CefRenderHandler> handler = client_->GetRenderHandler();
  if (!handler.get() || !handler->GetScreenPoint(this, event.x, event.y,
                                                 screenX, screenY)) {
    screenX = event.x;
    screenY = event.y;
  }

  gfx::Point client_pt(event.x, event.y);
  gfx::Point screen_pt(screenX, screenY);
  int modifiers = CefBrowserHostImpl::TranslateModifiers(event.modifiers);

  rvh->DragTargetDrop(client_pt, screen_pt, modifiers);
}

void CefBrowserHostImpl::DragSourceSystemDragEnded() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::DragSourceSystemDragEnded, this));
    return;
  }

  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  content::RenderViewHost* rvh =
      web_contents() ? web_contents()->GetRenderViewHost() : NULL;
  if (!rvh)
    return;

  rvh->DragSourceSystemDragEnded();
}

void CefBrowserHostImpl::DragSourceEndedAt(
    int x, int y, CefBrowserHost::DragOperationsMask op) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::DragSourceEndedAt, this, x, y, op));
    return;
  }

  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  content::RenderViewHost* rvh =
      web_contents() ? web_contents()->GetRenderViewHost() : NULL;
  if (!rvh)
    return;

  int screenX, screenY;

  CefRefPtr<CefRenderHandler> handler = client_->GetRenderHandler();
  if (!handler.get() || !handler->GetScreenPoint(this, x, y, screenX,
                                                 screenY)) {
    screenX = x;
    screenY = y;
  }

  blink::WebDragOperation drag_op = static_cast<blink::WebDragOperation>(op);

  rvh->DragSourceEndedAt(x, y, screenX, screenY, drag_op);
}


// content::WebContentsDelegate methods.
// -----------------------------------------------------------------------------

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
      PlatformCloseWindow();
    } else {
      // Keep a reference to the browser while it's in the process of being
      // destroyed.
      CefRefPtr<CefBrowserHostImpl> browser(this);

      // No window exists. Destroy the browser immediately.
      DestroyBrowser();
      if (!IsWindowless()) {
        // Release the reference added in PlatformCreateWindow().
        Release();
      }
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

bool CefBrowserHostImpl::AddMessageToConsole(content::WebContents* source,
                                             int32 level,
                                             const base::string16& message,
                                             int32 line_no,
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

bool CefBrowserHostImpl::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  if (client_.get()) {
    CefRefPtr<CefKeyboardHandler> handler = client_->GetKeyboardHandler();
    if (handler.get()) {
      CefKeyEvent cef_event;
      if (!GetCefKeyEvent(event, cef_event))
        return false;

      cef_event.focus_on_editable_field = focus_on_editable_field_;

      return handler->OnPreKeyEvent(this, cef_event, GetCefEventHandle(event),
                                    is_keyboard_shortcut);
    }
  }

  return false;
}

void CefBrowserHostImpl::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // Check to see if event should be ignored.
  if (event.skip_in_browser)
    return;

  if (client_.get()) {
    CefRefPtr<CefKeyboardHandler> handler = client_->GetKeyboardHandler();
    if (handler.get()) {
      CefKeyEvent cef_event;
      if (GetCefKeyEvent(event, cef_event)) {
        cef_event.focus_on_editable_field = focus_on_editable_field_;

        if (handler->OnKeyEvent(this, cef_event, GetCefEventHandle(event)))
          return;
      }
    }
  }

  PlatformHandleKeyboardEvent(event);
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

bool CefBrowserHostImpl::ShouldCreateWebContents(
    content::WebContents* web_contents,
    int route_id,
    int main_frame_route_id,
    WindowContainerType window_container_type,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace,
    content::WebContentsView** view,
    content::RenderViewHostDelegateView** delegate_view) {
  // In cases where the navigation will occur in a new render process the
  // |route_id| value will be MSG_ROUTING_NONE here (because the existing
  // renderer will not be able to communicate with the new renderer) and
  // OpenURLFromTab will be called after WebContentsCreated.
  base::AutoLock lock_scope(pending_popup_info_lock_);
  DCHECK(pending_popup_info_.get());
  if (pending_popup_info_->window_info.windowless_rendering_enabled) {
    // Use the OSR view instead of the default view.
    CefWebContentsViewOSR* view_or = new CefWebContentsViewOSR();
    view_or->set_web_contents(web_contents);
    *view = view_or;
    *delegate_view = view_or;
  }

  return true;
}

void CefBrowserHostImpl::WebContentsCreated(
    content::WebContents* source_contents,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  DCHECK(new_contents);

  scoped_ptr<PendingPopupInfo> pending_popup_info;
  {
    base::AutoLock lock_scope(pending_popup_info_lock_);
    pending_popup_info.reset(pending_popup_info_.release());
  }
  DCHECK(pending_popup_info.get());

  content::RenderViewHost* view_host = new_contents->GetRenderViewHost();
  content::RenderFrameHost* main_frame_host = new_contents->GetMainFrame();

  CefWindowHandle opener = kNullWindowHandle;
  scoped_refptr<CefBrowserInfo> info =
      CefContentBrowserClient::Get()->GetOrCreateBrowserInfo(
          view_host->GetProcess()->GetID(),
          view_host->GetRoutingID(),
          main_frame_host->GetProcess()->GetID(),
          main_frame_host->GetRoutingID());

  if (source_contents) {
    DCHECK(info->is_popup());
    opener = GetBrowserForContents(source_contents)->GetWindowHandle();
  } else {
    DCHECK(!info->is_popup());
  }

  scoped_refptr<CefBrowserContext> browser_context =
      static_cast<CefBrowserContext*>(new_contents->GetBrowserContext());
  DCHECK(browser_context.get());
  CefRefPtr<CefRequestContext> request_context =
      CefRequestContextImpl::GetForBrowserContext(browser_context).get();
  DCHECK(request_context.get());

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::CreateInternal(pending_popup_info->window_info,
                                         pending_popup_info->settings,
                                         pending_popup_info->client,
                                         new_contents, info, opener,
                                         request_context);
}

void CefBrowserHostImpl::DidNavigateMainFramePostCommit(
    content::WebContents* web_contents) {
  base::AutoLock lock_scope(state_lock_);
  has_document_ = false;
}

content::JavaScriptDialogManager*
    CefBrowserHostImpl::GetJavaScriptDialogManager(
        content::WebContents* source) {
  if (!dialog_manager_.get())
    dialog_manager_.reset(new CefJavaScriptDialogManager(this));
  return dialog_manager_.get();
}

void CefBrowserHostImpl::RunFileChooser(
    content::WebContents* web_contents,
    const content::FileChooserParams& params) {
  content::RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
  if (!render_view_host)
    return;

  FileChooserParams cef_params;
  static_cast<content::FileChooserParams&>(cef_params) = params;

  if (lister_) {
    // Cancel the previous upload folder run.
    lister_->Cancel();
    lister_.reset();
  }

  if (params.mode == content::FileChooserParams::UploadFolder) {
    RunFileChooserOnUIThread(cef_params,
        base::Bind(
            &CefBrowserHostImpl::OnRunFileChooserUploadFolderDelegateCallback,
            this, web_contents, params.mode));
    return;
  }

  RunFileChooserOnUIThread(cef_params,
      base::Bind(&CefBrowserHostImpl::OnRunFileChooserDelegateCallback, this,
                 web_contents, params.mode));
}

bool CefBrowserHostImpl::HandleContextMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  if (!menu_creator_.get())
    menu_creator_.reset(new CefMenuCreator(web_contents, this));
  return menu_creator_->CreateContextMenu(params);
}

bool CefBrowserHostImpl::SetPendingPopupInfo(
    scoped_ptr<PendingPopupInfo> info) {
  base::AutoLock lock_scope(pending_popup_info_lock_);
  if (pending_popup_info_.get())
    return false;
  pending_popup_info_.reset(info.release());
  return true;
}

void CefBrowserHostImpl::UpdatePreferredSize(content::WebContents* source,
                                             const gfx::Size& pref_size) {
  PlatformSizeTo(pref_size.width(), pref_size.height());
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
                 scoped_ptr<content::MediaStreamUI>());
    return;
  }

  // Based on chrome/browser/media/media_stream_devices_controller.cc
  bool microphone_requested =
      (request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE);
  bool webcam_requested =
      (request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE);
  if (microphone_requested || webcam_requested) {
    switch (request.request_type) {
      case content::MEDIA_OPEN_DEVICE:
      case content::MEDIA_DEVICE_ACCESS:
      case content::MEDIA_GENERATE_STREAM:
      case content::MEDIA_ENUMERATE_DEVICES:
        // Pick the desired device or fall back to the first available of the
        // given type.
        CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
            request.requested_audio_device_id,
            microphone_requested,
            false,
            &devices);

        CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
            request.requested_video_device_id,
            false,
            webcam_requested,
            &devices);
        break;
    }
  }

  callback.Run(devices, content::MEDIA_DEVICE_OK,
               scoped_ptr<content::MediaStreamUI>());
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
  browser_info_->add_render_frame_id(render_frame_host->GetProcess()->GetID(),
                                     render_frame_host->GetRoutingID());
}

void CefBrowserHostImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  browser_info_->remove_render_frame_id(
      render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID());
}

void CefBrowserHostImpl::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  browser_info_->add_render_view_id(render_view_host->GetProcess()->GetID(),
                                    render_view_host->GetRoutingID());

  // May be already registered if the renderer crashed previously.
  if (!registrar_->IsRegistered(
      this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
      content::Source<content::RenderViewHost>(render_view_host))) {
    registrar_->Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                    content::Source<content::RenderViewHost>(render_view_host));
  }
}

void CefBrowserHostImpl::RenderViewDeleted(
    content::RenderViewHost* render_view_host) {
  browser_info_->remove_render_view_id(render_view_host->GetProcess()->GetID(),
                                       render_view_host->GetRoutingID());

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

void CefBrowserHostImpl::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  const bool is_main_frame = !render_frame_host->GetParent();
  CefRefPtr<CefFrame> frame = GetOrCreateFrame(
      render_frame_host->GetRoutingID(),
      CefFrameHostImpl::kUnspecifiedFrameId,
      is_main_frame,
      base::string16(),
      url);
  OnLoadStart(frame, url, transition_type);
  if (is_main_frame)
    OnAddressChange(frame, url);
}

void CefBrowserHostImpl::DidFailProvisionalLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  const bool is_main_frame = !render_frame_host->GetParent();
  CefRefPtr<CefFrame> frame = GetOrCreateFrame(
      render_frame_host->GetRoutingID(),
      CefFrameHostImpl::kUnspecifiedFrameId,
      is_main_frame,
      base::string16(),
      GURL());
  OnLoadError(frame, validated_url, error_code, error_description);
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
  const bool is_main_frame = !render_frame_host->GetParent();
  CefRefPtr<CefFrame> frame = GetOrCreateFrame(
      render_frame_host->GetRoutingID(),
      CefFrameHostImpl::kUnspecifiedFrameId,
      is_main_frame,
      base::string16(),
      validated_url);
  OnLoadError(frame, validated_url, error_code, error_description);
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
  OnTitleChange(entry->GetTitle());
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
      for (; it != candidates.end(); ++it)
        icon_urls.push_back(it->icon_url.spec());
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
    const CefWindowInfo& window_info,
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* web_contents,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefWindowHandle opener,
    CefRefPtr<CefRequestContext> request_context)
    : content::WebContentsObserver(web_contents),
      window_info_(window_info),
      settings_(settings),
      client_(client),
      browser_info_(browser_info),
      opener_(opener),
      request_context_(request_context),
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
      devtools_frontend_(NULL),
      file_chooser_pending_(false) {
#if defined(USE_AURA)
  window_widget_ = NULL;
#endif
#if defined(USE_X11)
  window_x11_ = NULL;
#endif

  DCHECK(request_context_.get());

  DCHECK(!browser_info_->browser().get());
  browser_info_->set_browser(this);

  web_contents_.reset(web_contents);
  web_contents->SetDelegate(this);

  g_manager.Get().AddImpl(this);

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

  printing::PrintViewManager::CreateForWebContents(web_contents_.get());

  // Make sure RenderViewCreated is called at least one time.
  RenderViewCreated(web_contents->GetRenderViewHost());

  if (IsWindowless()) {
    CefRenderWidgetHostViewOSR* view = GetOSRHostView(web_contents);
    if (view)
      view->set_browser_impl(this);
  }
}

CefRefPtr<CefFrame> CefBrowserHostImpl::GetOrCreateFrame(
    int64 frame_id, int64 parent_frame_id, bool is_main_frame,
    base::string16 frame_name, const GURL& frame_url) {
  DCHECK(frame_id > CefFrameHostImpl::kInvalidFrameId);
  if (frame_id <= CefFrameHostImpl::kInvalidFrameId)
    return NULL;

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

    if (is_main_frame)
      main_frame_id_ = frame_id;

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
                                     const GURL& url,
                                     ui::PageTransition transition_type) {
  if (client_.get()) {
    CefRefPtr<CefLoadHandler> handler = client_->GetLoadHandler();
    if (handler.get()) {
      // Notify the handler that loading has started.
      handler->OnLoadStart(this, frame);
    }
  }
}

void CefBrowserHostImpl::OnLoadError(CefRefPtr<CefFrame> frame,
                                     const GURL& url,
                                     int error_code,
                                     const base::string16& error_description) {
  if (client_.get()) {
    CefRefPtr<CefLoadHandler> handler = client_->GetLoadHandler();
    if (handler.get()) {
      frame_destruction_pending_ = true;
      // Notify the handler that loading has failed.
      handler->OnLoadError(this, frame,
          static_cast<cef_errorcode_t>(error_code),
          CefString(error_description),
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

void CefBrowserHostImpl::RunFileChooserOnUIThread(
    const FileChooserParams& params,
    const RunFileChooserCallback& callback) {
  CEF_REQUIRE_UIT();

  if (file_chooser_pending_) {
    // Dismiss the new dialog immediately.
    callback.Run(0, std::vector<base::FilePath>());
    return;
  }

  file_chooser_pending_ = true;

  // Ensure that the |file_chooser_pending_| flag is cleared.
  const RunFileChooserCallback& host_callback =
      base::Bind(&CefBrowserHostImpl::OnRunFileChooserCallback, this, callback);

  bool handled = false;

  if (client_.get()) {
    CefRefPtr<CefDialogHandler> handler = client_->GetDialogHandler();
    if (handler.get()) {
      int mode = FILE_DIALOG_OPEN;
      switch (params.mode) {
        case content::FileChooserParams::Open:
          mode = FILE_DIALOG_OPEN;
          break;
        case content::FileChooserParams::OpenMultiple:
          mode = FILE_DIALOG_OPEN_MULTIPLE;
          break;
        case content::FileChooserParams::UploadFolder:
          mode = FILE_DIALOG_OPEN_FOLDER;
          break;
        case content::FileChooserParams::Save:
          mode = FILE_DIALOG_SAVE;
          break;
        default:
          NOTREACHED();
          break;
      }

      if (params.overwriteprompt)
        mode |= FILE_DIALOG_OVERWRITEPROMPT_FLAG;
      if (params.hidereadonly)
        mode |= FILE_DIALOG_HIDEREADONLY_FLAG;

      std::vector<base::string16>::const_iterator it;

      std::vector<CefString> accept_filters;
      it = params.accept_types.begin();
      for (; it != params.accept_types.end(); ++it)
        accept_filters.push_back(*it);

      CefRefPtr<CefFileDialogCallbackImpl> callbackImpl(
          new CefFileDialogCallbackImpl(host_callback));
      handled = handler->OnFileDialog(
          this,
          static_cast<cef_file_dialog_mode_t>(mode),
          params.title,
          params.default_file_name.value(),
          accept_filters,
          params.selected_accept_filter,
          callbackImpl.get());
      if (!handled) {
        if (callbackImpl->IsConnected()) {
          callbackImpl->Disconnect();
        } else {
          // User executed the callback even though they returned false.
          NOTREACHED();
          handled = true;
        }
      }
    }
  }

  if (!handled)
    PlatformRunFileChooser(params, host_callback);
}

void CefBrowserHostImpl::OnRunFileChooserCallback(
    const RunFileChooserCallback& callback,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  CEF_REQUIRE_UIT();

  file_chooser_pending_ = false;

  // Execute the callback asynchronously.
  CEF_POST_TASK(CEF_UIT,
      base::Bind(callback, selected_accept_filter, file_paths));
}

void CefBrowserHostImpl::OnRunFileChooserUploadFolderDelegateCallback(
    content::WebContents* web_contents,
    const content::FileChooserParams::Mode mode,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  CEF_REQUIRE_UIT();
  DCHECK (mode == content::FileChooserParams::UploadFolder);

  if (file_paths.size() == 0) {
    // Client canceled the file chooser.
    OnRunFileChooserDelegateCallback(web_contents, mode,
        selected_accept_filter, file_paths);
  } else {
    lister_.reset(new net::DirectoryLister(
        file_paths[0],
        net::DirectoryLister::NO_SORT,
        new UploadFolderHelper(
            base::Bind(&CefBrowserHostImpl::OnRunFileChooserDelegateCallback,
                       this, web_contents, mode))));
    lister_->Start();
  }
}

void CefBrowserHostImpl::OnRunFileChooserDelegateCallback(
    content::WebContents* web_contents,
    content::FileChooserParams::Mode mode,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  CEF_REQUIRE_UIT();

  if (lister_.get())
    lister_.reset();

  content::RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
  if (!render_view_host)
    return;

  // Convert FilePath list to SelectedFileInfo list.
  std::vector<content::FileChooserFileInfo> selected_files;
  for (size_t i = 0; i < file_paths.size(); ++i) {
    content::FileChooserFileInfo info;
    info.file_path = file_paths[i];
    selected_files.push_back(info);
  }

  // Notify our RenderViewHost in all cases.
  render_view_host->FilesSelectedInChooser(selected_files, mode);
}

void CefBrowserHostImpl::OnDevToolsWebContentsDestroyed() {
  devtools_observer_.reset();
  devtools_frontend_ = NULL;
}
