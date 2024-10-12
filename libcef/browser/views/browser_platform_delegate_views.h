// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BROWSER_PLATFORM_DELEGATE_VIEWS_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BROWSER_PLATFORM_DELEGATE_VIEWS_H_

#include "base/memory/weak_ptr.h"
#include "cef/libcef/browser/alloy/browser_platform_delegate_alloy.h"
#include "cef/libcef/browser/native/browser_platform_delegate_native.h"
#include "cef/libcef/browser/views/browser_view_impl.h"

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
  void SetBrowserView(CefRefPtr<CefBrowserView> browser_view) override;
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
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  CefEventHandle GetEventHandle(
      const input::NativeWebKeyboardEvent& event) const override;
  std::unique_ptr<CefMenuRunner> CreateMenuRunner() override;
  bool IsViewsHosted() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;

  // CefBrowserPlatformDelegateNative::WindowlessHandler methods:
  CefWindowHandle GetParentWindowHandle() const override;
  gfx::Point GetParentScreenPoint(const gfx::Point& view,
                                  bool want_dip_coords) const override;

 private:
  std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate_;

  // Holding a weak reference here because we want the CefBrowserViewImpl to be
  // destroyed first if all references are released by the client.
  // CefBrowserViewImpl destruction will then trigger destruction of any
  // associated CefBrowserHostBase (which owns this CefBrowserPlatformDelegate
  // object).
  base::WeakPtr<CefBrowserViewImpl> browser_view_;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BROWSER_PLATFORM_DELEGATE_VIEWS_H_
