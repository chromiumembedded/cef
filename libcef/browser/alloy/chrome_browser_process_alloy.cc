// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/chrome_browser_process_alloy.h"

#include <memory>

#include "libcef/browser/alloy/chrome_profile_manager_alloy.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/context.h"
#include "libcef/browser/extensions/extensions_browser_client.h"
#include "libcef/browser/prefs/browser_prefs.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_client.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/permissions/chrome_permissions_client.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/ui/prefs/pref_watcher.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/timer_update_scheduler.h"
#include "components/net_log/chrome_net_log.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/prefs/pref_service.h"
#include "content/browser/startup_helper.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switches.h"
#include "net/log/net_log_capture_mode.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_WIN)
#include "components/os_crypt/async/browser/dpapi_key_provider.h"
#endif

ChromeBrowserProcessAlloy::ChromeBrowserProcessAlloy() : locale_("en-US") {}

ChromeBrowserProcessAlloy::~ChromeBrowserProcessAlloy() {
  DCHECK((!initialized_ && !context_initialized_) || shutdown_);

  if (extensions::ExtensionsEnabled()) {
    extensions::ExtensionsBrowserClient::Set(nullptr);
    extensions_browser_client_.reset();
  }
}

void ChromeBrowserProcessAlloy::Initialize() {
  DCHECK(!initialized_);
  DCHECK(!context_initialized_);
  DCHECK(!shutdown_);
  DCHECK(!field_trial_list_);

  // Initialize this early before any code tries to check feature flags.
  field_trial_list_ = content::SetUpFieldTrialsAndFeatureList();

  if (extensions::ExtensionsEnabled()) {
    // Initialize extension global objects before creating the global
    // BrowserContext.
    extensions_client_ = std::make_unique<extensions::CefExtensionsClient>();
    extensions::ExtensionsClient::Set(extensions_client_.get());
    extensions_browser_client_ =
        std::make_unique<extensions::CefExtensionsBrowserClient>();
    extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());
  }

  // Make sure permissions client has been set.
  ChromePermissionsClient::GetInstance();

  initialized_ = true;
}

void ChromeBrowserProcessAlloy::OnContextInitialized() {
  CEF_REQUIRE_UIT();
  DCHECK(initialized_);
  DCHECK(!context_initialized_);
  DCHECK(!shutdown_);

  // OSCryptAsync provider configuration. If empty, this delegates all
  // encryption operations to OSCrypt.
  std::vector<std::pair<size_t, std::unique_ptr<os_crypt_async::KeyProvider>>>
      providers;

#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1373092): For Windows, continue to add providers behind
  // features, as support for them is added.
  if (base::FeatureList::IsEnabled(features::kEnableDPAPIEncryptionProvider)) {
    // The DPAPI key provider requires OSCrypt::Init to have already been called
    // to initialize the key storage. This happens in
    // AlloyBrowserMainParts::PreCreateMainMessageLoop.
    providers.emplace_back(std::make_pair(
        /*precedence=*/10u,
        std::make_unique<os_crypt_async::DPAPIKeyProvider>(local_state())));
  }
#endif  // BUILDFLAG(IS_WIN)

  os_crypt_async_ =
      std::make_unique<os_crypt_async::OSCryptAsync>(std::move(providers));

  // Trigger async initialization of OSCrypt key providers.
  std::ignore = os_crypt_async_->GetInstance(base::DoNothing());

  // Must be created after the NotificationService.
  print_job_manager_ = std::make_unique<printing::PrintJobManager>();
  profile_manager_ = std::make_unique<ChromeProfileManagerAlloy>();
  event_router_forwarder_ = new extensions::EventRouterForwarder();
  context_initialized_ = true;
}

void ChromeBrowserProcessAlloy::CleanupOnUIThread() {
  CEF_REQUIRE_UIT();
  DCHECK(initialized_);
  DCHECK(context_initialized_);
  DCHECK(!shutdown_);

  // Wait for the pending print jobs to finish. Don't do this later, since
  // this might cause a nested message loop to run, and we don't want pending
  // tasks to run once teardown has started.
  print_job_manager_->Shutdown();
  print_job_manager_.reset(nullptr);
  print_preview_dialog_controller_ = nullptr;

  profile_manager_.reset();
  event_router_forwarder_ = nullptr;

  if (SystemNetworkContextManager::GetInstance()) {
    SystemNetworkContextManager::DeleteInstance();
  }

  // Release any references held by objects associated with a Profile. The
  // Profile will be deleted later.
  for (const auto& browser_context : CefBrowserContext::GetAll()) {
    // Release any references to |local_state_|.
    auto profile = browser_context->AsProfile();
    PrefWatcher* pref_watcher = PrefWatcher::Get(profile);
    if (pref_watcher) {
      pref_watcher->Shutdown();
    }

    // Unregister observers for |background_printing_manager_|.
    if (background_printing_manager_) {
      background_printing_manager_->DeletePreviewContentsForBrowserContext(
          profile);
    }
  }

  local_state_.reset();
  browser_policy_connector_.reset();
  background_printing_manager_.reset();
  field_trial_list_.reset();
  component_updater_.reset();

  shutdown_ = true;
}

