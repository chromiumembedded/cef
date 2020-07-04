// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_BROWSER_PLATFORM_DELEGATE_CHROME_H_
#define CEF_LIBCEF_BROWSER_CHROME_BROWSER_PLATFORM_DELEGATE_CHROME_H_

#include "libcef/browser/browser_platform_delegate.h"

class Browser;

// Implementation of Chrome-based browser functionality.
class CefBrowserPlatformDelegateChrome : public CefBrowserPlatformDelegate {
 public:
  explicit CefBrowserPlatformDelegateChrome(SkColor background_color);

  // CefBrowserPlatformDelegate overrides.
  content::WebContents* CreateWebContents(
      CefBrowserHostImpl::CreateParams& create_params,
      bool& own_web_contents) override;
  void WebContentsDestroyed(content::WebContents* web_contents) override;
  void BrowserDestroyed(CefBrowserHostImpl* browser) override;
  bool CreateHostWindow() override;
  void CloseHostWindow() override;
  CefWindowHandle GetHostWindowHandle() const override;
  SkColor GetBackgroundColor() const override;
  bool CanUseSharedTexture() const override;
  bool CanUseExternalBeginFrame() const override;
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
  void SendFocusEvent(bool setFocus) override;
  gfx::Point GetScreenPoint(const gfx::Point& view) const override;
  void ViewText(const std::string& text) override;
  bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  CefEventHandle GetEventHandle(
      const content::NativeWebKeyboardEvent& event) const override;
  std::unique_ptr<CefMenuRunner> CreateMenuRunner() override;
  bool IsWindowless() const override;
  bool IsViewsHosted() const override;

 private:
  const SkColor background_color_;

  Browser* chrome_browser_;
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_BROWSER_PLATFORM_DELEGATE_CHROME_H_
