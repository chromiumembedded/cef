// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PREFS_PREF_HELPER_H_
#define CEF_LIBCEF_BROWSER_PREFS_PREF_HELPER_H_

#include "include/cef_values.h"

class PrefService;

namespace pref_helper {

// Function names and arguments match the CefPreferenceManager interface.

bool HasPreference(PrefService* pref_service, const CefString& name);

CefRefPtr<CefValue> GetPreference(PrefService* pref_service,
                                  const CefString& name);

CefRefPtr<CefDictionaryValue> GetAllPreferences(PrefService* pref_service,
                                                bool include_defaults);

bool CanSetPreference(PrefService* pref_service, const CefString& name);

bool SetPreference(PrefService* pref_service,
                   const CefString& name,
                   CefRefPtr<CefValue> value,
                   CefString& error);

}  // namespace pref_helper

#endif  // CEF_LIBCEF_BROWSER_PREFS_PREF_HELPER_H_
