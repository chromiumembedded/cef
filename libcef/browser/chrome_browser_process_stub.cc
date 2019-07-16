// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome_browser_process_stub.h"

#include "libcef/browser/browser_context.h"
#include "libcef/browser/chrome_profile_manager_stub.h"
#include "libcef/browser/context.h"
#include "libcef/browser/prefs/browser_prefs.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/net_service/util.h"

#include "base/command_line.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/ui/prefs/pref_watcher.h"
#include "components/net_log/chrome_net_log.h"
#include "components/net_log/net_export_file_writer.h"
#include "components/prefs/pref_service.h"
#include "content/browser/startup_helper.h"
#include "content/public/common/content_switches.h"
#include "net/log/net_log_capture_mode.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

ChromeBrowserProcessStub::ChromeBrowserProcessStub()
    : initialized_(false),
      context_initialized_(false),
      shutdown_(false),
      locale_("en-US") {}

ChromeBrowserProcessStub::~ChromeBrowserProcessStub() {
  DCHECK((!initialized_ && !context_initialized_) || shutdown_);
}

void ChromeBrowserProcessStub::Initialize() {
  DCHECK(!initialized_);
  DCHECK(!context_initialized_);
  DCHECK(!shutdown_);

  // Initialize this early before any code tries to check feature flags.
  content::SetUpFieldTrialsAndFeatureList();

  initialized_ = true;
}

void ChromeBrowserProcessStub::OnContextInitialized() {
  CEF_REQUIRE_UIT();
  DCHECK(initialized_);
  DCHECK(!context_initialized_);
  DCHECK(!shutdown_);

  // Must be created after the NotificationService.
  print_job_manager_.reset(new printing::PrintJobManager());
  profile_manager_.reset(new ChromeProfileManagerStub());
  event_router_forwarder_ = new extensions::EventRouterForwarder();

  context_initialized_ = true;
}

void ChromeBrowserProcessStub::Shutdown() {
  CEF_REQUIRE_UIT();
  DCHECK(initialized_);
  DCHECK(context_initialized_);
  DCHECK(!shutdown_);

  // Wait for the pending print jobs to finish. Don't do this later, since
  // this might cause a nested message loop to run, and we don't want pending
  // tasks to run once teardown has started.
  print_job_manager_->Shutdown();
  print_job_manager_.reset(NULL);

  profile_manager_.reset();
  event_router_forwarder_ = nullptr;

  if (net_service::IsEnabled() && SystemNetworkContextManager::GetInstance()) {
    SystemNetworkContextManager::DeleteInstance();
  }

  // Release any references to |local_state_| that are held by objects
  // associated with a Profile. The Profile will be deleted later.
  for (const auto& profile : CefBrowserContext::GetAll()) {
    PrefWatcher* pref_watcher = PrefWatcher::Get(profile);
    if (pref_watcher)
      pref_watcher->Shutdown();
  }

  local_state_.reset();
  browser_policy_connector_.reset();

  shutdown_ = true;
}

void ChromeBrowserProcessStub::ResourceDispatcherHostCreated() {
  NOTREACHED();
}

void ChromeBrowserProcessStub::EndSession() {
  NOTREACHED();
}

void ChromeBrowserProcessStub::FlushLocalStateAndReply(
    base::OnceClosure reply) {
  NOTREACHED();
}

metrics_services_manager::MetricsServicesManager*
ChromeBrowserProcessStub::GetMetricsServicesManager() {
  NOTREACHED();
  return NULL;
}

metrics::MetricsService* ChromeBrowserProcessStub::metrics_service() {
  NOTREACHED();
  return NULL;
}

rappor::RapporServiceImpl* ChromeBrowserProcessStub::rappor_service() {
  // Called from PluginInfoHostImpl::ReportMetrics.
  return NULL;
}

IOThread* ChromeBrowserProcessStub::io_thread() {
  return NULL;
}

