// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/main_runner.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "cef/libcef/browser/browser_message_loop.h"
#include "cef/libcef/browser/chrome/chrome_content_browser_client_cef.h"
#include "cef/libcef/browser/crashpad_runner.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/app_manager.h"
#include "cef/libcef/common/cef_switches.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_process_singleton.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "components/crash/core/app/crash_switches.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/metrics/persistent_system_profile.h"
#include "content/app/content_main_runner_impl.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <memory>

#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

CefMainRunner::CefMainRunner(bool multi_threaded_message_loop,
                             bool external_message_pump)
    : multi_threaded_message_loop_(multi_threaded_message_loop),
      external_message_pump_(external_message_pump) {}

bool CefMainRunner::Initialize(CefSettings* settings,
                               CefRefPtr<CefApp> application,
                               const CefMainArgs& args,
                               void* windows_sandbox_info,
                               bool* initialized,
                               base::OnceClosure context_initialized) {
  settings_ = settings;
  application_ = application;

  exit_code_ =
      ContentMainInitialize(args, windows_sandbox_info, &settings->no_sandbox,
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
                            settings->disable_signal_handlers
#else
                            false
#endif
      );
  if (exit_code_ >= 0) {
    LOG(ERROR) << "ContentMainInitialize failed with exit code " << exit_code_;
    return false;
  }

  exit_code_ = ContentMainRun(initialized, std::move(context_initialized));
  if (exit_code_ != content::RESULT_CODE_NORMAL_EXIT) {
    // Some exit codes are used to exit early, but are otherwise a normal
    // result. Don't log for those codes.
    if (!IsNormalResultCode(static_cast<ResultCode>(exit_code_))) {
      LOG(ERROR) << "ContentMainRun failed with exit code " << exit_code_;
    }
    return false;
  }

  initialized_ = true;
  return true;
}

void CefMainRunner::Shutdown(base::OnceClosure shutdown_on_ui_thread,
                             base::OnceClosure finalize_shutdown) {
  if (multi_threaded_message_loop_) {
    // Start shutdown on the UI thread. This is guaranteed to run before the
    // thread RunLoop has stopped.
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefMainRunner::StartShutdownOnUIThread,
                                 base::Unretained(this),
                                 std::move(shutdown_on_ui_thread)));

    // Finish shutdown on the UI thread after the thread RunLoop has stopped and
    // before running exit callbacks.
    ui_thread_->set_shutdown_callback(base::BindOnce(
        &CefMainRunner::FinishShutdownOnUIThread, base::Unretained(this)));

    // Blocks until the thread has stopped.
    ui_thread_->Stop();
    ui_thread_.reset();
  }

  if (!multi_threaded_message_loop_) {
    // Main thread and UI thread are the same.
    StartShutdownOnUIThread(std::move(shutdown_on_ui_thread));

    // |browser_runner_| may be nullptr when shutting down after early exit.
    if (browser_runner_) {
      browser_runner_->Shutdown();
      browser_runner_.reset();
    }

    FinishShutdownOnUIThread();
  }

  // Shut down the content runner.
  content::ContentMainShutdown(main_runner_.get());

  main_runner_.reset();

  std::move(finalize_shutdown).Run();

  main_delegate_.reset();
  keep_alive_.reset();
  settings_ = nullptr;
  application_ = nullptr;
}

void CefMainRunner::RunMessageLoop() {
  base::RunLoop run_loop;

  DCHECK(quit_callback_.is_null());
  quit_callback_ = run_loop.QuitClosure();

  // May be nullptr if content::ContentMainRun exits early.
  if (g_browser_process) {
    // The ScopedKeepAlive instance triggers shutdown logic when released on the
    // UI thread before terminating the message loop (e.g. from
    // CefQuitMessageLoop or FinishShutdownOnUIThread when running with
    // multi-threaded message loop).
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::APP_CONTROLLER, KeepAliveRestartOption::DISABLED);

    // The QuitClosure will be executed from BrowserProcessImpl::Unpin() via
    // KeepAliveRegistry when the last ScopedKeepAlive is released.
    // ScopedKeepAlives are also held by Browser objects.
    static_cast<BrowserProcessImpl*>(g_browser_process)
        ->SetQuitClosure(run_loop.QuitClosure());
  }

  // Blocks until QuitMessageLoop() is called.
  run_loop.Run();
}

void CefMainRunner::QuitMessageLoop() {
  if (!quit_callback_.is_null()) {
    if (HandleMainMessageLoopQuit()) {
      return;
    }
    std::move(quit_callback_).Run();
  }
}

// static
NO_STACK_PROTECTOR
int CefMainRunner::RunAsHelperProcess(const CefMainArgs& args,
                                      CefRefPtr<CefApp> application,
                                      void* windows_sandbox_info) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
#if BUILDFLAG(IS_WIN)
  command_line.ParseFromString(::GetCommandLineW());
#else
  command_line.InitFromArgv(args.argc, args.argv);
