// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BROWSER_PLATFORM_DELEGATE_VIEWS_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BROWSER_PLATFORM_DELEGATE_VIEWS_H_

#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/native/browser_platform_delegate_native.h"
#include "libcef/browser/views/browser_view_impl.h"

// Implementation of Views-based browser functionality.
class CefBrowserPlatformDelegateViews :
    public CefBrowserPlatformDelegate,
    public CefBrowserPlatformDelegateNative::WindowlessHandler {
 public:
  // Platform-specific behaviors will be delegated to |native_delegate|.
  // |browser_view_getter| may be initially empty for popup browsers.
  CefBrowserPlatformDelegateViews(
      std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
      CefRefPtr<CefBrowserViewImpl> browser_view);

  void set_browser_view(CefRefPtr<CefBrowserViewImpl> browser_view);

  // CefBrowserPlatformDelegate methods:
  void WebContentsCreated(content::WebContents* web_contents) override;
  void BrowserCreated(CefBrowserHostImpl* browser) override;
  void NotifyBrowserCreated() override;
  void NotifyBrowserDestroyed() override;
  void BrowserDestroyed(CefBrowserHostImpl* browser) override;
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
  void PopupBrowserCreated(
      CefBrowserHostImpl* new_browser,
      bool is_devtools) override;
  SkColor GetBackgroundColor() const override;
  void WasResized() override;
  void SendKeyEvent(const content::NativeWebKeyboardEvent& event) override;
  void SendMouseEvent(const blink::WebMouseEvent& event) override;
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event) override;
  void SendFocusEvent(bool setFocus) override;
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
  std::unique_ptr<CefFileDialogRunner> CreateFileDialogRunner() override;
  std::unique_ptr<CefJavaScriptDialogRunner> CreateJavaScriptDialogRunner() override;
  std::unique_ptr<CefMenuRunner> CreateMenuRunner() override;
  bool IsWindowless() const override;
  bool IsViewsHosted() const override;

  // CefBrowserPlatformDelegateNative::WindowlessHandler methods:
  CefWindowHandle GetParentWindowHandle() const override;
  gfx::Point GetParentScreenPoint(const gfx::Point& view) const override;

 private:
  std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate_;
  CefRefPtr<CefBrowserViewImpl> browser_view_;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BROWSER_PLATFORM_DELEGATE_VIEWS_H_
