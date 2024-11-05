// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/views/browser_platform_delegate_chrome_views.h"

#include "cef/include/views/cef_window.h"
#include "cef/libcef/browser/views/window_impl.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "components/zoom/zoom_controller.h"
#include "ui/views/widget/widget.h"

CefBrowserPlatformDelegateChromeViews::CefBrowserPlatformDelegateChromeViews(
    std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate,
    CefRefPtr<CefBrowserViewImpl> browser_view)
    : CefBrowserPlatformDelegateChrome(std::move(native_delegate)) {
  if (browser_view) {
    SetBrowserView(browser_view);
  }
}

void CefBrowserPlatformDelegateChromeViews::SetBrowserView(
    CefRefPtr<CefBrowserView> browser_view) {
  DCHECK(!browser_view_);
  DCHECK(browser_view);
  browser_view_ =
      static_cast<CefBrowserViewImpl*>(browser_view.get())->GetWeakPtr();
}

void CefBrowserPlatformDelegateChromeViews::WebContentsCreated(
    content::WebContents* web_contents,
    bool owned) {
  CefBrowserPlatformDelegateChrome::WebContentsCreated(web_contents, owned);
  browser_view_->WebContentsCreated(web_contents);
}

void CefBrowserPlatformDelegateChromeViews::WebContentsDestroyed(
    content::WebContents* web_contents) {
  CefBrowserPlatformDelegateChrome::WebContentsDestroyed(web_contents);
  // |browser_view_| may be destroyed before this callback arrives.
  if (browser_view_) {
    browser_view_->WebContentsDestroyed(web_contents);
  }
}

void CefBrowserPlatformDelegateChromeViews::BrowserCreated(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateChrome::BrowserCreated(browser);
  browser_view_->BrowserCreated(browser, base::RepeatingClosure());
}

void CefBrowserPlatformDelegateChromeViews::NotifyBrowserCreated() {
  if (auto delegate = browser_view_->delegate()) {
    delegate->OnBrowserCreated(browser_view_.get(), browser_.get());

    // DevTools windows hide the notification bubble by default. However, we
    // don't currently have the ability to intercept WebContents creation via
    // DevToolsWindow::Create(), so |show_by_default| will always be true here.
    const bool show_by_default =
        !DevToolsWindow::IsDevToolsWindow(web_contents_);

    bool show_zoom_bubble = show_by_default;
    const auto& state = browser_->settings().chrome_zoom_bubble;
    if (show_by_default && state == STATE_DISABLED) {
      show_zoom_bubble = false;
    } else if (!show_by_default && state == STATE_ENABLED) {
      show_zoom_bubble = true;
    }

    if (show_zoom_bubble != show_by_default) {
      // We may be called before TabHelpers::AttachTabHelpers(), so create
      // the ZoomController if necessary.
      zoom::ZoomController::CreateForWebContents(web_contents_);
      zoom::ZoomController::FromWebContents(web_contents_)
          ->SetShowsNotificationBubble(show_zoom_bubble);
    }
  }
}

void CefBrowserPlatformDelegateChromeViews::NotifyBrowserDestroyed() {
  // |browser_view_| may be destroyed before this callback arrives.
  if (browser_view_ && browser_view_->delegate()) {
    browser_view_->delegate()->OnBrowserDestroyed(browser_view_.get(),
                                                  browser_.get());
  }
}

void CefBrowserPlatformDelegateChromeViews::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateChrome::BrowserDestroyed(browser);
  // |browser_view_| may be destroyed before this callback arrives.
  if (browser_view_) {
    browser_view_->BrowserDestroyed(browser);
  }
  browser_view_ = nullptr;
}

void CefBrowserPlatformDelegateChromeViews::CloseHostWindow() {
  views::Widget* widget = GetWindowWidget();
  if (widget && !widget->IsClosed()) {
    widget->Close();
  }
}

CefWindowHandle CefBrowserPlatformDelegateChromeViews::GetHostWindowHandle()
    const {
  return view_util::GetWindowHandle(GetWindowWidget());
}

views::Widget* CefBrowserPlatformDelegateChromeViews::GetWindowWidget() const {
  if (browser_view_ && browser_view_->root_view()) {
    return browser_view_->root_view()->GetWidget();
  }
  return nullptr;
}

CefRefPtr<CefBrowserView>
CefBrowserPlatformDelegateChromeViews::GetBrowserView() const {
  return browser_view_.get();
}

void CefBrowserPlatformDelegateChromeViews::SetFocus(bool setFocus) {
  if (setFocus && browser_view_) {
    browser_view_->RequestFocusSync();
  }
}

bool CefBrowserPlatformDelegateChromeViews::IsViewsHosted() const {
  return true;
}

CefWindowImpl* CefBrowserPlatformDelegateChromeViews::GetWindowImpl() const {
  if (auto* widget = GetWindowWidget()) {
    CefRefPtr<CefWindow> window = view_util::GetWindowFor(widget);
    return static_cast<CefWindowImpl*>(window.get());
  }
  return nullptr;
}
