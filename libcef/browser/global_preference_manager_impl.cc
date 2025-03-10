// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/global_preference_manager_impl.h"

#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "base/strings/string_util.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/prefs/pref_helper.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/api_version_util.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/webui/flags/pref_service_flags_storage.h"

namespace {

std::string GetActiveGroupNameAsString(
    const base::FieldTrial::ActiveGroup& group) {
  constexpr std::string_view kNonBreakingHyphenUTF8 = "\xE2\x80\x91";
  std::string result = group.trial_name + ":" + group.group_name;
  base::ReplaceChars(result, "-", kNonBreakingHyphenUTF8, &result);
  return result;
}

}  // namespace

bool CefGlobalPreferenceManagerImpl::HasPreference(const CefString& name) {
  CEF_REQUIRE_UIT_RETURN(false);
  return pref_helper::HasPreference(g_browser_process->local_state(), name);
}

CefRefPtr<CefValue> CefGlobalPreferenceManagerImpl::GetPreference(
    const CefString& name) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  return pref_helper::GetPreference(g_browser_process->local_state(), name);
}

CefRefPtr<CefDictionaryValue> CefGlobalPreferenceManagerImpl::GetAllPreferences(
    bool include_defaults) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  return pref_helper::GetAllPreferences(g_browser_process->local_state(),
                                        include_defaults);
}

bool CefGlobalPreferenceManagerImpl::CanSetPreference(const CefString& name) {
  CEF_REQUIRE_UIT_RETURN(false);
  return pref_helper::CanSetPreference(g_browser_process->local_state(), name);
}

bool CefGlobalPreferenceManagerImpl::SetPreference(const CefString& name,
                                                   CefRefPtr<CefValue> value,
                                                   CefString& error) {
  CEF_REQUIRE_UIT_RETURN(false);
  return pref_helper::SetPreference(g_browser_process->local_state(), name,
                                    value, error);
}

CefRefPtr<CefRegistration>
CefGlobalPreferenceManagerImpl::AddPreferenceObserver(
    const CefString& name,
    CefRefPtr<CefPreferenceObserver> observer) {
  CEF_API_REQUIRE_ADDED(13401);
  CEF_REQUIRE_UIT_RETURN(nullptr);
  return CefContext::Get()->GetPrefRegistrar()->AddObserver(name, observer);
}

// static
void CefPreferenceManager::GetChromeVariationsAsSwitches(
    std::vector<CefString>& switches) {
  CEF_API_REQUIRE_ADDED(13401);

  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return;
  }

  switches.clear();

  // Based on ChromeFeatureListCreator::ConvertFlagsToSwitches().

  flags_ui::PrefServiceFlagsStorage flags_storage(
      g_browser_process->local_state());
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  about_flags::ConvertFlagsToSwitches(&flags_storage, &command_line,
                                      flags_ui::kNoSentinels);

  for (const auto& arg : command_line.argv()) {
    if (!arg.empty()) {
      switches.push_back(arg);
    }
  }
}

// static
void CefPreferenceManager::GetChromeVariationsAsStrings(
    std::vector<CefString>& strings) {
  CEF_API_REQUIRE_ADDED(13401);

  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return;
  }

  strings.clear();

  // Based on components/webui/version/version_handler_helper.cc
  // GetVariationsList().

  base::FieldTrial::ActiveGroups active_groups;
  // Include low anonymity trial groups in the version string, as it is only
  // displayed locally (and is useful for diagnostics purposes).
  base::FieldTrialListIncludingLowAnonymity::
      GetActiveFieldTrialGroupsForTesting(&active_groups);

  for (const auto& group : active_groups) {
    strings.push_back(GetActiveGroupNameAsString(group));
  }

  // Synthetic field trials.
  for (const auto& group :
       variations::SyntheticTrialsActiveGroupIdProvider::GetInstance()
           ->GetGroups()) {
    strings.push_back(GetActiveGroupNameAsString(group.active_group()));
  }
}

// static
CefRefPtr<CefPreferenceManager>
CefPreferenceManager::GetGlobalPreferenceManager() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return nullptr;
  }

  return new CefGlobalPreferenceManagerImpl();
}
