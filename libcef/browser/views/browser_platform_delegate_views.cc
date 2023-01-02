// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/views/browser_platform_delegate_views.h"

#include <utility>

#include "include/views/cef_window.h"
#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/views/browser_view_impl.h"
#include "libcef/browser/views/menu_runner_views.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/views/widget/widget.h"

namespace {

// Default popup window delegate implementation.
class PopupWindowDelegate : public CefWindowDelegate {
 public:
  explicit PopupWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
      : browser_view_(browser_view) {}

  PopupWindowDelegate(const PopupWindowDelegate&) = delete;
  PopupWindowDelegate& operator=(const PopupWindowDelegate&) = delete;

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->AddChildView(browser_view_);
    window->Show();
    browser_view_->RequestFocus();
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    browser_view_ = nullptr;
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      return browser->GetHost()->TryCloseBrowser();
    }
    return true;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;

  IMPLEMENT_REFCOUNTING(PopupWindowDelegate);
};

}  // namespace

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
    CefRefPtr<CefBrowserViewImpl> browser_view) {
  DCHECK(!browser_view_);
  DCHECK(browser_view);
  browser_view_ = browser_view;
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
    browser_view_->delegate()->OnBrowserCreated(browser_view_, browser_);
  }
}

void CefBrowserPlatformDelegateViews::NotifyBrowserDestroyed() {
  DCHECK(browser_view_);
  DCHECK(browser_);
  if (browser_view_->delegate()) {
    browser_view_->delegate()->OnBrowserDestroyed(browser_view_, browser_);
  }
}

void CefBrowserPlatformDelegateViews::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateAlloy::BrowserDestroyed(browser);

  browser_view_->BrowserDestroyed(browser);
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

void CefBrowserPlatformDelegateViews::PopupWebContentsCreated(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* new_web_contents,
    CefBrowserPlatformDelegate* new_platform_delegate,
    bool is_devtools) {
  DCHECK(new_platform_delegate->IsViewsHosted());
  CefBrowserPlatformDelegateViews* new_platform_delegate_impl =
      static_cast<CefBrowserPlatformDelegateViews*>(new_platform_delegate);

  CefRefPtr<CefBrowserViewDelegate> new_delegate;
  if (browser_view_->delegate()) {
    new_delegate = browser_view_->delegate()->GetDelegateForPopupBrowserView(
        browser_view_.get(), settings, client, is_devtools);
  }

  // Create a new BrowserView for the popup.
  CefRefPtr<CefBrowserViewImpl> new_browser_view =
      CefBrowserViewImpl::CreateForPopup(settings, new_delegate);

  // Associate the PlatformDelegate with the new BrowserView.
  new_platform_delegate_impl->SetBrowserView(new_browser_view);
}

void CefBrowserPlatformDelegateViews::PopupBrowserCreated(
    CefBrowserHostBase* new_browser,
    bool is_devtools) {
  CefRefPtr<CefBrowserView> new_browser_view =
      CefBrowserView::GetForBrowser(new_browser);
  DCHECK(new_browser_view);

  bool popup_handled = false;
  if (browser_view_->delegate()) {
    popup_handled = browser_view_->delegate()->OnPopupBrowserViewCreated(
        browser_view_.get(), new_browser_view.get(), is_devtools);
  }

  if (!popup_handled) {
    CefWindow::CreateTopLevelWindow(
        new PopupWindowDelegate(new_browser_view.get()));
  }
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
  // Will activate the Widget and result in a call to WebContents::Focus().
  if (setFocus && browser_view_->root_view()) {
    if (auto widget = GetWindowWidget()) {
      // Don't activate a minimized Widget, or it will be shown.
      if (widget->IsMinimized()) {
        return;
      }
    }
    browser_view_->root_view()->RequestFocus();
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
    const content::NativeWebKeyboardEvent& event) {
  // The BrowserView will handle accelerators.
  return browser_view_->HandleKeyboardEvent(event);
}

CefEventHandle CefBrowserPlatformDelegateViews::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
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
