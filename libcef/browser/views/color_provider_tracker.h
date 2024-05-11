// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_COLOR_PROVIDER_TRACKER_H_
#define CEF_LIBCEF_BROWSER_VIEWS_COLOR_PROVIDER_TRACKER_H_
#pragma once

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/color/color_provider_manager.h"

// Color registrations are managed by the global ColorProviderManager object.
// When the system theme changes (e.g. NativeThemeWin::UpdateDarkModeStatus or
// NativeThemeWin::OnSysColorChange is called) all existing platform NativeTheme
// objects (NativeTheme[Win|Mac|Gtk]) are notified. They then call
// NativeTheme::NotifyOnNativeThemeUpdated which calls
// ColorProviderManager::ResetColorProviderCache, followed by
// OnNativeThemeUpdated for each registered Widget, followed by
// ColorProviderManager::AfterNativeThemeUpdated. The problem is that Chromium
// creates multiple NativeTheme objects but each Widget only registers as an
// Observer for the one returned via Widget::GetNativeTheme. If a different
// NativeTheme is the last caller of ResetColorProviderCache then we don't get
// an opportunity to reapply global color overrides in the Widget's
// OnNativeThemeChanged callback. To work around this problem each Widget owns a
// Tracker object. The Tracker explicitly registers as an Observer on the
// ColorProviderManager to get callbacks from ResetColorProviderCache and
// AfterNativeThemeUpdated. If OnNativeThemeUpdated is not called for the Widget
// (which otherwise forwards the call to the Tracker) then the Tracker will call
// OnColorProviderCacheResetMissed from OnAfterNativeThemeUpdated.
class CefColorProviderTracker : public ui::ColorProviderManagerObserver {
 public:
  class Observer {
   public:
    // Called when the color provider cache is reset without a follow-up call to
    // OnNativeThemeUpdated.
    virtual void OnColorProviderCacheResetMissed() {}

   protected:
    virtual ~Observer() = default;
  };

  explicit CefColorProviderTracker(Observer* observer);

  CefColorProviderTracker(const CefColorProviderTracker&) = delete;
  CefColorProviderTracker& operator=(const CefColorProviderTracker&) = delete;

  // Notify us when OnNativeThemeUpdated is called.
  void OnNativeThemeUpdated();

 private:
  // ui::ColorProviderManagerObserver methods:
  void OnColorProviderCacheReset() override;
  void OnAfterNativeThemeUpdated() override;

  const raw_ptr<Observer> observer_;

  bool got_theme_updated_ = false;
  base::ScopedObservation<ui::ColorProviderManager,
                          ui::ColorProviderManagerObserver>
      color_provider_observation_{this};
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_COLOR_PROVIDER_TRACKER_H_
