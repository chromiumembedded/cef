// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_BROWSER_PROCESS_STUB_H_
#define CEF_LIBCEF_BROWSER_CHROME_BROWSER_PROCESS_STUB_H_

#include <string>

#include "chrome/browser/browser_process.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

// This file provides a stub implementation of Chrome's BrowserProcess object
// for use as an interop layer between CEF and files that live in chrome/.

class BackgroundModeManager {
 public:
  BackgroundModeManager();
  virtual ~BackgroundModeManager();
 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundModeManager);
};

class ChromeBrowserProcessStub : public BrowserProcess {
 public:
  ChromeBrowserProcessStub();
  ~ChromeBrowserProcessStub() override;

  // BrowserProcess implementation.
  void ResourceDispatcherHostCreated() override;
  void EndSession() override;
  metrics_services_manager::MetricsServicesManager*
      GetMetricsServicesManager() override;
  metrics::MetricsService* metrics_service() override;
  rappor::RapporService* rappor_service() override;
  IOThread* io_thread() override;
  WatchDogThread* watchdog_thread() override;
  ProfileManager* profile_manager() override;
  PrefService* local_state() override;
  net::URLRequestContextGetter* system_request_context() override;
  variations::VariationsService* variations_service() override;
  web_resource::PromoResourceService* promo_resource_service() override;
  BrowserProcessPlatformPart* platform_part() override;
  extensions::EventRouterForwarder*
      extension_event_router_forwarder() override;
  NotificationUIManager* notification_ui_manager() override;
  message_center::MessageCenter* message_center() override;
  policy::BrowserPolicyConnector* browser_policy_connector() override;
  policy::PolicyService* policy_service() override;
  IconManager* icon_manager() override;
  GLStringManager* gl_string_manager() override;
  GpuModeManager* gpu_mode_manager() override;
  void CreateDevToolsHttpProtocolHandler(
      chrome::HostDesktopType host_desktop_type,
      const std::string& ip,
      uint16_t port) override;
  unsigned int AddRefModule() override;
  unsigned int ReleaseModule() override;
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
      scoped_ptr<BackgroundModeManager> manager) override;
  StatusTray* status_tray() override;
  safe_browsing::SafeBrowsingService* safe_browsing_service() override;
  safe_browsing::ClientSideDetectionService*
      safe_browsing_detection_service() override;

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
#if defined(ENABLE_WEBRTC)
  WebRtcLogUploader* webrtc_log_uploader() override;
#endif
  network_time::NetworkTimeTracker* network_time_tracker() override;
  gcm::GCMDriver* gcm_driver() override;
  ShellIntegration::DefaultWebClientState
      CachedDefaultWebClientState() override;
  memory::TabManager* GetTabManager() override;

 private:
  std::string locale_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserProcessStub);
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_BROWSER_PROCESS_STUB_H_
