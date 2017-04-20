// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/prefs/browser_prefs.h"

#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/net/url_request_context_getter_impl.h"
#include "libcef/browser/prefs/renderer_prefs.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/browser/supervised_user/supervised_user_pref_store.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/locale_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_factory.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/features/features.h"
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
    case base::Value::Type::BOOLEAN: {
      if ("true" == resource_string)
        registry->RegisterBooleanPref(path, true);
      else if ("false" == resource_string)
        registry->RegisterBooleanPref(path, false);
      return;
    }

    case base::Value::Type::INTEGER: {
      int val;
      base::StringToInt(resource_string, &val);
      registry->RegisterIntegerPref(path, val);
      return;
    }

    case base::Value::Type::DOUBLE: {
      double val;
      base::StringToDouble(resource_string, &val);
      registry->RegisterDoublePref(path, val);
      return;
    }

    case base::Value::Type::STRING: {
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

std::unique_ptr<PrefService> CreatePrefService(
    Profile* profile,
    const base::FilePath& cache_path,
    bool persist_user_preferences,
    bool is_pre_initialization) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Use of PrefServiceSyncable is required by Chrome code such as
  // HostContentSettingsMapFactory that calls PrefServiceSyncableFromProfile.
  sync_preferences::PrefServiceSyncableFactory factory;

  // Used to store command-line preferences, most of which will be evaluated in
  // the CommandLinePrefStore constructor. Preferences set in this manner cannot
  // be overridden by the user.
  scoped_refptr<ChromeCommandLinePrefStore> command_line_pref_store(
      new ChromeCommandLinePrefStore(command_line));
  renderer_prefs::SetCommandLinePrefDefaults(command_line_pref_store.get());
  factory.set_command_line_prefs(command_line_pref_store);

  // True if preferences will be stored on disk.
  const bool store_on_disk = !cache_path.empty() && persist_user_preferences &&
                             !is_pre_initialization;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner;
  if (store_on_disk) {
    // Get sequenced task runner for making sure that file operations of
    // this profile (defined by |cache_path|) are executed in expected order
    // (what was previously assured by the FILE thread).
    sequenced_task_runner = JsonPrefStore::GetTaskRunnerForFile(
        cache_path, content::BrowserThread::GetBlockingPool());
  }

  // Used to store user preferences.
  scoped_refptr<PersistentPrefStore> user_pref_store;
  if (store_on_disk) {
    const base::FilePath& pref_path =
        cache_path.AppendASCII(browser_prefs::kUserPrefsFileName);
    scoped_refptr<JsonPrefStore> json_pref_store =
        new JsonPrefStore(pref_path, sequenced_task_runner,
                          std::unique_ptr<PrefFilter>());
    factory.set_user_prefs(json_pref_store.get());
  } else {
    scoped_refptr<TestingPrefStore> testing_pref_store = new TestingPrefStore();
    testing_pref_store->SetInitializationCompleted();
    factory.set_user_prefs(testing_pref_store.get());
  }

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Don't access factories during pre-initialization.
  if (!is_pre_initialization) {
    // Used to store supervised user preferences.
    SupervisedUserSettingsService* supervised_user_settings =
        SupervisedUserSettingsServiceFactory::GetForProfile(profile);

    if (store_on_disk) {
      supervised_user_settings->Init(
          cache_path, sequenced_task_runner.get(), true);
    } else {
      scoped_refptr<TestingPrefStore> testing_pref_store =
          new TestingPrefStore();
      testing_pref_store->SetInitializationCompleted();
      supervised_user_settings->Init(testing_pref_store);
    }

    scoped_refptr<PrefStore> supervised_user_prefs = make_scoped_refptr(
        new SupervisedUserPrefStore(supervised_user_settings));
    DCHECK(supervised_user_prefs->IsInitializationComplete());
    factory.set_supervised_user_prefs(supervised_user_prefs);
  }
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

  // Registry that will be populated with all known preferences. Preferences
  // are registered with default values that may be changed via a *PrefStore.
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable());

  // Default preferences.
  CefMediaCaptureDevicesDispatcher::RegisterPrefs(registry.get());
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry.get());
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry.get());
  HostContentSettingsMap::RegisterProfilePrefs(registry.get());
  CefURLRequestContextGetterImpl::RegisterPrefs(registry.get());
  renderer_prefs::RegisterProfilePrefs(registry.get());
  update_client::RegisterPrefs(registry.get());
  content_settings::CookieSettings::RegisterProfilePrefs(registry.get());
  chrome_browser_net::RegisterPredictionOptionsProfilePrefs(registry.get());

  // Print preferences.
  registry->RegisterBooleanPref(prefs::kPrintingEnabled, true);

  // Spell checking preferences.
  // Based on SpellcheckServiceFactory::RegisterProfilePrefs.
  registry->RegisterListPref(spellcheck::prefs::kSpellCheckDictionaries,
                             base::MakeUnique<base::ListValue>());
  std::string spellcheck_lang =
      command_line->GetSwitchValueASCII(switches::kOverrideSpellCheckLang);
  if (!spellcheck_lang.empty()) {
    registry->RegisterStringPref(spellcheck::prefs::kSpellCheckDictionary,
                                 spellcheck_lang);
  } else {
    RegisterLocalizedValue(registry.get(),
                           spellcheck::prefs::kSpellCheckDictionary,
                           base::Value::Type::STRING,
                           IDS_SPELLCHECK_DICTIONARY);
  }
  registry->RegisterBooleanPref(
      spellcheck::prefs::kSpellCheckUseSpellingService,
      command_line->HasSwitch(switches::kEnableSpellingService));
  registry->RegisterBooleanPref(
      spellcheck::prefs::kEnableSpellcheck,
      !command_line->HasSwitch(switches::kDisableSpellChecking));

  // Pepper flash preferences.
  // Based on DeviceIDFetcher::RegisterProfilePrefs.
  registry->RegisterBooleanPref(prefs::kEnableDRM, false);
  registry->RegisterStringPref(prefs::kDRMSalt, "");

  // Authentication preferences.
  registry->RegisterStringPref(prefs::kAuthServerWhitelist, "");
  registry->RegisterStringPref(prefs::kAuthNegotiateDelegateWhitelist, "");

  // Plugin preferences.
  // Based on chrome::RegisterBrowserUserPrefs.
  registry->RegisterBooleanPref(prefs::kPluginsAllowOutdated, false);
  registry->RegisterBooleanPref(prefs::kPluginsAlwaysAuthorize, false);

  // Theme preferences.
  // Based on ThemeServiceFactory::RegisterProfilePrefs.
  // TODO(cef/chrome): Remove this once CEF supports ThemeService.
#if defined(USE_X11)
  registry->RegisterBooleanPref(prefs::kUsesSystemTheme, false);
#endif
  registry->RegisterFilePathPref(prefs::kCurrentThemePackFilename,
                                 base::FilePath());
  registry->RegisterStringPref(prefs::kCurrentThemeID,
                               ThemeService::kDefaultThemeID);
  registry->RegisterDictionaryPref(prefs::kCurrentThemeImages);
  registry->RegisterDictionaryPref(prefs::kCurrentThemeColors);
  registry->RegisterDictionaryPref(prefs::kCurrentThemeTints);
  registry->RegisterDictionaryPref(prefs::kCurrentThemeDisplayProperties);

  // Browser UI preferences.
  // Based on chrome/browser/ui/browser_ui_prefs.cc RegisterBrowserPrefs.
  registry->RegisterBooleanPref(prefs::kAllowFileSelectionDialogs, true);

  // DevTools preferences.
  // Based on DevToolsWindow::RegisterProfilePrefs.
  registry->RegisterDictionaryPref(prefs::kDevToolsPreferences);

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
  return factory.CreateSyncable(registry.get());
}

}  // namespace browser_prefs
