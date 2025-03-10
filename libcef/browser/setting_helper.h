// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SETTING_HELPER_H_
#define CEF_LIBCEF_BROWSER_SETTING_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "cef/include/cef_registration.h"
#include "components/content_settings/core/browser/content_settings_observer.h"

class CefSettingObserver;
class CefRegistration;
class HostContentSettingsMap;

namespace setting_helper {

class Registration : public base::CheckedObserver {
 public:
  virtual void Detach() = 0;
  virtual void RunCallback(const CefString& requesting_url,
                           const CefString& top_level_url,
                           cef_content_setting_types_t content_type) const = 0;
};

class RegistrationImpl;

// Automatically manages the registration of one or more CefSettingObserver
// objects with a HostContentSettingsMap. When the Registrar is destroyed, all
// registered observers are automatically unregistered with the
// HostContentSettingsMap. Loosely based on PrefChangeRegistrar.
class Registrar final : public content_settings::Observer {
 public:
  Registrar() = default;

  Registrar(const Registrar&) = delete;
  Registrar& operator=(const Registrar&) = delete;

  ~Registrar();

  // Must be called before adding or removing observers. Can be called more
  // than once as long as the value of |settings| doesn't change.
  void Init(HostContentSettingsMap* settings);

  // Removes all observers and clears the reference to the
  // HostContentSettingsMap. `Init` must be called before adding or removing any
  // observers.
  void Reset();

  // Removes all observers that have been previously added with a call to Add.
  void RemoveAll();

  // Returns true if no observers are registered.
  bool IsEmpty() const;

  // Adds a setting observer. All registered observers will be automatically
  // unregistered and detached when the Registrar's destructor is called.
  CefRefPtr<CefRegistration> AddObserver(
      CefRefPtr<CefSettingObserver> observer);

 private:
  friend class RegistrationImpl;

  void RemoveObserver(Registration* registration);

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  raw_ptr<HostContentSettingsMap, AcrossTasksDanglingUntriaged> settings_ =
      nullptr;

  base::ObserverList<Registration> observers_;
};

}  // namespace setting_helper

#endif  // CEF_LIBCEF_BROWSER_SETTING_HELPER_H_
