// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/webview_host.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

WebViewHost::WebViewHost(BrowserWebViewDelegate* delegate)
  : delegate_(delegate) {
}

#if !defined(OS_MACOSX)
WebViewHost::~WebViewHost() {
}
#endif

bool WebViewHost::IsTransparent() {
  return static_cast<WebKit::WebView*>(webwidget_)->isTransparent();
}
