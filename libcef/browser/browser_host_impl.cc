// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#include <string>
#include <utility>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/chrome_scheme_handler.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools_delegate.h"
#include "libcef/browser/navigate_params.h"
#include "libcef/browser/scheme_registration.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/url_request_context_getter.h"
#include "libcef/browser/url_request_context_getter_proxy.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/http_header_utils.h"
#include "libcef/common/main_delegate.h"
#include "libcef/common/process_message_impl.h"
#include "libcef/common/request_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/file_chooser_params.h"
#include "ui/base/dialogs/selected_file_info.h"

#if defined(OS_WIN)
#include "libcef/browser/render_widget_host_view_osr.h"
#include "libcef/browser/web_contents_view_osr.h"
#endif

namespace {

class CreateBrowserHelper {
 public:
  CreateBrowserHelper(const CefWindowInfo& windowInfo,
                      CefRefPtr<CefClient> client,
                      const CefString& url,
                      const CefBrowserSettings& settings)
                      : window_info_(windowInfo),
                        client_(client),
                        url_(url),
                        settings_(settings) {}

  CefWindowInfo window_info_;
  CefRefPtr<CefClient> client_;
  CefString url_;
  CefBrowserSettings settings_;
};

void CreateBrowserWithHelper(CreateBrowserHelper* helper) {
  CefBrowserHost::CreateBrowserSync(helper->window_info_, helper->client_,
      helper->url_, helper->settings_);
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
    cef_event.modifiers |= KEY_SHIFT;
  if (event.modifiers & WebKit::WebKeyboardEvent::ControlKey)
    cef_event.modifiers |= KEY_CTRL;
  if (event.modifiers & WebKit::WebKeyboardEvent::AltKey)
    cef_event.modifiers |= KEY_ALT;
  if (event.modifiers & WebKit::WebKeyboardEvent::MetaKey)
    cef_event.modifiers |= KEY_META;
  if (event.modifiers & WebKit::WebKeyboardEvent::IsKeyPad)
    cef_event.modifiers |= KEY_KEYPAD;

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
        std::vector<FilePath> vec;
        if (!file_paths.empty()) {
          std::vector<CefString>::const_iterator it = file_paths.begin();
          for (; it != file_paths.end(); ++it)
            vec.push_back(FilePath(*it));

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
    std::vector<FilePath> file_paths;
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

  void Callback(const std::vector<FilePath>& file_paths) {
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
bool CefBrowserHost::CreateBrowser(const CefWindowInfo& windowInfo,
                                   CefRefPtr<CefClient> client,
                                   const CefString& url,
                                   const CefBrowserSettings& settings) {
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

  // Verify that render handler is in place for a windowless browser.
  if (CefBrowserHostImpl::IsWindowRenderingDisabled(windowInfo) &&
      !client->GetRenderHandler().get()) {
    NOTREACHED() << "CefRenderHandler implementation is required";
    return false;
  }

  // Create the browser on the UI thread.
  CreateBrowserHelper* helper =
      new CreateBrowserHelper(windowInfo, client, url, settings);
  CEF_POST_TASK(CEF_UIT, base::Bind(CreateBrowserWithHelper, helper));

  return true;
}

// static
CefRefPtr<CefBrowser> CefBrowserHost::CreateBrowserSync(
    const CefWindowInfo& windowInfo,
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings) {
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

  if (CefBrowserHostImpl::IsWindowRenderingDisabled(windowInfo) &&
      !client->GetRenderHandler().get()) {
    NOTREACHED() << "CefRenderHandler implementation is required";
    return NULL;
  }

  _Context->browser_context()->set_use_osr_next_contents_view(
      CefBrowserHostImpl::IsWindowRenderingDisabled(windowInfo));

  int browser_id = _Context->GetNextBrowserID();
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::Create(windowInfo, settings, client, NULL, browser_id,
                                 NULL);
  if (!url.empty())
    browser->LoadURL(CefFrameHostImpl::kMainFrameId, url);
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
    int browser_id,
    CefWindowHandle opener) {
  CEF_REQUIRE_UIT();

  if (web_contents == NULL) {
    web_contents = content::WebContents::Create(
        _Context->browser_context(),
        NULL,
        MSG_ROUTING_NONE,
        NULL);
  }

  CefRefPtr<CefBrowserHostImpl> browser =
      new CefBrowserHostImpl(window_info, settings, client, web_contents,
                             browser_id, opener);
  if (!browser->IsWindowRenderingDisabled() &&
      !browser->PlatformCreateWindow()) {
    return NULL;
  }

  // TODO(port): Implement this method to work on other platforms as part of
  // off-screen rendering support.
#if defined(OS_WIN)
  if (browser->IsWindowRenderingDisabled()) {
    CefRenderWidgetHostViewOSR* view =
        static_cast<CefRenderWidgetHostViewOSR*>(
            web_contents->GetRenderViewHost()->GetView());
    if (view)
      view->set_browser_impl(browser);
  }
#endif  // OS_WIN

  _Context->AddBrowser(browser);

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
  int render_view_id = -1;

  if (!content::ResourceRequestInfo::GetRenderViewForRequest(request,
                                                             &render_process_id,
                                                             &render_view_id)) {
    return NULL;
  }

  return GetBrowserByRoutingID(render_process_id, render_view_id);
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserByRoutingID(
    int render_process_id, int render_view_id) {
  if (CEF_CURRENTLY_ON_UIT()) {
    // Use the non-thread-safe but potentially faster approach.
    content::RenderViewHost* render_view_host =
        content::RenderViewHost::FromID(render_process_id, render_view_id);
    if (!render_view_host)
      return NULL;
    return GetBrowserForHost(render_view_host);
  } else {
    // Use the thread-safe approach.
    return _Context->GetBrowserByRoutingID(render_process_id, render_view_id);
  }
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserByChildID(
      int render_process_id) {
  if (CEF_CURRENTLY_ON_UIT()) {
    // Use the non-thread-safe but potentially faster approach.
    content::RenderProcessHost* render_process_host =
        content::RenderProcessHost::FromID(render_process_id);
    if (!render_process_host)
      return NULL;

    content::RenderProcessHost::RenderWidgetHostsIterator iter(
        render_process_host->GetRenderWidgetHostsIterator());
    if (!iter.IsAtEnd()) {
      const content::RenderWidgetHost* widget = iter.GetCurrentValue();

      // This implementation is based on an assumption that each process hosts a
      // single renderer. Revisit this implementation if the assumption proves
      // false.
      iter.Advance();
      DCHECK(iter.IsAtEnd());

      if (widget && widget->IsRenderView()) {
        return GetBrowserForHost(content::RenderViewHost::From(
            const_cast<content::RenderWidgetHost*>(widget)));
      }
    }

    return NULL;
  } else {
    // Use the thread-safe approach.
    return _Context->GetBrowserByRoutingID(render_process_id, 0);
  }
}


// CefBrowserHost methods.
// -----------------------------------------------------------------------------

CefRefPtr<CefBrowser> CefBrowserHostImpl::GetBrowser() {
  return this;
}

void CefBrowserHostImpl::CloseBrowser() {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (IsWindowRenderingDisabled()) {
      if (AllowDestroyBrowser()) {
        ParentWindowWillClose();
        DestroyBrowser();
      }
    } else {
      PlatformCloseWindow();
    }
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::CloseBrowser, this));
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
    params.default_file_name = FilePath(default_file_name);
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

void CefBrowserHostImpl::Invalidate(const CefRect& dirtyRect) {
  if (!IsWindowRenderingDisabled()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::Invalidate, this, dirtyRect));
    return;
  }

  if (!web_contents())
    return;

#if defined(OS_WIN)
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderViewHost()->GetView();
  CefRenderWidgetHostViewOSR* orview =
      static_cast<CefRenderWidgetHostViewOSR*>(view);

  if (orview) {
    gfx::Rect rect(dirtyRect.x, dirtyRect.y,
                   dirtyRect.width, dirtyRect.height);
    orview->Invalidate(rect);
  }
#else
  // TODO(port): Implement this method to work on other platforms as part of
  // off-screen rendering support.
  NOTREACHED();
#endif
}

void CefBrowserHostImpl::SendKeyEvent(const CefKeyEvent& event) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendKeyEvent, this, event));
    return;
  }

  if (!web_contents())
    return;

  content::RenderWidgetHost* widget = web_contents()->GetRenderViewHost();
  if (!widget)
    return;
  gfx::NativeEvent native_event;
  if (PlatformTranslateKeyEvent(native_event, event))
    widget->ForwardKeyboardEvent(content::NativeWebKeyboardEvent(native_event));
}

