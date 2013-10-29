// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#include <string>
#include <utility>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_pref_store.h"
#include "libcef/browser/chrome_scheme_handler.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools_delegate.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/navigate_params.h"
#include "libcef/browser/printing/print_view_manager.h"
#include "libcef/browser/render_widget_host_view_osr.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/scheme_handler.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/web_contents_view_osr.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/drag_data_impl.h"
#include "libcef/common/http_header_utils.h"
#include "libcef/common/main_delegate.h"
#include "libcef/common/process_message_impl.h"
#include "libcef/common/request_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/file_chooser_params.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

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

// Convert a NativeWebKeyboardEvent to a CefKeyEvent.
bool GetCefKeyEvent(const content::NativeWebKeyboardEvent& event,
                    CefKeyEvent& cef_event) {
  switch (event.type) {
    case WebKit::WebKeyboardEvent::RawKeyDown:
      cef_event.type = KEYEVENT_RAWKEYDOWN;
      break;
    case WebKit::WebKeyboardEvent::KeyDown:
      cef_event.type = KEYEVENT_KEYDOWN;
      break;
    case WebKit::WebKeyboardEvent::KeyUp:
      cef_event.type = KEYEVENT_KEYUP;
      break;
    case WebKit::WebKeyboardEvent::Char:
      cef_event.type = KEYEVENT_CHAR;
      break;
    default:
      return false;
  }

  cef_event.modifiers = 0;
  if (event.modifiers & WebKit::WebKeyboardEvent::ShiftKey)
    cef_event.modifiers |= EVENTFLAG_SHIFT_DOWN;
  if (event.modifiers & WebKit::WebKeyboardEvent::ControlKey)
    cef_event.modifiers |= EVENTFLAG_CONTROL_DOWN;
  if (event.modifiers & WebKit::WebKeyboardEvent::AltKey)
    cef_event.modifiers |= EVENTFLAG_ALT_DOWN;
  if (event.modifiers & WebKit::WebKeyboardEvent::MetaKey)
    cef_event.modifiers |= EVENTFLAG_COMMAND_DOWN;
  if (event.modifiers & WebKit::WebKeyboardEvent::IsKeyPad)
    cef_event.modifiers |= EVENTFLAG_IS_KEY_PAD;

  cef_event.windows_key_code = event.windowsKeyCode;
  cef_event.native_key_code = event.nativeKeyCode;
  cef_event.is_system_key = event.isSystemKey;
  cef_event.character = event.text[0];
  cef_event.unmodified_character = event.unmodifiedText[0];

  return true;
}

class CefFileDialogCallbackImpl : public CefFileDialogCallback {
 public:
  explicit CefFileDialogCallbackImpl(
      const CefBrowserHostImpl::RunFileChooserCallback& callback)
      : callback_(callback) {
  }
  ~CefFileDialogCallbackImpl() {
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

  virtual void Continue(const std::vector<CefString>& file_paths) OVERRIDE {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        std::vector<base::FilePath> vec;
        if (!file_paths.empty()) {
          std::vector<CefString>::const_iterator it = file_paths.begin();
          for (; it != file_paths.end(); ++it)
            vec.push_back(base::FilePath(*it));
        }
        callback_.Run(vec);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefFileDialogCallbackImpl::Continue, this, file_paths));
    }
  }

  virtual void Cancel() OVERRIDE {
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
    callback.Run(file_paths);
  }

  CefBrowserHostImpl::RunFileChooserCallback callback_;

  IMPLEMENT_REFCOUNTING(CefFileDialogCallbackImpl);
};

class CefRunFileDialogCallbackWrapper
    : public base::RefCountedThreadSafe<CefRunFileDialogCallbackWrapper> {
 public:
  CefRunFileDialogCallbackWrapper(CefRefPtr<CefBrowserHost> host,
                                  CefRefPtr<CefRunFileDialogCallback> callback)
      : host_(host),
        callback_(callback) {
  }

  void Callback(const std::vector<base::FilePath>& file_paths) {
    std::vector<CefString> paths;
    if (file_paths.size() > 0) {
      for (size_t i = 0; i < file_paths.size(); ++i)
        paths.push_back(file_paths[i].value());
    }
    callback_->OnFileDialogDismissed(host_, paths);
  }

 private:
  CefRefPtr<CefBrowserHost> host_;
  CefRefPtr<CefRunFileDialogCallback> callback_;
};

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

  CefBrowserSettings new_settings = settings;

  // Verify that render handler is in place for a windowless browser.
  if (CefBrowserHostImpl::IsWindowRenderingDisabled(windowInfo)) {
    if (!client->GetRenderHandler().get()) {
      NOTREACHED() << "CefRenderHandler implementation is required";
      return false;
    }
    if (new_settings.accelerated_compositing != STATE_DISABLED) {
      // Accelerated compositing is not supported when window rendering is
      // disabled.
      new_settings.accelerated_compositing = STATE_DISABLED;
    }
  }

  // Create the browser on the UI thread.
  CreateBrowserHelper* helper =
      new CreateBrowserHelper(windowInfo, client, url, new_settings,
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

  CefBrowserSettings new_settings = settings;

  // Verify that render handler is in place for a windowless browser.
  if (CefBrowserHostImpl::IsWindowRenderingDisabled(windowInfo)) {
    if (!client->GetRenderHandler().get()) {
      NOTREACHED() << "CefRenderHandler implementation is required";
      return NULL;
    }
    if (new_settings.accelerated_compositing != STATE_DISABLED) {
      // Accelerated compositing is not supported when window rendering is
      // disabled.
      new_settings.accelerated_compositing = STATE_DISABLED;
    }
  }

  CefContentBrowserClient::Get()->set_use_osr_next_contents_view(
      CefBrowserHostImpl::IsWindowRenderingDisabled(windowInfo));

  scoped_refptr<CefBrowserInfo> info =
      CefContentBrowserClient::Get()->CreateBrowserInfo();

  info->set_window_rendering_disabled(
      CefBrowserHostImpl::IsWindowRenderingDisabled(windowInfo));
  DCHECK(!info->is_popup());
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::Create(windowInfo, new_settings, client, NULL, info,
                                 NULL, request_context);
  if (!url.empty()) {
    browser->LoadURL(CefFrameHostImpl::kMainFrameId, url, content::Referrer(),
                     content::PAGE_TRANSITION_TYPED, std::string());
  }
  return browser.get();
}


