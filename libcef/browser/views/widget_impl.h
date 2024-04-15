// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_WIDGET_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_WIDGET_IMPL_H_
#pragma once

#include <map>

#include "libcef/browser/views/color_provider_tracker.h"
#include "libcef/browser/views/widget.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "ui/views/widget/widget.h"

class CefWindowView;
class Profile;

// Widget specialization to implement theme support for the Alloy runtime. The
// global NativeTheme (native/OS theme) will be used unless this Widget contains
// a BrowserView, in which case a Chrome theme associated with the BrowserView's
// Profile will be used.
//
// Theme support works as follows:
// - OnNativeThemeUpdated is called when the NativeTheme associated with this
//   Widget changes. For example, when switching the OS appearance between light
//   and dark mode.
// - OnColorProviderCacheResetMissed is called if some other NativeTheme not
//   associated with this Widget changes and we need to reapply global color
//   overrides (see CefColorProviderTracker for details).
// - OnThemeChanged is called when the client changes the Chrome theme
//   explicitly by calling CefRequestContext::SetChromeColorScheme.
// - GetThemeProvider, GetCustomTheme and GetColorProviderKey return objects
//   that are used internally to apply the current theme.
//
// Callers should use view_util methods (e.g. GetColor, ShouldUseDarkTheme, etc)
// instead of calling theme-related Widget methods directly.
class CefWidgetImpl : public views::Widget,
                      public CefWidget,
                      public CefColorProviderTracker::Observer,
                      public ThemeServiceObserver {
 public:
  explicit CefWidgetImpl(CefWindowView* window_view);
  ~CefWidgetImpl() override;

  CefWidgetImpl(const CefWidgetImpl&) = delete;
  CefWidgetImpl& operator=(const CefWidgetImpl&) = delete;

  // CefWidget methods:
  views::Widget* GetWidget() override { return this; }
  const views::Widget* GetWidget() const override { return this; }
  void Initialized() override;
  bool IsInitialized() const override { return initialized_; }
  void AddAssociatedProfile(Profile* profile) override;
  void RemoveAssociatedProfile(Profile* profile) override;
  Profile* GetThemeProfile() const override;

  // views::Widget methods:
  const ui::ThemeProvider* GetThemeProvider() const override;
  ui::ColorProviderKey::ThemeInitializerSupplier* GetCustomTheme()
      const override;

  // NativeWidgetDelegate methods:
  void OnNativeWidgetDestroyed() override;

  // ui::NativeThemeObserver methods:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;
  ui::ColorProviderKey GetColorProviderKey() const override;

  // ThemeServiceObserver methods:
  void OnThemeChanged() override;

 private:
  void NotifyThemeColorsChanged(bool chrome_theme, bool call_theme_changed);

  // Select a native theme that is appropriate for the current context. This is
  // currently only needed for Linux to switch between the regular NativeTheme
  // and the GTK NativeTheme instance.
  void SelectNativeTheme();

  // CefColorProviderTracker::Observer methods:
  void OnColorProviderCacheResetMissed() override;

  CefWindowView* window_view_;

  bool initialized_ = false;

  // Map of Profile* to count.
  using ProfileMap = std::map<Profile*, size_t>;
  ProfileMap associated_profiles_;

  CefColorProviderTracker color_provider_tracker_{this};

  base::WeakPtrFactory<CefWidgetImpl> weak_ptr_factory_{this};
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_WIDGET_IMPL_H_