void CefBrowserHostImpl::SendMouseClickEvent(int x, int y,
    MouseButtonType type, bool mouseUp, int clickCount) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendMouseClickEvent, this, x, y, type,
                   mouseUp, clickCount));
    return;
  }

  if (!web_contents())
    return;

  content::RenderWidgetHost* widget = web_contents()->GetRenderViewHost();
  if (!widget)
    return;
  WebKit::WebMouseEvent event;
  if (PlatformTranslateClickEvent(event, x, y, type, mouseUp, clickCount))
    widget->ForwardMouseEvent(event);
}

void CefBrowserHostImpl::SendMouseMoveEvent(int x, int y, bool mouseLeave) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendMouseMoveEvent, this, x, y,
                   mouseLeave));
    return;
  }

  if (!web_contents())
    return;

  content::RenderWidgetHost* widget = web_contents()->GetRenderViewHost();
  if (!widget)
    return;
  WebKit::WebMouseEvent event;
  if (PlatformTranslateMoveEvent(event, x, y, mouseLeave))
    widget->ForwardMouseEvent(event);
}

void CefBrowserHostImpl::SendMouseWheelEvent(int x, int y,
                                             int deltaX, int deltaY) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserHostImpl::SendMouseWheelEvent, this, x, y,
                   deltaX, deltaY));
    return;
  }

  if (!web_contents())
    return;

  content::RenderWidgetHost* widget = web_contents()->GetRenderViewHost();
  if (!widget)
    return;
  WebKit::WebMouseWheelEvent event;
  if (PlatformTranslateWheelEvent(event, x, y, deltaX, deltaY))
    widget->ForwardWheelEvent(event);
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

