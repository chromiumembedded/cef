// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_LINUX_H_
#define CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_LINUX_H_

#include "libcef/browser/native/browser_platform_delegate_native.h"

class CefWindowX11;

// Windowed browser implementation for Linux.
class CefBrowserPlatformDelegateNativeLinux :
    public CefBrowserPlatformDelegateNative {
 public:
  CefBrowserPlatformDelegateNativeLinux(const CefWindowInfo& window_info,
                                        SkColor background_color);

  // CefBrowserPlatformDelegate methods:
  void BrowserDestroyed(CefBrowserHostImpl* browser) override;
  bool CreateHostWindow() override;
  void CloseHostWindow() override;
  CefWindowHandle GetHostWindowHandle() const override;
  views::Widget* GetWindowWidget() const override;
  void SendFocusEvent(bool setFocus) override;
  void NotifyMoveOrResizeStarted() override;
  void SizeTo(int width, int height) override;
  gfx::Point GetScreenPoint(const gfx::Point& view) const override;
  void ViewText(const std::string& text) override;
  void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  void HandleExternalProtocol(const GURL& url) override;
  void TranslateKeyEvent(content::NativeWebKeyboardEvent& result,
                         const CefKeyEvent& key_event) const override;
  void TranslateClickEvent(blink::WebMouseEvent& result,
                           const CefMouseEvent& mouse_event,
                           CefBrowserHost::MouseButtonType type,
                           bool mouseUp, int clickCount) const override;
  void TranslateMoveEvent(blink::WebMouseEvent& result,
                          const CefMouseEvent& mouse_event,
                          bool mouseLeave) const override;
  void TranslateWheelEvent(blink::WebMouseWheelEvent& result,
                           const CefMouseEvent& mouse_event,
                           int deltaX, int deltaY) const override;
  CefEventHandle GetEventHandle(
      const content::NativeWebKeyboardEvent& event) const override;
  std::unique_ptr<CefMenuRunner> CreateMenuRunner() override;

 private:
  void TranslateMouseEvent(blink::WebMouseEvent& result,
                           const CefMouseEvent& mouse_event) const;

  // True if the host window has been created.
  bool host_window_created_;

  // Widget hosting the web contents. It will be deleted automatically when the
  // associated root window is destroyed.
  views::Widget* window_widget_;

  CefWindowX11* window_x11_;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_LINUX_H_
