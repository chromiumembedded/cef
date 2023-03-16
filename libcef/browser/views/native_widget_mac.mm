// Copyright 2023 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/native_widget_mac.h"

#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "libcef/browser/views/ns_window.h"

CefNativeWidgetMac::CefNativeWidgetMac(
    views::internal::NativeWidgetDelegate* delegate,
    CefRefPtr<CefWindow> window,
    CefWindowDelegate* window_delegate)
    : views::NativeWidgetMac(delegate),
      window_(window),
      window_delegate_(window_delegate) {}

NativeWidgetMacNSWindow* CefNativeWidgetMac::CreateNSWindow(
    const remote_cocoa::mojom::CreateWindowParams* params) {
  NSUInteger style_mask =
      NSWindowStyleMaskTitled | NSWindowStyleMaskMiniaturizable |
      NSWindowStyleMaskClosable | NSWindowStyleMaskResizable |
      NSWindowStyleMaskTexturedBackground;

  bool is_frameless = window_delegate_->IsFrameless(window_);

  auto window = [[CefNSWindow alloc] initWithStyle:style_mask
                                       isFrameless:is_frameless];

  if (is_frameless) {
    [window setTitlebarAppearsTransparent:YES];
    [window setTitleVisibility:NSWindowTitleHidden];
  }

  if (!window_delegate_->WithStandardWindowButtons(window_)) {
    [[window standardWindowButton:NSWindowCloseButton] setHidden:YES];
    [[window standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
    [[window standardWindowButton:NSWindowZoomButton] setHidden:YES];
  }

  return window;
}

void CefNativeWidgetMac::GetWindowFrameTitlebarHeight(
    bool* override_titlebar_height,
    float* titlebar_height) {
  if (window_delegate_->GetTitlebarHeight(window_, titlebar_height)) {
    *override_titlebar_height = true;
  } else {
    views::NativeWidgetMac::GetWindowFrameTitlebarHeight(
        override_titlebar_height, titlebar_height);
  }
}