// CefBrowserHostImpl static methods.
// -----------------------------------------------------------------------------

CefBrowserHostImpl::~CefBrowserHostImpl() {
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::Create(
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
  DCHECK(opener == NULL || browser_info->is_popup());

  if (!web_contents) {
    CefBrowserContext* browser_context = NULL;
    if (request_context.get()) {
      CefRequestContextImpl* request_context_impl =
          static_cast<CefRequestContextImpl*>(request_context.get());
      browser_context = request_context_impl->GetOrCreateBrowserContext();
    } else {
      browser_context = CefContentBrowserClient::Get()->browser_context();
    }
    DCHECK(browser_context);

    content::WebContents::CreateParams create_params(
        browser_context);
    web_contents = content::WebContents::Create(create_params);
  }

  CefRefPtr<CefBrowserHostImpl> browser =
      new CefBrowserHostImpl(window_info, settings, client, web_contents,
                             browser_info, opener);
  if (!browser->IsWindowRenderingDisabled() &&
      !browser->PlatformCreateWindow()) {
    return NULL;
  }

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
  content::WebContentsImpl* web_contents =
      static_cast<content::WebContentsImpl*>(host->GetDelegate());
  if (web_contents)
    return static_cast<CefBrowserHostImpl*>(web_contents->GetDelegate());
  return NULL;
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForContents(
    content::WebContents* contents) {
  DCHECK(contents);
  CEF_REQUIRE_UIT();
  return static_cast<CefBrowserHostImpl*>(contents->GetDelegate());
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForRequest(
    net::URLRequest* request) {
  DCHECK(request);
  CEF_REQUIRE_IOT();
  int render_process_id = -1;
  int render_view_id = MSG_ROUTING_NONE;

  if (!content::ResourceRequestInfo::GetRenderViewForRequest(
          request, &render_process_id, &render_view_id) ||
      render_process_id == -1 ||
      render_view_id == MSG_ROUTING_NONE) {
    return NULL;
  }

  return GetBrowserByRoutingID(render_process_id, render_view_id);
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserByRoutingID(
    int render_process_id, int render_view_id) {
  if (render_process_id == -1 || render_view_id == MSG_ROUTING_NONE)
    return NULL;

  if (CEF_CURRENTLY_ON_UIT()) {
    // Use the non-thread-safe but potentially faster approach.
    content::RenderViewHost* render_view_host =
        content::RenderViewHost::FromID(render_process_id, render_view_id);
    if (!render_view_host)
      return NULL;
    return GetBrowserForHost(render_view_host);
  } else {
    // Use the thread-safe approach.
    scoped_refptr<CefBrowserInfo> info =
        CefContentBrowserClient::Get()->GetBrowserInfo(render_process_id,
                                                       render_view_id);
    if (info.get()) {
      CefRefPtr<CefBrowserHostImpl> browser = info->browser();
      if (!browser.get()) {
        LOG(WARNING) << "Found browser id " << info->browser_id() <<
                        " but no browser object matching process id " <<
                        render_process_id << " and view id " << render_view_id;
      }
      return browser;
    }
    return NULL;
  }
}


// CefBrowserHost methods.
// -----------------------------------------------------------------------------

CefRefPtr<CefBrowser> CefBrowserHostImpl::GetBrowser() {
  return this;
}

void CefBrowserHostImpl::CloseBrowser(bool force_close) {
  if (CEF_CURRENTLY_ON_UIT()) {
    // Exit early if a close attempt is already pending and this method is
    // called again from somewhere other than WindowDestroyed().
    if (destruction_state_ >= DESTRUCTION_STATE_PENDING &&
        (IsWindowRenderingDisabled() || !window_destroyed_)) {
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
      contents->GetRenderViewHost()->FirePageBeforeUnload(false);
    } else {
      CloseContents(contents);
    }
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::CloseBrowser, this, force_close));
  }
}

void CefBrowserHostImpl::ParentWindowWillClose() {
}

void CefBrowserHostImpl::SetFocus(bool enable) {
  if (!enable)
    return;

  OnSetFocus(FOCUS_SOURCE_SYSTEM);
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

CefString CefBrowserHostImpl::GetDevToolsURL(bool http_scheme) {
  base::AutoLock lock_scope(state_lock_);
  return (http_scheme ? devtools_url_http_ : devtools_url_chrome_);
}

double CefBrowserHostImpl::GetZoomLevel() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return 0;
  }

  if (web_contents_.get())
    return web_contents_->GetZoomLevel();

  return 0;
}

void CefBrowserHostImpl::SetZoomLevel(double zoomLevel) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (web_contents_.get() && web_contents_->GetRenderViewHost())
      web_contents_->GetRenderViewHost()->SetZoomLevel(zoomLevel);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SetZoomLevel, this, zoomLevel));
  }
}

