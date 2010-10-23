// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "webview_host.h"
#include "browser_webview_delegate.h"
#include "browser_webview_mac.h"

#include "gfx/rect.h"
#include "gfx/size.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webpreferences.h"

using WebKit::WebDevToolsAgentClient;
using WebKit::WebSize;
using WebKit::WebView;

// static
WebViewHost* WebViewHost::Create(NSView* parent_view,
                                 BrowserWebViewDelegate* delegate,
                                 WebDevToolsAgentClient* dev_tools_client,
                                 const WebPreferences& prefs) {
  WebViewHost* host = new WebViewHost();

  NSRect content_rect = [parent_view frame];
  // bump down the top of the view so that it doesn't overlap the buttons
  // and URL field.  32 is an ad hoc constant.
  // TODO(awalker): replace explicit view layout with a little nib file
  // and use that for view geometry.
  content_rect.size.height -= 32;
  host->view_ = [[BrowserWebView alloc] initWithFrame:content_rect];
  // make the height and width track the window size.
  [host->view_ setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  [parent_view addSubview:host->view_];
  [host->view_ release];

  host->webwidget_ = WebView::create(delegate, dev_tools_client);
  prefs.Apply(host->webview());
  host->webview()->initializeMainFrame(delegate);
  host->webwidget_->resize(WebSize(content_rect.size.width,
                                   content_rect.size.height));

  return host;
}

WebView* WebViewHost::webview() const {
  return static_cast<WebView*>(webwidget_);
}

void WebViewHost::SetIsActive(bool active) {
  webview()->setIsActive(active);
}
