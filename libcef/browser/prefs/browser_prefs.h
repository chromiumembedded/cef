// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_
#define CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_

#include <memory>

namespace base {
class FilePath;
}

class PrefService;
class Profile;

namespace browser_prefs {

// Name for the user prefs JSON file.
extern const char kUserPrefsFileName[];

// Create the PrefService used to manage pref registration and storage. If
// |is_pre_initialization| is true a non-persistent PrefService will be created
// for temporary usage during BrowserContextServices initialization.
std::unique_ptr<PrefService> CreatePrefService(
    Profile* profile,
    const base::FilePath& cache_path,
    bool persist_user_preferences,
    bool is_pre_initialization);

}  // namespace browser_prefs

#endif  // CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_
