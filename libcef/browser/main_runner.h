// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MAIN_RUNNER_H_
#define CEF_LIBCEF_BROWSER_MAIN_RUNNER_H_
#pragma once

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "cef/include/cef_app.h"
#include "cef/libcef/browser/ui_thread.h"
#include "cef/libcef/common/chrome/chrome_main_delegate_cef.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/browser/browser_main_runner.h"

namespace base {
class WaitableEvent;
}

// Manages the main process lifespan and related objects.
class CefMainRunner final {
 public:
  CefMainRunner(bool multi_threaded_message_loop, bool external_message_pump);

  CefMainRunner(const CefMainRunner&) = delete;
  CefMainRunner& operator=(const CefMainRunner&) = delete;

  // Called from CefContext::Initialize.
  bool Initialize(CefSettings* settings,
                  CefRefPtr<CefApp> application,
                  const CefMainArgs& args,
                  void* windows_sandbox_info,
                  bool* initialized,
                  base::OnceClosure context_initialized);

  // Only valid after Initialize is called.
  int exit_code() const { return exit_code_; }

  // Called from CefContext::Shutdown.
  void Shutdown(base::OnceClosure shutdown_on_ui_thread,
                base::OnceClosure finalize_shutdown);

  void RunMessageLoop();
  void QuitMessageLoop();

  // Called from CefExecuteProcess.
  static int RunAsHelperProcess(const CefMainArgs& args,
                                CefRefPtr<CefApp> application,
                                void* windows_sandbox_info);

  // Called from ChromeMainDelegateCef.
  void PreBrowserMain();
  int RunMainProcess(content::MainFunctionParams main_function_params);

 private:
  // Called from Initialize().
  int ContentMainInitialize(const CefMainArgs& args,
                            void* windows_sandbox_info,
                            int* no_sandbox,
                            bool disable_signal_handlers);

  int ContentMainRun(bool* initialized, base::OnceClosure context_initialized);

  static void BeforeMainInitialize(const CefMainArgs& args);
  bool HandleMainMessageLoopQuit();

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

  void BeforeUIThreadInitialize();
  void BeforeUIThreadShutdown();

  const bool multi_threaded_message_loop_;
  const bool external_message_pump_;

  std::unique_ptr<content::ContentMainRunner> main_runner_;

  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
  std::unique_ptr<CefUIThread> ui_thread_;

  // Used to quit the current base::RunLoop.
  base::OnceClosure quit_callback_;

  int exit_code_ = -1;

  std::unique_ptr<ChromeMainDelegateCef> main_delegate_;

  std::unique_ptr<MainThreadStackSamplingProfiler> sampling_profiler_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  raw_ptr<CefSettings> settings_ = nullptr;
  CefRefPtr<CefApp> application_;
};

#endif  // CEF_LIBCEF_BROWSER_MAIN_RUNNER_H_
