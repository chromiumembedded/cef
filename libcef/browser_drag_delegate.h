// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that implements BaseDragSource for the test shell webview delegate.

#ifndef _BROWSER_DRAG_DELEGATE_H
#define _BROWSER_DRAG_DELEGATE_H

#include "base/base_drag_source.h"

namespace WebKit {
class WebView;
}

class BrowserDragDelegate : public BaseDragSource {
 public:
  BrowserDragDelegate(HWND source_hwnd, WebKit::WebView* webview)
      : BaseDragSource(),
        source_hwnd_(source_hwnd),
        webview_(webview) { }

 protected:
  // BaseDragSource
  virtual void OnDragSourceCancel();
  virtual void OnDragSourceDrop();
  virtual void OnDragSourceMove();

 private:
  WebKit::WebView* webview_;

  // A HWND for the source we are associated with, used for translating
  // mouse coordinates from screen to client coordinates.
  HWND source_hwnd_;
};

#endif  // _BROWSER_DRAG_DELEGATE_H

