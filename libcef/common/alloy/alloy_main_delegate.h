// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_ALLOY_ALLOY_MAIN_DELEGATE_H_
#define CEF_LIBCEF_COMMON_ALLOY_ALLOY_MAIN_DELEGATE_H_
#pragma once

#include <string>

#include "base/memory/raw_ptr.h"
#include "cef/include/cef_app.h"
#include "cef/libcef/common/alloy/alloy_content_client.h"
#include "cef/libcef/common/app_manager.h"
#include "cef/libcef/common/main_runner_handler.h"
#include "cef/libcef/common/resource_bundle_delegate.h"
#include "cef/libcef/common/task_runner_manager.h"
#include "content/public/app/content_main_delegate.h"

#if BUILDFLAG(IS_WIN)
#include "components/spellcheck/common/spellcheck_features.h"
#endif

namespace base {
class CommandLine;
}

class AlloyContentBrowserClient;
class AlloyContentRendererClient;
class ChromeContentUtilityClient;

// Manages state specific to the CEF runtime.
class AlloyMainDelegate : public content::ContentMainDelegate,
                          public CefAppManager,
                          public CefTaskRunnerManager {
 public:
  // |runner| and |settings| will be non-nullptr for the main process only,
  // and will outlive this object.
  AlloyMainDelegate(CefMainRunnerHandler* runner,
                    CefSettings* settings,
                    CefRefPtr<CefApp> application);

  AlloyMainDelegate(const AlloyMainDelegate&) = delete;
  AlloyMainDelegate& operator=(const AlloyMainDelegate&) = delete;

  ~AlloyMainDelegate() override;

  // content::ContentMainDelegate overrides.
  std::optional<int> PreBrowserMain() override;
  std::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
  std::optional<int> BasicStartupComplete() override;
  void PreSandboxStartup() override;
  absl::variant<int, content::MainFunctionParams> RunProcess(
      const std::string& process_type,
      content::MainFunctionParams main_function_params) override;
  void ProcessExiting(const std::string& process_type) override;
#if BUILDFLAG(IS_LINUX)
  void ZygoteForked() override;
#endif
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;

 protected:
  // CefAppManager overrides.
  CefRefPtr<CefApp> GetApplication() override { return application_; }
  content::ContentClient* GetContentClient() override {
    return &content_client_;
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
  void InitializeResourceBundle();

  const raw_ptr<CefMainRunnerHandler> runner_;
  const raw_ptr<CefSettings> settings_;
  CefRefPtr<CefApp> application_;

  std::unique_ptr<AlloyContentBrowserClient> browser_client_;
  std::unique_ptr<AlloyContentRendererClient> renderer_client_;
  std::unique_ptr<ChromeContentUtilityClient> utility_client_;
  AlloyContentClient content_client_;

  CefResourceBundleDelegate resource_bundle_delegate_;

#if BUILDFLAG(IS_WIN)
  // TODO: Add support for windows spellcheck service (see issue #3055).
  spellcheck::ScopedDisableBrowserSpellCheckerForTesting
      disable_browser_spellchecker_;
#endif
};

#endif  // CEF_LIBCEF_COMMON_ALLOY_ALLOY_MAIN_DELEGATE_H_