bool CefBrowserHostImpl::IsPopup() {
  return (opener_ != NULL);
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

bool CefBrowserHostImpl::AllowDestroyBrowser() {
  if (client_.get()) {
    CefRefPtr<CefLifeSpanHandler> handler =
        client_->GetLifeSpanHandler();
    if (handler.get()) {
      // Give the client a chance to handle this one.
      return !handler->DoClose(this);
    }
  }

  return true;
}

void CefBrowserHostImpl::DestroyBrowser() {
  CEF_REQUIRE_UIT();

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

  DetachAllFrames();

  request_context_proxy_ = NULL;

  // Remove the browser from the list maintained by the context.
  _Context->RemoveBrowser(this);
}

gfx::NativeView CefBrowserHostImpl::GetContentView() const {
  CEF_REQUIRE_UIT();
  if (!web_contents_.get())
    return NULL;
  return web_contents_->GetNativeView();
}

content::WebContents* CefBrowserHostImpl::GetWebContents() const {
  CEF_REQUIRE_UIT();
  return web_contents_.get();
}

net::URLRequestContextGetter* CefBrowserHostImpl::GetRequestContext() {
  CEF_REQUIRE_UIT();
  if (!request_context_proxy_) {
    request_context_proxy_ =
        new CefURLRequestContextGetterProxy(this,
            static_cast<CefURLRequestContextGetter*>(
                _Context->browser_context()->GetRequestContext()));
  }
  return request_context_proxy_.get();
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
    DLOG(ERROR) << "Invalid URL passed to CefBrowserHostImpl::Navigate: " <<
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

void CefBrowserHostImpl::LoadURL(int64 frame_id, const std::string& url) {
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
          DLOG(ERROR) <<
              "Invalid URL passed to CefBrowserHostImpl::LoadURL: " << url;
          return;
        }

        // Update the loading URL.
        OnLoadingURLChange(gurl);

        web_contents_->GetController().LoadURL(
            gurl,
            content::Referrer(),
            content::PAGE_TRANSITION_TYPED,
            std::string());
        OnSetFocus(FOCUS_SOURCE_NAVIGATION);
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefBrowserHostImpl::LoadURL, this, frame_id, url));
    }
  } else {
    CefNavigateParams params(GURL(url), content::PAGE_TRANSITION_TYPED);
    params.frame_id = frame_id;
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

bool CefBrowserHostImpl::HasIDMatch(int render_process_id, int render_view_id) {
  base::AutoLock lock_scope(state_lock_);
  if (render_process_id != render_process_id_)
    return false;
  return (render_view_id == 0 || render_view_id == render_view_id_);
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
      web_contents_->Focus();
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


// content::WebContentsDelegate methods.
// -----------------------------------------------------------------------------

content::WebContents* CefBrowserHostImpl::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // Start the new navigation.
  CefNavigateParams nav_params(params.url, params.transition);
  nav_params.referrer = params.referrer;
  nav_params.frame_id = params.source_frame_id;
  nav_params.disposition = params.disposition;
  nav_params.user_gesture = true;
  nav_params.override_encoding = params.override_encoding;
  nav_params.is_renderer_initiated = params.is_renderer_initiated;
  nav_params.transferred_global_request_id =
      params.transferred_global_request_id;
  Navigate(nav_params);

  return source;
}

void CefBrowserHostImpl::LoadingStateChanged(content::WebContents* source) {
  int current_index = web_contents_->GetController().GetCurrentEntryIndex();
  int max_index = web_contents_->GetController().GetEntryCount() - 1;

  bool is_loading, can_go_back, can_go_forward;

  {
    base::AutoLock lock_scope(state_lock_);
    is_loading = is_loading_ = web_contents_->IsLoading();
    can_go_back = can_go_back_ = (current_index > 0);
    can_go_forward = can_go_forward_ = (current_index < max_index);
  }

  if (client_.get()) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler.get()) {
      handler->OnLoadingStateChange(this, is_loading, can_go_back,
                                    can_go_forward);
    }
  }
}

