// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_BROWSER_PLATFORM_DELEGATE_CHROME_H_
#define CEF_LIBCEF_BROWSER_CHROME_BROWSER_PLATFORM_DELEGATE_CHROME_H_

#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/native/browser_platform_delegate_native.h"

class Browser;

// Implementation of Chrome-based browser functionality.
class CefBrowserPlatformDelegateChrome
    : public CefBrowserPlatformDelegate,
      public CefBrowserPlatformDelegateNative::WindowlessHandler {
 public:
  explicit CefBrowserPlatformDelegateChrome(
      std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate);

  // CefBrowserPlatformDelegate overrides.
  void WebContentsCreated(content::WebContents* web_contents,
                          bool owned) override;
  void WebContentsDestroyed(content::WebContents* web_contents) override;
  void BrowserCreated(CefBrowserHostBase* browser) override;
  void BrowserDestroyed(CefBrowserHostBase* browser) override;
  CefWindowHandle GetHostWindowHandle() const override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      const override;
  SkColor GetBackgroundColor() const override;
  void SendKeyEvent(const CefKeyEvent& event) override;
  void SendMouseClickEvent(const CefMouseEvent& event,
                           CefBrowserHost::MouseButtonType type,
                           bool mouseUp,
                           int clickCount) override;
  void SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave) override;
  void SendMouseWheelEvent(const CefMouseEvent& event,
                           int deltaX,
                           int deltaY) override;
  gfx::Point GetScreenPoint(const gfx::Point& view,
                            bool want_dip_coords) const override;
  void ViewText(const std::string& text) override;
  CefEventHandle GetEventHandle(
      const content::NativeWebKeyboardEvent& event) const override;
  bool IsPrintPreviewSupported() const override;

  // CefBrowserPlatformDelegateNative::WindowlessHandler methods:
  CefWindowHandle GetParentWindowHandle() const override;
  gfx::Point GetParentScreenPoint(const gfx::Point& view,
                                  bool want_dip_coords) const override;

  void set_chrome_browser(Browser* browser);

  CefBrowserPlatformDelegateNative* native_delegate() const {
    return native_delegate_.get();
  }

 protected:
  gfx::NativeWindow GetNativeWindow() const;

  std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate_;

  Browser* chrome_browser_ = nullptr;
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_BROWSER_PLATFORM_DELEGATE_CHROME_H_
