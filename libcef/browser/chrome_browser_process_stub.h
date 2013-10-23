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

// This file provides a stub implementation of chrome::BrowserProcess so that
// PrintJobWorker can determine the current locale.

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
  virtual ~ChromeBrowserProcessStub();

  // BrowserProcess implementation.
  virtual void ResourceDispatcherHostCreated() OVERRIDE;
  virtual void EndSession() OVERRIDE;
  virtual MetricsService* metrics_service() OVERRIDE;
  virtual IOThread* io_thread() OVERRIDE;
  virtual WatchDogThread* watchdog_thread() OVERRIDE;
  virtual ProfileManager* profile_manager() OVERRIDE;
  virtual PrefService* local_state() OVERRIDE;
  virtual net::URLRequestContextGetter* system_request_context() OVERRIDE;
  virtual chrome_variations::VariationsService* variations_service() OVERRIDE;
  virtual BrowserProcessPlatformPart* platform_part() OVERRIDE;
  virtual extensions::EventRouterForwarder*
        extension_event_router_forwarder() OVERRIDE;
  virtual NotificationUIManager* notification_ui_manager() OVERRIDE;
  virtual message_center::MessageCenter* message_center() OVERRIDE;
  virtual policy::BrowserPolicyConnector* browser_policy_connector() OVERRIDE;
  virtual policy::PolicyService* policy_service() OVERRIDE;
  virtual IconManager* icon_manager() OVERRIDE;
  virtual GLStringManager* gl_string_manager() OVERRIDE;
  virtual GpuModeManager* gpu_mode_manager() OVERRIDE;
  virtual RenderWidgetSnapshotTaker* GetRenderWidgetSnapshotTaker() OVERRIDE;
  virtual AutomationProviderList* GetAutomationProviderList() OVERRIDE;
  virtual void CreateDevToolsHttpProtocolHandler(
      chrome::HostDesktopType host_desktop_type,
      const std::string& ip,
      int port,
      const std::string& frontend_url) OVERRIDE;
  virtual unsigned int AddRefModule() OVERRIDE;
  virtual unsigned int ReleaseModule() OVERRIDE;
  virtual bool IsShuttingDown() OVERRIDE;
  virtual printing::PrintJobManager* print_job_manager() OVERRIDE;
  virtual printing::PrintPreviewDialogController*
      print_preview_dialog_controller() OVERRIDE;
  virtual printing::BackgroundPrintingManager*
      background_printing_manager() OVERRIDE;
  virtual IntranetRedirectDetector* intranet_redirect_detector() OVERRIDE;
  virtual const std::string& GetApplicationLocale() OVERRIDE;
  virtual void SetApplicationLocale(const std::string& locale) OVERRIDE;
  virtual DownloadStatusUpdater* download_status_updater() OVERRIDE;
  virtual DownloadRequestLimiter* download_request_limiter() OVERRIDE;
  virtual BackgroundModeManager* background_mode_manager() OVERRIDE;
  virtual void set_background_mode_manager_for_test(
      scoped_ptr<BackgroundModeManager> manager) OVERRIDE;
  virtual StatusTray* status_tray() OVERRIDE;
  virtual SafeBrowsingService* safe_browsing_service() OVERRIDE;
  virtual safe_browsing::ClientSideDetectionService*
      safe_browsing_detection_service() OVERRIDE;

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  virtual void StartAutoupdateTimer() OVERRIDE;
#endif

  virtual ChromeNetLog* net_log() OVERRIDE;
  virtual prerender::PrerenderTracker* prerender_tracker() OVERRIDE;
  virtual ComponentUpdateService* component_updater() OVERRIDE;
  virtual CRLSetFetcher* crl_set_fetcher() OVERRIDE;
  virtual PnaclComponentInstaller* pnacl_component_installer() OVERRIDE;
  virtual BookmarkPromptController* bookmark_prompt_controller() OVERRIDE;
  virtual MediaFileSystemRegistry*
      media_file_system_registry() OVERRIDE;
  virtual bool created_local_state() const OVERRIDE;
  virtual StorageMonitor* storage_monitor() OVERRIDE;
#if defined(ENABLE_WEBRTC)
  virtual WebRtcLogUploader* webrtc_log_uploader() OVERRIDE;
#endif

 private:
  std::string locale_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserProcessStub);
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_BROWSER_PROCESS_STUB_H_
