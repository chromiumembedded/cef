// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/views/browser_platform_delegate_views.h"

#include <utility>

#include "include/views/cef_window.h"
#include "libcef/browser/browser_host_impl.h"
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
      : browser_view_(browser_view) {
  }

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
    if (browser)
      return browser->GetHost()->TryCloseBrowser();
    return true;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;

  IMPLEMENT_REFCOUNTING(PopupWindowDelegate);
  DISALLOW_COPY_AND_ASSIGN(PopupWindowDelegate);
};

}  // namespace

CefBrowserPlatformDelegateViews::CefBrowserPlatformDelegateViews(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
    CefRefPtr<CefBrowserViewImpl> browser_view)
    : native_delegate_(std::move(native_delegate)),
      browser_view_(browser_view) {
  native_delegate_->set_windowless_handler(this);
}

void CefBrowserPlatformDelegateViews::set_browser_view(
    CefRefPtr<CefBrowserViewImpl> browser_view) {
  browser_view_ = browser_view;
}

void CefBrowserPlatformDelegateViews::WebContentsCreated(
    content::WebContents* web_contents) {
  browser_view_->WebContentsCreated(web_contents);
}

void CefBrowserPlatformDelegateViews::BrowserCreated(
    CefBrowserHostImpl* browser) {
  CefBrowserPlatformDelegate::BrowserCreated(browser);

  browser_view_->BrowserCreated(browser);
}

void CefBrowserPlatformDelegateViews::NotifyBrowserCreated() {
  DCHECK(browser_view_);
  DCHECK(browser_);
  if (browser_view_->delegate())
    browser_view_->delegate()->OnBrowserCreated(browser_view_, browser_);
}

void CefBrowserPlatformDelegateViews::NotifyBrowserDestroyed() {
  DCHECK(browser_view_);
  DCHECK(browser_);
  if (browser_view_->delegate())
    browser_view_->delegate()->OnBrowserDestroyed(browser_view_, browser_);
}

void CefBrowserPlatformDelegateViews::BrowserDestroyed(
    CefBrowserHostImpl* browser) {
  CefBrowserPlatformDelegate::BrowserDestroyed(browser);

  browser_view_->BrowserDestroyed(browser);
  browser_view_ = nullptr;
}

bool CefBrowserPlatformDelegateViews::CreateHostWindow() {
  // Nothing to do here.
  return true;
}

void CefBrowserPlatformDelegateViews::CloseHostWindow() {
  views::Widget* widget = GetWindowWidget();
  if (widget && !widget->IsClosed())
    widget->Close();
}

CefWindowHandle CefBrowserPlatformDelegateViews::GetHostWindowHandle() const {
  return view_util::GetWindowHandle(GetWindowWidget());
}

views::Widget* CefBrowserPlatformDelegateViews::GetWindowWidget() const {
  if (browser_view_->root_view())
    return browser_view_->root_view()->GetWidget();
  return nullptr;
}

CefRefPtr<CefBrowserView>
    CefBrowserPlatformDelegateViews::GetBrowserView() const {
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
  new_platform_delegate_impl->set_browser_view(new_browser_view);
}

void CefBrowserPlatformDelegateViews::PopupBrowserCreated(
    CefBrowserHostImpl* new_browser,
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
  content::RenderViewHost* host = browser_->web_contents()->GetRenderViewHost();
  if (host)
    host->GetWidget()->WasResized();
}

void CefBrowserPlatformDelegateViews::SendKeyEvent(
    const content::NativeWebKeyboardEvent& event) {
  content::RenderViewHost* host = browser_->web_contents()->GetRenderViewHost();
  if (host)
    host->GetWidget()->ForwardKeyboardEvent(event);
}

void CefBrowserPlatformDelegateViews::SendMouseEvent(
    const blink::WebMouseEvent& event) {
  content::RenderViewHost* host = browser_->web_contents()->GetRenderViewHost();
  if (host)
    host->GetWidget()->ForwardMouseEvent(event);
}

void CefBrowserPlatformDelegateViews::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  content::RenderViewHost* host = browser_->web_contents()->GetRenderViewHost();
  if (host)
    host->GetWidget()->ForwardWheelEvent(event);
}

void CefBrowserPlatformDelegateViews::SendFocusEvent(bool setFocus) {
  // Will result in a call to WebContents::Focus().
  if (setFocus && browser_view_->root_view())
    browser_view_->root_view()->RequestFocus();
}

gfx::Point CefBrowserPlatformDelegateViews::GetScreenPoint(
    const gfx::Point& view_pt) const {
  if (!browser_view_->root_view())
    return view_pt;

  gfx::Point screen_point = view_pt;
  view_util::ConvertPointToScreen(browser_view_->root_view(), &screen_point,
                                  true);
  return screen_point;
}

void CefBrowserPlatformDelegateViews::ViewText(const std::string& text) {
  native_delegate_->ViewText(text);
}

void CefBrowserPlatformDelegateViews::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // The BrowserView will handle accelerators.
  browser_view_->HandleKeyboardEvent(event);
}

void CefBrowserPlatformDelegateViews::HandleExternalProtocol(const GURL& url) {
  native_delegate_->HandleExternalProtocol(url);
}

void CefBrowserPlatformDelegateViews::TranslateKeyEvent(
    content::NativeWebKeyboardEvent& result,
    const CefKeyEvent& key_event) const {
  native_delegate_->TranslateKeyEvent(result, key_event);
}

void CefBrowserPlatformDelegateViews::TranslateClickEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp, int clickCount) const {
  native_delegate_->TranslateClickEvent(result, mouse_event, type, mouseUp,
                                        clickCount);
}

void CefBrowserPlatformDelegateViews::TranslateMoveEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    bool mouseLeave) const {
  native_delegate_->TranslateMoveEvent(result, mouse_event, mouseLeave);
}

void CefBrowserPlatformDelegateViews::TranslateWheelEvent(
    blink::WebMouseWheelEvent& result,
    const CefMouseEvent& mouse_event,
    int deltaX, int deltaY) const {
  native_delegate_->TranslateWheelEvent(result, mouse_event, deltaX, deltaY);
}

CefEventHandle CefBrowserPlatformDelegateViews::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  return native_delegate_->GetEventHandle(event);
}

std::unique_ptr<CefFileDialogRunner>
    CefBrowserPlatformDelegateViews::CreateFileDialogRunner() {
  return native_delegate_->CreateFileDialogRunner();
}

std::unique_ptr<CefJavaScriptDialogRunner>
    CefBrowserPlatformDelegateViews::CreateJavaScriptDialogRunner() {
  return native_delegate_->CreateJavaScriptDialogRunner();
}

std::unique_ptr<CefMenuRunner>
    CefBrowserPlatformDelegateViews::CreateMenuRunner() {
  return base::WrapUnique(new CefMenuRunnerViews(browser_view_.get()));
}

bool CefBrowserPlatformDelegateViews::IsWindowless() const {
  return false;
}

bool CefBrowserPlatformDelegateViews::IsViewsHosted() const {
  return true;
}

CefWindowHandle CefBrowserPlatformDelegateViews::GetParentWindowHandle() const {
  return GetHostWindowHandle();
}

gfx::Point CefBrowserPlatformDelegateViews::GetParentScreenPoint(
    const gfx::Point& view) const {
  return GetScreenPoint(view);
}