void CefBrowserHostImpl::RunFileDialog(
      FileDialogMode mode,
      const CefString& title,
      const CefString& default_file_name,
      const std::vector<CefString>& accept_types,
      CefRefPtr<CefRunFileDialogCallback> callback) {
  DCHECK(callback.get());
  if (!callback.get())
    return;

  content::FileChooserParams params;
  switch (mode) {
    case FILE_DIALOG_OPEN:
      params.mode = content::FileChooserParams::Open;
      break;
    case FILE_DIALOG_OPEN_MULTIPLE:
      params.mode = content::FileChooserParams::OpenMultiple;
      break;
    case FILE_DIALOG_SAVE:
      params.mode = content::FileChooserParams::Save;
      break;
  }
  params.title = title;
  if (!default_file_name.empty())
    params.default_file_name = base::FilePath(default_file_name);
  if (!accept_types.empty()) {
    std::vector<CefString>::const_iterator it = accept_types.begin();
    for (; it != accept_types.end(); ++it)
      params.accept_types.push_back(*it);
  }

  scoped_refptr<CefRunFileDialogCallbackWrapper> wrapper =
      new CefRunFileDialogCallbackWrapper(this, callback);
  RunFileChooser(params,
      base::Bind(&CefRunFileDialogCallbackWrapper::Callback, wrapper));
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

  scoped_ptr<content::DownloadUrlParameters> params;
  params.reset(
      content::DownloadUrlParameters::FromWebContents(web_contents(), gurl));
  manager->DownloadUrl(params.Pass());
}

void CefBrowserHostImpl::Print() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (!web_contents_)
      return;
    printing::PrintViewManager::FromWebContents(
        web_contents_.get())->PrintNow();
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::Print, this));
  }
}

void CefBrowserHostImpl::Find(int identifier, const CefString& searchText,
                              bool forward, bool matchCase, bool findNext) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (!web_contents_)
      return;

    WebKit::WebFindOptions options;
    options.forward = forward;
    options.matchCase = matchCase;
    options.findNext = findNext;
    web_contents()->GetRenderViewHost()->Find(identifier, searchText, options);
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
    web_contents()->GetRenderViewHost()->StopFinding(action);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::StopFinding, this, clearSelection));
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
  return IsWindowRenderingDisabled(window_info_);
}

void CefBrowserHostImpl::WasResized() {
  if (!IsWindowRenderingDisabled()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::Bind(&CefBrowserHostImpl::WasResized, this));
    return;
  }

  if (!web_contents())
    return;

  content::RenderWidgetHost* widget = web_contents()->GetRenderViewHost();
  if (widget)
    widget->WasResized();
}

void CefBrowserHostImpl::WasHidden(bool hidden) {
  if (!IsWindowRenderingDisabled()) {
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

  content::RenderWidgetHostImpl* widget =
      content::RenderWidgetHostImpl::From(web_contents()->GetRenderViewHost());
  if (!widget)
    return;

  if (hidden) 
    widget->WasHidden();
  else
    widget->WasShown();
}

void CefBrowserHostImpl::NotifyScreenInfoChanged() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::NotifyScreenInfoChanged, this));
    return;
  }

  if (!IsWindowRenderingDisabled()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!web_contents())
    return;

  content::RenderWidgetHostView* view =
      web_contents()->GetRenderViewHost()->GetView();
  if (!view)
    return;

  CefRenderWidgetHostViewOSR* orview =
      static_cast<CefRenderWidgetHostViewOSR*>(view);

  orview->OnScreenInfoChanged();
}

void CefBrowserHostImpl::Invalidate(const CefRect& dirtyRect,
                                    PaintElementType type) {
  if (!IsWindowRenderingDisabled()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::Invalidate, this, dirtyRect, type));
    return;
  }

  if (!web_contents())
    return;

  content::RenderWidgetHostView* view =
      web_contents()->GetRenderViewHost()->GetView();
  CefRenderWidgetHostViewOSR* orview =
      static_cast<CefRenderWidgetHostViewOSR*>(view);

  if (orview) {
    gfx::Rect rect(dirtyRect.x, dirtyRect.y,
                   dirtyRect.width, dirtyRect.height);
    orview->Invalidate(rect, type);
  }
}

