// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/chrome/views/chrome_browser_widget.h"

#include "cef/libcef/browser/chrome/chrome_browser_host_impl.h"
#include "cef/libcef/browser/chrome/views/chrome_browser_frame_view.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/browser/views/window_view.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

#if BUILDFLAG(IS_MAC)
#include "cef/libcef/browser/views/native_widget_mac.h"
#include "cef/libcef/browser/views/view_util.h"
#include "ui/views/widget/native_widget_private.h"
#endif

ChromeBrowserWidget::ChromeBrowserWidget(CefWindowView* window_view)
    : window_view_(window_view) {}

ChromeBrowserWidget::~ChromeBrowserWidget() {
  DCHECK(associated_profiles_.empty());
}

void ChromeBrowserWidget::Init(BrowserView* browser_view, Browser* browser) {
  DCHECK(browser_view);
  DCHECK(browser);

  DCHECK(!BrowserWidget::browser_view());

  // Initialize BrowserWidget state.
  SetBrowserView(browser_view);

  // Stub implementation of BrowserFrameView that is not actually added to the
  // views hierarchy.
  frame_view_ = std::make_unique<ChromeBrowserFrameView>(this, browser_view);
  SetBrowserFrameView(frame_view_.get());

  // Initialize BrowserView state.
  browser_view->InitBrowser(browser);

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

void ChromeBrowserWidget::Initialized() {
  initialized_ = true;

  // Based on BrowserWidget::InitBrowserWidget.
  // This is the first call that will trigger theme-related client callbacks.
#if BUILDFLAG(IS_LINUX)
  // Calls ThemeChanged() or OnNativeThemeUpdated().
  SelectNativeTheme();
#else
  // Calls ThemeChanged().
  SetNativeTheme(ui::NativeTheme::GetInstanceForNativeUi());
#endif
}

void ChromeBrowserWidget::AddAssociatedProfile(Profile* profile) {
  DCHECK(profile);

  // Always call ThemeChanged() when the Chrome style BrowserView is added.
  bool call_theme_changed =
      browser_view() && browser_view()->GetProfile() == profile;

  ProfileMap::iterator it = associated_profiles_.find(profile);
  if (it != associated_profiles_.end()) {
    // Another instance of a known Profile.
    (it->second)++;
  } else {
    auto* current_profile = GetThemeProfile();

    associated_profiles_.insert(std::make_pair(profile, 1));

    if (auto* theme_service = ThemeServiceFactory::GetForProfile(profile)) {
      theme_service->AddObserver(this);
    }

    // Potentially switching to a different theme.
    call_theme_changed |= GetThemeProfile() != current_profile;
  }

  if (call_theme_changed) {
    // Calls ThemeChanged().
    UserChangedTheme(BrowserThemeChangeType::kBrowserTheme);
  }
}

void ChromeBrowserWidget::RemoveAssociatedProfile(Profile* profile) {
  DCHECK(profile);
  ProfileMap::iterator it = associated_profiles_.find(profile);
  if (it == associated_profiles_.end()) {
    DCHECK(false);  // Not reached.
    return;
  }
  if (--(it->second) > 0) {
    // More instances of the Profile exist.
    return;
  }

  auto* current_profile = GetThemeProfile();

  associated_profiles_.erase(it);

  if (auto* theme_service = ThemeServiceFactory::GetForProfile(profile)) {
    theme_service->RemoveObserver(this);
  }

  auto* new_profile = GetThemeProfile();
  if (new_profile != current_profile) {
    // Switching to a different theme.
    NotifyThemeColorsChanged(/*chrome_theme=*/!!new_profile);
  }
}

Profile* ChromeBrowserWidget::GetThemeProfile() const {
  // Always prefer the Browser Profile, if any.
  if (browser_view()) {
    return browser_view()->GetProfile();
  }
  if (!associated_profiles_.empty()) {
    return associated_profiles_.begin()->first;
  }
  return nullptr;
}

bool ChromeBrowserWidget::ToggleFullscreenMode() {
  if (browser_view()) {
    // Toggle fullscreen mode via the Chrome command for consistent behavior.
    chrome::ToggleFullscreenMode(browser_view()->browser());
    return true;
  }
  return false;
}

void ChromeBrowserWidget::UserChangedTheme(
    BrowserThemeChangeType theme_change_type) {
  // Callback from Browser::OnThemeChanged() and OnNativeThemeUpdated().

  // Calls ThemeChanged() and possibly SelectNativeTheme().
  BrowserWidget::UserChangedTheme(theme_change_type);

  NotifyThemeColorsChanged(/*chrome_theme=*/!native_theme_change_);
}

views::internal::RootView* ChromeBrowserWidget::CreateRootView() {
  // Bypass the BrowserWidget implementation.
  return views::Widget::CreateRootView();
}

std::unique_ptr<views::FrameView> ChromeBrowserWidget::CreateFrameView() {
  // Bypass the BrowserWidget implementation.
  return views::Widget::CreateFrameView();
}

void ChromeBrowserWidget::Activate() {
  if (browser_view() && browser_view()->browser() &&
      browser_view()->browser()->is_type_devtools()) {
    if (auto browser_host = ChromeBrowserHostImpl::GetBrowserForBrowser(
            browser_view()->browser())) {
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
  BrowserWidget::Activate();
}

void ChromeBrowserWidget::OnNativeWidgetDestroyed() {
  if (browser_view()) {
    // Remove the listener registration added in BrowserView::InitBrowser().
    if (auto focus_manager = browser_view()->GetFocusManager()) {
      focus_manager->RemoveFocusChangeListener(browser_view());
    }

    // Proceed with Browser-related object teardown. Order of destruction
    // is BrowserView, BrowserWidget (this), Browser.

    // Release the unique_ptr reference that BrowserView holds to BrowserWidget
    // as the BrowserView will be destroyed first.
    browser_view()->DeleteBrowserWindow();

    // Destruction logic from BrowserWidget::OnNativeWidgetDestroyed.
    Browser* const browser = browser_view()->browser();
    browser->set_force_skip_warning_user_on_close(true);
    browser->OnWindowClosing();

    // Invoke the pre-window-destruction lifecycle hook before the BrowserView
    // and BrowserWidget are destroyed.
    browser->GetFeatures().TearDownPreBrowserWindowDestruction();

    // Release the unique_ptr reference that Browser holds to BrowserView
    // (BrowserWindow) as the BrowserView will be destroyed first.
    browser->ReleaseBrowserWindow();

    // Release raw_ptr<> references to the BrowserView before it's destroyed.
    window_view_ = nullptr;
    SetBrowserView(nullptr);

    // Delete the stub BrowserFrameView implementation.
    if (frame_view_) {
      SetBrowserFrameView(nullptr);
      frame_view_.reset();
    }

    // Proceed with Widget destruction. Results in a call to
    // CefWindowWidgetDelegate::WidgetIsZombie which tears down the views
    // hierarchy (deletes BrowserView, etc) and deletes BrowserWidget.
    // Intentionally skipping BrowserWidget::OnNativeWidgetDestroyed here as the
    // logic is incorporated inline.
    Widget::OnNativeWidgetDestroyed();
    // BrowserView and BrowserWidget have been destroyed at this point.

    browser->SynchronouslyDestroyBrowser();
    // Browser has been destroyed at this point.
  } else {
    // Intentionally skipping BrowserWidget::OnNativeWidgetDestroyed here
    // because |browser_view()| is nullptr.
    Widget::OnNativeWidgetDestroyed();
  }
}

void ChromeBrowserWidget::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  // TODO: Reduce the frequency of this callback on Windows/Linux.
  // See https://issues.chromium.org/issues/40280130#comment7

  color_provider_tracker_.OnNativeThemeUpdated();

  native_theme_change_ = true;

  // Calls UserChangedTheme().
  BrowserWidget::OnNativeThemeUpdated(observed_theme);

  native_theme_change_ = false;
}

ui::ColorProviderKey ChromeBrowserWidget::GetColorProviderKey() const {
  if (browser_view()) {
    // Use the default Browser implementation.
    return BrowserWidget::GetColorProviderKey();
  }

  const auto& widget_key = Widget::GetColorProviderKey();
  if (auto* profile = GetThemeProfile()) {
    return CefWidget::GetColorProviderKey(widget_key, profile);
  }
  return widget_key;
}

void ChromeBrowserWidget::OnThemeChanged() {
  if (browser_view()) {
    // Ignore these notifications if we have a Browser.
    return;
  }

  // When the Chrome theme changes, the NativeTheme may also change.
  SelectNativeTheme();

  NotifyThemeColorsChanged(/*chrome_theme=*/true);
}

void ChromeBrowserWidget::OnColorProviderCacheResetMissed() {
  // Ignore calls during Widget::Init().
  if (!initialized_) {
    return;
  }

  NotifyThemeColorsChanged(/*chrome_theme=*/false);
}

void ChromeBrowserWidget::NotifyThemeColorsChanged(bool chrome_theme) {
  if (window_view_) {
    window_view_->OnThemeColorsChanged(chrome_theme);

    // Call ThemeChanged() asynchronously to avoid possible reentrancy.
    CEF_POST_TASK(TID_UI, base::BindOnce(&ChromeBrowserWidget::ThemeChanged,
                                         weak_ptr_factory_.GetWeakPtr()));
  }
}
