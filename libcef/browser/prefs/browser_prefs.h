// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_
#define CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_

#include <memory>

namespace base {
class FilePath;
}

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace browser_prefs {

// Name for the user prefs JSON file.
extern const char kUserPrefsFileName[];

// Register preferences specific to CEF.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Create the PrefService used to manage pref registration and storage.
// |profile| will be nullptr for the system-level PrefService. Used with the
// Alloy runtime only.
std::unique_ptr<PrefService> CreatePrefService(Profile* profile,
                                               const base::FilePath& cache_path,
                                               bool persist_user_preferences);

// Returns the value for populating the accept-language HTTP request header.
// |browser_context| and/or |browser| may be nullptr. If |expand| is true then
// base languages and Q values may be added.
std::string GetAcceptLanguageList(Profile* profile);

// Set preferences for a newly initialized Profile.
void SetInitialProfilePrefs(Profile* profile);

}  // namespace browser_prefs

#endif  // CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_
