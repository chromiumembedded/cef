// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MAIN_RUNNER_H_
#define CEF_LIBCEF_BROWSER_MAIN_RUNNER_H_
#pragma once

#include "include/cef_app.h"
#include "libcef/common/main_runner_delegate.h"
#include "libcef/common/main_runner_handler.h"

#include "base/functional/callback.h"
#include "content/public/browser/browser_main_runner.h"

namespace base {
class WaitableEvent;
}

namespace content {
class ContentMainRunner;
}  // namespace content

class CefUIThread;

// Manages the main process lifespan and related objects.
class CefMainRunner : public CefMainRunnerHandler {
 public:
  CefMainRunner(bool multi_threaded_message_loop, bool external_message_pump);

  CefMainRunner(const CefMainRunner&) = delete;
  CefMainRunner& operator=(const CefMainRunner&) = delete;

  ~CefMainRunner() override;

  // Called from CefContext::Initialize.
  bool Initialize(CefSettings* settings,
                  CefRefPtr<CefApp> application,
                  const CefMainArgs& args,
                  void* windows_sandbox_info,
                  bool* initialized,
                  base::OnceClosure context_initialized);

  // Called from CefContext::Shutdown.
  void Shutdown(base::OnceClosure shutdown_on_ui_thread,
                base::OnceClosure finalize_shutdown);

  void RunMessageLoop();
  void QuitMessageLoop();

  // Called from CefExecuteProcess.
  static int RunAsHelperProcess(const CefMainArgs& args,
                                CefRefPtr<CefApp> application,
                                void* windows_sandbox_info);

 private:
  // Called from Initialize().
  int ContentMainInitialize(const CefMainArgs& args,
                            void* windows_sandbox_info,
                            int* no_sandbox);
  int ContentMainRun(bool* initialized, base::OnceClosure context_initialized);

  // CefMainRunnerHandler methods:
  void PreBrowserMain() override;
  int RunMainProcess(content::MainFunctionParams main_function_params) override;

  // Create the UI thread when running with multi-threaded message loop mode.
  bool CreateUIThread(base::OnceClosure setup_callback);

  // Called on the UI thread after the context is initialized.
  void OnContextInitialized(base::OnceClosure context_initialized);

  // Performs shutdown actions that need to occur on the UI thread before the
  // thread RunLoop has stopped.
  void StartShutdownOnUIThread(base::OnceClosure shutdown_on_ui_thread);

  // Performs shutdown actions that need to occur on the UI thread after the
  // thread RunLoop has stopped and before running exit callbacks.
  void FinishShutdownOnUIThread();

  const bool multi_threaded_message_loop_;
  const bool external_message_pump_;

  std::unique_ptr<CefMainRunnerDelegate> main_delegate_;
  std::unique_ptr<content::ContentMainRunner> main_runner_;

  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
  std::unique_ptr<CefUIThread> ui_thread_;

  // Used to quit the current base::RunLoop.
  base::OnceClosure quit_callback_;
};

#endif  // CEF_LIBCEF_BROWSER_MAIN_RUNNER_H_
