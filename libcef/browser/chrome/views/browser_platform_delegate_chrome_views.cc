// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/views/browser_platform_delegate_chrome_views.h"

#include "include/views/cef_window.h"

#include "chrome/browser/ui/browser.h"
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
    if (browser)
      return browser->GetHost()->TryCloseBrowser();
    return true;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;

  IMPLEMENT_REFCOUNTING(PopupWindowDelegate);
};

}  // namespace

CefBrowserPlatformDelegateChromeViews::CefBrowserPlatformDelegateChromeViews(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
    CefRefPtr<CefBrowserViewImpl> browser_view)
    : CefBrowserPlatformDelegateChrome(std::move(native_delegate)) {
  if (browser_view)
    SetBrowserView(browser_view);
}

void CefBrowserPlatformDelegateChromeViews::SetBrowserView(
    CefRefPtr<CefBrowserViewImpl> browser_view) {
  DCHECK(!browser_view_);
  DCHECK(browser_view);
  browser_view_ = browser_view;
}

void CefBrowserPlatformDelegateChromeViews::WebContentsCreated(
    content::WebContents* web_contents,
    bool owned) {
  CefBrowserPlatformDelegateChrome::WebContentsCreated(web_contents, owned);
  browser_view_->WebContentsCreated(web_contents);
}

void CefBrowserPlatformDelegateChromeViews::BrowserCreated(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateChrome::BrowserCreated(browser);
  browser_view_->BrowserCreated(browser, base::RepeatingClosure());
}

void CefBrowserPlatformDelegateChromeViews::NotifyBrowserCreated() {
  if (browser_view_->delegate())
    browser_view_->delegate()->OnBrowserCreated(browser_view_, browser_);
}

void CefBrowserPlatformDelegateChromeViews::NotifyBrowserDestroyed() {
  if (browser_view_->delegate())
    browser_view_->delegate()->OnBrowserDestroyed(browser_view_, browser_);
}

void CefBrowserPlatformDelegateChromeViews::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateChrome::BrowserDestroyed(browser);
  browser_view_->BrowserDestroyed(browser);
}

void CefBrowserPlatformDelegateChromeViews::CloseHostWindow() {
  views::Widget* widget = GetWindowWidget();
  if (widget && !widget->IsClosed())
    widget->Close();
}

views::Widget* CefBrowserPlatformDelegateChromeViews::GetWindowWidget() const {
  if (browser_view_->root_view())
    return browser_view_->root_view()->GetWidget();
  return nullptr;
}

CefRefPtr<CefBrowserView>
CefBrowserPlatformDelegateChromeViews::GetBrowserView() const {
  return browser_view_.get();
}

void CefBrowserPlatformDelegateChromeViews::PopupWebContentsCreated(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* new_web_contents,
    CefBrowserPlatformDelegate* new_platform_delegate,
    bool is_devtools) {
  DCHECK(new_platform_delegate->IsViewsHosted());
  auto* new_platform_delegate_impl =
      static_cast<CefBrowserPlatformDelegateChromeViews*>(
          new_platform_delegate);

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

void CefBrowserPlatformDelegateChromeViews::PopupBrowserCreated(
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

bool CefBrowserPlatformDelegateChromeViews::IsViewsHosted() const {
  return true;
}
