// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/chrome_browser_process_alloy.h"

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
#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/ui/prefs/pref_watcher.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/timer_update_scheduler.h"
#include "components/net_log/chrome_net_log.h"
#include "components/prefs/pref_service.h"
#include "content/browser/startup_helper.h"
#include "content/public/common/content_switches.h"
#include "net/log/net_log_capture_mode.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

ChromeBrowserProcessAlloy::ChromeBrowserProcessAlloy()
    : initialized_(false),
      context_initialized_(false),
      shutdown_(false),
      locale_("en-US") {}

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
    extensions_client_.reset(new extensions::CefExtensionsClient());
    extensions::ExtensionsClient::Set(extensions_client_.get());
    extensions_browser_client_.reset(
        new extensions::CefExtensionsBrowserClient);
    extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());
  }

  initialized_ = true;
}

void ChromeBrowserProcessAlloy::OnContextInitialized() {
  CEF_REQUIRE_UIT();
  DCHECK(initialized_);
  DCHECK(!context_initialized_);
  DCHECK(!shutdown_);

  // Must be created after the NotificationService.
  print_job_manager_.reset(new printing::PrintJobManager());
  profile_manager_.reset(new ChromeProfileManagerAlloy());
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
    if (pref_watcher)
      pref_watcher->Shutdown();

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
  NOTREACHED();
}

void ChromeBrowserProcessAlloy::FlushLocalStateAndReply(
    base::OnceClosure reply) {
  NOTREACHED();
}

metrics_services_manager::MetricsServicesManager*
ChromeBrowserProcessAlloy::GetMetricsServicesManager() {
  NOTREACHED();
  return nullptr;
}

metrics::MetricsService* ChromeBrowserProcessAlloy::metrics_service() {
  NOTREACHED();
  return nullptr;
}

SystemNetworkContextManager*
ChromeBrowserProcessAlloy::system_network_context_manager() {
  DCHECK(SystemNetworkContextManager::GetInstance());
  return SystemNetworkContextManager::GetInstance();
}

network::NetworkQualityTracker*
ChromeBrowserProcessAlloy::network_quality_tracker() {
  NOTREACHED();
  return nullptr;
}

ProfileManager* ChromeBrowserProcessAlloy::profile_manager() {
  DCHECK(context_initialized_);
  return profile_manager_.get();
}

