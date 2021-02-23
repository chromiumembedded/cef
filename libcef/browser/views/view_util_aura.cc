// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/view_util.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/widget.h"

namespace view_util {

gfx::NativeWindow GetNativeWindow(views::Widget* widget) {
  if (widget) {
    aura::Window* window = widget->GetNativeWindow();
    if (window)
      return window->GetRootWindow();
  }
  return nullptr;
}

gfx::NativeView GetNativeView(views::Widget* widget) {
  return GetNativeWindow(widget);
}

CefWindowHandle GetWindowHandle(views::Widget* widget) {
  // Same implementation as views::HWNDForView() but cross-platform.
  if (widget) {
    aura::Window* window = widget->GetNativeWindow();
    if (window && window->GetRootWindow())
      return window->GetHost()->GetAcceleratedWidget();
  }
  return kNullWindowHandle;
}

}  // namespace view_util
