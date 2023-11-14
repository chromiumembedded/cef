// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/chrome/views/chrome_browser_frame.h"

#include "libcef/browser/chrome/chrome_browser_host_impl.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

#if BUILDFLAG(IS_MAC)
#include "libcef/browser/views/native_widget_mac.h"
#include "libcef/browser/views/view_util.h"
#include "ui/views/widget/native_widget_private.h"
#endif

void ChromeBrowserFrame::Init(BrowserView* browser_view,
                              std::unique_ptr<Browser> browser) {
  DCHECK(browser_view);
  DCHECK(browser);

  DCHECK(!browser_view_);
  browser_view_ = browser_view;

  // Initialize BrowserFrame state.
  InitBrowserView(browser_view);

  // Initialize BrowserView state.
  browser_view->InitBrowser(std::move(browser));

#if BUILDFLAG(IS_MAC)
  // Initialize native window state.
  if (auto native_window = view_util::GetNativeWindow(this)) {
    if (auto* native_widget_private = views::internal::NativeWidgetPrivate::
            GetNativeWidgetForNativeWindow(native_window)) {
      auto* native_widget_mac =
          static_cast<CefNativeWidgetMac*>(native_widget_private);
      native_widget_mac->SetBrowserView(browser_view);
      native_widget_mac->OnWindowInitialized();
    }
  }
#endif  // BUILDFLAG(IS_MAC)
}

void ChromeBrowserFrame::ToggleFullscreenMode() {
  chrome::ToggleFullscreenMode(browser_view_->browser());
}

views::internal::RootView* ChromeBrowserFrame::CreateRootView() {
  // Bypass the BrowserFrame implementation.
  return views::Widget::CreateRootView();
}

std::unique_ptr<views::NonClientFrameView>
ChromeBrowserFrame::CreateNonClientFrameView() {
  // Bypass the BrowserFrame implementation.
  return views::Widget::CreateNonClientFrameView();
}

void ChromeBrowserFrame::Activate() {
  if (browser_view_ && browser_view_->browser() &&
      browser_view_->browser()->is_type_devtools()) {
    if (auto browser_host = ChromeBrowserHostImpl::GetBrowserForBrowser(
            browser_view_->browser())) {
      if (browser_host->platform_delegate()->HasExternalParent()) {
        // Handle activation of DevTools with external parent via the platform
        // delegate. On Windows the default platform implementation
        // (HWNDMessageHandler::Activate) will call SetForegroundWindow but that
        // doesn't seem to work for DevTools windows when activated via the
        // right-click context menu.
        browser_host->SetFocus(true);
        return;
      }
    }
  }

  // Proceed with default handling.
  BrowserFrame::Activate();
}
