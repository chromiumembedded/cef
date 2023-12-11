// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_CHROME_CHROME_MAIN_RUNNER_DELEGATE_CEF_
#define CEF_LIBCEF_COMMON_CHROME_CHROME_MAIN_RUNNER_DELEGATE_CEF_

#include <memory>

#include "include/cef_app.h"
#include "libcef/common/main_runner_delegate.h"
#include "libcef/common/main_runner_handler.h"

class ChromeMainDelegateCef;
class MainThreadStackSamplingProfiler;
class ScopedKeepAlive;

class ChromeMainRunnerDelegate : public CefMainRunnerDelegate {
 public:
  // |runner| will be non-nullptr for the main process only, and will outlive
  // this object.
  ChromeMainRunnerDelegate(CefMainRunnerHandler* runner,
                           CefSettings* settings,
                           CefRefPtr<CefApp> application);

  ChromeMainRunnerDelegate(const ChromeMainRunnerDelegate&) = delete;
  ChromeMainRunnerDelegate& operator=(const ChromeMainRunnerDelegate&) = delete;

  ~ChromeMainRunnerDelegate() override;

 protected:
  // CefMainRunnerDelegate overrides.
  content::ContentMainDelegate* GetContentMainDelegate() override;
  void BeforeMainThreadInitialize(const CefMainArgs& args) override;
  void BeforeMainThreadRun(bool multi_threaded_message_loop) override;
  void BeforeMainMessageLoopRun(base::RunLoop* run_loop) override;
  bool HandleMainMessageLoopQuit() override;
  void BeforeUIThreadInitialize() override;
  void BeforeUIThreadShutdown() override;
  void AfterUIThreadShutdown() override;
  void BeforeExecuteProcess(const CefMainArgs& args) override;
  void AfterExecuteProcess() override;

 private:
  std::unique_ptr<ChromeMainDelegateCef> main_delegate_;

  std::unique_ptr<MainThreadStackSamplingProfiler> sampling_profiler_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  CefMainRunnerHandler* const runner_;
  CefSettings* const settings_;
  CefRefPtr<CefApp> application_;

  bool multi_threaded_message_loop_ = false;
};

#endif  // CEF_LIBCEF_COMMON_CHROME_CHROME_MAIN_RUNNER_DELEGATE_CEF_
