// Copyright 2023 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_NATIVE_WIDGET_MAC_H_
#define CEF_LIBCEF_BROWSER_VIEWS_NATIVE_WIDGET_MAC_H_
#pragma once

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/widget/native_widget_mac.h"

class CefNativeWidgetMac : public views::NativeWidgetMac {
 public:
  CefNativeWidgetMac(views::internal::NativeWidgetDelegate* delegate,
                     bool is_frameless,
                     bool with_window_buttons,
                     absl::optional<float> title_bar_height);
  ~CefNativeWidgetMac() override = default;

  CefNativeWidgetMac(const CefNativeWidgetMac&) = delete;
  CefNativeWidgetMac& operator=(const CefNativeWidgetMac&) = delete;

 protected:
  // NativeWidgetMac:
  NativeWidgetMacNSWindow* CreateNSWindow(
      const remote_cocoa::mojom::CreateWindowParams* params) override;

  void GetWindowFrameTitlebarHeight(bool* override_titlebar_height,
                                    float* titlebar_height) override;

 private:
  const bool is_frameless_;
  const bool with_window_buttons_;
  const absl::optional<float> title_bar_height_;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_NATIVE_WIDGET_MAC_H_
