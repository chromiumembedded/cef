// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome_browser_process_stub.h"

#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/chrome_profile_manager_stub.h"
#include "libcef/browser/component_updater/cef_component_updater_configurator.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/configurator.h"
#include "ui/message_center/message_center.h"

namespace {

void RegisterComponentsForUpdate(
    component_updater::ComponentUpdateService* cus) {
  base::ThreadRestrictions::ScopedAllowIO scoped_allow_io;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWidevineCdm)) {
    RegisterWidevineCdmComponent(cus);
  }
}

}  // namespace

ChromeBrowserProcessStub::ChromeBrowserProcessStub()
  : initialized_(false),
    shutdown_(false),
    locale_("en-US") {
  chrome::SetBrowserContextIncognitoHelper(this);
}

ChromeBrowserProcessStub::~ChromeBrowserProcessStub() {
  DCHECK(!initialized_ || shutdown_);
  g_browser_process = NULL;
  chrome::SetBrowserContextIncognitoHelper(nullptr);
}

void ChromeBrowserProcessStub::Initialize() {
  CEF_REQUIRE_UIT();
  DCHECK(!initialized_);
  DCHECK(!shutdown_);

  // Must be created after the NotificationService.
  print_job_manager_.reset(new printing::PrintJobManager());
  profile_manager_.reset(new ChromeProfileManagerStub());
  event_router_forwarder_ = new extensions::EventRouterForwarder();

  // Creating the component updater does not do anything initially. Components
  // need to be registered and Start() needs to be called.
  scoped_refptr<CefBrowserContextImpl> browser_context =
      CefContentBrowserClient::Get()->browser_context();
  scoped_refptr<update_client::Configurator> configurator =
      component_updater::MakeCefComponentUpdaterConfigurator(
          base::CommandLine::ForCurrentProcess(),
          browser_context->request_context().get(),
          browser_context->GetPrefs());
  component_updater_.reset(component_updater::ComponentUpdateServiceFactory(
                                configurator).release());
  RegisterComponentsForUpdate(component_updater_.get());

  initialized_ = true;
}

void ChromeBrowserProcessStub::Shutdown() {
  CEF_REQUIRE_UIT();
  DCHECK(initialized_);
  DCHECK(!shutdown_);

  // Wait for the pending print jobs to finish. Don't do this later, since
  // this might cause a nested message loop to run, and we don't want pending
  // tasks to run once teardown has started.
  print_job_manager_->Shutdown();
  print_job_manager_.reset(NULL);

  profile_manager_.reset();
  event_router_forwarder_ = nullptr;
  component_updater_.reset();

  shutdown_ = true;
}

void ChromeBrowserProcessStub::ResourceDispatcherHostCreated() {
  NOTIMPLEMENTED();
};

void ChromeBrowserProcessStub::EndSession() {
  NOTIMPLEMENTED();
};

metrics_services_manager::MetricsServicesManager*
    ChromeBrowserProcessStub::GetMetricsServicesManager() {
  NOTIMPLEMENTED();
  return NULL;
}

metrics::MetricsService* ChromeBrowserProcessStub::metrics_service() {
  NOTIMPLEMENTED();
  return NULL;
}

rappor::RapporService* ChromeBrowserProcessStub::rappor_service() {
  NOTIMPLEMENTED();
  return NULL;
}

IOThread* ChromeBrowserProcessStub::io_thread() {
  NOTIMPLEMENTED();
  return NULL;
}

WatchDogThread* ChromeBrowserProcessStub::watchdog_thread() {
  NOTIMPLEMENTED();
  return NULL;
}

ProfileManager* ChromeBrowserProcessStub::profile_manager() {
  return profile_manager_.get();
}

PrefService* ChromeBrowserProcessStub::local_state() {
  NOTIMPLEMENTED();
  return NULL;
}

net::URLRequestContextGetter*
  ChromeBrowserProcessStub::system_request_context() {
  NOTIMPLEMENTED();
  return NULL;
}

variations::VariationsService*
  ChromeBrowserProcessStub::variations_service() {
  NOTIMPLEMENTED();
  return NULL;
}

BrowserProcessPlatformPart* ChromeBrowserProcessStub::platform_part() {
  NOTIMPLEMENTED();
  return NULL;
}

extensions::EventRouterForwarder*
    ChromeBrowserProcessStub::extension_event_router_forwarder() {
  return event_router_forwarder_.get();
}

NotificationUIManager* ChromeBrowserProcessStub::notification_ui_manager() {
  NOTIMPLEMENTED();
  return NULL;
}

NotificationPlatformBridge*
  ChromeBrowserProcessStub::notification_platform_bridge() {
  NOTIMPLEMENTED();
  return NULL;
}

message_center::MessageCenter* ChromeBrowserProcessStub::message_center() {
  NOTIMPLEMENTED();
  return NULL;
}

policy::BrowserPolicyConnector*
  ChromeBrowserProcessStub::browser_policy_connector() {
  NOTIMPLEMENTED();
  return NULL;
}

policy::PolicyService* ChromeBrowserProcessStub::policy_service() {
  NOTIMPLEMENTED();
  return NULL;
}