SystemNetworkContextManager*
ChromeBrowserProcessStub::system_network_context_manager() {
  DCHECK(SystemNetworkContextManager::GetInstance());
  return SystemNetworkContextManager::GetInstance();
}

net_log::NetExportFileWriter*
ChromeBrowserProcessStub::net_export_file_writer() {
  if (!net_export_file_writer_) {
    net_export_file_writer_ = std::make_unique<net_log::NetExportFileWriter>();
  }
  return net_export_file_writer_.get();
}

network::NetworkQualityTracker*
ChromeBrowserProcessStub::network_quality_tracker() {
  NOTREACHED();
  return NULL;
}

WatchDogThread* ChromeBrowserProcessStub::watchdog_thread() {
  NOTREACHED();
  return NULL;
}

ProfileManager* ChromeBrowserProcessStub::profile_manager() {
  DCHECK(context_initialized_);
  return profile_manager_.get();
}

PrefService* ChromeBrowserProcessStub::local_state() {
  DCHECK(initialized_);
  if (!local_state_) {
    const CefSettings& settings = CefContext::Get()->settings();
    const base::FilePath& cache_path =
        base::FilePath(CefString(&settings.cache_path));

    // Used for very early NetworkService initialization.
    local_state_ = browser_prefs::CreatePrefService(
        nullptr, cache_path, !!settings.persist_user_preferences);
  }
  return local_state_.get();
}

