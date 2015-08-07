// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_pref_store.h"

#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/net/pref_proxy_config_tracker_impl.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "extensions/browser/extension_prefs.h"
#include "grit/cef_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// A helper function for registering localized values.
// Based on CreateLocaleDefaultValue from
// components/pref_registry/pref_registry_syncable.cc.
void RegisterLocalizedValue(PrefRegistrySimple* registry,
                            const char* path,
                            base::Value::Type type,
                            int message_id) {
  const std::string resource_string = l10n_util::GetStringUTF8(message_id);
  DCHECK(!resource_string.empty());
  switch (type) {
    case base::Value::TYPE_BOOLEAN: {
      if ("true" == resource_string)
        registry->RegisterBooleanPref(path, true);
      else if ("false" == resource_string)
        registry->RegisterBooleanPref(path, false);
      return;
    }

    case base::Value::TYPE_INTEGER: {
      int val;
      base::StringToInt(resource_string, &val);
      registry->RegisterIntegerPref(path, val);
      return;
    }

    case base::Value::TYPE_DOUBLE: {
      double val;
      base::StringToDouble(resource_string, &val);
      registry->RegisterDoublePref(path, val);
      return;
    }

    case base::Value::TYPE_STRING: {
      registry->RegisterStringPref(path, resource_string);
      return;
    }

    default: {
      NOTREACHED() <<
          "list and dictionary types cannot have default locale values";
    }
  }
  NOTREACHED();
}

}  // namespace

CefBrowserPrefStore::CefBrowserPrefStore() {
}

scoped_ptr<PrefService> CefBrowserPrefStore::CreateService() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  base::PrefServiceFactory factory;
  factory.set_command_line_prefs(
      new CommandLinePrefStore(command_line));
  factory.set_user_prefs(this);

  scoped_refptr< user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable());

  // Default settings.
  CefMediaCaptureDevicesDispatcher::RegisterPrefs(registry.get());
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry.get());
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry.get());

  // Print settings.
  registry->RegisterBooleanPref(prefs::kPrintingEnabled, true);

  // Spell checking settings.
  // Based on SpellcheckServiceFactory::RegisterProfilePrefs.
  registry->RegisterListPref(prefs::kSpellCheckDictionaries,
                             new base::ListValue);
  std::string spellcheck_lang =
      command_line->GetSwitchValueASCII(switches::kOverrideSpellCheckLang);
  if (!spellcheck_lang.empty()) {
    registry->RegisterStringPref(prefs::kSpellCheckDictionary, spellcheck_lang);
  } else {
    RegisterLocalizedValue(registry.get(),
                           prefs::kSpellCheckDictionary,
                           base::Value::TYPE_STRING,
                           IDS_SPELLCHECK_DICTIONARY);
  }
  registry->RegisterBooleanPref(prefs::kSpellCheckUseSpellingService,
      command_line->HasSwitch(switches::kEnableSpellingService));
  registry->RegisterBooleanPref(prefs::kEnableContinuousSpellcheck, true);
  // The kEnableSpellingAutoCorrect command-line value is also checked in
  // SpellCheckProvider::autoCorrectWord.
  registry->RegisterBooleanPref(prefs::kEnableAutoSpellCorrect,
      command_line->HasSwitch(switches::kEnableSpellingAutoCorrect));

  // Pepper flash settings.
  // Based on DeviceIDFetcher::RegisterProfilePrefs.
  registry->RegisterBooleanPref(prefs::kEnableDRM, false);
  registry->RegisterStringPref(prefs::kDRMSalt, "");

  return factory.Create(registry.get());
}

CefBrowserPrefStore::~CefBrowserPrefStore() {
}
