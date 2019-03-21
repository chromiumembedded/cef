// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a stub implementation of Chrome's BrowserProcess object
// for use as an interop layer between CEF and files that live in chrome/.

#ifndef CEF_LIBCEF_BROWSER_CHROME_BROWSER_PROCESS_STUB_H_
#define CEF_LIBCEF_BROWSER_CHROME_BROWSER_PROCESS_STUB_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "media/media_buildflags.h"

class ChromeProfileManagerStub;

class BackgroundModeManager {
 public:
  BackgroundModeManager();
  virtual ~BackgroundModeManager();

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundModeManager);
};

class ChromeBrowserProcessStub : public BrowserProcess,
                                 public chrome::BrowserContextIncognitoHelper {
 public:
  ChromeBrowserProcessStub();
  ~ChromeBrowserProcessStub() override;

  void Initialize();
  void OnContextInitialized();
  void Shutdown();

  // BrowserProcess implementation.
  void ResourceDispatcherHostCreated() override;
  void EndSession() override;
  void FlushLocalStateAndReply(base::OnceClosure reply) override;
  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager()
      override;
  metrics::MetricsService* metrics_service() override;
  rappor::RapporServiceImpl* rappor_service() override;
  IOThread* io_thread() override;
  SystemNetworkContextManager* system_network_context_manager() override;
  net_log::NetExportFileWriter* net_export_file_writer() override;
  network::NetworkQualityTracker* network_quality_tracker() override;
  WatchDogThread* watchdog_thread() override;
  ProfileManager* profile_manager() override;
  PrefService* local_state() override;
  net::URLRequestContextGetter* system_request_context() override;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      override;
  variations::VariationsService* variations_service() override;
  BrowserProcessPlatformPart* platform_part() override;
  extensions::EventRouterForwarder* extension_event_router_forwarder() override;
  NotificationUIManager* notification_ui_manager() override;
  NotificationPlatformBridge* notification_platform_bridge() override;
  policy::ChromeBrowserPolicyConnector* browser_policy_connector() override;
  policy::PolicyService* policy_service() override;
  IconManager* icon_manager() override;
  GpuModeManager* gpu_mode_manager() override;
  void CreateDevToolsProtocolHandler() override;
  void CreateDevToolsAutoOpener() override;
  bool IsShuttingDown() override;
  printing::PrintJobManager* print_job_manager() override;
  printing::PrintPreviewDialogController* print_preview_dialog_controller()
      override;
  printing::BackgroundPrintingManager* background_printing_manager() override;
  IntranetRedirectDetector* intranet_redirect_detector() override;
  const std::string& GetApplicationLocale() override;
  void SetApplicationLocale(const std::string& locale) override;
  DownloadStatusUpdater* download_status_updater() override;
  DownloadRequestLimiter* download_request_limiter() override;
  BackgroundModeManager* background_mode_manager() override;
  void set_background_mode_manager_for_test(
      std::unique_ptr<BackgroundModeManager> manager) override;
  StatusTray* status_tray() override;
  safe_browsing::SafeBrowsingService* safe_browsing_service() override;
  safe_browsing::ClientSideDetectionService* safe_browsing_detection_service()
      override;
  subresource_filter::RulesetService* subresource_filter_ruleset_service()
      override;
  optimization_guide::OptimizationGuideService* optimization_guide_service()
      override;

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  void StartAutoupdateTimer() override;
#endif

  net_log::ChromeNetLog* net_log() override;
  component_updater::ComponentUpdateService* component_updater() override;
  component_updater::SupervisedUserWhitelistInstaller*
  supervised_user_whitelist_installer() override;
  MediaFileSystemRegistry* media_file_system_registry() override;
  WebRtcLogUploader* webrtc_log_uploader() override;
  network_time::NetworkTimeTracker* network_time_tracker() override;
  gcm::GCMDriver* gcm_driver() override;
  shell_integration::DefaultWebClientState CachedDefaultWebClientState()
      override;
  resource_coordinator::TabManager* GetTabManager() override;
  resource_coordinator::ResourceCoordinatorParts* resource_coordinator_parts()
      override;
  prefs::InProcessPrefServiceFactory* pref_service_factory() const override;

  // BrowserContextIncognitoHelper implementation.
  content::BrowserContext* GetBrowserContextRedirectedInIncognito(
      content::BrowserContext* context) override;
  content::BrowserContext* GetBrowserContextOwnInstanceInIncognito(
      content::BrowserContext* context) override;

 private:
  bool initialized_;
  bool context_initialized_;
  bool shutdown_;

  std::string locale_;
  std::unique_ptr<printing::PrintJobManager> print_job_manager_;
  std::unique_ptr<ChromeProfileManagerStub> profile_manager_;
  scoped_refptr<extensions::EventRouterForwarder> event_router_forwarder_;
  std::unique_ptr<net_log::ChromeNetLog> net_log_;
  std::unique_ptr<net_log::NetExportFileWriter> net_export_file_writer_;
  std::unique_ptr<PrefService> local_state_;
  // Must be destroyed after |local_state_|.
  std::unique_ptr<policy::ChromeBrowserPolicyConnector>
      browser_policy_connector_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserProcessStub);
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_BROWSER_PROCESS_STUB_H_
