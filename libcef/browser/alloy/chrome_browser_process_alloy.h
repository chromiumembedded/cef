// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a stub implementation of Chrome's BrowserProcess object
// for use as an interop layer between CEF and files that live in chrome/.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_CHROME_BROWSER_PROCESS_ALLOY_H_
#define CEF_LIBCEF_BROWSER_ALLOY_CHROME_BROWSER_PROCESS_ALLOY_H_

#include <memory>
#include <string>

#include "base/metrics/field_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "media/media_buildflags.h"

namespace extensions {
class ExtensionsBrowserClient;
class ExtensionsClient;
}  // namespace extensions

class ChromeProfileManagerAlloy;

class BackgroundModeManager {
 public:
  BackgroundModeManager();

  BackgroundModeManager(const BackgroundModeManager&) = delete;
  BackgroundModeManager& operator=(const BackgroundModeManager&) = delete;

  virtual ~BackgroundModeManager();
};

class ChromeBrowserProcessAlloy : public BrowserProcess {
 public:
  ChromeBrowserProcessAlloy();

  ChromeBrowserProcessAlloy(const ChromeBrowserProcessAlloy&) = delete;
  ChromeBrowserProcessAlloy& operator=(const ChromeBrowserProcessAlloy&) =
      delete;

  ~ChromeBrowserProcessAlloy() override;

  void Initialize();
  void OnContextInitialized();
  void CleanupOnUIThread();

  // BrowserProcess implementation.
  void EndSession() override;
  void FlushLocalStateAndReply(base::OnceClosure reply) override;
  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager()
      override;
  metrics::MetricsService* metrics_service() override;
  SystemNetworkContextManager* system_network_context_manager() override;
  network::NetworkQualityTracker* network_quality_tracker() override;
  embedder_support::OriginTrialsSettingsStorage*
  GetOriginTrialsSettingsStorage() override;
  ProfileManager* profile_manager() override;
  PrefService* local_state() override;
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
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  BackgroundModeManager* background_mode_manager() override;
  void set_background_mode_manager_for_test(
      std::unique_ptr<BackgroundModeManager> manager) override;
#endif
  StatusTray* status_tray() override;
  safe_browsing::SafeBrowsingService* safe_browsing_service() override;
  subresource_filter::RulesetService* subresource_filter_ruleset_service()
      override;
  StartupData* startup_data() override;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  void StartAutoupdateTimer() override;
#endif

  component_updater::ComponentUpdateService* component_updater() override;
  MediaFileSystemRegistry* media_file_system_registry() override;
  WebRtcLogUploader* webrtc_log_uploader() override;
  network_time::NetworkTimeTracker* network_time_tracker() override;
  gcm::GCMDriver* gcm_driver() override;
  resource_coordinator::TabManager* GetTabManager() override;
  resource_coordinator::ResourceCoordinatorParts* resource_coordinator_parts()
      override;
  os_crypt_async::OSCryptAsync* os_crypt_async() override;
  BuildState* GetBuildState() override;
  SerialPolicyAllowedPorts* serial_policy_allowed_ports() override;
  HidSystemTrayIcon* hid_system_tray_icon() override;
  UsbSystemTrayIcon* usb_system_tray_icon() override;

 private:
  bool initialized_ = false;
  bool context_initialized_ = false;
  bool shutdown_ = false;

  std::unique_ptr<extensions::ExtensionsClient> extensions_client_;
  std::unique_ptr<extensions::ExtensionsBrowserClient>
      extensions_browser_client_;

  std::string locale_;
  std::unique_ptr<printing::PrintJobManager> print_job_manager_;
  std::unique_ptr<ChromeProfileManagerAlloy> profile_manager_;
  scoped_refptr<extensions::EventRouterForwarder> event_router_forwarder_;
  std::unique_ptr<printing::PrintPreviewDialogController>
      print_preview_dialog_controller_;
  std::unique_ptr<printing::BackgroundPrintingManager>
      background_printing_manager_;
  std::unique_ptr<PrefService> local_state_;

  // Must be destroyed after |local_state_|.
  std::unique_ptr<policy::ChromeBrowserPolicyConnector>
      browser_policy_connector_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  std::unique_ptr<component_updater::ComponentUpdateService> component_updater_;

  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_CHROME_BROWSER_PROCESS_ALLOY_H_