PrefService* ChromeBrowserProcessAlloy::local_state() {
  DCHECK(initialized_);
  if (!local_state_) {
    // Use a location that is shared by all request contexts.
    const CefSettings& settings = CefContext::Get()->settings();
    const base::FilePath& root_cache_path =
        base::FilePath(CefString(&settings.root_cache_path));

    // Used for very early NetworkService initialization.
    // Always persist preferences for this PrefService if possible because it
    // contains the cookie encryption key on Windows.
    local_state_ =
        browser_prefs::CreatePrefService(nullptr /* profile */, root_cache_path,
                                         true /* persist_user_preferences */);
  }
  return local_state_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeBrowserProcessAlloy::shared_url_loader_factory() {
  NOTREACHED();
  return nullptr;
}

variations::VariationsService* ChromeBrowserProcessAlloy::variations_service() {
  NOTREACHED();
  return nullptr;
}

BrowserProcessPlatformPart* ChromeBrowserProcessAlloy::platform_part() {
  NOTREACHED();
  return nullptr;
}

extensions::EventRouterForwarder*
ChromeBrowserProcessAlloy::extension_event_router_forwarder() {
  DCHECK(context_initialized_);
  return event_router_forwarder_.get();
}

NotificationUIManager* ChromeBrowserProcessAlloy::notification_ui_manager() {
  NOTREACHED();
  return nullptr;
}

NotificationPlatformBridge*
ChromeBrowserProcessAlloy::notification_platform_bridge() {
  NOTREACHED();
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
  NOTREACHED();
  return nullptr;
}

GpuModeManager* ChromeBrowserProcessAlloy::gpu_mode_manager() {
  NOTREACHED();
  return nullptr;
}

void ChromeBrowserProcessAlloy::CreateDevToolsProtocolHandler() {
  NOTREACHED();
}

void ChromeBrowserProcessAlloy::CreateDevToolsAutoOpener() {
  NOTREACHED();
}

bool ChromeBrowserProcessAlloy::IsShuttingDown() {
  NOTREACHED();
  return false;
}

printing::PrintJobManager* ChromeBrowserProcessAlloy::print_job_manager() {
  DCHECK(context_initialized_);
  return print_job_manager_.get();
}

printing::PrintPreviewDialogController*
ChromeBrowserProcessAlloy::print_preview_dialog_controller() {
  if (!print_preview_dialog_controller_.get()) {
    print_preview_dialog_controller_ =
        new printing::PrintPreviewDialogController();
  }
  return print_preview_dialog_controller_.get();
}

printing::BackgroundPrintingManager*
ChromeBrowserProcessAlloy::background_printing_manager() {
  if (!background_printing_manager_.get()) {
    background_printing_manager_.reset(
        new printing::BackgroundPrintingManager());
  }
  return background_printing_manager_.get();
}

IntranetRedirectDetector*
ChromeBrowserProcessAlloy::intranet_redirect_detector() {
  NOTREACHED();
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
  NOTREACHED();
  return nullptr;
}

DownloadRequestLimiter* ChromeBrowserProcessAlloy::download_request_limiter() {
  NOTREACHED();
  return nullptr;
}

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
BackgroundModeManager* ChromeBrowserProcessAlloy::background_mode_manager() {
  NOTREACHED();
  return nullptr;
}

void ChromeBrowserProcessAlloy::set_background_mode_manager_for_test(
    std::unique_ptr<BackgroundModeManager> manager) {
  NOTREACHED();
}
#endif

StatusTray* ChromeBrowserProcessAlloy::status_tray() {
  NOTREACHED();
  return nullptr;
}

safe_browsing::SafeBrowsingService*
ChromeBrowserProcessAlloy::safe_browsing_service() {
  return nullptr;
}

subresource_filter::RulesetService*
ChromeBrowserProcessAlloy::subresource_filter_ruleset_service() {
  NOTREACHED();
  return nullptr;
}

federated_learning::FlocSortingLshClustersService*
ChromeBrowserProcessAlloy::floc_sorting_lsh_clusters_service() {
  NOTREACHED();
  return nullptr;
}

StartupData* ChromeBrowserProcessAlloy::startup_data() {
  NOTREACHED();
  return nullptr;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
void ChromeBrowserProcessAlloy::StartAutoupdateTimer() {}
#endif

component_updater::ComponentUpdateService*
ChromeBrowserProcessAlloy::component_updater() {
  if (component_updater_)
    return component_updater_.get();

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
  NOTREACHED();
  return nullptr;
}

WebRtcLogUploader* ChromeBrowserProcessAlloy::webrtc_log_uploader() {
  NOTREACHED();
  return nullptr;
}

network_time::NetworkTimeTracker*
ChromeBrowserProcessAlloy::network_time_tracker() {
  NOTREACHED();
  return nullptr;
}

gcm::GCMDriver* ChromeBrowserProcessAlloy::gcm_driver() {
  NOTREACHED();
  return nullptr;
}

resource_coordinator::TabManager* ChromeBrowserProcessAlloy::GetTabManager() {
  NOTREACHED();
  return nullptr;
}

resource_coordinator::ResourceCoordinatorParts*
ChromeBrowserProcessAlloy::resource_coordinator_parts() {
  NOTREACHED();
  return nullptr;
}

BuildState* ChromeBrowserProcessAlloy::GetBuildState() {
  NOTREACHED();
  return nullptr;
}

SerialPolicyAllowedPorts*
ChromeBrowserProcessAlloy::serial_policy_allowed_ports() {
  NOTREACHED();
  return nullptr;
}

breadcrumbs::BreadcrumbPersistentStorageManager*
ChromeBrowserProcessAlloy::GetBreadcrumbPersistentStorageManager() {
  NOTREACHED();
  return nullptr;
}
