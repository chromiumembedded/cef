// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/prefs/pref_registrar.h"

#include "base/memory/raw_ptr.h"
#include "cef/include/cef_app.h"
#include "cef/include/cef_browser_process_handler.h"
#include "cef/include/cef_preference.h"
#include "cef/libcef/common/app_manager.h"
#include "cef/libcef/common/values_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_store.h"

namespace pref_registrar {

namespace {

class CefPreferenceRegistrarImpl : public CefPreferenceRegistrar {
 public:
  explicit CefPreferenceRegistrarImpl(PrefRegistrySimple* registry)
      : registry_(registry) {}

  CefPreferenceRegistrarImpl(const CefPreferenceRegistrarImpl&) = delete;
  CefPreferenceRegistrarImpl& operator=(const CefPreferenceRegistrarImpl&) =
      delete;

  // CefPreferenceRegistrar methods.
  bool AddPreference(const CefString& name,
                     CefRefPtr<CefValue> default_value) override {
    const std::string nameStr = name;
    if (registry_->defaults()->GetValue(nameStr, nullptr)) {
      LOG(ERROR) << "Trying to register a previously registered preference: "
                 << nameStr;
      return false;
    }

    switch (default_value->GetType()) {
      case VTYPE_INVALID:
      case VTYPE_NULL:
      case VTYPE_BINARY:
        break;
      case VTYPE_BOOL:
        registry_->RegisterBooleanPref(nameStr, default_value->GetBool());
        return true;
      case VTYPE_INT:
        registry_->RegisterIntegerPref(nameStr, default_value->GetInt());
        return true;
      case VTYPE_DOUBLE:
        registry_->RegisterDoublePref(nameStr, default_value->GetDouble());
        return true;
      case VTYPE_STRING:
        registry_->RegisterStringPref(
            nameStr, std::string_view(default_value->GetString().ToString()));
        return true;
      case VTYPE_DICTIONARY:
      case VTYPE_LIST:
        RegisterComplexPref(nameStr, default_value);
        return true;
      case VTYPE_NUM_VALUES:
        DCHECK(false);
        return false;
    };

    LOG(ERROR) << "Invalid value type for preference: " << nameStr;
    return false;
  }

 private:
  void RegisterComplexPref(const std::string& name,
                           CefRefPtr<CefValue> default_value) {
    CefValueImpl* impl = static_cast<CefValueImpl*>(default_value.get());
    auto impl_value = impl->CopyValue();

    if (impl_value.type() == base::Value::Type::DICT) {
      registry_->RegisterDictionaryPref(std::string_view(name),
                                        std::move(impl_value.GetDict()));
    } else if (impl_value.type() == base::Value::Type::LIST) {
      registry_->RegisterListPref(std::string_view(name),
                                  std::move(impl_value.GetList()));
    } else {
      DCHECK(false);
    }
  }

  const raw_ptr<PrefRegistrySimple> registry_;
};

}  // namespace

void RegisterCustomPrefs(cef_preferences_type_t type,
                         PrefRegistrySimple* registry) {
  if (auto app = CefAppManager::Get()->GetApplication()) {
    if (auto handler = app->GetBrowserProcessHandler()) {
      CefPreferenceRegistrarImpl registrar(registry);
      handler->OnRegisterCustomPreferences(type, &registrar);
    }
  }
}

}  // namespace pref_registrar