#endif

  // Wait for the debugger as early in process initialization as possible.
  if (command_line.HasSwitch(switches::kWaitForDebugger)) {
    base::debug::WaitForDebugger(60, true);
  }

  // If no process type is specified then it represents the browser process and
  // we do nothing.
  if (!command_line.HasSwitch(switches::kProcessType)) {
    return -1;
  }

  const std::string& process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty()) {
    // Early exit on invalid process type.
    return CEF_RESULT_CODE_BAD_PROCESS_TYPE;
  }

  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    return crashpad_runner::RunAsCrashpadHandler(command_line);
  }

  auto main_delegate = std::make_unique<ChromeMainDelegateCef>(
      /*runner=*/nullptr, /*settings=*/nullptr, application);
  BeforeMainInitialize(args);

  // Execute the secondary process.
  content::ContentMainParams main_params(main_delegate.get());
#if BUILDFLAG(IS_WIN)
  // Configure child processes to be killed by the system after the main process
  // goes away. The main process uses the default shutdown order, which is
  // 0x280. Note that lower numbers here mean "kill later" and higher numbers
  // mean "kill sooner". We want to avoid child processes dying first because
  // they may be relaunched, resulting in relaunch failures and crashes like
  // IntentionallyCrashBrowserForUnusableGpuProcess.
  ::SetProcessShutdownParameters(0x280 - 1, SHUTDOWN_NORETRY);

  // Initialize the sandbox services.
  // Match the logic in MainDllLoader::Launch.
  sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};

  // IsUnsandboxedSandboxType() can't be used here because its result can be
  // gated behind a feature flag, which are not yet initialized.
  const bool is_sandboxed =
      sandbox::policy::SandboxTypeFromCommandLine(command_line) !=
      sandbox::mojom::Sandbox::kNoSandbox;

  // When using cef_sandbox_info_create() the sandbox info will always be
  // initialized. This is incorrect for cases where the sandbox is disabled, and
  // we adjust for that here.
  if (!is_sandboxed || windows_sandbox_info == nullptr) {
    if (is_sandboxed) {
      // For child processes that are running as --no-sandbox, don't
      // initialize the sandbox info, otherwise they'll be treated as brokers
      // (as if they were the browser).
      content::InitializeSandboxInfo(
          &sandbox_info, IsExtensionPointDisableSet()
                             ? sandbox::MITIGATION_EXTENSION_POINT_DISABLE
                             : 0);
    }
    windows_sandbox_info = &sandbox_info;
  }

  main_params.instance = args.instance;
  main_params.sandbox_info =
      static_cast<sandbox::SandboxInterfaceInfo*>(windows_sandbox_info);
#else
  main_params.argc = args.argc;
  main_params.argv = const_cast<const char**>(args.argv);
#endif
  return content::ContentMain(std::move(main_params));
}

int CefMainRunner::ContentMainInitialize(const CefMainArgs& args,
                                         void* windows_sandbox_info,
                                         int* no_sandbox,
                                         bool disable_signal_handlers) {
  BeforeMainInitialize(args);

  main_delegate_ =
      std::make_unique<ChromeMainDelegateCef>(this, settings_, application_);

  // Initialize the content runner.
  main_runner_ = content::ContentMainRunner::Create();
  content::ContentMainParams main_params(main_delegate_.get());

#if BUILDFLAG(IS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
  if (windows_sandbox_info == nullptr) {
    windows_sandbox_info = &sandbox_info;
    *no_sandbox = true;
  }

  main_params.instance = args.instance;
  main_params.sandbox_info =
      static_cast<sandbox::SandboxInterfaceInfo*>(windows_sandbox_info);
#else
  main_params.argc = args.argc;
  main_params.argv = const_cast<const char**>(args.argv);
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  main_params.disable_signal_handlers = disable_signal_handlers;
#endif

  return content::ContentMainInitialize(std::move(main_params),
                                        main_runner_.get());
}

int CefMainRunner::ContentMainRun(bool* initialized,
                                  base::OnceClosure context_initialized) {
  if (multi_threaded_message_loop_) {
    // Detach from the main thread so that these objects can be attached and
    // modified from the UI thread going forward.
    metrics::GlobalPersistentSystemProfile::GetInstance()
        ->DetachFromCurrentThread();
  }

  int exit_code = -1;

  if (multi_threaded_message_loop_) {
    // Detach the CommandLine from the main thread so that it can be
    // attached and modified from the UI thread going forward.
    base::CommandLine::ForCurrentProcess()->DetachFromCurrentSequence();

    base::WaitableEvent uithread_startup_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    if (!CreateUIThread(base::BindOnce(
            [](CefMainRunner* runner, base::WaitableEvent* event,
               int* exit_code) {
              runner->BeforeUIThreadInitialize();
              *exit_code = content::ContentMainRun(runner->main_runner_.get());
              event->Signal();
            },
            base::Unretained(this), base::Unretained(&uithread_startup_event),
            base::Unretained(&exit_code)))) {
      return exit_code;
    }

    *initialized = true;

    // We need to wait until content::ContentMainRun has finished.
    uithread_startup_event.Wait();
  } else {
    *initialized = true;
    BeforeUIThreadInitialize();
    exit_code = content::ContentMainRun(main_runner_.get());
  }

  if (exit_code == content::RESULT_CODE_NORMAL_EXIT) {
    // content::ContentMainRun was successful and we're not exiting early.
    if (CEF_CURRENTLY_ON_UIT()) {
      OnContextInitialized(std::move(context_initialized));
    } else {
      // Continue initialization on the UI thread.
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefMainRunner::OnContextInitialized,
                                   base::Unretained(this),
                                   std::move(context_initialized)));
    }
  } else {
    // content::ContentMainRun exited early. Reset initialized state.
    *initialized = false;
  }

  return exit_code;
}

