// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/view_util.h"

#import <Cocoa/Cocoa.h>

#include "include/internal/cef_types_mac.h"
#include "libcef/browser/views/native_widget_mac.h"

#include "ui/views/widget/widget.h"

namespace view_util {

namespace {

constexpr char kNativeHostViewKey[] = "CefNativeHostViewKey";

// For Venura 13.3.1.
constexpr float kDefaultTitleBarHeight = 30;

}  // namespace

gfx::NativeWindow GetNativeWindow(views::Widget* widget) {
  if (widget) {
    return widget->GetNativeWindow();
  }
  return gfx::NativeWindow();
}

gfx::NativeView GetNativeView(views::Widget* widget) {
  if (widget) {
    return widget->GetNativeView();
  }
  return gfx::NativeView();
}

CefWindowHandle GetWindowHandle(views::Widget* widget) {
  // |view| is a wrapper type from native_widget_types.h.
  auto view = GetNativeView(widget);
  if (view) {
    return CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(view.GetNativeNSView());
  }
  return kNullWindowHandle;
}

CefWindowHandle GetWindowHandle(gfx::NativeWindow window) {
  // |window| is a wrapper type from native_widget_types.h.
  if (window) {
    NSWindow* nswindow = window.GetNativeNSWindow();
    return CAST_NSVIEW_TO_CEF_WINDOW_HANDLE([nswindow contentView]);
  }
  return kNullWindowHandle;
}

views::NativeWidget* CreateNativeWidget(
    views::internal::NativeWidgetDelegate* delegate,
    CefRefPtr<CefWindow> window,
    CefWindowDelegate* window_delegate) {
  return new CefNativeWidgetMac(delegate, window, window_delegate);
}

void SetHostView(views::Widget* widget, views::View* host_view) {
  widget->SetNativeWindowProperty(kNativeHostViewKey, host_view);
}

views::View* GetHostView(views::Widget* widget) {
  return static_cast<views::View*>(
      widget->GetNativeWindowProperty(kNativeHostViewKey));
}

float GetNSWindowTitleBarHeight(views::Widget* widget) {
  if (auto window = GetNativeWindow(widget)) {
    NSWindow* nswindow = window.GetNativeNSWindow();
    return nswindow.frame.size.height -
           [nswindow contentRectForFrameRect:nswindow.frame].size.height;
  }
  return kDefaultTitleBarHeight;
}

}  // namespace view_util
