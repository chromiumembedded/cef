// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_MAIN_DELEGATE_H_
#define CEF_LIBCEF_COMMON_MAIN_DELEGATE_H_
#pragma once

#include <string>

#include "libcef/common/content_client.h"
#include "include/cef_app.h"

#include "base/compiler_specific.h"
#include "content/public/app/content_main_delegate.h"

namespace base {
class CommandLine;
class Thread;
}

namespace content {
class BrowserMainRunner;
}

class CefContentBrowserClient;
class CefContentRendererClient;
class CefContentUtilityClient;

class CefMainDelegate : public content::ContentMainDelegate {
 public:
  explicit CefMainDelegate(CefRefPtr<CefApp> application);
  ~CefMainDelegate() override;

  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  void SandboxInitialized(const std::string& process_type) override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  void ProcessExiting(const std::string& process_type) override;
#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
  void ZygoteForked() override;
#endif
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient*
      CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;

  // Shut down the browser runner.
  void ShutdownBrowser();

  CefContentBrowserClient* browser_client() { return browser_client_.get(); }
  CefContentClient* content_client() { return &content_client_; }

 private:
  void InitializeResourceBundle();

  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
  std::unique_ptr<base::Thread> ui_thread_;

  std::unique_ptr<CefContentBrowserClient> browser_client_;
  std::unique_ptr<CefContentRendererClient> renderer_client_;
  std::unique_ptr<CefContentUtilityClient> utility_client_;
  CefContentClient content_client_;

  DISALLOW_COPY_AND_ASSIGN(CefMainDelegate);
};

#endif  // CEF_LIBCEF_COMMON_MAIN_DELEGATE_H_
