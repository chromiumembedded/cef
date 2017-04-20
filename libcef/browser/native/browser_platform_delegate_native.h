// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_H_
#define CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_H_

#include "libcef/browser/browser_platform_delegate.h"

// Base implementation of native browser functionality.
class CefBrowserPlatformDelegateNative : public CefBrowserPlatformDelegate {
 public:
  // Used by the windowless implementation to override specific functionality
  // when delegating to the native implementation.
  class WindowlessHandler {
   public:
    // Returns the parent window handle.
    virtual CefWindowHandle GetParentWindowHandle() const = 0;

    // Convert from view coordinates to screen coordinates.
    virtual gfx::Point GetParentScreenPoint(const gfx::Point& view) const = 0;

   protected:
    virtual ~WindowlessHandler() {}
  };

  // CefBrowserPlatformDelegate methods:
  SkColor GetBackgroundColor() const override;
  void WasResized() override;
  void SendKeyEvent(const content::NativeWebKeyboardEvent& event) override;
  void SendMouseEvent(const blink::WebMouseEvent& event) override;
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event) override;
  bool IsWindowless() const override;
  bool IsViewsHosted() const override;

  const CefWindowInfo& window_info() const { return window_info_; }

  void set_windowless_handler(WindowlessHandler* handler) {
    windowless_handler_ = handler;
  }

 protected:
  CefBrowserPlatformDelegateNative(const CefWindowInfo& window_info,
                                   SkColor background_color);

  CefWindowInfo window_info_;
  const SkColor background_color_;

  WindowlessHandler* windowless_handler_;  // Not owned by this object.
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_H_
