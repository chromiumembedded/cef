// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PREFS_PREF_HELPER_H_
#define CEF_LIBCEF_BROWSER_PREFS_PREF_HELPER_H_

#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "cef/include/cef_registration.h"
#include "cef/include/cef_values.h"
#include "components/prefs/pref_observer.h"

class CefPreferenceObserver;
class CefRegistration;
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

class Registration : public base::CheckedObserver {
 public:
  virtual void Detach() = 0;
  virtual void RunCallback() const = 0;
  virtual void RunCallback(const CefString& name) const = 0;
};

class RegistrationImpl;

// Automatically manages the registration of one or more CefPreferenceObserver
// objects with a PrefService. When the Registrar is destroyed, all registered
// observers are automatically unregistered with the PrefService. Loosely based
// on PrefChangeRegistrar.
class Registrar final : public PrefObserver {
 public:
  Registrar() = default;

  Registrar(const Registrar&) = delete;
  Registrar& operator=(const Registrar&) = delete;

  ~Registrar();

  // Must be called before adding or removing observers. Can be called more
  // than once as long as the value of |service| doesn't change.
  void Init(PrefService* service);

  // Removes all observers and clears the reference to the PrefService.
  // `Init` must be called before adding or removing any observers.
  void Reset();

  // Removes all observers that have been previously added with a call to Add.
  void RemoveAll();

  // Returns true if no observers are registered.
  bool IsEmpty() const;

  // Adds a pref |observer| for the specified pref |name|. All registered
  // observers will be automatically unregistered and detached when the
  // Registrar's destructor is called.
  CefRefPtr<CefRegistration> AddObserver(
      const CefString& name,
      CefRefPtr<CefPreferenceObserver> observer);

 private:
  friend class RegistrationImpl;

  void RemoveObserver(std::string_view name, Registration* registration);

  // PrefObserver:
  void OnPreferenceChanged(PrefService* service,
                           std::string_view pref_name) override;

  raw_ptr<PrefService, AcrossTasksDanglingUntriaged> service_ = nullptr;

  // Observers registered for a preference by name.
  using ObserverMap =
      std::unordered_map<std::string, base::ObserverList<Registration>>;
  ObserverMap name_observers_;

  // Observers registered for all preferences.
  base::ObserverList<Registration> all_observers_;
};

}  // namespace pref_helper

#endif  // CEF_LIBCEF_BROWSER_PREFS_PREF_HELPER_H_
