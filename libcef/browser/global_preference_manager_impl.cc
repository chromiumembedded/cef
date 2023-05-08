// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/global_preference_manager_impl.h"

#include "libcef/browser/context.h"
#include "libcef/browser/prefs/pref_helper.h"
#include "libcef/browser/thread_util.h"

#include "chrome/browser/browser_process.h"

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
