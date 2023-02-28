// Copyright 2023 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_NS_WINDOW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_NS_WINDOW_H_
#pragma once

#include "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"

@interface CefNSWindow : NativeWidgetMacNSWindow {
 @private
  bool is_frameless_;
}
- (id)initWithStyle:(NSUInteger)style_mask isFrameless:(bool)is_frameless;

- (BOOL)shouldCenterTrafficLights;
@end

#endif  // CEF_LIBCEF_BROWSER_VIEWS_NS_WINDOW_H_