void CefBrowserHostImpl::SendKeyEvent(const CefKeyEvent& event) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendKeyEvent, this, event));
    return;
  }

  content::NativeWebKeyboardEvent web_event;
  PlatformTranslateKeyEvent(web_event, event);

  if (!IsWindowRenderingDisabled()) {
    content::RenderWidgetHost* widget = web_contents()->GetRenderViewHost();
    if (widget)
      widget->ForwardKeyboardEvent(web_event);
  } else {
    if (!web_contents())
      return;
    content::RenderWidgetHostView* view =
        web_contents()->GetRenderViewHost()->GetView();
    CefRenderWidgetHostViewOSR* orview =
        static_cast<CefRenderWidgetHostViewOSR*>(view);
    if (orview)
      orview->SendKeyEvent(web_event);
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

  WebKit::WebMouseEvent web_event;
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

  WebKit::WebMouseEvent web_event;
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

  WebKit::WebMouseWheelEvent web_event;
  PlatformTranslateWheelEvent(web_event, event, deltaX, deltaY);

  if (!IsWindowRenderingDisabled()) {
    content::RenderWidgetHost* widget = web_contents()->GetRenderViewHost();
    if (widget)
      widget->ForwardWheelEvent(web_event);
  } else {
    if (!web_contents())
      return;
    content::RenderWidgetHostView* view =
        web_contents()->GetRenderViewHost()->GetView();
    CefRenderWidgetHostViewOSR* orview =
        static_cast<CefRenderWidgetHostViewOSR*>(view);

    if (orview)
      orview->SendMouseWheelEvent(web_event);
  }
}

int CefBrowserHostImpl::TranslateModifiers(uint32 cef_modifiers) {
  int webkit_modifiers = 0;
  // Set modifiers based on key state.
  if (cef_modifiers & EVENTFLAG_SHIFT_DOWN)
    webkit_modifiers |= WebKit::WebInputEvent::ShiftKey;
  if (cef_modifiers & EVENTFLAG_CONTROL_DOWN)
    webkit_modifiers |= WebKit::WebInputEvent::ControlKey;
  if (cef_modifiers & EVENTFLAG_ALT_DOWN)
    webkit_modifiers |= WebKit::WebInputEvent::AltKey;
  if (cef_modifiers & EVENTFLAG_COMMAND_DOWN)
    webkit_modifiers |= WebKit::WebInputEvent::MetaKey;
  if (cef_modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    webkit_modifiers |= WebKit::WebInputEvent::LeftButtonDown;
  if (cef_modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    webkit_modifiers |= WebKit::WebInputEvent::MiddleButtonDown;
  if (cef_modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    webkit_modifiers |= WebKit::WebInputEvent::RightButtonDown;
  if (cef_modifiers & EVENTFLAG_CAPS_LOCK_ON)
    webkit_modifiers |= WebKit::WebInputEvent::CapsLockOn;
  if (cef_modifiers & EVENTFLAG_NUM_LOCK_ON)
    webkit_modifiers |= WebKit::WebInputEvent::NumLockOn;
  if (cef_modifiers & EVENTFLAG_IS_LEFT)
    webkit_modifiers |= WebKit::WebInputEvent::IsLeft;
  if (cef_modifiers & EVENTFLAG_IS_RIGHT)
    webkit_modifiers |= WebKit::WebInputEvent::IsRight;
  if (cef_modifiers & EVENTFLAG_IS_KEY_PAD)
    webkit_modifiers |= WebKit::WebInputEvent::IsKeyPad;
  return webkit_modifiers;
}

void CefBrowserHostImpl::SendMouseEvent(const WebKit::WebMouseEvent& event) {
  if (!IsWindowRenderingDisabled()) {
    content::RenderWidgetHost* widget = web_contents()->GetRenderViewHost();
    if (widget)
      widget->ForwardMouseEvent(event);
  } else {
    if (!web_contents())
      return;
    content::RenderWidgetHostView* view =
        web_contents()->GetRenderViewHost()->GetView();
    CefRenderWidgetHostViewOSR* orview =
        static_cast<CefRenderWidgetHostViewOSR*>(view);

    if (orview)
      orview->SendMouseEvent(event);
  }
}

void CefBrowserHostImpl::SendFocusEvent(bool setFocus) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendFocusEvent, this, setFocus));
    return;
  }

  if (!web_contents())
    return;

  content::RenderWidgetHostImpl* widget =
      content::RenderWidgetHostImpl::From(web_contents()->GetRenderViewHost());
  if (!widget)
    return;
  if (setFocus) {
    widget->GotFocus();
    widget->SetActive(true);
  } else {
    widget->SetActive(false);
    widget->Blur();
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
  if (!widget)
    return;
  widget->LostCapture();
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
    if (web_contents_.get())
      web_contents_->GetController().Reload(true);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::Reload, this));
  }
}

