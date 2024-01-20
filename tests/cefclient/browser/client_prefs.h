// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_PREFS_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_PREFS_H_
#pragma once

#include <optional>

#include "include/cef_base.h"
#include "include/cef_preference.h"

namespace client::prefs {

// Register global preferences with default values.
void RegisterGlobalPreferences(CefRawPtr<CefPreferenceRegistrar> registrar);

// Load/save window restore info.
bool LoadWindowRestorePreferences(cef_show_state_t& show_state,
                                  std::optional<CefRect>& dip_bounds);
bool SaveWindowRestorePreferences(cef_show_state_t show_state,
                                  std::optional<CefRect> dip_bounds);

}  // namespace client::prefs

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_PREFS_H_