void CefBrowserHostImpl::CloseContents(content::WebContents* source) {
  PlatformCloseWindow();
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

bool CefBrowserHostImpl::ShouldCreateWebContents(
    content::WebContents* web_contents,
    int route_id,
    WindowContainerType window_container_type,
    const string16& frame_name,
    const GURL& target_url) {
  // Start with the current browser window's client and settings.
  pending_client_ = client_;
  pending_settings_ = settings_;

  // TODO(cef): Figure out how to populate these values.
  // See: http://crbug.com/110510
  CefPopupFeatures features;

  pending_window_info_ = CefWindowInfo();
#if defined(OS_WIN)
  pending_window_info_.SetAsPopup(NULL, CefString());
#endif

#if (defined(OS_WIN) || defined(OS_MACOSX))
  // Default to the size from the popup features.
  if (features.xSet)
    pending_window_info_.x = features.x;
  if (features.ySet)
    pending_window_info_.y = features.y;
  if (features.widthSet)
    pending_window_info_.width = features.width;
  if (features.heightSet)
    pending_window_info_.height = features.height;
#endif

  if (client_.get()) {
    CefRefPtr<CefLifeSpanHandler> handler = client_->GetLifeSpanHandler();
    // Give the handler an opportunity to modify window attributes, handler,
    // or cancel the window creation.
    if (handler.get() &&
        handler->OnBeforePopup(this, features, pending_window_info_,
                               target_url.spec(), pending_client_,
                               pending_settings_)) {
      pending_client_ = NULL;
      return false;
    }
  }

  if (IsWindowRenderingDisabled(pending_window_info_) &&
      !pending_client_->GetRenderHandler().get()) {
    NOTREACHED() << "CefRenderHandler implementation is required";
    return false;
  }

  _Context->browser_context()->set_use_osr_next_contents_view(
      IsWindowRenderingDisabled(pending_window_info_));

  return true;
}

void CefBrowserHostImpl::WebContentsCreated(
    content::WebContents* source_contents,
    int64 source_frame_id,
    const GURL& target_url,
    content::WebContents* new_contents) {
  CefWindowHandle opener = NULL;
  if (source_contents)
    opener = GetBrowserForContents(source_contents)->GetWindowHandle();

  CefContentBrowserClient::NewPopupBrowserInfo info;
  CefContentBrowserClient::Get()->GetNewPopupBrowserInfo(
      new_contents->GetRenderProcessHost()->GetID(),
      new_contents->GetRoutingID(),
      &info);
  DCHECK_GT(info.browser_id, 0);

  CefRefPtr<CefBrowserHostImpl> browser = CefBrowserHostImpl::Create(
      pending_window_info_, pending_settings_, pending_client_, new_contents,
      info.browser_id, opener);

  pending_client_ = NULL;
}

void CefBrowserHostImpl::DidNavigateMainFramePostCommit(
    content::WebContents* tab) {
  base::AutoLock lock_scope(state_lock_);
  has_document_ = false;
}

content::JavaScriptDialogCreator*
    CefBrowserHostImpl::GetJavaScriptDialogCreator() {
  if (!dialog_creator_.get())
    dialog_creator_.reset(new CefJavaScriptDialogCreator(this));
  return dialog_creator_.get();
}

void CefBrowserHostImpl::RunFileChooser(
    content::WebContents* tab,
    const content::FileChooserParams& params) {
  content::RenderViewHost* render_view_host = tab->GetRenderViewHost();
  if (!render_view_host)
    return;

  RunFileChooserOnUIThread(params,
      base::Bind(&CefBrowserHostImpl::OnRunFileChooserDelegateCallback, this,
                 tab));
}

void CefBrowserHostImpl::UpdatePreferredSize(content::WebContents* source,
                                             const gfx::Size& pref_size) {
  PlatformSizeTo(pref_size.width(), pref_size.height());
}

void CefBrowserHostImpl::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest* request,
    const content::MediaResponseCallback& callback) {
  CEF_CURRENTLY_ON_UIT();

  content::MediaStreamDevices devices;
  for (content::MediaStreamDeviceMap::const_iterator it =
       request->devices.begin(); it != request->devices.end(); ++it) {
    devices.push_back(*it->second.begin());
  }

  // TODO(cef): Give the user an opportunity to approve the device list or run
  // the callback with an empty device list to cancel the request.
  callback.Run(devices);
}


