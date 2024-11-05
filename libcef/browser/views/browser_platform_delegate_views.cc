// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/views/browser_platform_delegate_views.h"

#include <utility>

#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "cef/libcef/browser/views/browser_view_impl.h"
#include "cef/libcef/browser/views/menu_runner_views.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/views/widget/widget.h"

CefBrowserPlatformDelegateViews::CefBrowserPlatformDelegateViews(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
    CefRefPtr<CefBrowserViewImpl> browser_view)
    : native_delegate_(std::move(native_delegate)) {
  if (browser_view) {
    SetBrowserView(browser_view);
  }
  native_delegate_->set_windowless_handler(this);
}

void CefBrowserPlatformDelegateViews::SetBrowserView(
    CefRefPtr<CefBrowserView> browser_view) {
  DCHECK(!browser_view_);
  DCHECK(browser_view);
  browser_view_ =
      static_cast<CefBrowserViewImpl*>(browser_view.get())->GetWeakPtr();
}

void CefBrowserPlatformDelegateViews::WebContentsCreated(
    content::WebContents* web_contents,
    bool owned) {
  CefBrowserPlatformDelegateAlloy::WebContentsCreated(web_contents, owned);
  native_delegate_->WebContentsCreated(web_contents, /*owned=*/false);
  browser_view_->WebContentsCreated(web_contents);
}

void CefBrowserPlatformDelegateViews::WebContentsDestroyed(
    content::WebContents* web_contents) {
  CefBrowserPlatformDelegateAlloy::WebContentsDestroyed(web_contents);
  // |browser_view_| may be destroyed before this callback arrives.
  if (browser_view_) {
    browser_view_->WebContentsDestroyed(web_contents);
  }
  native_delegate_->WebContentsDestroyed(web_contents);
}

void CefBrowserPlatformDelegateViews::BrowserCreated(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateAlloy::BrowserCreated(browser);

  native_delegate_->BrowserCreated(browser);
  browser_view_->BrowserCreated(browser, GetBoundsChangedCallback());
}

void CefBrowserPlatformDelegateViews::NotifyBrowserCreated() {
  DCHECK(browser_view_);
  DCHECK(browser_);
  if (browser_view_->delegate()) {
    browser_view_->delegate()->OnBrowserCreated(browser_view_.get(),
                                                browser_.get());
  }
}

void CefBrowserPlatformDelegateViews::NotifyBrowserDestroyed() {
  DCHECK(browser_);
  // |browser_view_| may be destroyed before this callback arrives.
  if (browser_view_ && browser_view_->delegate()) {
    browser_view_->delegate()->OnBrowserDestroyed(browser_view_.get(),
                                                  browser_.get());
  }
}

void CefBrowserPlatformDelegateViews::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateAlloy::BrowserDestroyed(browser);

  // |browser_view_| may be destroyed before this callback arrives.
  if (browser_view_) {
    browser_view_->BrowserDestroyed(browser);
  }
  browser_view_ = nullptr;
  native_delegate_->BrowserDestroyed(browser);
}

bool CefBrowserPlatformDelegateViews::CreateHostWindow() {
  // Nothing to do here.
  return true;
}

void CefBrowserPlatformDelegateViews::CloseHostWindow() {
  views::Widget* widget = GetWindowWidget();
  if (widget && !widget->IsClosed()) {
    widget->Close();
  }
}

CefWindowHandle CefBrowserPlatformDelegateViews::GetHostWindowHandle() const {
  return view_util::GetWindowHandle(GetWindowWidget());
}

views::Widget* CefBrowserPlatformDelegateViews::GetWindowWidget() const {
  if (browser_view_->root_view()) {
    return browser_view_->root_view()->GetWidget();
  }
  return nullptr;
}

CefRefPtr<CefBrowserView> CefBrowserPlatformDelegateViews::GetBrowserView()
    const {
  return browser_view_.get();
}

SkColor CefBrowserPlatformDelegateViews::GetBackgroundColor() const {
  return native_delegate_->GetBackgroundColor();
}

void CefBrowserPlatformDelegateViews::WasResized() {
  native_delegate_->WasResized();
}

void CefBrowserPlatformDelegateViews::SendKeyEvent(const CefKeyEvent& event) {
  native_delegate_->SendKeyEvent(event);
}

void CefBrowserPlatformDelegateViews::SendMouseClickEvent(
    const CefMouseEvent& event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp,
    int clickCount) {
  native_delegate_->SendMouseClickEvent(event, type, mouseUp, clickCount);
}

void CefBrowserPlatformDelegateViews::SendMouseMoveEvent(
    const CefMouseEvent& event,
    bool mouseLeave) {
  native_delegate_->SendMouseMoveEvent(event, mouseLeave);
}

void CefBrowserPlatformDelegateViews::SendMouseWheelEvent(
    const CefMouseEvent& event,
    int deltaX,
    int deltaY) {
  native_delegate_->SendMouseWheelEvent(event, deltaX, deltaY);
}

void CefBrowserPlatformDelegateViews::SendTouchEvent(
    const CefTouchEvent& event) {
  native_delegate_->SendTouchEvent(event);
}

void CefBrowserPlatformDelegateViews::SetFocus(bool setFocus) {
  if (setFocus && browser_view_) {
    browser_view_->RequestFocusSync();
  }
}

gfx::Point CefBrowserPlatformDelegateViews::GetScreenPoint(
    const gfx::Point& view_pt,
    bool want_dip_coords) const {
  if (!browser_view_->root_view()) {
    return view_pt;
  }

  gfx::Point screen_point = view_pt;
  view_util::ConvertPointToScreen(browser_view_->root_view(), &screen_point,
                                  /*output_pixel_coords=*/!want_dip_coords);
  return screen_point;
}

void CefBrowserPlatformDelegateViews::ViewText(const std::string& text) {
  native_delegate_->ViewText(text);
}

bool CefBrowserPlatformDelegateViews::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  // The BrowserView will handle accelerators.
  return browser_view_->HandleKeyboardEvent(event);
}

CefEventHandle CefBrowserPlatformDelegateViews::GetEventHandle(
    const input::NativeWebKeyboardEvent& event) const {
  return native_delegate_->GetEventHandle(event);
}

std::unique_ptr<CefMenuRunner>
CefBrowserPlatformDelegateViews::CreateMenuRunner() {
  return base::WrapUnique(new CefMenuRunnerViews(browser_view_.get()));
}

bool CefBrowserPlatformDelegateViews::IsViewsHosted() const {
  return true;
}

gfx::Point CefBrowserPlatformDelegateViews::GetDialogPosition(
    const gfx::Size& size) {
  const gfx::Rect& bounds = browser_view_->root_view()->GetBoundsInScreen();

  // Offset relative to the top-level content view.
  gfx::Point offset = bounds.origin();
  view_util::ConvertPointFromScreen(
      browser_view_->root_view()->GetWidget()->GetRootView(), &offset, false);

  return gfx::Point(offset.x() + (bounds.width() - size.width()) / 2,
                    offset.y() + (bounds.height() - size.height()) / 2);
}

gfx::Size CefBrowserPlatformDelegateViews::GetMaximumDialogSize() {
  return browser_view_->root_view()->GetBoundsInScreen().size();
}

CefWindowHandle CefBrowserPlatformDelegateViews::GetParentWindowHandle() const {
  return GetHostWindowHandle();
}

gfx::Point CefBrowserPlatformDelegateViews::GetParentScreenPoint(
    const gfx::Point& view,
    bool want_dip_coords) const {
  return GetScreenPoint(view, want_dip_coords);
}
