// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/views/widget_impl.h"

#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/browser/views/window_view.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#endif

CefWidgetImpl::CefWidgetImpl(CefWindowView* window_view)
    : window_view_(window_view) {}

CefWidgetImpl::~CefWidgetImpl() {
  DCHECK(associated_profiles_.empty());
}

void CefWidgetImpl::Initialized() {
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

void CefWidgetImpl::AddAssociatedProfile(Profile* profile) {
  DCHECK(profile);
  ProfileMap::iterator it = associated_profiles_.find(profile);
  if (it != associated_profiles_.end()) {
    // Another instance of a known Profile.
    (it->second)++;
    return;
  }

  auto* current_profile = GetThemeProfile();

  associated_profiles_.insert(std::make_pair(profile, 1));

  if (auto* theme_service = ThemeServiceFactory::GetForProfile(profile)) {
    theme_service->AddObserver(this);
  }

  auto* new_profile = GetThemeProfile();
  if (new_profile != current_profile) {
    // Switching to a different theme.
    NotifyThemeColorsChanged(/*chrome_theme=*/!!new_profile,
                             /*call_theme_changed=*/true);
  }
}

void CefWidgetImpl::RemoveAssociatedProfile(Profile* profile) {
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
    NotifyThemeColorsChanged(/*chrome_theme=*/!!new_profile,
                             /*call_theme_changed=*/true);
  }
}

Profile* CefWidgetImpl::GetThemeProfile() const {
  if (!associated_profiles_.empty()) {
    return associated_profiles_.begin()->first;
  }
  return nullptr;
}

const ui::ThemeProvider* CefWidgetImpl::GetThemeProvider() const {
  auto* profile = GetThemeProfile();
  if (!profile) {
    return Widget::GetThemeProvider();
  }

  // Based on BrowserFrame::GetThemeProvider.
  return &ThemeService::GetThemeProviderForProfile(profile);
}

ui::ColorProviderKey::ThemeInitializerSupplier* CefWidgetImpl::GetCustomTheme()
    const {
  auto* profile = GetThemeProfile();
  if (!profile) {
    return Widget::GetCustomTheme();
  }

  // Based on BrowserFrame::GetCustomTheme.
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile);
  return theme_service->UsingDeviceTheme() ? nullptr
                                           : theme_service->GetThemeSupplier();
}

void CefWidgetImpl::OnNativeWidgetDestroyed() {
  window_view_ = nullptr;
  views::Widget::OnNativeWidgetDestroyed();
}

void CefWidgetImpl::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  // TODO: Reduce the frequency of this callback on Windows/Linux.
  // See https://issues.chromium.org/issues/40280130#comment7

  color_provider_tracker_.OnNativeThemeUpdated();

  // Native/OS theme changed.
  NotifyThemeColorsChanged(/*chrome_theme=*/false,
                           /*call_theme_changed=*/false);

  // Calls ThemeChanged().
  Widget::OnNativeThemeUpdated(observed_theme);
}

ui::ColorProviderKey CefWidgetImpl::GetColorProviderKey() const {
  const auto& widget_key = Widget::GetColorProviderKey();
  if (auto* profile = GetThemeProfile()) {
    return CefWidget::GetColorProviderKey(widget_key, profile);
  }
  return widget_key;
}

void CefWidgetImpl::OnThemeChanged() {
  // When the Chrome theme changes, the NativeTheme may also change.
  SelectNativeTheme();

  NotifyThemeColorsChanged(/*chrome_theme=*/true, /*call_theme_changed=*/true);
}

void CefWidgetImpl::NotifyThemeColorsChanged(bool chrome_theme,
                                             bool call_theme_changed) {
  if (window_view_) {
    window_view_->OnThemeColorsChanged(chrome_theme);
    if (call_theme_changed) {
      // Call ThemeChanged() asynchronously to avoid possible reentrancy.
      CEF_POST_TASK(TID_UI, base::BindOnce(&CefWidgetImpl::ThemeChanged,
                                           weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void CefWidgetImpl::SelectNativeTheme() {
  // Based on BrowserFrame::SelectNativeTheme.
#if BUILDFLAG(IS_LINUX)
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();

  // Always use the NativeTheme for forced color modes.
  if (ui::NativeTheme::IsForcedDarkMode() ||
      ui::NativeTheme::IsForcedLightMode()) {
    SetNativeTheme(native_theme);
    return;
  }

  const auto* linux_ui_theme =
      ui::LinuxUiTheme::GetForWindow(GetNativeWindow());
  SetNativeTheme(linux_ui_theme ? linux_ui_theme->GetNativeTheme()
                                : native_theme);
#endif
}

void CefWidgetImpl::OnColorProviderCacheResetMissed() {
  // Ignore calls during Widget::Init().
  if (!initialized_) {
    return;
  }

  NotifyThemeColorsChanged(/*chrome_theme=*/false,
                           /*call_theme_changed=*/true);
}
