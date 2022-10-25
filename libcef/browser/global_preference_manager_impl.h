// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_GLOBAL_PREFERENCE_MANAGER_IMPL_H_
#define CEF_LIBCEF_BROWSER_GLOBAL_PREFERENCE_MANAGER_IMPL_H_
#pragma once

#include "include/cef_preference.h"

// Implementation of the CefPreferenceManager interface for global preferences.
class CefGlobalPreferenceManagerImpl : public CefPreferenceManager {
 public:
  CefGlobalPreferenceManagerImpl() = default;

  CefGlobalPreferenceManagerImpl(const CefGlobalPreferenceManagerImpl&) =
      delete;
  CefGlobalPreferenceManagerImpl& operator=(
      const CefGlobalPreferenceManagerImpl&) = delete;

  // CefPreferenceManager methods.
  bool HasPreference(const CefString& name) override;
  CefRefPtr<CefValue> GetPreference(const CefString& name) override;
  CefRefPtr<CefDictionaryValue> GetAllPreferences(
      bool include_defaults) override;
  bool CanSetPreference(const CefString& name) override;
  bool SetPreference(const CefString& name,
                     CefRefPtr<CefValue> value,
                     CefString& error) override;

 private:
  IMPLEMENT_REFCOUNTING(CefGlobalPreferenceManagerImpl);
};

#endif  // CEF_LIBCEF_BROWSER_GLOBAL_PREFERENCE_MANAGER_IMPL_H_
