// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BROWSER_PLATFORM_DELEGATE_VIEWS_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BROWSER_PLATFORM_DELEGATE_VIEWS_H_

#include "libcef/browser/alloy/browser_platform_delegate_alloy.h"
#include "libcef/browser/native/browser_platform_delegate_native.h"
#include "libcef/browser/views/browser_view_impl.h"

// Implementation of Views-based browser functionality.
class CefBrowserPlatformDelegateViews
    : public CefBrowserPlatformDelegateAlloy,
      public CefBrowserPlatformDelegateNative::WindowlessHandler {
 public:
  // Platform-specific behaviors will be delegated to |native_delegate|.
  // |browser_view_getter| may be initially empty for popup browsers.
  CefBrowserPlatformDelegateViews(
      std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
      CefRefPtr<CefBrowserViewImpl> browser_view);

  // CefBrowserPlatformDelegate methods:
  void WebContentsCreated(content::WebContents* web_contents,
                          bool owned) override;
  void WebContentsDestroyed(content::WebContents* web_contents) override;
  void BrowserCreated(CefBrowserHostBase* browser) override;
  void NotifyBrowserCreated() override;
  void NotifyBrowserDestroyed() override;
  void BrowserDestroyed(CefBrowserHostBase* browser) override;
  bool CreateHostWindow() override;
  void CloseHostWindow() override;
  CefWindowHandle GetHostWindowHandle() const override;
  views::Widget* GetWindowWidget() const override;
  CefRefPtr<CefBrowserView> GetBrowserView() const override;
  void PopupWebContentsCreated(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      content::WebContents* new_web_contents,
      CefBrowserPlatformDelegate* new_platform_delegate,
      bool is_devtools) override;
  void PopupBrowserCreated(CefBrowserHostBase* new_browser,
                           bool is_devtools) override;
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
  bool IsViewsHosted() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;

  // CefBrowserPlatformDelegateNative::WindowlessHandler methods:
  CefWindowHandle GetParentWindowHandle() const override;
  gfx::Point GetParentScreenPoint(const gfx::Point& view,
                                  bool want_dip_coords) const override;

 private:
  void SetBrowserView(CefRefPtr<CefBrowserViewImpl> browser_view);

  std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate_;
  CefRefPtr<CefBrowserViewImpl> browser_view_;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BROWSER_PLATFORM_DELEGATE_VIEWS_H_
