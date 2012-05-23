// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_WEBVIEW_HOST_H_
#define CEF_LIBCEF_WEBVIEW_HOST_H_
#pragma once

#include "libcef/webwidget_host.h"
#include "base/basictypes.h"
#include "ui/gfx/native_widget_types.h"

#if defined(TOOLKIT_GTK)
#include "webkit/plugins/npapi/gtk_plugin_container_manager.h"
#endif

class BrowserWebViewDelegate;

namespace webkit_glue {
struct WebPreferences;
}

namespace WebKit {
class WebDevToolsAgentClient;
class WebView;
}

// This class is a simple NativeView-based host for a WebView
class WebViewHost : public WebWidgetHost {
 public:
  // The new instance is deleted once the associated NativeView is destroyed.
  // The newly created window should be resized after it is created, using the
  // MoveWindow (or equivalent) function.
  static WebViewHost* Create(gfx::NativeView parent_view,
                             const gfx::Rect& rect,
                             BrowserWebViewDelegate* delegate,
                             PaintDelegate* paint_delegate,
                             WebKit::WebDevToolsAgentClient* devtools_client,
                             const webkit_glue::WebPreferences& prefs);

  virtual ~WebViewHost();

  WebKit::WebView* webview() const;

#if defined(TOOLKIT_GTK)
  // Create a new plugin parent container for a given plugin XID.
  void CreatePluginContainer(gfx::PluginWindowHandle id);

  // Destroy the plugin parent container when a plugin has been destroyed.
  void DestroyPluginContainer(gfx::PluginWindowHandle id);

  webkit::npapi::GtkPluginContainerManager* plugin_container_manager() {
    return &plugin_container_manager_;
  }

  virtual void KeyEvent(GdkEventKey* event);
#elif defined(OS_MACOSX)
  void SetIsActive(bool active);
  virtual void MouseEvent(NSEvent* event);
  virtual void KeyEvent(NSEvent* event);
  virtual void SetFocus(bool enable);
#endif

 protected:
  explicit WebViewHost(BrowserWebViewDelegate* delegate);

#if defined(OS_WIN)
  virtual bool WndProc(UINT message, WPARAM wparam, LPARAM lparam);
  virtual void MouseEvent(UINT message, WPARAM wparam, LPARAM lparam);
  virtual void KeyEvent(UINT message, WPARAM wparam, LPARAM lparam);
#endif

#if defined(TOOLKIT_GTK)
  // Helper class that creates and moves plugin containers.
  webkit::npapi::GtkPluginContainerManager plugin_container_manager_;
#endif

  // The delegate pointer will always outlive the WebViewHost object.
  BrowserWebViewDelegate* delegate_;
};

#endif  // CEF_LIBCEF_WEBVIEW_HOST_H_
