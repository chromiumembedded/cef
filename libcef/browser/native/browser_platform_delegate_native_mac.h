// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_MAC_H_
#define CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_MAC_H_

#include "libcef/browser/native/browser_platform_delegate_native.h"

#if defined(__OBJC__)
@class CefWindowDelegate;
#else
class CefWindowDelegate;
#endif

namespace content {
class RenderWidgetHostViewMac;
}

// Windowed browser implementation for Mac OS X.
class CefBrowserPlatformDelegateNativeMac
    : public CefBrowserPlatformDelegateNative {
 public:
  CefBrowserPlatformDelegateNativeMac(const CefWindowInfo& window_info,
                                      SkColor background_color);

  // CefBrowserPlatformDelegate methods:
  void BrowserDestroyed(CefBrowserHostBase* browser) override;
  bool CreateHostWindow() override;
  void CloseHostWindow() override;
  CefWindowHandle GetHostWindowHandle() const override;
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
  std::unique_ptr<CefJavaScriptDialogRunner> CreateJavaScriptDialogRunner()
      override;
  std::unique_ptr<CefMenuRunner> CreateMenuRunner() override;

  // CefBrowserPlatformDelegateNative methods:
  content::NativeWebKeyboardEvent TranslateWebKeyEvent(
      const CefKeyEvent& key_event) const override;
  blink::WebMouseEvent TranslateWebClickEvent(
      const CefMouseEvent& mouse_event,
      CefBrowserHost::MouseButtonType type,
      bool mouseUp,
      int clickCount) const override;
  blink::WebMouseEvent TranslateWebMoveEvent(const CefMouseEvent& mouse_event,
                                             bool mouseLeave) const override;
  blink::WebMouseWheelEvent TranslateWebWheelEvent(
      const CefMouseEvent& mouse_event,
      int deltaX,
      int deltaY) const override;

 private:
  void TranslateWebMouseEvent(blink::WebMouseEvent& result,
                              const CefMouseEvent& mouse_event) const;

  content::RenderWidgetHostViewMac* GetHostView() const;

  // True if the host window has been created.
  bool host_window_created_ = false;

  CefWindowDelegate* __strong window_delegate_;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_MAC_H_
