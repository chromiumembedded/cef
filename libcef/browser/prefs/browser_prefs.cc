// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/prefs/browser_prefs.h"

#include "libcef/browser/browser_context.h"
#include "libcef/browser/context.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/prefs/pref_registrar.h"
#include "libcef/browser/prefs/pref_store.h"
#include "libcef/browser/prefs/renderer_prefs.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/webui/accessibility/accessibility_ui.h"
#include "chrome/browser/ui/webui/print_preview/policy_settings.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/locale_settings.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/domain_reliability/domain_reliability_prefs.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/media_device_salt/media_device_id_salt.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_hats_trigger_helper.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_search_api/safe_search_util.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_factory.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/buildflags/buildflags.h"
#include "net/http/http_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_WIN)
#include "components/os_crypt/sync/os_crypt.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_pref_store.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#endif

namespace browser_prefs {

namespace {

// Match the logic in chrome/browser/net/profile_network_context_service.cc.
std::string ComputeAcceptLanguageFromPref(const std::string& language_pref) {
  std::string accept_languages_str =
      net::HttpUtil::ExpandLanguageList(language_pref);
  return net::HttpUtil::GenerateAcceptLanguageHeader(accept_languages_str);
}

// Return the most relevant setting based on |profile|.
std::string GetAcceptLanguageListSetting(Profile* profile) {
  if (auto* browser_context = CefBrowserContext::FromProfile(profile)) {
    const auto& settings = browser_context->settings();
    if (settings.accept_language_list.length > 0) {
      return CefString(&settings.accept_language_list);
    }
  }

  const auto& settings = CefContext::Get()->settings();
  if (settings.accept_language_list.length > 0) {
    return CefString(&settings.accept_language_list);
  }

  return std::string();
}

}  // namespace

const char kUserPrefsFileName[] = "UserPrefs.json";
const char kLocalPrefsFileName[] = "LocalPrefs.json";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  pref_registrar::RegisterCustomPrefs(CEF_PREFERENCES_TYPE_GLOBAL, registry);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  pref_registrar::RegisterCustomPrefs(CEF_PREFERENCES_TYPE_REQUEST_CONTEXT,
                                      registry);
}

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
    sequenced_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }

  // Used to store user preferences.
  scoped_refptr<PersistentPrefStore> user_pref_store;
  if (store_on_disk) {
    const base::FilePath& pref_path = cache_path.AppendASCII(
        profile ? kUserPrefsFileName : kLocalPrefsFileName);
    scoped_refptr<JsonPrefStore> json_pref_store = new JsonPrefStore(
        pref_path, std::unique_ptr<PrefFilter>(), sequenced_task_runner);
    factory.set_user_prefs(json_pref_store.get());
  } else {
    scoped_refptr<CefPrefStore> cef_pref_store = new CefPrefStore();
    cef_pref_store->SetInitializationCompleted();
    factory.set_user_prefs(cef_pref_store.get());
  }

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (profile) {
    // Used to store supervised user preferences.
    auto* supervised_user_settings =
        SupervisedUserSettingsServiceFactory::GetForKey(
            profile->GetProfileKey());

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
  }
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

  // Default preferences.
  CefMediaCaptureDevicesDispatcher::RegisterPrefs(registry.get());
  certificate_transparency::prefs::RegisterPrefs(registry.get());
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(registry.get());
  media_router::RegisterLocalStatePrefs(registry.get());
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry.get());
  ProfileNetworkContextService::RegisterLocalStatePrefs(registry.get());
  SSLConfigServiceManager::RegisterPrefs(registry.get());
  update_client::RegisterPrefs(registry.get());

  if (!profile) {
    component_updater::RegisterComponentUpdateServicePrefs(registry.get());
    domain_reliability::RegisterPrefs(registry.get());
    SystemNetworkContextManager::RegisterPrefs(registry.get());
#if BUILDFLAG(IS_WIN)
    OSCrypt::RegisterLocalPrefs(registry.get());
#endif
  }

  // Browser process preferences.
  // Based on chrome/browser/browser_process_impl.cc RegisterPrefs.
  registry->RegisterBooleanPref(prefs::kAllowCrossOriginAuthPrompt, false);

  // Browser UI preferences.
  // Based on chrome/browser/ui/browser_ui_prefs.cc RegisterBrowserPrefs.
  registry->RegisterBooleanPref(prefs::kAllowFileSelectionDialogs, true);

  // Based on chrome/browser/ui/browser_ui_prefs.cc RegisterBrowserUserPrefs.
  registry->RegisterBooleanPref(prefs::kPrintPreviewUseSystemDefaultPrinter,
                                false);

  // Profile preferences.
  // Based on chrome/browser/profiles/profiles_state.cc RegisterPrefs.
  registry->RegisterStringPref(prefs::kProfileLastUsed, std::string());

  if (profile) {
    // Call RegisterProfilePrefs() for all services listed by
    // EnsureBrowserContextKeyedServiceFactoriesBuilt().
    BrowserContextDependencyManager::GetInstance()
        ->RegisterProfilePrefsForServices(registry.get());

    // Default profile preferences.
    AccessibilityUIMessageHandler::RegisterProfilePrefs(registry.get());
    extensions::ExtensionPrefs::RegisterProfilePrefs(registry.get());
    HostContentSettingsMap::RegisterProfilePrefs(registry.get());
    language::LanguagePrefs::RegisterProfilePrefs(registry.get());
    media_router::RegisterAccessCodeProfilePrefs(registry.get());
    media_router::RegisterProfilePrefs(registry.get());
    media_device_salt::MediaDeviceIDSalt::RegisterProfilePrefs(registry.get());
    PermissionBubbleMediaAccessHandler::RegisterProfilePrefs(registry.get());
    permissions::PermissionActionsHistory::RegisterProfilePrefs(registry.get());
    permissions::PermissionHatsTriggerHelper::RegisterProfilePrefs(
        registry.get());
    prefetch::RegisterPredictionOptionsProfilePrefs(registry.get());
    privacy_sandbox::RegisterProfilePrefs(registry.get());
    ProfileNetworkContextService::RegisterProfilePrefs(registry.get());
    safe_browsing::RegisterProfilePrefs(registry.get());
    unified_consent::UnifiedConsentService::RegisterPrefs(registry.get());

    const std::string& locale =
        command_line->GetSwitchValueASCII(switches::kLang);
    DCHECK(!locale.empty());
    renderer_prefs::RegisterProfilePrefs(registry.get(), locale);

    // Print preferences.
    // Based on ProfileImpl::RegisterProfilePrefs.
    registry->RegisterBooleanPref(policy::policy_prefs::kForceGoogleSafeSearch,
                                  false);
    registry->RegisterIntegerPref(policy::policy_prefs::kForceYouTubeRestrict,
                                  safe_search_api::YOUTUBE_RESTRICT_OFF);
    registry->RegisterStringPref(prefs::kAllowedDomainsForApps, std::string());
    registry->RegisterBooleanPref(prefs::kPrintingEnabled, true);
    registry->RegisterBooleanPref(prefs::kPrintPreviewDisabled,
                                  !extensions::PrintPreviewEnabled());
    registry->RegisterStringPref(
        prefs::kPrintPreviewDefaultDestinationSelectionRules, std::string());
    registry->RegisterBooleanPref(prefs::kEnableMediaRouter, true);
    printing::PolicySettings::RegisterProfilePrefs(registry.get());
    printing::PrintPreviewStickySettings::RegisterProfilePrefs(registry.get());
    DownloadPrefs::RegisterProfilePrefs(registry.get());

    // Cache preferences.
    // Based on ProfileImpl::RegisterProfilePrefs.
    registry->RegisterFilePathPref(prefs::kDiskCacheDir, cache_path);
    registry->RegisterIntegerPref(prefs::kDiskCacheSize, 0);

    // Based on Profile::RegisterProfilePrefs.
    registry->RegisterBooleanPref(prefs::kSearchSuggestEnabled, false);
    registry->RegisterStringPref(prefs::kSessionExitType, std::string());

    // Based on ChromeContentBrowserClient::RegisterProfilePrefs.
    registry->RegisterBooleanPref(
        prefs::kAccessControlAllowMethodsInCORSPreflightSpecConformant, true);

    // Based on browser_prefs::RegisterProfilePrefs.
    registry->RegisterBooleanPref(prefs::kAccessibilityPdfOcrAlwaysActive,
                                  false);
    registry->RegisterBooleanPref(prefs::kBlockTruncatedCookies, true);

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

    // DevTools preferences.
    // Based on DevToolsWindow::RegisterProfilePrefs.
    registry->RegisterDictionaryPref(prefs::kDevToolsPreferences);
    registry->RegisterDictionaryPref(prefs::kDevToolsEditedFiles);

    // Language preferences. Used by ProfileNetworkContextService and
    // InterceptedRequestHandlerWrapper.
    const std::string& accept_language_list =
        GetAcceptLanguageListSetting(profile);
    if (!accept_language_list.empty()) {
      registry->SetDefaultPrefValue(language::prefs::kAcceptLanguages,
                                    base::Value(accept_language_list));
    }
    registry->RegisterListPref(prefs::kWebRtcLocalIpsAllowedUrls);

    // Always do this after all other profile prefs.
    RegisterProfilePrefs(registry.get());
  } else {
    // Always do this after all other local state prefs.
    RegisterLocalStatePrefs(registry.get());
  }

  // Build the PrefService that manages the PrefRegistry and PrefStores.
  return factory.CreateSyncable(registry.get());
}

