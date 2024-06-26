// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_
#define CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_

#include <memory>
#include <string>

class PrefRegistrySimple;
class Profile;

namespace browser_prefs {

// Register preferences specific to CEF.
// Called from chrome/browser/prefs/browser_prefs.cc
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns the value for populating the accept-language HTTP request header.
// |browser_context| and/or |browser| may be nullptr. If |expand| is true then
// base languages and Q values may be added.
std::string GetAcceptLanguageList(Profile* profile);

// Set preferences for a newly initialized Profile.
void SetInitialProfilePrefs(Profile* profile);

}  // namespace browser_prefs

#endif  // CEF_LIBCEF_BROWSER_PREFS_BROWSER_PREFS_H_
