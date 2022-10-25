// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PREFS_PREF_REGISTRAR_H_
#define CEF_LIBCEF_BROWSER_PREFS_PREF_REGISTRAR_H_

#include "include/internal/cef_types.h"

class PrefRegistrySimple;

namespace pref_registrar {

void RegisterCustomPrefs(cef_preferences_type_t type,
                         PrefRegistrySimple* registry);

}  // namespace pref_registrar

#endif  // CEF_LIBCEF_BROWSER_PREFS_PREF_REGISTRAR_H_