void CefBrowserHostImpl::ReloadIgnoreCache() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (web_contents_.get())
      web_contents_->GetController().ReloadIgnoringCache(true);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::ReloadIgnoreCache, this));
  }
}

void CefBrowserHostImpl::StopLoad() {
  if (CEF_CURRENTLY_ON_UIT()) {
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

  registrar_.reset(NULL);
  response_manager_.reset(NULL);
  content::WebContentsObserver::Observe(NULL);
  web_contents_.reset(NULL);
  menu_creator_.reset(NULL);

  DetachAllFrames();

  CefContentBrowserClient::Get()->RemoveBrowserInfo(browser_info_);
  browser_info_->set_browser(NULL);
}

gfx::NativeView CefBrowserHostImpl::GetContentView() const {
  CEF_REQUIRE_UIT();
  if (!web_contents_.get())
    return NULL;
  return web_contents_->GetView()->GetNativeView();
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
  return GetOrCreateFrame(info->GetFrameID(), info->GetParentFrameID(),
                          info->IsMainFrame(), string16(), GURL());
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
                           content::PAGE_TRANSITION_TYPED);
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
    content::PageTransition transition,
    const std::string& extra_headers) {
  if (frame_id == CefFrameHostImpl::kMainFrameId) {
    // Go through the navigation controller.
    if (CEF_CURRENTLY_ON_UIT()) {
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

        // Update the loading URL.
        OnLoadingURLChange(gurl);

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

  params.arguments.Append(base::Value::CreateStringValue(string));
  params.arguments.Append(base::Value::CreateStringValue(url));

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

    params.arguments.Append(base::Value::CreateStringValue(command));

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

    params.arguments.Append(base::Value::CreateBooleanValue(is_javascript));
    params.arguments.Append(base::Value::CreateStringValue(code));
    params.arguments.Append(base::Value::CreateStringValue(script_url));
    params.arguments.Append(base::Value::CreateIntegerValue(script_start_line));

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

GURL CefBrowserHostImpl::GetLoadingURL() {
  base::AutoLock lock_scope(state_lock_);
  return loading_url_;
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

    if (web_contents_.get())
      web_contents_->GetView()->Focus();
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::OnSetFocus, this, source));
  }
}

void CefBrowserHostImpl::RunFileChooser(
    const content::FileChooserParams& params,
    const RunFileChooserCallback& callback) {
  CEF_POST_TASK(CEF_UIT,
      base::Bind(&CefBrowserHostImpl::RunFileChooserOnUIThread, this, params,
                 callback));
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
#endif // !defined(OS_MACOSX)

// content::WebContentsDelegate methods.
// -----------------------------------------------------------------------------

content::WebContents* CefBrowserHostImpl::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // Start a navigation that will result in the creation of a new render
  // process.
  LoadURL(CefFrameHostImpl::kMainFrameId, params.url.spec(), params.referrer,
          params.transition, params.extra_headers);

  return source;
}

void CefBrowserHostImpl::LoadingStateChanged(content::WebContents* source) {
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
  if (client_.get() && (IsWindowRenderingDisabled() || !window_destroyed_)) {
    CefRefPtr<CefLifeSpanHandler> handler =
        client_->GetLifeSpanHandler();
    if (handler.get()) {
      close_browser = !handler->DoClose(this);
    }
  }

  if (close_browser) {
    if (destruction_state_ != DESTRUCTION_STATE_ACCEPTED)
      destruction_state_ = DESTRUCTION_STATE_ACCEPTED;

    if (!IsWindowRenderingDisabled() && !window_destroyed_) {
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
      if (!IsWindowRenderingDisabled()) {
        // Release the reference added in PlatformCreateWindow().
        Release();
      }
    }
  } else if (destruction_state_ != DESTRUCTION_STATE_NONE) {
    destruction_state_ = DESTRUCTION_STATE_NONE;
  }
}

void CefBrowserHostImpl::UpdateTargetURL(content::WebContents* source,
                                         int32 page_id,
                                         const GURL& url) {
  if (client_.get()) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler.get())
      handler->OnStatusMessage(this, url.spec());
  }
}

bool CefBrowserHostImpl::AddMessageToConsole(content::WebContents* source,
                                             int32 level,
                                             const string16& message,
                                             int32 line_no,
                                             const string16& source_id) {
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

void CefBrowserHostImpl::WebContentsFocused(content::WebContents* contents) {
  if (client_.get()) {
    CefRefPtr<CefFocusHandler> handler = client_->GetFocusHandler();
    if (handler.get())
      handler->OnGotFocus(this);
  }
}

bool CefBrowserHostImpl::HandleContextMenu(
    const content::ContextMenuParams& params) {
  if (!menu_creator_.get())
    menu_creator_.reset(new CefMenuCreator(this));
  return menu_creator_->CreateContextMenu(params);
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

#if defined(OS_WIN)
      CefEventHandle os_event = const_cast<CefEventHandle>(&event.os_event);
#else
      CefEventHandle os_event = event.os_event;
#endif

      cef_event.focus_on_editable_field = focus_on_editable_field_;

      return handler->OnPreKeyEvent(this, cef_event, os_event,
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
#if defined(OS_WIN)
        CefEventHandle os_event = const_cast<CefEventHandle>(&event.os_event);
#else
        CefEventHandle os_event = event.os_event;
#endif

        cef_event.focus_on_editable_field = focus_on_editable_field_;

        if (handler->OnKeyEvent(this, cef_event, os_event))
          return;
      }
    }
  }

  PlatformHandleKeyboardEvent(event);
}

