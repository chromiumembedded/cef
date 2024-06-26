// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/prefs/browser_prefs.h"

#include "cef/libcef/browser/browser_context.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/prefs/pref_registrar.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace browser_prefs {

namespace {

// Match the logic in chrome/browser/net/profile_network_context_service.cc.
std::string ComputeAcceptLanguageFromPref(const std::string& language_pref) {
  std::string accept_languages_str =
      net::HttpUtil::ExpandLanguageList(language_pref);
  return net::HttpUtil::GenerateAcceptLanguageHeader(accept_languages_str);
}

// Return the most relevant setting based on |profile|.
std::string GetAcceptLanguageListSetting(Profile* profile) {
  if (auto* browser_context = CefBrowserContext::FromProfile(profile)) {
    const auto& settings = browser_context->settings();
    if (settings.accept_language_list.length > 0) {
      return CefString(&settings.accept_language_list);
    }
  }

  const auto& settings = CefContext::Get()->settings();
  if (settings.accept_language_list.length > 0) {
    return CefString(&settings.accept_language_list);
  }

  return std::string();
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  pref_registrar::RegisterCustomPrefs(CEF_PREFERENCES_TYPE_GLOBAL, registry);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  pref_registrar::RegisterCustomPrefs(CEF_PREFERENCES_TYPE_REQUEST_CONTEXT,
                                      registry);
}

std::string GetAcceptLanguageList(Profile* profile) {
  // Always prefer to the CEF settings configuration, if specified.
  std::string accept_language_list = GetAcceptLanguageListSetting(profile);
  if (accept_language_list.empty() && profile) {
    // Fall back to the preference value. The default value comes from the
    // configured locale (IDS_ACCEPT_LANGUAGES) which is then overridden by the
    // user preference in chrome://settings/languages, all managed by
    // language::LanguagePrefs.
    accept_language_list =
        profile->GetPrefs()->GetString(language::prefs::kAcceptLanguages);
  }

  if (!accept_language_list.empty()) {
    return ComputeAcceptLanguageFromPref(accept_language_list);
  }
  return std::string();
}

void SetInitialProfilePrefs(Profile* profile) {
  auto* prefs = profile->GetPrefs();

  // Language preferences.
  const std::string& accept_language_list =
      GetAcceptLanguageListSetting(profile);
  if (!accept_language_list.empty()) {
    // Used by ProfileNetworkContextService and InterceptedRequestHandlerWrapper
    // (via GetAcceptLanguageList) for request headers, and
    // renderer_preferences_util::UpdateFromSystemSettings() for
    // `navigator.language`.
    prefs->SetString(language::prefs::kAcceptLanguages, accept_language_list);

    // Necessary to avoid a reset of the kAcceptLanguages value in
    // LanguagePrefs::UpdateAcceptLanguagesPref().
    prefs->SetString(language::prefs::kSelectedLanguages, accept_language_list);
  }
}

}  // namespace browser_prefs
