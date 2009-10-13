// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webview_host.h"
#include "browser_webview_delegate.h"

#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/win_util.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webview.h"

static const wchar_t kWindowClassName[] = L"WebViewHost";

/*static*/
WebViewHost* WebViewHost::Create(HWND parent_view,
                                 BrowserWebViewDelegate* delegate,
                                 const WebPreferences& prefs) {
  WebViewHost* host = new WebViewHost();

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
  win_util::SetWindowUserData(host->view_, host);

  host->webwidget_ = WebView::Create(delegate);
  prefs.Apply(host->webview());
  host->webview()->initializeMainFrame(delegate);

  return host;
}

WebView* WebViewHost::webview() const {
  return static_cast<WebView*>(webwidget_);
}

