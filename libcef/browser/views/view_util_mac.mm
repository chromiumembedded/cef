// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/view_util.h"

#include "include/internal/cef_types_mac.h"

#include "ui/views/widget/widget.h"

namespace view_util {

gfx::NativeWindow GetNativeWindow(views::Widget* widget) {
  if (widget)
    return widget->GetNativeWindow();
  return gfx::NativeWindow();
}

gfx::NativeView GetNativeView(views::Widget* widget) {
  if (widget)
    return widget->GetNativeView();
  return gfx::NativeView();
}

CefWindowHandle GetWindowHandle(views::Widget* widget) {
  auto view = GetNativeView(widget);
  if (view)
    return CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(view.GetNativeNSView());
  return kNullWindowHandle;
}

}  // namespace view_util