std::string GetAcceptLanguageList(Profile* profile) {
  // Always prefer to the CEF settings configuration, if specified.
  std::string accept_language_list = GetAcceptLanguageListSetting(profile);
  if (accept_language_list.empty() && profile) {
    // Fall back to the preference value. For the Alloy runtime the default
    // value comes from browser_prefs::CreatePrefService() above. For the Chrome
    // runtime the default value comes from the configured locale
    // (IDS_ACCEPT_LANGUAGES) which is then overridden by the user preference in
    // chrome://settings/languages, all managed by language::LanguagePrefs.
    accept_language_list =
        profile->GetPrefs()->GetString(language::prefs::kAcceptLanguages);
  }

  if (!accept_language_list.empty()) {
    return ComputeAcceptLanguageFromPref(accept_language_list);
  }
  return std::string();
}

void SetInitialProfilePrefs(Profile* profile) {
  auto* prefs = profile->GetPrefs();

  // Language preferences.
  const std::string& accept_language_list =
      GetAcceptLanguageListSetting(profile);
  if (!accept_language_list.empty()) {
    // Used by ProfileNetworkContextService and InterceptedRequestHandlerWrapper
    // (via GetAcceptLanguageList) for request headers, and
    // renderer_preferences_util::UpdateFromSystemSettings() for
    // `navigator.language`.
    prefs->SetString(language::prefs::kAcceptLanguages, accept_language_list);

    // Necessary to avoid a reset of the kAcceptLanguages value in
    // LanguagePrefs::UpdateAcceptLanguagesPref().
    prefs->SetString(language::prefs::kSelectedLanguages, accept_language_list);
  }
}

}  // namespace browser_prefs