void ChromeBrowserProcessAlloy::EndSession() {
  DCHECK(false);
}

void ChromeBrowserProcessAlloy::FlushLocalStateAndReply(
    base::OnceClosure reply) {
  DCHECK(false);
}

metrics_services_manager::MetricsServicesManager*
ChromeBrowserProcessAlloy::GetMetricsServicesManager() {
  DCHECK(false);
  return nullptr;
}

metrics::MetricsService* ChromeBrowserProcessAlloy::metrics_service() {
  DCHECK(false);
  return nullptr;
}

SystemNetworkContextManager*
ChromeBrowserProcessAlloy::system_network_context_manager() {
  DCHECK(SystemNetworkContextManager::GetInstance());
  return SystemNetworkContextManager::GetInstance();
}

network::NetworkQualityTracker*
ChromeBrowserProcessAlloy::network_quality_tracker() {
  DCHECK(false);
  return nullptr;
}

embedder_support::OriginTrialsSettingsStorage*
ChromeBrowserProcessAlloy::GetOriginTrialsSettingsStorage() {
  DCHECK(false);
  return nullptr;
}

ProfileManager* ChromeBrowserProcessAlloy::profile_manager() {
  DCHECK(context_initialized_);
  return profile_manager_.get();
}

PrefService* ChromeBrowserProcessAlloy::local_state() {
  DCHECK(initialized_);
  if (!local_state_) {
    base::FilePath user_data_path;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_path);
    DCHECK(!user_data_path.empty());

    // Used for very early NetworkService initialization.
    // Always persist preferences for this PrefService if possible because it
    // contains the cookie encryption key on Windows.
    local_state_ =
        browser_prefs::CreatePrefService(nullptr /* profile */, user_data_path,
                                         true /* persist_user_preferences */);
  }
  return local_state_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeBrowserProcessAlloy::shared_url_loader_factory() {
  DCHECK(false);
  return nullptr;
}

variations::VariationsService* ChromeBrowserProcessAlloy::variations_service() {
  DCHECK(false);
  return nullptr;
}

BrowserProcessPlatformPart* ChromeBrowserProcessAlloy::platform_part() {
  DCHECK(false);
  return nullptr;
}

extensions::EventRouterForwarder*
ChromeBrowserProcessAlloy::extension_event_router_forwarder() {
  DCHECK(context_initialized_);
  return event_router_forwarder_.get();
}

NotificationUIManager* ChromeBrowserProcessAlloy::notification_ui_manager() {
  DCHECK(false);
  return nullptr;
}

NotificationPlatformBridge*
ChromeBrowserProcessAlloy::notification_platform_bridge() {
  DCHECK(false);
  return nullptr;
}

policy::ChromeBrowserPolicyConnector*
ChromeBrowserProcessAlloy::browser_policy_connector() {
  if (!browser_policy_connector_) {
    browser_policy_connector_ =
        std::make_unique<policy::ChromeBrowserPolicyConnector>();
  }
  return browser_policy_connector_.get();
}

policy::PolicyService* ChromeBrowserProcessAlloy::policy_service() {
  return browser_policy_connector()->GetPolicyService();
}

IconManager* ChromeBrowserProcessAlloy::icon_manager() {
  DCHECK(false);
  return nullptr;
}

GpuModeManager* ChromeBrowserProcessAlloy::gpu_mode_manager() {
  DCHECK(false);
  return nullptr;
}

void ChromeBrowserProcessAlloy::CreateDevToolsProtocolHandler() {
  DCHECK(false);
}

void ChromeBrowserProcessAlloy::CreateDevToolsAutoOpener() {
  DCHECK(false);
}

bool ChromeBrowserProcessAlloy::IsShuttingDown() {
  DCHECK(false);
  return false;
}

printing::PrintJobManager* ChromeBrowserProcessAlloy::print_job_manager() {
  DCHECK(context_initialized_);
  return print_job_manager_.get();
}

