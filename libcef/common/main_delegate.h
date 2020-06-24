// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_MAIN_DELEGATE_H_
#define CEF_LIBCEF_COMMON_MAIN_DELEGATE_H_
#pragma once

#include <string>

#include "include/cef_app.h"
#include "libcef/common/content_client.h"

#include "base/compiler_specific.h"
#include "content/public/app/content_main_delegate.h"

namespace base {
class CommandLine;
}

class CefContentBrowserClient;
class CefContentRendererClient;
class ChromeContentUtilityClient;

// Manages state specific to the CEF runtime.
class CefMainDelegate : public content::ContentMainDelegate {
 public:
  class Runner {
   public:
    virtual void PreCreateMainMessageLoop() = 0;
    virtual int RunMainProcess(
        const content::MainFunctionParams& main_function_params) = 0;

   protected:
    virtual ~Runner() {}
  };

  // |runner| and |settings| will be non-nullptr for the main process only,
  // and will outlive this object.
  CefMainDelegate(Runner* runner,
                  CefSettings* settings,
                  CefRefPtr<CefApp> application);
  ~CefMainDelegate() override;

  void PreCreateMainMessageLoop() override;
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  void SandboxInitialized(const std::string& process_type) override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  void ProcessExiting(const std::string& process_type) override;
#if defined(OS_LINUX)
  void ZygoteForked() override;
#endif
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;

  CefContentBrowserClient* browser_client() { return browser_client_.get(); }
  CefContentClient* content_client() { return &content_client_; }

  // Called from MainRunner at various initialization/shutdown stages to create
  // and clean up global state.
  static void CefInitialize();
  static void MainThreadInitialize();
  static void UIThreadInitialize();
  static void UIThreadShutdown();
  static void MainThreadShutdown();

 private:
  void InitializeResourceBundle();

  Runner* const runner_;
  CefSettings* const settings_;

  std::unique_ptr<CefContentBrowserClient> browser_client_;
  std::unique_ptr<CefContentRendererClient> renderer_client_;
  std::unique_ptr<ChromeContentUtilityClient> utility_client_;
  CefContentClient content_client_;

  DISALLOW_COPY_AND_ASSIGN(CefMainDelegate);
};

#endif  // CEF_LIBCEF_COMMON_MAIN_DELEGATE_H_
