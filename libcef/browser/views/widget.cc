// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/widget.h"

#include "libcef/browser/chrome/views/chrome_browser_frame.h"
#include "libcef/browser/views/widget_impl.h"

// static
CefWidget* CefWidget::Create(CefWindowView* window_view) {
  if (cef::IsChromeRuntimeEnabled()) {
    return new ChromeBrowserFrame(window_view);
  }
  return new CefWidgetImpl(window_view);
}

// static
CefWidget* CefWidget::GetForWidget(views::Widget* widget) {
  if (cef::IsChromeRuntimeEnabled()) {
    return static_cast<ChromeBrowserFrame*>(widget);
  }
  return static_cast<CefWidgetImpl*>(widget);
}