// content::WebContentsObserver methods.
// -----------------------------------------------------------------------------

void CefBrowserHostImpl::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  SetRenderViewHost(render_view_host);
}

void CefBrowserHostImpl::RenderViewDeleted(
    content::RenderViewHost* render_view_host) {
  registrar_->Remove(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
      content::Source<content::RenderViewHost>(render_view_host));
}

void CefBrowserHostImpl::RenderViewReady() {
  if (IsPopup()) {
    CefContentBrowserClient::Get()->ClearNewPopupBrowserInfo(
        web_contents()->GetRenderProcessHost()->GetID(),
        web_contents()->GetRoutingID());
  }

  // Send the queued messages.
  queue_messages_ = false;
  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }
}

void CefBrowserHostImpl::RenderViewGone(base::TerminationStatus status) {
  queue_messages_ = true;

  cef_termination_status_t ts = TS_ABNORMAL_TERMINATION;
  if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED)
    ts = TS_PROCESS_WAS_KILLED;
  else if (status == base::TERMINATION_STATUS_PROCESS_CRASHED)
    ts = TS_PROCESS_CRASHED;
  else if (status != base::TERMINATION_STATUS_ABNORMAL_TERMINATION)
    return;

  if (client_.get()) {
    CefRefPtr<CefLoadHandler> handler = client_->GetLoadHandler();
    if (handler.get())
      handler->OnRenderProcessTerminated(this, ts);
  }
}