IconManager* ChromeBrowserProcessStub::icon_manager() {
  NOTIMPLEMENTED();
  return NULL;
}

GpuModeManager* ChromeBrowserProcessStub::gpu_mode_manager() {
  NOTIMPLEMENTED();
  return NULL;
}

GpuProfileCache* ChromeBrowserProcessStub::gpu_profile_cache() {
  NOTIMPLEMENTED();
  return NULL;
}

void ChromeBrowserProcessStub::CreateDevToolsHttpProtocolHandler(
    const std::string& ip,
    uint16_t port) {
  NOTIMPLEMENTED();
}

void ChromeBrowserProcessStub::CreateDevToolsAutoOpener() {
  NOTIMPLEMENTED();
}

bool ChromeBrowserProcessStub::IsShuttingDown() {
  NOTIMPLEMENTED();
  return false;
}

printing::PrintJobManager* ChromeBrowserProcessStub::print_job_manager() {
  return print_job_manager_.get();
}

printing::PrintPreviewDialogController*
    ChromeBrowserProcessStub::print_preview_dialog_controller() {
  NOTIMPLEMENTED();
  return NULL;
}

printing::BackgroundPrintingManager*
    ChromeBrowserProcessStub::background_printing_manager() {
  NOTIMPLEMENTED();
  return NULL;
}

IntranetRedirectDetector*
  ChromeBrowserProcessStub::intranet_redirect_detector() {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
  return NULL;
}

DownloadRequestLimiter* ChromeBrowserProcessStub::download_request_limiter() {
  NOTIMPLEMENTED();
  return NULL;
}

BackgroundModeManager* ChromeBrowserProcessStub::background_mode_manager() {
  NOTIMPLEMENTED();
  return NULL;
}

void ChromeBrowserProcessStub::set_background_mode_manager_for_test(
  std::unique_ptr<BackgroundModeManager> manager) {
  NOTIMPLEMENTED();
}

StatusTray* ChromeBrowserProcessStub::status_tray() {
  NOTIMPLEMENTED();
  return NULL;
}

safe_browsing::SafeBrowsingService*
    ChromeBrowserProcessStub::safe_browsing_service() {
  NOTIMPLEMENTED();
  return NULL;
}

safe_browsing::ClientSideDetectionService*
    ChromeBrowserProcessStub::safe_browsing_detection_service() {
  NOTIMPLEMENTED();
  return NULL;
}

subresource_filter::RulesetService*
    ChromeBrowserProcessStub::subresource_filter_ruleset_service() {
  NOTIMPLEMENTED();
  return NULL;
}

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
void ChromeBrowserProcessStub::StartAutoupdateTimer() {
}
#endif

net_log::ChromeNetLog* ChromeBrowserProcessStub::net_log() {
  NOTIMPLEMENTED();
  return NULL;
}

component_updater::ComponentUpdateService*
    ChromeBrowserProcessStub::component_updater() {
  return component_updater_.get();
}

CRLSetFetcher* ChromeBrowserProcessStub::crl_set_fetcher() {
  NOTIMPLEMENTED();
  return NULL;
}

component_updater::PnaclComponentInstaller*
    ChromeBrowserProcessStub::pnacl_component_installer() {
  NOTIMPLEMENTED();
  return NULL;
}

component_updater::SupervisedUserWhitelistInstaller*
    ChromeBrowserProcessStub::supervised_user_whitelist_installer() {
  NOTIMPLEMENTED();
  return NULL;
}

MediaFileSystemRegistry*
    ChromeBrowserProcessStub::media_file_system_registry() {
  NOTIMPLEMENTED();
  return NULL;
}

bool ChromeBrowserProcessStub::created_local_state() const {
  NOTIMPLEMENTED();
  return false;
}

#if defined(ENABLE_WEBRTC)
WebRtcLogUploader* ChromeBrowserProcessStub::webrtc_log_uploader() {
  NOTIMPLEMENTED();
  return NULL;
}
#endif

network_time::NetworkTimeTracker*
    ChromeBrowserProcessStub::network_time_tracker() {
  NOTIMPLEMENTED();
  return NULL;
}

gcm::GCMDriver* ChromeBrowserProcessStub::gcm_driver() {
  NOTIMPLEMENTED();
  return NULL;
}

shell_integration::DefaultWebClientState
ChromeBrowserProcessStub::CachedDefaultWebClientState() {
  NOTIMPLEMENTED();
  return shell_integration::UNKNOWN_DEFAULT;
}

memory::TabManager* ChromeBrowserProcessStub::GetTabManager() {
  NOTIMPLEMENTED();
  return NULL;
}

content::BrowserContext*
ChromeBrowserProcessStub::GetBrowserContextRedirectedInIncognito(
    content::BrowserContext* context) {
  return CefBrowserContextImpl::GetForContext(context).get();
}

content::BrowserContext*
ChromeBrowserProcessStub::GetBrowserContextOwnInstanceInIncognito(
    content::BrowserContext* context) {
  return GetBrowserContextRedirectedInIncognito(context);
}
