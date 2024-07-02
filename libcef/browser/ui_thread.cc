// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/ui_thread.h"

#include "base/at_exit.h"
#include "base/logging.h"
#include "cef/libcef/browser/main_runner.h"
#include "cef/libcef/browser/thread_util.h"
#include "content/browser/scheduler/browser_task_executor.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/base/ozone_buildflags.h"
#if BUILDFLAG(IS_OZONE_X11)
#include "ui/ozone/platform/x11/ozone_platform_x11.h"
#endif
#endif

#if BUILDFLAG(IS_WIN)
#include <Objbase.h>
#endif

CefUIThread::CefUIThread(CefMainRunner* runner,
                         base::OnceClosure setup_callback)
    : runner_(runner), setup_callback_(std::move(setup_callback)) {}

CefUIThread::~CefUIThread() {
  Stop();
}

void CefUIThread::Start() {
  base::AutoLock lock(thread_lock_);
  bool success = base::PlatformThread::CreateWithType(
      0, this, &thread_, base::ThreadType::kDefault);
  if (!success) {
    LOG(FATAL) << "failed to UI create thread";
  }
}

void CefUIThread::Stop() {
  base::AutoLock lock(thread_lock_);

  if (!stopping_) {
    stopping_ = true;
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefMainRunner::QuitMessageLoop,
                                          base::Unretained(runner_)));
  }

  // Can't join if the |thread_| is either already gone or is non-joinable.
  if (thread_.is_null()) {
    return;
  }

  base::PlatformThread::Join(thread_);
  thread_ = base::PlatformThreadHandle();

  stopping_ = false;
}

bool CefUIThread::WaitUntilThreadStarted() const {
  DCHECK(owning_sequence_checker_.CalledOnValidSequence());
  start_event_.Wait();
  return true;
}

void CefUIThread::InitializeBrowserRunner(
    content::MainFunctionParams main_function_params) {
#if BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_OZONE_X11)
  // Disable creation of GtkUi (interface to GTK desktop features) and cause
  // ui::GetDefaultLinuxUi() (and related functions) to return nullptr. We
  // can't use GtkUi in combination with multi-threaded-message-loop because
  // Chromium's GTK implementation doesn't use GDK threads. Light/dark theme
  // changes will still be detected via DarkModeManagerLinux.
  ui::SetMultiThreadedMessageLoopX11();
#endif
#endif

  // Use our own browser process runner.
  browser_runner_ = content::BrowserMainRunner::Create();

  // Initialize browser process state. Uses the current thread's message loop.
  int exit_code = browser_runner_->Initialize(std::move(main_function_params));
  CHECK_EQ(exit_code, -1);
}

void CefUIThread::ThreadMain() {
  base::PlatformThread::SetName("CefUIThread");

#if BUILDFLAG(IS_WIN)
  // Initializes the COM library on the current thread.
  CoInitialize(nullptr);
#endif

  start_event_.Signal();

  std::move(setup_callback_).Run();

  runner_->RunMessageLoop();

  // Stop may be called before InitializeBrowserRunner if
  // content::ContentMainRun was not successful (for example, due to process
  // singleton relaunch).
  if (browser_runner_) {
    browser_runner_->Shutdown();
    browser_runner_.reset();
  }

  // This will be a no-op if there is no BrowserTaskExecutor.
  content::BrowserTaskExecutor::Shutdown();

  if (!shutdown_callback_.is_null()) {
    std::move(shutdown_callback_).Run();
  }

  // Run exit callbacks on the UI thread to avoid sequence check failures.
  base::AtExitManager::ProcessCallbacksNow();

#if BUILDFLAG(IS_WIN)
  // Closes the COM library on the current thread. CoInitialize must
  // be balanced by a corresponding call to CoUninitialize.
  CoUninitialize();
#endif
}
