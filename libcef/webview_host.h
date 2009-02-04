// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _WEBVIEW_HOST_H
#define _WEBVIEW_HOST_H

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "webwidget_host.h"

struct WebPreferences;
class WebView;
class WebViewDelegate;

// This class is a simple ViewHandle-based host for a WebView
class WebViewHost : public WebWidgetHost {
 public:
  // The new instance is deleted once the associated ViewHandle is destroyed.
  // The newly created window should be resized after it is created, using the
  // MoveWindow (or equivalent) function.
  static WebViewHost* Create(gfx::NativeView parent_window,
                             WebViewDelegate* delegate,
                             const WebPreferences& prefs);

  WebView* webview() const;

 protected:
#if defined(OS_WIN)
  virtual bool WndProc(UINT message, WPARAM wparam, LPARAM lparam) {
    return false;
  }
#endif
};

#endif  // _WEBVIEW_HOST_H