printing::PrintPreviewDialogController*
ChromeBrowserProcessAlloy::print_preview_dialog_controller() {
  if (!print_preview_dialog_controller_) {
    print_preview_dialog_controller_ =
        std::make_unique<printing::PrintPreviewDialogController>();
  }
  return print_preview_dialog_controller_.get();
}

printing::BackgroundPrintingManager*
ChromeBrowserProcessAlloy::background_printing_manager() {
  if (!background_printing_manager_.get()) {
    background_printing_manager_ =
        std::make_unique<printing::BackgroundPrintingManager>();
  }
  return background_printing_manager_.get();
}

IntranetRedirectDetector*
ChromeBrowserProcessAlloy::intranet_redirect_detector() {
  DCHECK(false);
  return nullptr;
}

const std::string& ChromeBrowserProcessAlloy::GetApplicationLocale() {
  DCHECK(!locale_.empty());
  return locale_;
}

void ChromeBrowserProcessAlloy::SetApplicationLocale(
    const std::string& locale) {
  locale_ = locale;
}

DownloadStatusUpdater* ChromeBrowserProcessAlloy::download_status_updater() {
  DCHECK(false);
  return nullptr;
}

DownloadRequestLimiter* ChromeBrowserProcessAlloy::download_request_limiter() {
  DCHECK(false);
  return nullptr;
}

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
BackgroundModeManager* ChromeBrowserProcessAlloy::background_mode_manager() {
  DCHECK(false);
  return nullptr;
}

void ChromeBrowserProcessAlloy::set_background_mode_manager_for_test(
    std::unique_ptr<BackgroundModeManager> manager) {
  DCHECK(false);
}
#endif

StatusTray* ChromeBrowserProcessAlloy::status_tray() {
  DCHECK(false);
  return nullptr;
}

safe_browsing::SafeBrowsingService*
ChromeBrowserProcessAlloy::safe_browsing_service() {
  return nullptr;
}

subresource_filter::RulesetService*
ChromeBrowserProcessAlloy::subresource_filter_ruleset_service() {
  DCHECK(false);
  return nullptr;
}

StartupData* ChromeBrowserProcessAlloy::startup_data() {
  DCHECK(false);
  return nullptr;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
void ChromeBrowserProcessAlloy::StartAutoupdateTimer() {}
#endif

component_updater::ComponentUpdateService*
ChromeBrowserProcessAlloy::component_updater() {
  if (component_updater_) {
    return component_updater_.get();
  }

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    return nullptr;
  }

  std::unique_ptr<component_updater::UpdateScheduler> scheduler =
      std::make_unique<component_updater::TimerUpdateScheduler>();

  component_updater_ = component_updater::ComponentUpdateServiceFactory(
      component_updater::MakeChromeComponentUpdaterConfigurator(
          base::CommandLine::ForCurrentProcess(),
          g_browser_process->local_state()),
      std::move(scheduler), /*brand=*/std::string());

  return component_updater_.get();
}

MediaFileSystemRegistry*
ChromeBrowserProcessAlloy::media_file_system_registry() {
  DCHECK(false);
  return nullptr;
}

WebRtcLogUploader* ChromeBrowserProcessAlloy::webrtc_log_uploader() {
  DCHECK(false);
  return nullptr;
}

network_time::NetworkTimeTracker*
ChromeBrowserProcessAlloy::network_time_tracker() {
  DCHECK(false);
  return nullptr;
}

gcm::GCMDriver* ChromeBrowserProcessAlloy::gcm_driver() {
  DCHECK(false);
  return nullptr;
}

resource_coordinator::TabManager* ChromeBrowserProcessAlloy::GetTabManager() {
  DCHECK(false);
  return nullptr;
}

resource_coordinator::ResourceCoordinatorParts*
ChromeBrowserProcessAlloy::resource_coordinator_parts() {
  DCHECK(false);
  return nullptr;
}

os_crypt_async::OSCryptAsync* ChromeBrowserProcessAlloy::os_crypt_async() {
  DCHECK(os_crypt_async_);
  return os_crypt_async_.get();
}

BuildState* ChromeBrowserProcessAlloy::GetBuildState() {
  DCHECK(false);
  return nullptr;
}

SerialPolicyAllowedPorts*
ChromeBrowserProcessAlloy::serial_policy_allowed_ports() {
  DCHECK(false);
  return nullptr;
}

HidSystemTrayIcon* ChromeBrowserProcessAlloy::hid_system_tray_icon() {
  DCHECK(false);
  return nullptr;
}

UsbSystemTrayIcon* ChromeBrowserProcessAlloy::usb_system_tray_icon() {
  DCHECK(false);
  return nullptr;
}
