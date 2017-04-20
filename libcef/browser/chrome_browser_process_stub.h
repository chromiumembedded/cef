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

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "base/compiler_specific.h"
#include "media/media_features.h"

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

  void Initialize(const base::CommandLine& command_line);
  void OnContextInitialized();
  void Shutdown();

  // BrowserProcess implementation.
  void ResourceDispatcherHostCreated() override;
  void EndSession() override;
  metrics_services_manager::MetricsServicesManager*
      GetMetricsServicesManager() override;
  metrics::MetricsService* metrics_service() override;
  rappor::RapporServiceImpl* rappor_service() override;
  IOThread* io_thread() override;
  WatchDogThread* watchdog_thread() override;
  ukm::UkmService* ukm_service() override;
  ProfileManager* profile_manager() override;
  PrefService* local_state() override;
  net::URLRequestContextGetter* system_request_context() override;
  variations::VariationsService* variations_service() override;
  BrowserProcessPlatformPart* platform_part() override;
  extensions::EventRouterForwarder*
      extension_event_router_forwarder() override;
  NotificationUIManager* notification_ui_manager() override;
  NotificationPlatformBridge* notification_platform_bridge() override;
  message_center::MessageCenter* message_center() override;
  policy::BrowserPolicyConnector* browser_policy_connector() override;
  policy::PolicyService* policy_service() override;
  IconManager* icon_manager() override;
  GpuModeManager* gpu_mode_manager() override;
  GpuProfileCache* gpu_profile_cache() override;
  void CreateDevToolsHttpProtocolHandler(const std::string& ip,
                                         uint16_t port) override;
  void CreateDevToolsAutoOpener() override;
  bool IsShuttingDown() override;
  printing::PrintJobManager* print_job_manager() override;
  printing::PrintPreviewDialogController*
      print_preview_dialog_controller() override;
  printing::BackgroundPrintingManager*
      background_printing_manager() override;
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
  safe_browsing::ClientSideDetectionService*
      safe_browsing_detection_service() override;
  subresource_filter::ContentRulesetService*
      subresource_filter_ruleset_service() override;

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  void StartAutoupdateTimer() override;
#endif

  net_log::ChromeNetLog* net_log() override;
  component_updater::ComponentUpdateService*
      component_updater() override;
  CRLSetFetcher* crl_set_fetcher() override;
  component_updater::PnaclComponentInstaller*
      pnacl_component_installer() override;
  component_updater::SupervisedUserWhitelistInstaller*
      supervised_user_whitelist_installer() override;
  MediaFileSystemRegistry*
      media_file_system_registry() override;
  bool created_local_state() const override;
#if BUILDFLAG(ENABLE_WEBRTC)
  WebRtcLogUploader* webrtc_log_uploader() override;
#endif
  network_time::NetworkTimeTracker* network_time_tracker() override;
  gcm::GCMDriver* gcm_driver() override;
  shell_integration::DefaultWebClientState
      CachedDefaultWebClientState() override;
  memory::TabManager* GetTabManager() override;
  physical_web::PhysicalWebDataSource* GetPhysicalWebDataSource() override;

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

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserProcessStub);
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_BROWSER_PROCESS_STUB_H_
