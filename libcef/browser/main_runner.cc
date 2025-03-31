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
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/app_manager.h"
#include "cef/libcef/common/cef_switches.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_process_singleton.h"
#include "chrome/common/chrome_result_codes.h"
#include "components/crash/core/app/crash_switches.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/metrics/persistent_system_profile.h"
#include "content/app/content_main_runner_impl.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <memory>

#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

namespace {

// Based on components/crash/core/app/run_as_crashpad_handler_win.cc
// Remove the "--type=crashpad-handler" command-line flag that will otherwise
// confuse the crashpad handler.
// Chrome uses an embedded crashpad handler on Windows only and imports this
// function via the existing "run_as_crashpad_handler" target defined in
// components/crash/core/app/BUILD.gn. CEF uses an embedded handler on all
// platforms so we define the function here instead of using the existing
// target (because we can't use that target on macOS).
int RunAsCrashpadHandler(const base::CommandLine& command_line) {
  base::CommandLine::StringVector argv = command_line.argv();
  const base::CommandLine::StringType process_type =
      FILE_PATH_LITERAL("--type=");
  argv.erase(
      std::remove_if(argv.begin(), argv.end(),
                     [&process_type](const base::CommandLine::StringType& str) {
                       return base::StartsWith(str, process_type,
                                               base::CompareCase::SENSITIVE) ||
                              (!str.empty() && str[0] == L'/');
                     }),
      argv.end());

#if BUILDFLAG(IS_POSIX)
  // HandlerMain on POSIX uses the system version of getopt_long which expects
  // the first argument to be the program name.
  argv.insert(argv.begin(), command_line.GetProgram().value());
#endif

  std::unique_ptr<char*[]> argv_as_utf8(new char*[argv.size() + 1]);
  std::vector<std::string> storage;
  storage.reserve(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
#if BUILDFLAG(IS_WIN)
    storage.push_back(base::WideToUTF8(argv[i]));
#else
    storage.push_back(argv[i]);
#endif
    argv_as_utf8[i] = &storage[i][0];
  }
  argv_as_utf8[argv.size()] = nullptr;
  argv.clear();
  return crashpad::HandlerMain(static_cast<int>(storage.size()),
                               argv_as_utf8.get(), nullptr);
}

}  // namespace

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

    browser_runner_->Shutdown();
    browser_runner_.reset();

    FinishShutdownOnUIThread();
  }

  // Shut down the content runner.
  content::ContentMainShutdown(main_runner_.get());

  main_runner_.reset();

  std::move(finalize_shutdown).Run();

  main_delegate_.reset();
  sampling_profiler_.reset();
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
  const std::string& process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty()) {
    return -1;
  }

  auto main_delegate = std::make_unique<ChromeMainDelegateCef>(
      /*runner=*/nullptr, /*settings=*/nullptr, application);
  BeforeMainInitialize(args);

  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    return RunAsCrashpadHandler(command_line);
  }

  // Execute the secondary process.
  content::ContentMainParams main_params(main_delegate.get());
#if BUILDFLAG(IS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
  if (windows_sandbox_info == nullptr) {
    content::InitializeSandboxInfo(&sandbox_info);
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

              if (*exit_code != content::RESULT_CODE_NORMAL_EXIT) {
                runner->FinishShutdownOnUIThread();
              }

              event->Signal();
            },
            base::Unretained(this), base::Unretained(&uithread_startup_event),
            base::Unretained(&exit_code)))) {
      return exit_code;
    }

    *initialized = true;

    // We need to wait until content::ContentMainRun has finished.
    uithread_startup_event.Wait();

    if (exit_code != content::RESULT_CODE_NORMAL_EXIT) {
      // content::ContentMainRun was not successful (for example, due to process
      // singleton relaunch). Stop the UI thread and block until done.
      ui_thread_->Stop();
      ui_thread_.reset();
    }
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
  // May be nullptr if content::ContentMainRun exits early.
  if (!g_browser_process) {
    // Proceed with direct execution of the QuitClosure().
    return false;
  }

  // May be called multiple times. See comments in RunMainMessageLoopBefore.
  keep_alive_.reset();

  // Cancel direct execution of the QuitClosure() in QuitMessageLoop. We
  // instead wait for all Chrome browser windows to exit.
  return true;
}

void CefMainRunner::PreBrowserMain() {
  if (external_message_pump_) {
    InitExternalMessagePumpFactoryForUI();
  }
}

int CefMainRunner::RunMainProcess(
    content::MainFunctionParams main_function_params) {
  if (!multi_threaded_message_loop_) {
    // Use our own browser process runner.
    browser_runner_ = content::BrowserMainRunner::Create();

    // Initialize browser process state. Results in a call to
    // PreBrowserMain() which creates the UI message loop.
    int exit_code =
        browser_runner_->Initialize(std::move(main_function_params));
    if (exit_code >= 0) {
      return exit_code;
    }
  } else {
    // Running on the separate UI thread.
    DCHECK(ui_thread_);
    ui_thread_->InitializeBrowserRunner(std::move(main_function_params));
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
  CEF_REQUIRE_UIT();

  // Execute all pending tasks now before proceeding with shutdown. Otherwise,
  // objects bound to tasks and released at the end of shutdown via
  // BrowserTaskExecutor::Shutdown may attempt to access other objects that have
  // already been destroyed (for example, if teardown results in a call to
  // RenderProcessHostImpl::Cleanup).
  content::BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(
      content::BrowserThread::UI);
  content::BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(
      content::BrowserThread::IO);

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
  sampling_profiler_ = std::make_unique<MainThreadStackSamplingProfiler>();
}

void CefMainRunner::BeforeUIThreadShutdown() {
  static_cast<ChromeContentBrowserClientCef*>(
      CefAppManager::Get()->GetContentClient()->browser())
      ->CleanupOnUIThread();
  main_delegate_->CleanupOnUIThread();

  sampling_profiler_.reset();
}