bool CefBrowserHostImpl::CanDragEnter(
    content::WebContents* source,
    const content::DropData& data,
    WebKit::WebDragOperationsMask mask) {
  CefRefPtr<CefDragHandler> handler = client_->GetDragHandler();
  if (handler.get()) {
    CefRefPtr<CefDragData> drag_data(new CefDragDataImpl(data));
    if (handler->OnDragEnter(this, drag_data,
            static_cast<CefDragHandler::DragOperationsMask>(mask))) {
      return false;
    }
  }
  return true;
}

bool CefBrowserHostImpl::ShouldCreateWebContents(
    content::WebContents* web_contents,
      int route_id,
      WindowContainerType window_container_type,
      const string16& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) {
  // In cases where the navigation will occur in a new render process the
  // |route_id| value will be MSG_ROUTING_NONE here (because the existing
  // renderer will not be able to communicate with the new renderer) and
  // OpenURLFromTab will be called after WebContentsCreated.
  base::AutoLock lock_scope(pending_popup_info_lock_);
  DCHECK(pending_popup_info_.get());
  CefContentBrowserClient::Get()->set_use_osr_next_contents_view(
      IsWindowRenderingDisabled(pending_popup_info_->window_info));

  return true;
}

void CefBrowserHostImpl::WebContentsCreated(
    content::WebContents* source_contents,
    int64 source_frame_id,
    const string16& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  scoped_ptr<PendingPopupInfo> pending_popup_info;
  {
    base::AutoLock lock_scope(pending_popup_info_lock_);
    pending_popup_info.reset(pending_popup_info_.release());
  }
  DCHECK(pending_popup_info.get());

  CefWindowHandle opener = NULL;
  scoped_refptr<CefBrowserInfo> info =
      CefContentBrowserClient::Get()->GetOrCreateBrowserInfo(
          new_contents->GetRenderProcessHost()->GetID(),
          new_contents->GetRoutingID());

  if (source_contents) {
    DCHECK(info->is_popup());
    opener = GetBrowserForContents(source_contents)->GetWindowHandle();
  } else {
    DCHECK(!info->is_popup());
  }

  CefRefPtr<CefBrowserHostImpl> browser = CefBrowserHostImpl::Create(
      pending_popup_info->window_info, pending_popup_info->settings,
      pending_popup_info->client, new_contents, info, opener, NULL);
}

void CefBrowserHostImpl::DidNavigateMainFramePostCommit(
    content::WebContents* tab) {
  base::AutoLock lock_scope(state_lock_);
  has_document_ = false;
}

content::JavaScriptDialogManager*
    CefBrowserHostImpl::GetJavaScriptDialogManager() {
  if (!dialog_manager_.get())
    dialog_manager_.reset(new CefJavaScriptDialogManager(this));
  return dialog_manager_.get();
}

void CefBrowserHostImpl::RunFileChooser(
    content::WebContents* tab,
    const content::FileChooserParams& params) {
  content::RenderViewHost* render_view_host = tab->GetRenderViewHost();
  if (!render_view_host)
    return;

  RunFileChooserOnUIThread(params,
      base::Bind(&CefBrowserHostImpl::OnRunFileChooserDelegateCallback, this,
                 tab, params.mode));
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
  CEF_CURRENTLY_ON_UIT();

  content::MediaStreamDevices devices;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kEnableMediaStream)) {
    // Cancel the request.
    callback.Run(devices, scoped_ptr<content::MediaStreamUI>());
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
        // For open device request pick the desired device or fall back to the
        // first available of the given type.
        CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
            (microphone_requested ? request.requested_audio_device_id :
                                    request.requested_video_device_id),
            microphone_requested,
            webcam_requested,
            &devices);
        break;
      case content::MEDIA_DEVICE_ACCESS:
      case content::MEDIA_GENERATE_STREAM:
      case content::MEDIA_ENUMERATE_DEVICES:
        // Get the default devices for the request.
        CefMediaCaptureDevicesDispatcher::GetInstance()->
            GetDefaultDevices(CefContentBrowserClient::Get()->pref_service(),
                              microphone_requested,
                              webcam_requested,
                              &devices);
        break;
    }
  }

  callback.Run(devices, scoped_ptr<content::MediaStreamUI>());
}


// content::WebContentsObserver methods.
// -----------------------------------------------------------------------------

