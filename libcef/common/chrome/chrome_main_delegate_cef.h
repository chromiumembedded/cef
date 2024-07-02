// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_CHROME_CHROME_MAIN_DELEGATE_CEF_
#define CEF_LIBCEF_COMMON_CHROME_CHROME_MAIN_DELEGATE_CEF_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "cef/include/cef_app.h"
#include "cef/libcef/common/app_manager.h"
#include "cef/libcef/common/chrome/chrome_content_client_cef.h"
#include "cef/libcef/common/resource_bundle_delegate.h"
#include "cef/libcef/common/task_runner_manager.h"
#include "chrome/app/chrome_main_delegate.h"

class CefMainRunner;
class ChromeContentBrowserClientCef;
class ChromeContentRendererClientCef;

// CEF override of ChromeMainDelegate
class ChromeMainDelegateCef : public ChromeMainDelegate,
                              public CefAppManager,
                              public CefTaskRunnerManager {
 public:
  // |runner| will be non-nullptr for the main process only, and will outlive
  // this object.
  ChromeMainDelegateCef(CefMainRunner* runner,
                        CefSettings* settings,
                        CefRefPtr<CefApp> application);

  ChromeMainDelegateCef(const ChromeMainDelegateCef&) = delete;
  ChromeMainDelegateCef& operator=(const ChromeMainDelegateCef&) = delete;

  ~ChromeMainDelegateCef() override;

 protected:
  // ChromeMainDelegate overrides.
  std::optional<int> BasicStartupComplete() override;
  void PreSandboxStartup() override;
  void SandboxInitialized(const std::string& process_type) override;
  std::optional<int> PreBrowserMain() override;
  std::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
  absl::variant<int, content::MainFunctionParams> RunProcess(
      const std::string& process_type,
      content::MainFunctionParams main_function_params) override;
#if BUILDFLAG(IS_LINUX)
  void ZygoteForked() override;
#endif
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  ui::ResourceBundle::Delegate* GetResourceBundleDelegate() override {
    return &resource_bundle_delegate_;
  }

  // CefAppManager overrides.
  CefRefPtr<CefApp> GetApplication() override { return application_; }
  content::ContentClient* GetContentClient() override {
    return &chrome_content_client_cef_;
  }
  CefRefPtr<CefRequestContext> GetGlobalRequestContext() override;
  CefBrowserContext* CreateNewBrowserContext(
      const CefRequestContextSettings& settings,
      base::OnceClosure initialized_cb) override;

  // CefTaskRunnerManager overrides.
  scoped_refptr<base::SingleThreadTaskRunner> GetBackgroundTaskRunner()
      override;
  scoped_refptr<base::SingleThreadTaskRunner> GetUserVisibleTaskRunner()
      override;
  scoped_refptr<base::SingleThreadTaskRunner> GetUserBlockingTaskRunner()
      override;
  scoped_refptr<base::SingleThreadTaskRunner> GetRenderTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetWebWorkerTaskRunner() override;

 private:
  ChromeContentBrowserClientCef* content_browser_client() const;
  ChromeContentRendererClientCef* content_renderer_client() const;

  const raw_ptr<CefMainRunner> runner_;
  const raw_ptr<CefSettings> settings_;
  CefRefPtr<CefApp> application_;

  // We use this instead of ChromeMainDelegate::chrome_content_client_.
  ChromeContentClientCef chrome_content_client_cef_;

  CefResourceBundleDelegate resource_bundle_delegate_;
};

#endif  // CEF_LIBCEF_COMMON_CHROME_CHROME_MAIN_DELEGATE_CEF_
