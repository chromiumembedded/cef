// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/chrome/views/chrome_browser_frame.h"

#include "cef/libcef/browser/chrome/chrome_browser_host_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/browser/views/window_view.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

#if BUILDFLAG(IS_MAC)
#include "cef/libcef/browser/views/native_widget_mac.h"
#include "cef/libcef/browser/views/view_util.h"
#include "ui/views/widget/native_widget_private.h"
#endif

ChromeBrowserFrame::ChromeBrowserFrame(CefWindowView* window_view)
    : window_view_(window_view) {}

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

void ChromeBrowserFrame::Initialized() {
  initialized_ = true;

  // Based on BrowserFrame::InitBrowserFrame.
  // This is the first call that will trigger theme-related client callbacks.
#if BUILDFLAG(IS_LINUX)
  // Calls ThemeChanged() or OnNativeThemeUpdated().
  SelectNativeTheme();
#else
  // Calls ThemeChanged().
  SetNativeTheme(ui::NativeTheme::GetInstanceForNativeUi());
#endif
}

void ChromeBrowserFrame::AddAssociatedProfile(Profile* /*profile*/) {
  // Calls ThemeChanged().
  UserChangedTheme(BrowserThemeChangeType::kBrowserTheme);
}

void ChromeBrowserFrame::RemoveAssociatedProfile(Profile* /*profile*/) {}

Profile* ChromeBrowserFrame::GetThemeProfile() const {
  if (browser_view_) {
    return browser_view_->GetProfile();
  }
  return nullptr;
}

bool ChromeBrowserFrame::ToggleFullscreenMode() {
  if (browser_view_) {
    // Toggle fullscreen mode via the Chrome command for consistent behavior.
    chrome::ToggleFullscreenMode(browser_view_->browser());
    return true;
  }
  return false;
}

void ChromeBrowserFrame::UserChangedTheme(
    BrowserThemeChangeType theme_change_type) {
  // Callback from Browser::OnThemeChanged() and OnNativeThemeUpdated().

  // Calls ThemeChanged() and possibly SelectNativeTheme().
  BrowserFrame::UserChangedTheme(theme_change_type);

  NotifyThemeColorsChanged(/*chrome_theme=*/!native_theme_change_);
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

void ChromeBrowserFrame::OnNativeWidgetDestroyed() {
  window_view_ = nullptr;
  BrowserFrame::OnNativeWidgetDestroyed();
}

void ChromeBrowserFrame::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  // TODO: Reduce the frequency of this callback on Windows/Linux.
  // See https://issues.chromium.org/issues/40280130#comment7

  color_provider_tracker_.OnNativeThemeUpdated();

  native_theme_change_ = true;

  // Calls UserChangedTheme().
  BrowserFrame::OnNativeThemeUpdated(observed_theme);

  native_theme_change_ = false;
}

void ChromeBrowserFrame::OnColorProviderCacheResetMissed() {
  // Ignore calls during Widget::Init().
  if (!initialized_) {
    return;
  }

  NotifyThemeColorsChanged(/*chrome_theme=*/false);
}

void ChromeBrowserFrame::NotifyThemeColorsChanged(bool chrome_theme) {
  if (window_view_) {
    window_view_->OnThemeColorsChanged(chrome_theme);

    // Call ThemeChanged() asynchronously to avoid possible reentrancy.
    CEF_POST_TASK(TID_UI, base::BindOnce(&ChromeBrowserFrame::ThemeChanged,
                                         weak_ptr_factory_.GetWeakPtr()));
  }
}