void CefBrowserHostImpl::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  browser_info_->add_render_id(render_view_host->GetProcess()->GetID(),
                               render_view_host->GetRoutingID());

  // Update the DevTools URLs, if any.
  CefDevToolsDelegate* devtools_delegate =
      CefContentBrowserClient::Get()->devtools_delegate();
  if (devtools_delegate) {
    base::AutoLock lock_scope(state_lock_);
    devtools_url_http_ =
        devtools_delegate->GetDevToolsURL(render_view_host, true);
    devtools_url_chrome_ =
        devtools_delegate->GetDevToolsURL(render_view_host, false);
  }

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
  browser_info_->remove_render_id(render_view_host->GetProcess()->GetID(),
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
    if (handler.get())
      handler->OnRenderProcessTerminated(this, ts);
  }
}

void CefBrowserHostImpl::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    const string16& frame_unique_name,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  CefRefPtr<CefFrame> frame = GetOrCreateFrame(frame_id,
      CefFrameHostImpl::kUnspecifiedFrameId, is_main_frame, string16(), url);
  OnLoadStart(frame, url, transition_type);
  if (is_main_frame)
    OnAddressChange(frame, url);
}

void CefBrowserHostImpl::DidFailProvisionalLoad(
    int64 frame_id,
    const string16& frame_unique_name,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    content::RenderViewHost* render_view_host) {
  CefRefPtr<CefFrame> frame = GetOrCreateFrame(frame_id,
      CefFrameHostImpl::kUnspecifiedFrameId, is_main_frame, string16(),
      GURL());
  OnLoadError(frame, validated_url, error_code, error_description);
}

void CefBrowserHostImpl::DocumentAvailableInMainFrame() {
  base::AutoLock lock_scope(state_lock_);
  has_document_ = true;
}

void CefBrowserHostImpl::DidFailLoad(
    int64 frame_id,
    const GURL& validated_url,
    bool is_main_frame,
    int error_code,
    const string16& error_description,
    content::RenderViewHost* render_view_host) {
  CefRefPtr<CefFrame> frame = GetOrCreateFrame(frame_id,
      CefFrameHostImpl::kUnspecifiedFrameId, is_main_frame, string16(),
      validated_url);
  OnLoadError(frame, validated_url, error_code, error_description);
  OnLoadEnd(frame, validated_url, error_code);
}

