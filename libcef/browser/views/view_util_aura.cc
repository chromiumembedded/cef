// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/view_util.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/widget.h"

namespace view_util {

gfx::NativeWindow GetNativeWindow(views::Widget* widget) {
  if (widget) {
    return widget->GetNativeWindow();
  }
  return nullptr;
}

gfx::NativeView GetNativeView(views::Widget* widget) {
  if (widget) {
    return widget->GetNativeView();
  }
  return nullptr;
}

CefWindowHandle GetWindowHandle(views::Widget* widget) {
  // Same implementation as views::HWNDForView() but cross-platform.
  if (widget) {
    return GetWindowHandle(widget->GetNativeWindow());
  }
  return kNullWindowHandle;
}

CefWindowHandle GetWindowHandle(gfx::NativeWindow window) {
  // |window| is an aura::Window*.
  if (window && window->GetRootWindow()) {
    return window->GetHost()->GetAcceleratedWidget();
  }
  return kNullWindowHandle;
}

views::NativeWidget* CreateNativeWidget(
    views::internal::NativeWidgetDelegate* delegate,
    CefRefPtr<CefWindow> window,
    CefWindowDelegate* window_delegate) {
  return nullptr;
}

void SetHostView(views::Widget* widget, views::View* host_view) {
  widget->GetNativeView()->SetProperty(views::kHostViewKey, host_view);
}

views::View* GetHostView(views::Widget* widget) {
  return widget->GetNativeView()->GetProperty(views::kHostViewKey);
}

}  // namespace view_util
