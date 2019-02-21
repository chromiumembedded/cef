// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/prefs/browser_prefs.h"

#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/net/url_request_context_getter_impl.h"
#include "libcef/browser/prefs/pref_store.h"
#include "libcef/browser/prefs/renderer_prefs.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/accessibility/accessibility_ui.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/plugins/plugin_info_host_impl.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/browser/renderer_host/pepper/device_id_fetcher.h"
#include "chrome/browser/supervised_user/supervised_user_pref_store.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/locale_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_factory.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

namespace browser_prefs {

const char kUserPrefsFileName[] = "UserPrefs.json";

std::unique_ptr<PrefService> CreatePrefService(Profile* profile,
                                               const base::FilePath& cache_path,
                                               bool persist_user_preferences) {
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
  const bool store_on_disk = !cache_path.empty() && persist_user_preferences;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner;
  if (store_on_disk) {
    // Get sequenced task runner for making sure that file operations are
    // executed in expected order (what was previously assured by the FILE
    // thread).
    sequenced_task_runner = base::CreateSequencedTaskRunnerWithTraits(
        {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }

  // Used to store user preferences.
  scoped_refptr<PersistentPrefStore> user_pref_store;
  if (store_on_disk) {
    const base::FilePath& pref_path =
        cache_path.AppendASCII(browser_prefs::kUserPrefsFileName);
    scoped_refptr<JsonPrefStore> json_pref_store = new JsonPrefStore(
        pref_path, std::unique_ptr<PrefFilter>(), sequenced_task_runner);
    factory.set_user_prefs(json_pref_store.get());
  } else {
    scoped_refptr<CefPrefStore> cef_pref_store = new CefPrefStore();
    cef_pref_store->SetInitializationCompleted();
    factory.set_user_prefs(cef_pref_store.get());
  }

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Used to store supervised user preferences.
  SupervisedUserSettingsService* supervised_user_settings =
      SupervisedUserSettingsServiceFactory::GetForProfile(profile);

  if (store_on_disk) {
    supervised_user_settings->Init(cache_path, sequenced_task_runner.get(),
                                   true);
  } else {
    scoped_refptr<CefPrefStore> cef_pref_store = new CefPrefStore();
    cef_pref_store->SetInitializationCompleted();
    supervised_user_settings->Init(cef_pref_store);
  }

  scoped_refptr<PrefStore> supervised_user_prefs =
      base::MakeRefCounted<SupervisedUserPrefStore>(supervised_user_settings);
  DCHECK(supervised_user_prefs->IsInitializationComplete());
  factory.set_supervised_user_prefs(supervised_user_prefs);
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

  // Registry that will be populated with all known preferences. Preferences
  // are registered with default values that may be changed via a *PrefStore.
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable());

  // Some preferences are specific to CEF and others are defined in Chromium.
  // The preferred approach for registering preferences defined in Chromium is
  // as follows:
  //
  // 1. If a non-static RegisterProfilePrefs() method exists in a *Factory
  //    class then add a *Factory::GetInstance() call in
  //    EnsureBrowserContextKeyedServiceFactoriesBuilt().
  // 2. If a static RegisterPrefs() method exists then call that method in the
  //    "Default preferences" section below.
  // 3. If the default values are not appropriate but the set of registered
  //    preferences is otherwise fine then change the defaults by calling
  //    SetDefaultPrefValue after calling the existing registration method.
  // 4. If the original registration method contains many unused preferences or
  //    otherwise inappropiate logic (e.g. calls to objects that CEF doesn't
  //    use) then register the preferences directly instead of calling the
  //    existing registration method.

  // Call RegisterProfilePrefs() for all services listed by
  // EnsureBrowserContextKeyedServiceFactoriesBuilt().
  BrowserContextDependencyManager::GetInstance()
      ->RegisterProfilePrefsForServices(profile, registry.get());

  // Default preferences.
  AccessibilityUIMessageHandler::RegisterProfilePrefs(registry.get());
  CefMediaCaptureDevicesDispatcher::RegisterPrefs(registry.get());
  CefURLRequestContextGetterImpl::RegisterPrefs(registry.get());
  chrome_browser_net::RegisterPredictionOptionsProfilePrefs(registry.get());
  DeviceIDFetcher::RegisterProfilePrefs(registry.get());
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry.get());
  GoogleURLTracker::RegisterProfilePrefs(registry.get());
  HostContentSettingsMap::RegisterProfilePrefs(registry.get());
  language::RegisterProfilePrefs(registry.get());
  PluginInfoHostImpl::RegisterUserPrefs(registry.get());
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry.get());
  renderer_prefs::RegisterProfilePrefs(registry.get());
  update_client::RegisterPrefs(registry.get());

  // Print preferences.
  // Based on ProfileImpl::RegisterProfilePrefs.
  registry->RegisterBooleanPref(prefs::kPrintingEnabled, true);

  // Spell checking preferences.
  // Modify defaults from SpellcheckServiceFactory::RegisterProfilePrefs.
  std::string spellcheck_lang =
      command_line->GetSwitchValueASCII(switches::kOverrideSpellCheckLang);
  if (!spellcheck_lang.empty()) {
    registry->SetDefaultPrefValue(spellcheck::prefs::kSpellCheckDictionary,
                                  base::Value(spellcheck_lang));
  }
  const bool enable_spelling_service_ =
      command_line->HasSwitch(switches::kEnableSpellingService);
  registry->SetDefaultPrefValue(
      spellcheck::prefs::kSpellCheckUseSpellingService,
      base::Value(enable_spelling_service_));
  registry->SetDefaultPrefValue(spellcheck::prefs::kSpellCheckEnable,
                                base::Value(!enable_spelling_service_));

  // Pepper flash preferences.
  // Modify defaults from DeviceIDFetcher::RegisterProfilePrefs.
  registry->SetDefaultPrefValue(prefs::kEnableDRM, base::Value(false));

  // Authentication preferences.
  // Based on IOThread::RegisterPrefs.
  registry->RegisterStringPref(prefs::kAuthServerWhitelist, "");
  registry->RegisterStringPref(prefs::kAuthNegotiateDelegateWhitelist, "");

  // Browser UI preferences.
  // Based on chrome/browser/ui/browser_ui_prefs.cc RegisterBrowserPrefs.
  registry->RegisterBooleanPref(prefs::kAllowFileSelectionDialogs, true);

  // DevTools preferences.
  // Based on DevToolsWindow::RegisterProfilePrefs.
  registry->RegisterDictionaryPref(prefs::kDevToolsPreferences);
  registry->RegisterDictionaryPref(prefs::kDevToolsEditedFiles);

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