void CefBrowserHostImpl::PluginCrashed(const base::FilePath& plugin_path,
                                       base::ProcessId plugin_pid) {
  if (client_.get()) {
    CefRefPtr<CefRequestHandler> handler = client_->GetRequestHandler();
    if (handler.get())
      handler->OnPluginCrashed(this, plugin_path.value());
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
    IPC_MESSAGE_HANDLER(CefHostMsg_FrameDetached, DetachFrame)
    IPC_MESSAGE_HANDLER(CefHostMsg_FrameFocusChange, SetFocusedFrame)
    IPC_MESSAGE_HANDLER(CefHostMsg_DidFinishLoad, OnDidFinishLoad)
    IPC_MESSAGE_HANDLER(CefHostMsg_LoadingURLChange, OnLoadingURLChange)
    IPC_MESSAGE_HANDLER(CefHostMsg_Request, OnRequest)
    IPC_MESSAGE_HANDLER(CefHostMsg_Response, OnResponse)
    IPC_MESSAGE_HANDLER(CefHostMsg_ResponseAck, OnResponseAck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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
                                           string16 name) {
  bool is_main_frame = (parent_frame_id == CefFrameHostImpl::kMainFrameId);
  GetOrCreateFrame(frame_id, parent_frame_id, is_main_frame, name, GURL());
}

void CefBrowserHostImpl::OnDidFinishLoad(int64 frame_id,
                                         const GURL& validated_url,
                                         bool is_main_frame,
                                         int http_status_code) {
  CefRefPtr<CefFrame> frame = GetOrCreateFrame(frame_id,
      CefFrameHostImpl::kUnspecifiedFrameId, is_main_frame, string16(),
      validated_url);

  // Give internal scheme handlers an opportunity to update content.
  scheme::DidFinishLoad(frame, validated_url);

  OnLoadEnd(frame, validated_url, http_status_code);
}

void CefBrowserHostImpl::OnLoadingURLChange(const GURL& loading_url) {
  base::AutoLock lock_scope(state_lock_);
  loading_url_ = loading_url;
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
  } else if (params.name == scheme::kChromeProcessMessage) {
    scheme::OnChromeProcessMessage(this, params.arguments);
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
         type == content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE ||
         type == content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED);

   if (type == content::NOTIFICATION_LOAD_STOP ||
       type == content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED) {
    string16 title;

    if (type == content::NOTIFICATION_LOAD_STOP) {
      content::NavigationController* controller =
          content::Source<content::NavigationController>(source).ptr();
      title = controller->GetWebContents()->GetTitle();
    } else {
      content::WebContents* web_contents =
          content::Source<content::WebContents>(source).ptr();
      title = web_contents->GetTitle();
    }

    // Don't notify if the title hasn't changed.
    if (title == title_)
      return;

    title_ = title;

    if (client_.get()) {
      CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
      if (handler.get())
        handler->OnTitleChange(this, title);
    }
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
    CefWindowHandle opener)
    : content::WebContentsObserver(web_contents),
      window_info_(window_info),
      settings_(settings),
      client_(client),
      browser_info_(browser_info),
      opener_(opener),
      is_loading_(false),
      can_go_back_(false),
      can_go_forward_(false),
      has_document_(false),
      queue_messages_(true),
      main_frame_id_(CefFrameHostImpl::kInvalidFrameId),
      focused_frame_id_(CefFrameHostImpl::kInvalidFrameId),
      destruction_state_(DESTRUCTION_STATE_NONE),
      window_destroyed_(false),
      is_in_onsetfocus_(false),
      focus_on_editable_field_(false),
      mouse_cursor_change_disabled_(false),
      file_chooser_pending_(false) {
  DCHECK(!browser_info_->browser().get());
  browser_info_->set_browser(this);

  web_contents_.reset(web_contents);
  web_contents->SetDelegate(this);

  CefBrowserContext* browser_context =
      static_cast<CefBrowserContext*>(web_contents->GetBrowserContext());
  request_context_ = new CefRequestContextImpl(browser_context);

  registrar_.reset(new content::NotificationRegistrar);
  registrar_->Add(this, content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
                  content::Source<content::WebContents>(web_contents));

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

  if (IsWindowRenderingDisabled()) {
    CefRenderWidgetHostViewOSR* view =
        static_cast<CefRenderWidgetHostViewOSR*>(
            web_contents->GetRenderViewHost()->GetView());
    if (view)
      view->set_browser_impl(this);
  }
}

CefRefPtr<CefFrame> CefBrowserHostImpl::GetOrCreateFrame(
    int64 frame_id, int64 parent_frame_id, bool is_main_frame,
    string16 frame_name, const GURL& frame_url) {
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

void CefBrowserHostImpl::DetachFrame(int64 frame_id) {
  base::AutoLock lock_scope(state_lock_);

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
        focused_frame ? frame_id : CefFrameHostImpl::kInvalidFrameId;
  }

  if (unfocused_frame)
    unfocused_frame->SetFocused(false);
  if (focused_frame)
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
                                     content::PageTransition transition_type) {
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
                                     const string16& error_description) {
  if (client_.get()) {
    CefRefPtr<CefLoadHandler> handler = client_->GetLoadHandler();
    if (handler.get()) {
      // Notify the handler that loading has failed.
      handler->OnLoadError(this, frame,
          static_cast<cef_errorcode_t>(error_code),
          CefString(error_description),
          url.spec());
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

void CefBrowserHostImpl::RunFileChooserOnUIThread(
    const content::FileChooserParams& params,
    const RunFileChooserCallback& callback) {
  CEF_REQUIRE_UIT();

  if (file_chooser_pending_) {
    // Dismiss the new dialog immediately.
    callback.Run(std::vector<base::FilePath>());
    return;
  }

  if (params.mode == content::FileChooserParams::UploadFolder) {
    NOTIMPLEMENTED();
    callback.Run(std::vector<base::FilePath>());
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
      cef_file_dialog_mode_t mode = FILE_DIALOG_OPEN;
      switch (params.mode) {
        case content::FileChooserParams::Open:
          mode = FILE_DIALOG_OPEN;
          break;
        case content::FileChooserParams::OpenMultiple:
          mode = FILE_DIALOG_OPEN_MULTIPLE;
          break;
        case content::FileChooserParams::Save:
          mode = FILE_DIALOG_SAVE;
          break;
        default:
          NOTREACHED();
          break;
      }

      std::vector<CefString> accept_types;
      std::vector<string16>::const_iterator it = params.accept_types.begin();
      for (; it != params.accept_types.end(); ++it)
        accept_types.push_back(*it);

      CefRefPtr<CefFileDialogCallbackImpl> callbackImpl(
          new CefFileDialogCallbackImpl(host_callback));
      handled = handler->OnFileDialog(this, mode, params.title,
                                      params.default_file_name.value(),
                                      accept_types, callbackImpl.get());
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
    const std::vector<base::FilePath>& file_paths) {
  CEF_REQUIRE_UIT();

  file_chooser_pending_ = false;

  // Execute the callback asynchronously.
  CEF_POST_TASK(CEF_UIT, base::Bind(callback, file_paths));
}

void CefBrowserHostImpl::OnRunFileChooserDelegateCallback(
    content::WebContents* tab,
    content::FileChooserParams::Mode mode,
    const std::vector<base::FilePath>& file_paths) {
  CEF_REQUIRE_UIT();

  content::RenderViewHost* render_view_host = tab->GetRenderViewHost();
  if (!render_view_host)
    return;

  // Convert FilePath list to SelectedFileInfo list.
  std::vector<ui::SelectedFileInfo> selected_files;
  for (size_t i = 0; i < file_paths.size(); ++i) {
    selected_files.push_back(
        ui::SelectedFileInfo(file_paths[i], base::FilePath()));
  }

  // Notify our RenderViewHost in all cases.
  render_view_host->FilesSelectedInChooser(selected_files, mode);
}