// static
void CefMainRunner::BeforeMainInitialize(const CefMainArgs& args) {
#if BUILDFLAG(IS_WIN)
  base::CommandLine::Init(0, nullptr);
#else
  base::CommandLine::Init(args.argc, args.argv);
#endif
}

bool CefMainRunner::HandleMainMessageLoopQuit() {
  // May be called multiple times. See comments in RunMainMessageLoopBefore.
  keep_alive_.reset();

  // If we're initialized it means that the BrowserProcessImpl was created and
  // registered as a KeepAliveStateObserver, in which case we wait for all
  // Chrome browser windows to exit. Otherwise, continue with direct execution
  // of the QuitClosure() in QuitMessageLoop.
  return initialized_;
}

void CefMainRunner::PreBrowserMain() {
  if (external_message_pump_) {
    InitExternalMessagePumpFactoryForUI();
  }
}

int CefMainRunner::RunMainProcess(
    content::MainFunctionParams main_function_params) {
  int exit_code;

  if (!multi_threaded_message_loop_) {
    // Use our own browser process runner.
    browser_runner_ = content::BrowserMainRunner::Create();

    // Initialize browser process state. Results in a call to
    // PreBrowserMain() which creates the UI message loop.
    exit_code = browser_runner_->Initialize(std::move(main_function_params));
  } else {
    // Running on the separate UI thread.
    DCHECK(ui_thread_);
    exit_code =
        ui_thread_->InitializeBrowserRunner(std::move(main_function_params));
  }

  if (exit_code >= 0) {
    return exit_code;
  }

  return 0;
}

bool CefMainRunner::CreateUIThread(base::OnceClosure setup_callback) {
  DCHECK(!ui_thread_);

  ui_thread_ = std::make_unique<CefUIThread>(this, std::move(setup_callback));
  ui_thread_->Start();
  ui_thread_->WaitUntilThreadStarted();

  if (external_message_pump_) {
    InitExternalMessagePumpFactoryForUI();
  }
  return true;
}

void CefMainRunner::OnContextInitialized(
    base::OnceClosure context_initialized) {
  CEF_REQUIRE_UIT();

  std::move(context_initialized).Run();
}

void CefMainRunner::StartShutdownOnUIThread(
    base::OnceClosure shutdown_on_ui_thread) {
  // |initialized_| will be false if shutting down after early exit.
  if (initialized_) {
    CEF_REQUIRE_UIT();

    // Execute all pending tasks now before proceeding with shutdown. Otherwise,
    // objects bound to tasks and released at the end of shutdown via
    // BrowserTaskExecutor::Shutdown may attempt to access other objects that
    // have already been destroyed (for example, if teardown results in a call
    // to RenderProcessHostImpl::Cleanup).
    content::BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(
        content::BrowserThread::UI);
    content::BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(
        content::BrowserThread::IO);
  }

  std::move(shutdown_on_ui_thread).Run();
  BeforeUIThreadShutdown();
}

void CefMainRunner::FinishShutdownOnUIThread() {
  if (multi_threaded_message_loop_) {
    // Don't wait for this to be called in ChromeMainDelegate::ProcessExiting.
    // It is safe to call multiple times.
    ChromeProcessSingleton::DeleteInstance();
  }

  static_cast<content::ContentMainRunnerImpl*>(main_runner_.get())
      ->ShutdownOnUIThread();
}

void CefMainRunner::BeforeUIThreadInitialize() {
  static_cast<ChromeContentBrowserClient*>(
      CefAppManager::Get()->GetContentClient()->browser())
      ->SetSamplingProfiler(
          std::make_unique<MainThreadStackSamplingProfiler>());
}

void CefMainRunner::BeforeUIThreadShutdown() {
  // |initialized_| will be false if shutting down after early exit.
  if (initialized_) {
    static_cast<ChromeContentBrowserClientCef*>(
        CefAppManager::Get()->GetContentClient()->browser())
        ->CleanupOnUIThread();
  }
  main_delegate_->CleanupOnUIThread();
}
