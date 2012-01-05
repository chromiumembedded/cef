// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webview_host.h"
#include "browser_webview_delegate.h"
#include "cef_context.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebInputEventFactory.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webpreferences.h"

using WebKit::WebDevToolsAgentClient;
using WebKit::WebKeyboardEvent;
using WebKit::WebInputEvent;
using WebKit::WebInputEventFactory;
using WebKit::WebView;

static const wchar_t kWindowClassName[] = L"WebViewHost";

/*static*/
WebViewHost* WebViewHost::Create(HWND parent_view,
                                 const gfx::Rect&,
                                 BrowserWebViewDelegate* delegate,
                                 PaintDelegate* paint_delegate,
                                 WebDevToolsAgentClient* dev_tools_client,
                                 const WebPreferences& prefs) {
  WebViewHost* host = new WebViewHost(delegate);

  if (!paint_delegate) {
    static bool registered_class = false;
    if (!registered_class) {
      WNDCLASSEX wcex = {0};
      wcex.cbSize        = sizeof(wcex);
      wcex.style         = CS_DBLCLKS;
      wcex.lpfnWndProc   = WebWidgetHost::WndProc;
      wcex.hInstance     = GetModuleHandle(NULL);
      wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
      wcex.lpszClassName = kWindowClassName;
      RegisterClassEx(&wcex);
      registered_class = true;
    }

    host->view_ = CreateWindow(kWindowClassName, NULL,
                               WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0,
                               0, 0, parent_view, NULL,
                               GetModuleHandle(NULL), NULL);
    ui::SetWindowUserData(host->view_, host);
  } else {
    host->paint_delegate_ = paint_delegate;
  }

#if defined(WEBKIT_HAS_WEB_AUTO_FILL_CLIENT)
  host->webwidget_ = WebView::create(delegate, NULL);
#else
  host->webwidget_ = WebView::create(delegate);
#endif
  host->webview()->setDevToolsAgentClient(dev_tools_client);
  host->webview()->setPermissionClient(delegate);
  prefs.Apply(host->webview());
  host->webview()->initializeMainFrame(delegate);

  return host;
}

WebView* WebViewHost::webview() const {
  return static_cast<WebView*>(webwidget_);
}

bool WebViewHost::WndProc(UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
  case WM_SETFOCUS:
    // Set the current WebViewHost in case a drag action is started before mouse
    // events are detected for the window.
    _Context->set_current_webviewhost(this);
    break;
  }
  return false;
}

void WebViewHost::MouseEvent(UINT message, WPARAM wparam, LPARAM lparam) {
  _Context->set_current_webviewhost(this);
  WebWidgetHost::MouseEvent(message, wparam, lparam);
}

void WebViewHost::KeyEvent(UINT message, WPARAM wparam, LPARAM lparam) {
  // Give the client a chance to handle keyboard events before they're passed
  // to WebKit.
  const WebKeyboardEvent& event = WebInputEventFactory::keyboardEvent(
      view_, message, wparam, lparam);
  if (delegate_->OnKeyboardEvent(event, false))
    return;

  WebWidgetHost::KeyEvent(message, wparam, lparam);
}
