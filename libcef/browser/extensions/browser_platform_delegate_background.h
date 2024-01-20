// Copyright 2017 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BROWSER_PLATFORM_DELEGATE_BACKGROUND_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BROWSER_PLATFORM_DELEGATE_BACKGROUND_H_

#include "libcef/browser/alloy/browser_platform_delegate_alloy.h"
#include "libcef/browser/native/browser_platform_delegate_native.h"

// Implementation of browser functionality for background script hosts.
class CefBrowserPlatformDelegateBackground
    : public CefBrowserPlatformDelegateAlloy,
      public CefBrowserPlatformDelegateNative::WindowlessHandler {
 public:
  // Platform-specific behaviors will be delegated to |native_delegate|.
  explicit CefBrowserPlatformDelegateBackground(
      std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate);

  // CefBrowserPlatformDelegate methods:
  bool CreateHostWindow() override;
  void CloseHostWindow() override;
  CefWindowHandle GetHostWindowHandle() const override;
  SkColor GetBackgroundColor() const override;
  void WasResized() override;
  void SendKeyEvent(const CefKeyEvent& event) override;
  void SendMouseClickEvent(const CefMouseEvent& event,
                           CefBrowserHost::MouseButtonType type,
                           bool mouseUp,
                           int clickCount) override;
  void SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave) override;
  void SendMouseWheelEvent(const CefMouseEvent& event,
                           int deltaX,
                           int deltaY) override;
  void SendTouchEvent(const CefTouchEvent& event) override;
  void SetFocus(bool setFocus) override;
  gfx::Point GetScreenPoint(const gfx::Point& view,
                            bool want_dip_coords) const override;
  void ViewText(const std::string& text) override;
  bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  CefEventHandle GetEventHandle(
      const content::NativeWebKeyboardEvent& event) const override;
  std::unique_ptr<CefMenuRunner> CreateMenuRunner() override;

  // CefBrowserPlatformDelegateNative::WindowlessHandler methods:
  CefWindowHandle GetParentWindowHandle() const override;
  gfx::Point GetParentScreenPoint(const gfx::Point& view,
                                  bool want_dip_coords) const override;

 private:
  std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate_;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BROWSER_PLATFORM_DELEGATE_BACKGROUND_H_
