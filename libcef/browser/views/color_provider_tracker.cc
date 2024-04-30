// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/views/color_provider_tracker.h"

#include "base/check.h"

CefColorProviderTracker::CefColorProviderTracker(Observer* observer)
    : observer_(observer) {
  DCHECK(observer_);
  color_provider_observation_.Observe(&ui::ColorProviderManager::Get());
}

void CefColorProviderTracker::OnNativeThemeUpdated() {
  got_theme_updated_ = true;
}

void CefColorProviderTracker::OnColorProviderCacheReset() {
  // May be followed by a call to OnNativeThemeUpdated.
  got_theme_updated_ = false;
}

void CefColorProviderTracker::OnAfterNativeThemeUpdated() {
  if (!got_theme_updated_) {
    observer_->OnColorProviderCacheResetMissed();
  }
}
