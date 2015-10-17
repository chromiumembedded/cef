// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_
#define CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_

#include "base/memory/scoped_ptr.h"

class PrefService;

namespace base {
class FilePath;
}

namespace browser_prefs {

// Name for the user prefs JSON file.
extern const char kUserPrefsFileName[];

// Create the PrefService used to manage pref registration and storage.
scoped_ptr<PrefService> CreatePrefService(const base::FilePath& pref_path);

}  // namespace browser_prefs

#endif  // CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_