net::URLRequestContextGetter*
ChromeBrowserProcessStub::system_request_context() {
  NOTREACHED();
  return NULL;
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeBrowserProcessStub::shared_url_loader_factory() {
  NOTREACHED();
  return NULL;
}

variations::VariationsService* ChromeBrowserProcessStub::variations_service() {
  NOTREACHED();
  return NULL;
}

BrowserProcessPlatformPart* ChromeBrowserProcessStub::platform_part() {
  NOTREACHED();
  return NULL;
}

extensions::EventRouterForwarder*
ChromeBrowserProcessStub::extension_event_router_forwarder() {
  DCHECK(context_initialized_);
  return event_router_forwarder_.get();
}

NotificationUIManager* ChromeBrowserProcessStub::notification_ui_manager() {
  NOTREACHED();
  return NULL;
}

NotificationPlatformBridge*
ChromeBrowserProcessStub::notification_platform_bridge() {
  NOTREACHED();
  return NULL;
}

policy::ChromeBrowserPolicyConnector*
ChromeBrowserProcessStub::browser_policy_connector() {
  if (!browser_policy_connector_) {
    browser_policy_connector_ =
        std::make_unique<policy::ChromeBrowserPolicyConnector>();
  }
  return browser_policy_connector_.get();
}

policy::PolicyService* ChromeBrowserProcessStub::policy_service() {
  return browser_policy_connector()->GetPolicyService();
}

IconManager* ChromeBrowserProcessStub::icon_manager() {
  NOTREACHED();
  return NULL;
}

GpuModeManager* ChromeBrowserProcessStub::gpu_mode_manager() {
  NOTREACHED();
  return NULL;
}

void ChromeBrowserProcessStub::CreateDevToolsProtocolHandler() {
  NOTREACHED();
}

void ChromeBrowserProcessStub::CreateDevToolsAutoOpener() {
  NOTREACHED();
}

bool ChromeBrowserProcessStub::IsShuttingDown() {
  NOTREACHED();
  return false;
}

printing::PrintJobManager* ChromeBrowserProcessStub::print_job_manager() {
  DCHECK(context_initialized_);
  return print_job_manager_.get();
}

printing::PrintPreviewDialogController*
ChromeBrowserProcessStub::print_preview_dialog_controller() {
  NOTREACHED();
  return NULL;
}

printing::BackgroundPrintingManager*
ChromeBrowserProcessStub::background_printing_manager() {
  NOTREACHED();
  return NULL;
}

IntranetRedirectDetector*
ChromeBrowserProcessStub::intranet_redirect_detector() {
  NOTREACHED();
  return NULL;
}

const std::string& ChromeBrowserProcessStub::GetApplicationLocale() {
  DCHECK(!locale_.empty());
  return locale_;
}

void ChromeBrowserProcessStub::SetApplicationLocale(const std::string& locale) {
  locale_ = locale;
}

DownloadStatusUpdater* ChromeBrowserProcessStub::download_status_updater() {
  NOTREACHED();
  return NULL;
}

DownloadRequestLimiter* ChromeBrowserProcessStub::download_request_limiter() {
  NOTREACHED();
  return NULL;
}

BackgroundModeManager* ChromeBrowserProcessStub::background_mode_manager() {
  NOTREACHED();
  return NULL;
}

void ChromeBrowserProcessStub::set_background_mode_manager_for_test(
    std::unique_ptr<BackgroundModeManager> manager) {
  NOTREACHED();
}

StatusTray* ChromeBrowserProcessStub::status_tray() {
  NOTREACHED();
  return NULL;
}

safe_browsing::SafeBrowsingService*
ChromeBrowserProcessStub::safe_browsing_service() {
  return NULL;
}

safe_browsing::ClientSideDetectionService*
ChromeBrowserProcessStub::safe_browsing_detection_service() {
  NOTREACHED();
  return NULL;
}

subresource_filter::RulesetService*
ChromeBrowserProcessStub::subresource_filter_ruleset_service() {
  NOTREACHED();
  return NULL;
}

optimization_guide::OptimizationGuideService*
ChromeBrowserProcessStub::optimization_guide_service() {
  NOTREACHED();
  return NULL;
}

StartupData* ChromeBrowserProcessStub::startup_data() {
  NOTREACHED();
  return NULL;
}

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
void ChromeBrowserProcessStub::StartAutoupdateTimer() {}
#endif

net_log::ChromeNetLog* ChromeBrowserProcessStub::net_log() {
  DCHECK(initialized_);
  if (!net_log_) {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    net_log_ = std::make_unique<net_log::ChromeNetLog>();
    if (command_line.HasSwitch(network::switches::kLogNetLog)) {
      net_log_->StartWritingToFile(
          command_line.GetSwitchValuePath(network::switches::kLogNetLog),
          net::GetNetCaptureModeFromCommandLine(command_line,
                                                network::switches::kLogNetLog),
          command_line.GetCommandLineString(), std::string());
    }
  }
  return net_log_.get();
}

component_updater::ComponentUpdateService*
ChromeBrowserProcessStub::component_updater() {
  NOTREACHED();
  return NULL;
}

component_updater::SupervisedUserWhitelistInstaller*
ChromeBrowserProcessStub::supervised_user_whitelist_installer() {
  NOTREACHED();
  return NULL;
}

MediaFileSystemRegistry*
ChromeBrowserProcessStub::media_file_system_registry() {
  NOTREACHED();
  return NULL;
}

WebRtcLogUploader* ChromeBrowserProcessStub::webrtc_log_uploader() {
  NOTREACHED();
  return NULL;
}

network_time::NetworkTimeTracker*
ChromeBrowserProcessStub::network_time_tracker() {
  NOTREACHED();
  return NULL;
}

gcm::GCMDriver* ChromeBrowserProcessStub::gcm_driver() {
  NOTREACHED();
  return NULL;
}

shell_integration::DefaultWebClientState
ChromeBrowserProcessStub::CachedDefaultWebClientState() {
  NOTREACHED();
  return shell_integration::UNKNOWN_DEFAULT;
}

resource_coordinator::TabManager* ChromeBrowserProcessStub::GetTabManager() {
  NOTREACHED();
  return NULL;
}

resource_coordinator::ResourceCoordinatorParts*
ChromeBrowserProcessStub::resource_coordinator_parts() {
  NOTREACHED();
  return NULL;
}

prefs::InProcessPrefServiceFactory*
ChromeBrowserProcessStub::pref_service_factory() const {
  NOTREACHED();
  return NULL;
}
