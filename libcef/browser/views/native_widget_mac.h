// Copyright 2023 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_NATIVE_WIDGET_MAC_H_
#define CEF_LIBCEF_BROWSER_VIEWS_NATIVE_WIDGET_MAC_H_
#pragma once

#include "include/internal/cef_ptr.h"

#include "ui/views/widget/native_widget_mac.h"

class BrowserView;
class CefWindow;
class CefWindowDelegate;

class CefNativeWidgetMac : public views::NativeWidgetMac {
 public:
  CefNativeWidgetMac(views::internal::NativeWidgetDelegate* delegate,
                     CefRefPtr<CefWindow> window,
                     CefWindowDelegate* window_delegate);
  ~CefNativeWidgetMac() override = default;

  CefNativeWidgetMac(const CefNativeWidgetMac&) = delete;
  CefNativeWidgetMac& operator=(const CefNativeWidgetMac&) = delete;

  void SetBrowserView(BrowserView* browser_view);

  // NativeWidgetMac:
  void ValidateUserInterfaceItem(
      int32_t command,
      remote_cocoa::mojom::ValidateUserInterfaceItemResult* result) override;
  bool WillExecuteCommand(int32_t command,
                          WindowOpenDisposition window_open_disposition,
                          bool is_before_first_responder) override;
  bool ExecuteCommand(int32_t command,
                      WindowOpenDisposition window_open_disposition,
                      bool is_before_first_responder) override;
  NativeWidgetMacNSWindow* CreateNSWindow(
      const remote_cocoa::mojom::CreateWindowParams* params) override;
  void GetWindowFrameTitlebarHeight(bool* override_titlebar_height,
                                    float* titlebar_height) override;
  void OnWindowFullscreenTransitionStart() override;
  void OnWindowFullscreenTransitionComplete() override;
  void OnWindowInitialized() override;

 private:
  const CefRefPtr<CefWindow> window_;
  CefWindowDelegate* const window_delegate_;

  // Returns true if the CefWindow is fully initialized.
  bool IsCefWindowInitialized() const;

  BrowserView* browser_view_ = nullptr;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_NATIVE_WIDGET_MAC_H_
