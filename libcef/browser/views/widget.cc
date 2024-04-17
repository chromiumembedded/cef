// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/widget.h"

#include "libcef/browser/chrome/views/chrome_browser_frame.h"
#include "libcef/browser/views/view_util.h"
#include "libcef/browser/views/widget_impl.h"
#include "libcef/browser/views/window_impl.h"

// static
CefWidget* CefWidget::Create(CefWindowView* window_view) {
  if (window_view->IsChromeStyle()) {
    return new ChromeBrowserFrame(window_view);
  }
  return new CefWidgetImpl(window_view);
}

// static
CefWidget* CefWidget::GetForWidget(views::Widget* widget) {
  if (auto window = view_util::GetWindowFor(widget)) {
    if (auto* window_view =
            static_cast<CefWindowImpl*>(window.get())->cef_window_view()) {
      if (window_view->IsChromeStyle()) {
        return static_cast<ChromeBrowserFrame*>(widget);
      }
      return static_cast<CefWidgetImpl*>(widget);
    }
  }
  return nullptr;
}
