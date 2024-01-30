// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/chrome/chrome_main_runner_delegate.h"

#include "libcef/common/app_manager.h"
#include "libcef/common/chrome/chrome_main_delegate_cef.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_process_singleton.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/metrics/persistent_system_profile.h"

ChromeMainRunnerDelegate::ChromeMainRunnerDelegate(
    CefMainRunnerHandler* runner,
    CefSettings* settings,
    CefRefPtr<CefApp> application)
    : runner_(runner), settings_(settings), application_(application) {}

ChromeMainRunnerDelegate::~ChromeMainRunnerDelegate() = default;

content::ContentMainDelegate*
ChromeMainRunnerDelegate::GetContentMainDelegate() {
  if (!main_delegate_) {
    main_delegate_ = std::make_unique<ChromeMainDelegateCef>(runner_, settings_,
                                                             application_);
  }
  return main_delegate_.get();
}

void ChromeMainRunnerDelegate::BeforeMainThreadInitialize(
    const CefMainArgs& args) {
#if BUILDFLAG(IS_WIN)
  base::CommandLine::Init(0, nullptr);
#else
  base::CommandLine::Init(args.argc, args.argv);
#endif
}

void ChromeMainRunnerDelegate::BeforeMainThreadRun(
    bool multi_threaded_message_loop) {
  if (multi_threaded_message_loop) {
    multi_threaded_message_loop_ = true;

    // Detach from the main thread so that these objects can be attached and
    // modified from the UI thread going forward.
    metrics::GlobalPersistentSystemProfile::GetInstance()
        ->DetachFromCurrentThread();
  }
}

void ChromeMainRunnerDelegate::BeforeMainMessageLoopRun(
    base::RunLoop* run_loop) {
  // May be nullptr if content::ContentMainRun exits early.
  if (!g_browser_process) {
    return;
  }

  // The ScopedKeepAlive instance triggers shutdown logic when released on the
  // UI thread before terminating the message loop (e.g. from CefQuitMessageLoop
  // or FinishShutdownOnUIThread when running with multi-threaded message loop).
  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::APP_CONTROLLER, KeepAliveRestartOption::DISABLED);

  // The QuitClosure will be executed from BrowserProcessImpl::Unpin() via
  // KeepAliveRegistry when the last ScopedKeepAlive is released.
  // ScopedKeepAlives are also held by Browser objects.
  static_cast<BrowserProcessImpl*>(g_browser_process)
      ->SetQuitClosure(run_loop->QuitClosure());
}

bool ChromeMainRunnerDelegate::HandleMainMessageLoopQuit() {
  // May be nullptr if content::ContentMainRun exits early.
  if (!g_browser_process) {
    // Proceed with direct execution of the QuitClosure().
    return false;
  }

  // May be called multiple times. See comments in RunMainMessageLoopBefore.
  keep_alive_.reset();

  // Cancel direct execution of the QuitClosure() in
  // CefMainRunner::QuitMessageLoop. We instead wait for all Chrome browser
  // windows to exit.
  return true;
}

void ChromeMainRunnerDelegate::BeforeUIThreadInitialize() {
  sampling_profiler_ = std::make_unique<MainThreadStackSamplingProfiler>();
}

void ChromeMainRunnerDelegate::BeforeUIThreadShutdown() {
  static_cast<ChromeContentBrowserClient*>(
      CefAppManager::Get()->GetContentClient()->browser())
      ->CleanupOnUIThread();
  main_delegate_->CleanupOnUIThread();

  sampling_profiler_.reset();
}

void ChromeMainRunnerDelegate::AfterUIThreadShutdown() {
  if (multi_threaded_message_loop_) {
    // Don't wait for this to be called in ChromeMainDelegate::ProcessExiting.
    // It is safe to call multiple times.
    ChromeProcessSingleton::DeleteInstance();
  }
}

void ChromeMainRunnerDelegate::BeforeExecuteProcess(const CefMainArgs& args) {
  BeforeMainThreadInitialize(args);
}

void ChromeMainRunnerDelegate::AfterExecuteProcess() {
  AfterMainThreadShutdown();
}
