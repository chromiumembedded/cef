// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome_browser_process_stub.h"

#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/chrome_profile_manager_stub.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "chrome/browser/net/chrome_net_log_helper.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "components/net_log/chrome_net_log.h"
#include "content/public/common/content_switches.h"
#include "ui/message_center/message_center.h"

ChromeBrowserProcessStub::ChromeBrowserProcessStub()
  : initialized_(false),
    context_initialized_(false),
    shutdown_(false),
    locale_("en-US") {
  chrome::SetBrowserContextIncognitoHelper(this);
}

ChromeBrowserProcessStub::~ChromeBrowserProcessStub() {
  DCHECK((!initialized_ && !context_initialized_) || shutdown_);
  g_browser_process = NULL;
  chrome::SetBrowserContextIncognitoHelper(nullptr);
}

void ChromeBrowserProcessStub::Initialize(
    const base::CommandLine& command_line) {
  DCHECK(!initialized_);
  DCHECK(!context_initialized_);
  DCHECK(!shutdown_);

  base::FilePath net_log_path;
  if (command_line.HasSwitch(switches::kLogNetLog))
    net_log_path = command_line.GetSwitchValuePath(switches::kLogNetLog);
  net_log_.reset(new net_log::ChromeNetLog(
      net_log_path, GetNetCaptureModeFromCommandLine(command_line),
      command_line.GetCommandLineString(), std::string()));

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

  shutdown_ = true;
}

void ChromeBrowserProcessStub::ResourceDispatcherHostCreated() {
  NOTREACHED();
};

void ChromeBrowserProcessStub::EndSession() {
  NOTREACHED();
};

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
  NOTREACHED();
  return NULL;
}

IOThread* ChromeBrowserProcessStub::io_thread() {
  return NULL;
}

WatchDogThread* ChromeBrowserProcessStub::watchdog_thread() {
  NOTREACHED();
  return NULL;
}

ukm::UkmService* ChromeBrowserProcessStub::ukm_service() {
  NOTREACHED();
  return NULL;
}

ProfileManager* ChromeBrowserProcessStub::profile_manager() {
  DCHECK(context_initialized_);
  return profile_manager_.get();
}

PrefService* ChromeBrowserProcessStub::local_state() {
  DCHECK(context_initialized_);
  return profile_manager_->GetLastUsedProfile(
      profile_manager_->user_data_dir())->GetPrefs();
}

net::URLRequestContextGetter*
  ChromeBrowserProcessStub::system_request_context() {
  NOTREACHED();
  return NULL;
}

variations::VariationsService*
  ChromeBrowserProcessStub::variations_service() {
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

message_center::MessageCenter* ChromeBrowserProcessStub::message_center() {
  NOTREACHED();
  return NULL;
}

policy::BrowserPolicyConnector*
  ChromeBrowserProcessStub::browser_policy_connector() {
  NOTREACHED();
  return NULL;
}

policy::PolicyService* ChromeBrowserProcessStub::policy_service() {
  NOTREACHED();
  return NULL;
}

IconManager* ChromeBrowserProcessStub::icon_manager() {
  NOTREACHED();
  return NULL;
}

GpuModeManager* ChromeBrowserProcessStub::gpu_mode_manager() {
  NOTREACHED();
  return NULL;
}

GpuProfileCache* ChromeBrowserProcessStub::gpu_profile_cache() {
  NOTREACHED();
  return NULL;
}

void ChromeBrowserProcessStub::CreateDevToolsHttpProtocolHandler(
    const std::string& ip,
    uint16_t port) {
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

const std::string &ChromeBrowserProcessStub::GetApplicationLocale() {
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
  NOTREACHED();
  return NULL;
}

safe_browsing::ClientSideDetectionService*
    ChromeBrowserProcessStub::safe_browsing_detection_service() {
  NOTREACHED();
  return NULL;
}

subresource_filter::ContentRulesetService*
   ChromeBrowserProcessStub::subresource_filter_ruleset_service() {
  NOTREACHED();
  return NULL;
}

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
void ChromeBrowserProcessStub::StartAutoupdateTimer() {
}
#endif

net_log::ChromeNetLog* ChromeBrowserProcessStub::net_log() {
  DCHECK(initialized_);
  return net_log_.get();
}

component_updater::ComponentUpdateService*
    ChromeBrowserProcessStub::component_updater() {
  NOTREACHED();
  return NULL;
}

CRLSetFetcher* ChromeBrowserProcessStub::crl_set_fetcher() {
  NOTREACHED();
  return NULL;
}

component_updater::PnaclComponentInstaller*
    ChromeBrowserProcessStub::pnacl_component_installer() {
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

bool ChromeBrowserProcessStub::created_local_state() const {
  NOTREACHED();
  return false;
}

#if BUILDFLAG(ENABLE_WEBRTC)
WebRtcLogUploader* ChromeBrowserProcessStub::webrtc_log_uploader() {
  NOTREACHED();
  return NULL;
}
#endif

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

memory::TabManager* ChromeBrowserProcessStub::GetTabManager() {
  NOTREACHED();
  return NULL;
}

physical_web::PhysicalWebDataSource*
ChromeBrowserProcessStub::GetPhysicalWebDataSource() {
  NOTREACHED();
  return NULL;
}

content::BrowserContext*
ChromeBrowserProcessStub::GetBrowserContextRedirectedInIncognito(
    content::BrowserContext* context) {
  return CefBrowserContextImpl::GetForContext(context);
}

content::BrowserContext*
ChromeBrowserProcessStub::GetBrowserContextOwnInstanceInIncognito(
    content::BrowserContext* context) {
  return GetBrowserContextRedirectedInIncognito(context);
}
