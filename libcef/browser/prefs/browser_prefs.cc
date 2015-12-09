// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/prefs/browser_prefs.h"

#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/prefs/renderer_prefs.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_filter.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/testing_pref_store.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "grit/cef_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace browser_prefs {

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

const char kUserPrefsFileName[] = "UserPrefs.json";

scoped_ptr<PrefService> CreatePrefService(const base::FilePath& pref_path) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  base::PrefServiceFactory factory;

  // Used to store command-line preferences, most of which will be evaluated in
  // the CommandLinePrefStore constructor. Preferences set in this manner cannot
  // be overridden by the user.
  scoped_refptr<CommandLinePrefStore> command_line_pref_store(
      new CommandLinePrefStore(command_line));
  renderer_prefs::SetCommandLinePrefDefaults(command_line_pref_store.get());
  factory.set_command_line_prefs(command_line_pref_store);

  // Used to store user preferences.
  scoped_refptr<PersistentPrefStore> user_pref_store;
  if (!pref_path.empty()) {
    // Store preferences on disk.
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        JsonPrefStore::GetTaskRunnerForFile(
            pref_path,
            content::BrowserThread::GetBlockingPool());
    scoped_refptr<JsonPrefStore> json_pref_store =
        new JsonPrefStore(pref_path, task_runner, scoped_ptr<PrefFilter>());
    factory.set_user_prefs(json_pref_store.get());
  } else {
    // Store preferences in memory.
    scoped_refptr<TestingPrefStore> testing_pref_store = new TestingPrefStore();
    testing_pref_store->SetInitializationCompleted();
    factory.set_user_prefs(testing_pref_store.get());
  }

  // Registry that will be populated with all known preferences. Preferences
  // are registered with default values that may be changed via a *PrefStore.
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable());

  // Default preferences.
  CefMediaCaptureDevicesDispatcher::RegisterPrefs(registry.get());
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry.get());
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry.get());
  HostContentSettingsMap::RegisterProfilePrefs(registry.get());
  renderer_prefs::RegisterProfilePrefs(registry.get());

  // Print preferences.
  registry->RegisterBooleanPref(prefs::kPrintingEnabled, true);

  // Spell checking preferences.
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

  // Pepper flash preferences.
  // Based on DeviceIDFetcher::RegisterProfilePrefs.
  registry->RegisterBooleanPref(prefs::kEnableDRM, false);
  registry->RegisterStringPref(prefs::kDRMSalt, "");

  // Plugin preferences.
  // Based on chrome::RegisterBrowserUserPrefs.
  registry->RegisterBooleanPref(prefs::kPluginsAllowOutdated, false);
  registry->RegisterBooleanPref(prefs::kPluginsAlwaysAuthorize, false);

  // Network preferences.
  // Based on IOThread::RegisterPrefs.
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  registry->RegisterStringPref(prefs::kGSSAPILibraryName, std::string());
#endif

  if (command_line->HasSwitch(switches::kEnablePreferenceTesting)) {
    // Preferences used with unit tests.
    registry->RegisterBooleanPref("test.bool", true);
    registry->RegisterIntegerPref("test.int", 2);
    registry->RegisterDoublePref("test.double", 5.0);
    registry->RegisterStringPref("test.string", "default");
    registry->RegisterListPref("test.list");
    registry->RegisterDictionaryPref("test.dict");
  }

  // Build the PrefService that manages the PrefRegistry and PrefStores.
  return factory.Create(registry.get());
}

}  // namespace browser_prefs
