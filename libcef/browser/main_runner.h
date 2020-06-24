// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MAIN_RUNNER_H_
#define CEF_LIBCEF_BROWSER_MAIN_RUNNER_H_
#pragma once

#include "libcef/common/main_delegate.h"

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/public/browser/browser_main_runner.h"

namespace base {
class WaitableEvent;
}

namespace content {
class ContentMainDelegate;
class ContentServiceManagerMainDelegate;
struct MainFunctionParams;
}  // namespace content

namespace service_manager {
struct MainParams;
}

class CefUIThread;

// Manages the main process lifespan and related objects.
class CefMainRunner : public CefMainDelegate::Runner {
 public:
  CefMainRunner(bool multi_threaded_message_loop, bool external_message_pump);
  ~CefMainRunner();

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

  bool IsCefRuntime() const { return runtime_type_ == RuntimeType::CEF; }

  // Called from CefExecuteProcess.
  static int RunAsHelperProcess(const CefMainArgs& args,
                                CefRefPtr<CefApp> application,
                                void* windows_sandbox_info);

 private:
  // Called from Initialize().
  void CefRuntimeInitialize(CefSettings* settings, CefRefPtr<CefApp> app);
  int ContentMainInitialize(const CefMainArgs& args,
                            void* windows_sandbox_info,
                            int* no_sandbox);
  bool ContentMainRun(bool* initialized, base::OnceClosure context_initialized);

  // CefMainDelegate::Runner methods:
  void PreCreateMainMessageLoop() override;
  int RunMainProcess(
      const content::MainFunctionParams& main_function_params) override;

  // Create the UI thread when running with multi-threaded message loop mode.
  bool CreateUIThread(base::OnceClosure setup_callback);

  // Called on the UI thread after the context is initialized.
  void OnContextInitialized(base::OnceClosure context_initialized);

  // Performs shutdown actions that need to occur on the UI thread before any
  // threads are destroyed.
  void FinishShutdownOnUIThread(base::OnceClosure shutdown_on_ui_thread,
                                base::WaitableEvent* uithread_shutdown_event);

  // Destroys the runtime and related objects.
  void FinalizeShutdown(base::OnceClosure finalize_shutdown);

  const bool multi_threaded_message_loop_;
  const bool external_message_pump_;

  enum class RuntimeType {
    UNINITIALIZED,
    CEF,
  };
  RuntimeType runtime_type_ = RuntimeType::UNINITIALIZED;

  std::unique_ptr<content::ContentMainDelegate> main_delegate_;
  std::unique_ptr<content::ContentServiceManagerMainDelegate> sm_main_delegate_;
  std::unique_ptr<service_manager::MainParams> sm_main_params_;

  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
  std::unique_ptr<CefUIThread> ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(CefMainRunner);
};

#endif  // CEF_LIBCEF_BROWSER_MAIN_RUNNER_H_
