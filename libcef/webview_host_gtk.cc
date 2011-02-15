// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "webview_host.h"
#include "browser_webview_delegate.h"

#include "base/logging.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/plugins/npapi/gtk_plugin_container.h"

using WebKit::WebDevToolsAgentClient;
using WebKit::WebView;

// static
WebViewHost* WebViewHost::Create(GtkWidget* parent_view,
                                 const gfx::Rect& rect,
                                 BrowserWebViewDelegate* delegate,
                                 WebDevToolsAgentClient* dev_tools_client,
                                 const WebPreferences& prefs) {
  WebViewHost* host = new WebViewHost();

  host->view_ = WebWidgetHost::CreateWidget(parent_view, host);
  host->plugin_container_manager_.set_host_widget(host->view_);

#if defined(WEBKIT_HAS_WEB_AUTO_FILL_CLIENT)
  host->webwidget_ = WebView::create(delegate, dev_tools_client, NULL);
#else
  host->webwidget_ = WebView::create(delegate, dev_tools_client);
#endif
  prefs.Apply(host->webview());
  host->webview()->initializeMainFrame(delegate);
  host->webwidget_->layout();

  return host;
}

WebView* WebViewHost::webview() const {
  return static_cast<WebView*>(webwidget_);
}

void WebViewHost::CreatePluginContainer(gfx::PluginWindowHandle id) {
  plugin_container_manager_.CreatePluginContainer(id);
}

void WebViewHost::DestroyPluginContainer(gfx::PluginWindowHandle id) {
  plugin_container_manager_.DestroyPluginContainer(id);
}