void CefBrowserHostImpl::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
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
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    content::RenderViewHost* render_view_host) {
  CefRefPtr<CefFrame> frame = GetOrCreateFrame(frame_id,
      CefFrameHostImpl::kUnspecifiedFrameId, is_main_frame, string16(),
      validated_url);
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

void CefBrowserHostImpl::PluginCrashed(const FilePath& plugin_path) {
  if (client_.get()) {
    CefRefPtr<CefLoadHandler> handler = client_->GetLoadHandler();
    if (handler.get())
      handler->OnPluginCrashed(this, plugin_path.value());
  }
}

bool CefBrowserHostImpl::OnMessageReceived(const IPC::Message& message) {
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
  DCHECK(type == content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED ||
         type == content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE);

  if (type == content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED) {
    std::pair<content::NavigationEntry*, bool>* title =
        content::Details<std::pair<content::NavigationEntry*, bool> >(
            details).ptr();

    if (title->first) {
      if (client_.get()) {
        CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
        if (handler.get()) {
          CefString title_str = title->first->GetTitleForDisplay("");
          handler->OnTitleChange(this, title_str);
        }
      }
    }
  } else if (type == content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE) {
    focus_on_editable_field_ = *content::Details<bool>(details).ptr();
  }
}


// CefBrowserHostImpl private methods.
// -----------------------------------------------------------------------------

CefBrowserHostImpl::CefBrowserHostImpl(const CefWindowInfo& window_info,
                                       const CefBrowserSettings& settings,
                                       CefRefPtr<CefClient> client,
                                       content::WebContents* web_contents,
                                       int browser_id,
                                       CefWindowHandle opener)
    : content::WebContentsObserver(web_contents),
      window_info_(window_info),
      settings_(settings),
      client_(client),
      browser_id_(browser_id),
      opener_(opener),
      render_process_id_(MSG_ROUTING_NONE),
      render_view_id_(MSG_ROUTING_NONE),
      is_loading_(false),
      can_go_back_(false),
      can_go_forward_(false),
      has_document_(false),
      queue_messages_(true),
      main_frame_id_(CefFrameHostImpl::kInvalidFrameId),
      focused_frame_id_(CefFrameHostImpl::kInvalidFrameId),
      is_in_onsetfocus_(false),
      focus_on_editable_field_(false),
      file_chooser_pending_(false) {
  web_contents_.reset(web_contents);
  web_contents->SetDelegate(this);

  registrar_.reset(new content::NotificationRegistrar);
  registrar_->Add(this, content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
                  content::Source<content::WebContents>(web_contents));

  response_manager_.reset(new CefResponseManager);

  placeholder_frame_ =
      new CefFrameHostImpl(this, CefFrameHostImpl::kInvalidFrameId, true);

  SetRenderViewHost(web_contents->GetRenderViewHost());
}

void CefBrowserHostImpl::SetRenderViewHost(content::RenderViewHost* rvh) {
  {
    base::AutoLock lock_scope(state_lock_);

    render_view_id_ = rvh->GetRoutingID();
    render_process_id_ = rvh->GetProcess()->GetID();

    // Update the DevTools URLs, if any.
    CefDevToolsDelegate* devtools_delegate = _Context->devtools_delegate();
    if (devtools_delegate) {
      devtools_url_http_ = devtools_delegate->GetDevToolsURL(rvh, true);
      devtools_url_chrome_ = devtools_delegate->GetDevToolsURL(rvh, false);
    }
  }

  if (!registrar_->IsRegistered(
      this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
      content::Source<content::RenderViewHost>(rvh))) {
    registrar_->Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                    content::Source<content::RenderViewHost>(rvh));
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

  base::AutoLock lock_scope(state_lock_);

  if (is_main_frame)
    main_frame_id_ = frame_id;

  CefRefPtr<CefFrameHostImpl> frame;

  // Check if a frame object already exists.
  FrameMap::const_iterator it = frames_.find(frame_id);
  if (it != frames_.end())
    frame = it->second.get();

  if (!frame.get()) {
    // Create a new frame object.
    frame = new CefFrameHostImpl(this, frame_id, is_main_frame);
    frames_.insert(std::make_pair(frame_id, frame));
  }

  if (!url.empty())
    frame->SetURL(url);
  if (!name.empty())
    frame->SetName(name);
  if (parent_frame_id != CefFrameHostImpl::kUnspecifiedFrameId)
    frame->SetParentId(parent_frame_id);

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
  base::AutoLock lock_scope(state_lock_);

  FrameMap::const_iterator it = frames_.begin();
  for (; it != frames_.end(); ++it)
    it->second->Detach();

  frames_.clear();

  if (main_frame_id_ != CefFrameHostImpl::kInvalidFrameId)
    main_frame_id_ = CefFrameHostImpl::kInvalidFrameId;
  if (focused_frame_id_ != CefFrameHostImpl::kInvalidFrameId)
    focused_frame_id_ = CefFrameHostImpl::kInvalidFrameId;
}

void CefBrowserHostImpl::SetFocusedFrame(int64 frame_id) {
  base::AutoLock lock_scope(state_lock_);

  if (focused_frame_id_ != CefFrameHostImpl::kInvalidFrameId) {
    // Unfocus the previously focused frame.
    FrameMap::const_iterator it = frames_.find(frame_id);
    if (it != frames_.end())
      it->second->SetFocused(false);
  }

  if (frame_id != CefFrameHostImpl::kInvalidFrameId) {
    // Focus the newly focused frame.
    FrameMap::iterator it = frames_.find(frame_id);
    if (it != frames_.end()) {
      it->second->SetFocused(true);
      focused_frame_id_ = frame_id;
      return;
    }
  }

  // No valid frame found.
  focused_frame_id_ = CefFrameHostImpl::kInvalidFrameId;
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
    callback.Run(std::vector<FilePath>());
    return;
  }

  if (params.mode == content::FileChooserParams::OpenFolder) {
    NOTIMPLEMENTED();
    callback.Run(std::vector<FilePath>());
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
      cef_file_dialog_mode_t mode;
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
    const std::vector<FilePath>& file_paths) {
  CEF_REQUIRE_UIT();

  file_chooser_pending_ = false;

  // Execute the callback asynchronously.
  CEF_POST_TASK(CEF_UIT, base::Bind(callback, file_paths));
}

void CefBrowserHostImpl::OnRunFileChooserDelegateCallback(
    content::WebContents* tab,
    const std::vector<FilePath>& file_paths) {
  CEF_REQUIRE_UIT();

  content::RenderViewHost* render_view_host = tab->GetRenderViewHost();
  if (!render_view_host)
    return;

  const int kReadFilePermissions =
      base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_EXCLUSIVE_READ |
      base::PLATFORM_FILE_ASYNC;

  // Convert FilePath list to SelectedFileInfo list.
  std::vector<ui::SelectedFileInfo> selected_files;
  for (size_t i = 0; i < file_paths.size(); ++i)
    selected_files.push_back(ui::SelectedFileInfo(file_paths[i], FilePath()));

  // Notify our RenderViewHost in all cases.
  render_view_host->FilesSelectedInChooser(selected_files,
                                           kReadFilePermissions);
}
