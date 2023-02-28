// Copyright 2023 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/native_widget_mac.h"

#include "libcef/browser/views/ns_window.h"

CefNativeWidgetMac::CefNativeWidgetMac(
    views::internal::NativeWidgetDelegate* delegate,
    bool is_frameless,
    bool with_window_buttons,
    absl::optional<float> title_bar_height)
    : views::NativeWidgetMac(delegate),
      is_frameless_(is_frameless),
      with_window_buttons_(with_window_buttons),
      title_bar_height_(title_bar_height) {}

NativeWidgetMacNSWindow* CefNativeWidgetMac::CreateNSWindow(
    const remote_cocoa::mojom::CreateWindowParams* params) {
  NSUInteger style_mask =
      NSWindowStyleMaskTitled | NSWindowStyleMaskMiniaturizable |
      NSWindowStyleMaskClosable | NSWindowStyleMaskResizable |
      NSWindowStyleMaskTexturedBackground;
  auto window = [[CefNSWindow alloc] initWithStyle:style_mask
                                       isFrameless:is_frameless_];

  if (is_frameless_) {
    [window setTitlebarAppearsTransparent:YES];
    [window setTitleVisibility:NSWindowTitleHidden];
  }

  if (!with_window_buttons_) {
    [[window standardWindowButton:NSWindowCloseButton] setHidden:YES];
    [[window standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
    [[window standardWindowButton:NSWindowZoomButton] setHidden:YES];
  }

  return window;
}

void CefNativeWidgetMac::GetWindowFrameTitlebarHeight(
    bool* override_titlebar_height,
    float* titlebar_height) {
  if (title_bar_height_) {
    *override_titlebar_height = true;
    *titlebar_height = title_bar_height_.value();
  } else {
    views::NativeWidgetMac::GetWindowFrameTitlebarHeight(
        override_titlebar_height, titlebar_height);
  }
}
