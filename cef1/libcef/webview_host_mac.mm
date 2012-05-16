// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "libcef/webview_host.h"
#include "libcef/browser_webview_delegate.h"
#include "libcef/browser_webview_mac.h"
#include "libcef/cef_context.h"

#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/mac/WebInputEventFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webpreferences.h"

using WebKit::WebDevToolsAgentClient;
using WebKit::WebKeyboardEvent;
using WebKit::WebInputEvent;
using WebKit::WebInputEventFactory;
using WebKit::WebSize;
using WebKit::WebView;

// static
WebViewHost* WebViewHost::Create(NSView* parent_view,
                                 const gfx::Rect& rect,
                                 BrowserWebViewDelegate* delegate,
                                 PaintDelegate* paint_delegate,
                                 WebDevToolsAgentClient* dev_tools_client,
                                 const WebPreferences& prefs) {
  WebViewHost* host = new WebViewHost(delegate);

  NSRect content_rect = {{rect.x(), rect.y()}, {rect.width(), rect.height()}};
  if (!paint_delegate) {
    host->view_ = [[BrowserWebView alloc] initWithFrame:content_rect];
    [host->view_ setAutoresizingMask:(NSViewWidthSizable |
                                      NSViewHeightSizable)];
    [parent_view addSubview:host->view_];
    [host->view_ release];
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
  host->webwidget_->resize(WebSize(content_rect.size.width,
                                   content_rect.size.height));

  return host;
}

WebViewHost::~WebViewHost() {
  BrowserWebView* webView = static_cast<BrowserWebView*>(view_);
  webView.browser = NULL;
}

WebView* WebViewHost::webview() const {
  return static_cast<WebView*>(webwidget_);
}

void WebViewHost::SetIsActive(bool active) {
  webview()->setIsActive(active);
}

void WebViewHost::MouseEvent(NSEvent* event) {
  _Context->set_current_webviewhost(this);
  WebWidgetHost::MouseEvent(event);
}

void WebViewHost::KeyEvent(NSEvent *event) {
  WebKeyboardEvent keyboard_event(WebInputEventFactory::keyboardEvent(event));
  if (delegate_->OnKeyboardEvent(keyboard_event, false))
    return;

  WebWidgetHost::KeyEvent(event);
}

void WebViewHost::SetFocus(bool enable) {
  if (enable) {
    // Set the current WebViewHost in case a drag action is started before mouse
    // events are detected for the window.
    _Context->set_current_webviewhost(this);
  }
  WebWidgetHost::SetFocus(enable);
}
