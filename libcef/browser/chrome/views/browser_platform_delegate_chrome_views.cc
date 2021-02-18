// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/views/browser_platform_delegate_chrome_views.h"

#include "chrome/browser/ui/browser.h"
#include "ui/views/widget/widget.h"

CefBrowserPlatformDelegateChromeViews::CefBrowserPlatformDelegateChromeViews(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
    CefRefPtr<CefBrowserViewImpl> browser_view)
    : CefBrowserPlatformDelegateChrome(std::move(native_delegate)),
      browser_view_(browser_view) {}

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

bool CefBrowserPlatformDelegateChromeViews::IsViewsHosted() const {
  return true;
}
