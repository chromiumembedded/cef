// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/browser_platform_delegate_chrome.h"

#include "libcef/browser/views/view_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/common/pref_names.h"
#include "ui/display/display.h"
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

CefWindowHandle CefBrowserPlatformDelegateChrome::GetHostWindowHandle() const {
  return view_util::GetWindowHandle(GetNativeWindow());
}

web_modal::WebContentsModalDialogHost*
CefBrowserPlatformDelegateChrome::GetWebContentsModalDialogHost() const {
  if (chrome_browser_) {
    ChromeWebModalDialogManagerDelegate* manager = chrome_browser_;
    return manager->GetWebContentsModalDialogHost();
  }
  DCHECK(false);
  return nullptr;
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
    const gfx::Point& view,
    bool want_dip_coords) const {
  auto screen = display::Screen::GetScreen();

  // Get device (pixel) coordinates.
  auto screen_rect = screen->DIPToScreenRectInWindow(
      GetNativeWindow(), gfx::Rect(view, gfx::Size(0, 0)));
  auto screen_point = screen_rect.origin();

  if (want_dip_coords) {
    // Convert to DIP coordinates.
    const auto& display = view_util::GetDisplayNearestPoint(
        screen_point, /*input_pixel_coords=*/true);
    view_util::ConvertPointFromPixels(&screen_point,
                                      display.device_scale_factor());
  }

  return screen_point;
}

void CefBrowserPlatformDelegateChrome::ViewText(const std::string& text) {
  native_delegate_->ViewText(text);
}

CefEventHandle CefBrowserPlatformDelegateChrome::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  return native_delegate_->GetEventHandle(event);
}

bool CefBrowserPlatformDelegateChrome::IsPrintPreviewSupported() const {
  return chrome_browser_ && !chrome_browser_->profile()->GetPrefs()->GetBoolean(
                                prefs::kPrintPreviewDisabled);
}

CefWindowHandle CefBrowserPlatformDelegateChrome::GetParentWindowHandle()
    const {
  return GetHostWindowHandle();
}

gfx::Point CefBrowserPlatformDelegateChrome::GetParentScreenPoint(
    const gfx::Point& view,
    bool want_dip_coords) const {
  return GetScreenPoint(view, want_dip_coords);
}

void CefBrowserPlatformDelegateChrome::set_chrome_browser(Browser* browser) {
  chrome_browser_ = browser;
}

gfx::NativeWindow CefBrowserPlatformDelegateChrome::GetNativeWindow() const {
  if (chrome_browser_ && chrome_browser_->window()) {
    return chrome_browser_->window()->GetNativeWindow();
  }
  DCHECK(false);
  return gfx::NativeWindow();
}
