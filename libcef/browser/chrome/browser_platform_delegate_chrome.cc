// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/browser_platform_delegate_chrome.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"

CefBrowserPlatformDelegateChrome::CefBrowserPlatformDelegateChrome(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate)
    : native_delegate_(std::move(native_delegate)) {
  native_delegate_->set_windowless_handler(this);
}

void CefBrowserPlatformDelegateChrome::WebContentsCreated(
    content::WebContents* web_contents,
    bool owned) {
  CefBrowserPlatformDelegate::WebContentsCreated(web_contents, owned);
  native_delegate_->WebContentsCreated(web_contents, /*owned=*/false);
}

void CefBrowserPlatformDelegateChrome::WebContentsDestroyed(
    content::WebContents* web_contents) {
  CefBrowserPlatformDelegate::WebContentsDestroyed(web_contents);
  native_delegate_->WebContentsDestroyed(web_contents);
}

void CefBrowserPlatformDelegateChrome::BrowserCreated(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegate::BrowserCreated(browser);
  native_delegate_->BrowserCreated(browser);
}

void CefBrowserPlatformDelegateChrome::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegate::BrowserDestroyed(browser);
  native_delegate_->BrowserDestroyed(browser);
}

SkColor CefBrowserPlatformDelegateChrome::GetBackgroundColor() const {
  return native_delegate_->GetBackgroundColor();
}

void CefBrowserPlatformDelegateChrome::SendKeyEvent(const CefKeyEvent& event) {
  native_delegate_->SendKeyEvent(event);
}

void CefBrowserPlatformDelegateChrome::SendMouseClickEvent(
    const CefMouseEvent& event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp,
    int clickCount) {
  native_delegate_->SendMouseClickEvent(event, type, mouseUp, clickCount);
}

void CefBrowserPlatformDelegateChrome::SendMouseMoveEvent(
    const CefMouseEvent& event,
    bool mouseLeave) {
  native_delegate_->SendMouseMoveEvent(event, mouseLeave);
}

void CefBrowserPlatformDelegateChrome::SendMouseWheelEvent(
    const CefMouseEvent& event,
    int deltaX,
    int deltaY) {
  native_delegate_->SendMouseWheelEvent(event, deltaX, deltaY);
}

gfx::Point CefBrowserPlatformDelegateChrome::GetScreenPoint(
    const gfx::Point& view) const {
  auto screen = display::Screen::GetScreen();

  gfx::NativeWindow native_window =
      chrome_browser_ ? chrome_browser_->window()->GetNativeWindow() : nullptr;

  // Returns screen pixel coordinates.
  auto screen_rect = screen->DIPToScreenRectInWindow(
      native_window, gfx::Rect(view, gfx::Size(0, 0)));
  return screen_rect.origin();
}

void CefBrowserPlatformDelegateChrome::ViewText(const std::string& text) {
  native_delegate_->ViewText(text);
}

CefEventHandle CefBrowserPlatformDelegateChrome::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  return native_delegate_->GetEventHandle(event);
}

CefWindowHandle CefBrowserPlatformDelegateChrome::GetParentWindowHandle()
    const {
  return GetHostWindowHandle();
}

gfx::Point CefBrowserPlatformDelegateChrome::GetParentScreenPoint(
    const gfx::Point& view) const {
  return GetScreenPoint(view);
}

void CefBrowserPlatformDelegateChrome::set_chrome_browser(Browser* browser) {
  chrome_browser_ = browser;
}
