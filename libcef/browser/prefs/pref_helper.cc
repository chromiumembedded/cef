// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/prefs/pref_helper.h"

#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "cef/include/cef_preference.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/values_impl.h"
#include "components/prefs/pref_service.h"

namespace pref_helper {

namespace {

const char* GetTypeString(base::Value::Type type) {
  switch (type) {
    case base::Value::Type::NONE:
      return "NULL";
    case base::Value::Type::BOOLEAN:
      return "BOOLEAN";
    case base::Value::Type::INTEGER:
      return "INTEGER";
    case base::Value::Type::DOUBLE:
      return "DOUBLE";
    case base::Value::Type::STRING:
      return "STRING";
    case base::Value::Type::BINARY:
      return "BINARY";
    case base::Value::Type::DICT:
      return "DICTIONARY";
    case base::Value::Type::LIST:
      return "LIST";
  }

  DCHECK(false);
  return "UNKNOWN";
}

}  // namespace

bool HasPreference(PrefService* pref_service, const CefString& name) {
  return (pref_service->FindPreference(std::string_view(name.ToString())) !=
          nullptr);
}

CefRefPtr<CefValue> GetPreference(PrefService* pref_service,
                                  const CefString& name) {
  const PrefService::Preference* pref =
      pref_service->FindPreference(std::string_view(name.ToString()));
  if (!pref) {
    return nullptr;
  }
  return new CefValueImpl(pref->GetValue()->Clone());
}

CefRefPtr<CefDictionaryValue> GetAllPreferences(PrefService* pref_service,
                                                bool include_defaults) {
  // Returns a DeepCopy of the value.
  auto values = pref_service->GetPreferenceValues(
      include_defaults ? PrefService::INCLUDE_DEFAULTS
                       : PrefService::EXCLUDE_DEFAULTS);

  // CefDictionaryValueImpl takes ownership of |values| contents.
  return new CefDictionaryValueImpl(std::move(values), /*read_only=*/false);
}

bool CanSetPreference(PrefService* pref_service, const CefString& name) {
  const PrefService::Preference* pref =
      pref_service->FindPreference(std::string_view(name.ToString()));
  return (pref && pref->IsUserModifiable());
}

bool SetPreference(PrefService* pref_service,
                   const CefString& name,
                   CefRefPtr<CefValue> value,
                   CefString& error) {
  // The below validation logic should match PrefService::SetUserPrefValue.

  const PrefService::Preference* pref =
      pref_service->FindPreference(std::string_view(name.ToString()));
  if (!pref) {
    error = "Trying to modify an unregistered preference";
    return false;
  }

  if (!pref->IsUserModifiable()) {
    error = "Trying to modify a preference that is not user modifiable";
    return false;
  }

  if (!value.get()) {
    // Reset the preference to its default value.
    pref_service->ClearPref(std::string_view(name.ToString()));
    return true;
  }

  if (!value->IsValid()) {
    error = "A valid value is required";
    return false;
  }

  CefValueImpl* impl = static_cast<CefValueImpl*>(value.get());

  CefValueImpl::ScopedLockedValue scoped_locked_value(impl);
  base::Value* impl_value = impl->GetValueUnsafe();

  if (pref->GetType() != impl_value->type()) {
    error = base::StringPrintf(
        "Trying to set a preference of type %s to value of type %s",
        GetTypeString(pref->GetType()), GetTypeString(impl_value->type()));
    return false;
  }

  // PrefService will make a DeepCopy of |impl_value|.
  pref_service->Set(std::string_view(name.ToString()), *impl_value);
  return true;
}

class RegistrationImpl final : public Registration, public CefRegistration {
 public:
  RegistrationImpl(Registrar* registrar,
                   const CefString& name,
                   CefRefPtr<CefPreferenceObserver> observer)
      : registrar_(registrar), name_(name), observer_(observer) {
    DCHECK(registrar_);
    DCHECK(observer_);
  }

  RegistrationImpl(const RegistrationImpl&) = delete;
  RegistrationImpl& operator=(const RegistrationImpl&) = delete;

  ~RegistrationImpl() override {
    CEF_REQUIRE_UIT();
    if (registrar_) {
      registrar_->RemoveObserver(name_.ToString(), this);
    }
  }

  void Detach() override {
    registrar_ = nullptr;
    observer_ = nullptr;
  }

  void RunCallback() const override { RunCallback(name_); }

  void RunCallback(const CefString& name) const override {
    observer_->OnPreferenceChanged(name);
  }

 private:
  raw_ptr<Registrar> registrar_;
  CefString name_;
  CefRefPtr<CefPreferenceObserver> observer_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(RegistrationImpl);
};

Registrar::~Registrar() {
  RemoveAll();
}

void Registrar::Init(PrefService* service) {
  DCHECK(service);
  DCHECK(IsEmpty() || service_ == service);
  service_ = service;
}

void Registrar::Reset() {
  RemoveAll();
  service_ = nullptr;
}

void Registrar::RemoveAll() {
  if (!name_observers_.empty()) {
    for (auto& [name, registrations] : name_observers_) {
      service_->RemovePrefObserver(name, this);
      for (auto& registration : registrations) {
        registration.Detach();
      }
    }
    name_observers_.clear();
  }

  if (!all_observers_.empty()) {
    service_->RemovePrefObserverAllPrefs(this);
    for (auto& registration : all_observers_) {
      registration.Detach();
    }
    all_observers_.Clear();
  }
}

bool Registrar::IsEmpty() const {
  return name_observers_.empty() && all_observers_.empty();
}

CefRefPtr<CefRegistration> Registrar::AddObserver(
    const CefString& name,
    CefRefPtr<CefPreferenceObserver> observer) {
  CHECK(service_);

  RegistrationImpl* impl = new RegistrationImpl(this, name, observer);

  if (name.empty()) {
    if (all_observers_.empty()) {
      service_->AddPrefObserverAllPrefs(this);
    }
    all_observers_.AddObserver(impl);
  } else {
    const std::string& name_str = name.ToString();
    if (!name_observers_.contains(name_str)) {
      service_->AddPrefObserver(name_str, this);
    }
    name_observers_[name_str].AddObserver(impl);
  }

  return impl;
}

void Registrar::RemoveObserver(std::string_view name,
                               Registration* registration) {
  CHECK(service_);

  if (name.empty()) {
    all_observers_.RemoveObserver(registration);
    if (all_observers_.empty()) {
      service_->RemovePrefObserverAllPrefs(this);
    }
  } else {
    auto it = name_observers_.find(std::string(name));
    DCHECK(it != name_observers_.end());
    it->second.RemoveObserver(registration);
    if (it->second.empty()) {
      name_observers_.erase(it);
      service_->RemovePrefObserver(name, this);
    }
  }
}

void Registrar::OnPreferenceChanged(PrefService* service,
                                    std::string_view pref_name) {
  std::string pref_name_str(pref_name);
  if (!name_observers_.empty()) {
    auto it = name_observers_.find(pref_name_str);
    if (it != name_observers_.end()) {
      for (Registration& registration : it->second) {
        registration.RunCallback();
      }
    }
  }

  if (!all_observers_.empty()) {
    CefString name_str(pref_name_str);
    for (Registration& registration : all_observers_) {
      registration.RunCallback(name_str);
    }
  }
}

}  // namespace pref_helper
