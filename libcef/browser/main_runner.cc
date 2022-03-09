// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/main_runner.h"

#include "libcef/browser/browser_message_loop.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/alloy/alloy_main_runner_delegate.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/chrome/chrome_main_runner_delegate.h"
#include "libcef/features/runtime.h"

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/app/content_main_runner_impl.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_WIN)
#include <Objbase.h>
#include <windows.h>
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "components/crash/core/app/crash_switches.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"
#endif

namespace {

enum class RuntimeType {
  UNINITIALIZED,
  ALLOY,
  CHROME,
};
RuntimeType g_runtime_type = RuntimeType::UNINITIALIZED;

std::unique_ptr<CefMainRunnerDelegate> MakeDelegate(
    RuntimeType type,
    CefMainRunnerHandler* runner,
    CefSettings* settings,
    CefRefPtr<CefApp> application) {
  if (type == RuntimeType::ALLOY) {
    g_runtime_type = RuntimeType::ALLOY;
    return std::make_unique<AlloyMainRunnerDelegate>(runner, settings,
                                                     application);
  } else {
    g_runtime_type = RuntimeType::CHROME;
    return std::make_unique<ChromeMainRunnerDelegate>(runner, settings,
                                                      application);
  }
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

// Based on components/crash/core/app/run_as_crashpad_handler_win.cc
// Remove the "--type=crashpad-handler" command-line flag that will otherwise
// confuse the crashpad handler.
// Chrome uses an embedded crashpad handler on Windows only and imports this
// function via the existing "run_as_crashpad_handler" target defined in
// components/crash/core/app/BUILD.gn. CEF uses an embedded handler on both
// Windows and macOS so we define the function here instead of using the
// existing target (because we can't use that target on macOS).
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

#if BUILDFLAG(IS_MAC)
  // HandlerMain on macOS uses the system version of getopt_long which expects
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

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

}  // namespace

// Used to run the UI on a separate thread.
class CefUIThread : public base::PlatformThread::Delegate {
 public:
  CefUIThread(CefMainRunner* runner, base::OnceClosure setup_callback)
      : runner_(runner), setup_callback_(std::move(setup_callback)) {}
  ~CefUIThread() override { Stop(); }

  void Start() {
    base::AutoLock lock(thread_lock_);
    bool success = base::PlatformThread::CreateWithPriority(
        0, this, &thread_, base::ThreadPriority::NORMAL);
    if (!success) {
      LOG(FATAL) << "failed to UI create thread";
    }
  }

  void Stop() {
    base::AutoLock lock(thread_lock_);

    if (!stopping_) {
      stopping_ = true;
      CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefMainRunner::QuitMessageLoop,
                                            base::Unretained(runner_)));
    }

    // Can't join if the |thread_| is either already gone or is non-joinable.
    if (thread_.is_null())
      return;

    base::PlatformThread::Join(thread_);
    thread_ = base::PlatformThreadHandle();

    stopping_ = false;
  }

  bool WaitUntilThreadStarted() const {
    DCHECK(owning_sequence_checker_.CalledOnValidSequence());
    start_event_.Wait();
    return true;
  }

  void InitializeBrowserRunner(
      content::MainFunctionParams main_function_params) {
    // Use our own browser process runner.
    browser_runner_ = content::BrowserMainRunner::Create();

    // Initialize browser process state. Uses the current thread's message loop.
    int exit_code =
        browser_runner_->Initialize(std::move(main_function_params));
    CHECK_EQ(exit_code, -1);
  }

 protected:
  void ThreadMain() override {
    base::PlatformThread::SetName("CefUIThread");

#if BUILDFLAG(IS_WIN)
    // Initializes the COM library on the current thread.
    CoInitialize(nullptr);
#endif

    start_event_.Signal();

    std::move(setup_callback_).Run();

    runner_->RunMessageLoop();

    browser_runner_->Shutdown();
    browser_runner_.reset();

    content::BrowserTaskExecutor::Shutdown();

    // Run exit callbacks on the UI thread to avoid sequence check failures.
    base::AtExitManager::ProcessCallbacksNow();

#if BUILDFLAG(IS_WIN)
    // Closes the COM library on the current thread. CoInitialize must
    // be balanced by a corresponding call to CoUninitialize.
    CoUninitialize();
#endif
  }

  CefMainRunner* const runner_;
  base::OnceClosure setup_callback_;

  std::unique_ptr<content::BrowserMainRunner> browser_runner_;

  bool stopping_ = false;

  // The thread's handle.
  base::PlatformThreadHandle thread_;
  mutable base::Lock thread_lock_;  // Protects |thread_|.

  mutable base::WaitableEvent start_event_;

  // This class is not thread-safe, use this to verify access from the owning
  // sequence of the Thread.
  base::SequenceChecker owning_sequence_checker_;
};

CefMainRunner::CefMainRunner(bool multi_threaded_message_loop,
                             bool external_message_pump)
    : multi_threaded_message_loop_(multi_threaded_message_loop),
      external_message_pump_(external_message_pump) {}

CefMainRunner::~CefMainRunner() = default;

bool CefMainRunner::Initialize(CefSettings* settings,
                               CefRefPtr<CefApp> application,
                               const CefMainArgs& args,
                               void* windows_sandbox_info,
                               bool* initialized,
                               base::OnceClosure context_initialized) {
  DCHECK(!main_delegate_);
  main_delegate_ = MakeDelegate(
      settings->chrome_runtime ? RuntimeType::CHROME : RuntimeType::ALLOY, this,
      settings, application);

  const int exit_code =
      ContentMainInitialize(args, windows_sandbox_info, &settings->no_sandbox);
  if (exit_code >= 0) {
    NOTREACHED() << "ContentMainInitialize failed";
    return false;
  }

  if (!ContentMainRun(initialized, std::move(context_initialized))) {
    NOTREACHED() << "ContentMainRun failed";
    return false;
  }

  return true;
}

void CefMainRunner::Shutdown(base::OnceClosure shutdown_on_ui_thread,
                             base::OnceClosure finalize_shutdown) {
  if (multi_threaded_message_loop_) {
    // Events that will be used to signal when shutdown is complete. Start in
    // non-signaled mode so that the event will block.
    base::WaitableEvent uithread_shutdown_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    // Finish shutdown on the UI thread.
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefMainRunner::FinishShutdownOnUIThread,
                       base::Unretained(this), std::move(shutdown_on_ui_thread),
                       &uithread_shutdown_event));

    /// Block until UI thread shutdown is complete.
    uithread_shutdown_event.Wait();

    FinalizeShutdown(std::move(finalize_shutdown));
  } else {
    // Finish shutdown on the current thread, which should be the UI thread.
    FinishShutdownOnUIThread(std::move(shutdown_on_ui_thread), nullptr);

    FinalizeShutdown(std::move(finalize_shutdown));
  }
}

void CefMainRunner::RunMessageLoop() {
  base::RunLoop run_loop;

  DCHECK(quit_when_idle_callback_.is_null());
  quit_when_idle_callback_ = run_loop.QuitWhenIdleClosure();

  main_delegate_->BeforeMainMessageLoopRun(&run_loop);

  // Blocks until QuitMessageLoop() is called.
  run_loop.Run();
}

void CefMainRunner::QuitMessageLoop() {
  if (!quit_when_idle_callback_.is_null()) {
    if (main_delegate_->HandleMainMessageLoopQuit())
      return;
    std::move(quit_when_idle_callback_).Run();
  }
}

// static
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
  if (command_line.HasSwitch(switches::kWaitForDebugger))
    base::debug::WaitForDebugger(60, true);

  // If no process type is specified then it represents the browser process and
  // we do nothing.
  const std::string& process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty())
    return -1;

  auto runtime_type = command_line.HasSwitch(switches::kEnableChromeRuntime)
                          ? RuntimeType::CHROME
                          : RuntimeType::ALLOY;
  auto main_delegate = MakeDelegate(runtime_type, /*runner=*/nullptr,
                                    /*settings=*/nullptr, application);
  main_delegate->BeforeExecuteProcess(args);

  int result;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    result = RunAsCrashpadHandler(command_line);
    main_delegate->AfterExecuteProcess();
    return result;
  }
#endif

  // Execute the secondary process.
  content::ContentMainParams main_params(
      main_delegate->GetContentMainDelegate());
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
  result = content::ContentMain(std::move(main_params));

  main_delegate->AfterExecuteProcess();

  return result;
}

int CefMainRunner::ContentMainInitialize(const CefMainArgs& args,
                                         void* windows_sandbox_info,
                                         int* no_sandbox) {
  main_delegate_->BeforeMainThreadInitialize(args);

  // Initialize the content runner.
  main_runner_ = content::ContentMainRunner::Create();
  content::ContentMainParams main_params(
      main_delegate_->GetContentMainDelegate());

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

  return content::ContentMainInitialize(std::move(main_params),
                                        main_runner_.get());
}

bool CefMainRunner::ContentMainRun(bool* initialized,
                                   base::OnceClosure context_initialized) {
  main_delegate_->BeforeMainThreadRun();

  if (multi_threaded_message_loop_) {
    base::WaitableEvent uithread_startup_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    if (!CreateUIThread(base::BindOnce(
            [](CefMainRunner* runner, base::WaitableEvent* event) {
              content::ContentMainRun(runner->main_runner_.get());
              event->Signal();
            },
            base::Unretained(this),
            base::Unretained(&uithread_startup_event)))) {
      return false;
    }

    *initialized = true;

    // We need to wait until content::ContentMainRun has finished.
    uithread_startup_event.Wait();
  } else {
    *initialized = true;
    content::ContentMainRun(main_runner_.get());
  }

  if (CEF_CURRENTLY_ON_UIT()) {
    OnContextInitialized(std::move(context_initialized));
  } else {
    // Continue initialization on the UI thread.
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefMainRunner::OnContextInitialized,
                                          base::Unretained(this),
                                          std::move(context_initialized)));
  }

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
    // AlloyBrowserMain::PreBrowserMain() which creates the UI message
    // loop.
    int exit_code =
        browser_runner_->Initialize(std::move(main_function_params));
    if (exit_code >= 0)
      return exit_code;
  } else {
    // Running on the separate UI thread.
    DCHECK(ui_thread_);
    ui_thread_->InitializeBrowserRunner(std::move(main_function_params));
  }

  return 0;
}

bool CefMainRunner::CreateUIThread(base::OnceClosure setup_callback) {
  DCHECK(!ui_thread_);

  ui_thread_.reset(new CefUIThread(this, std::move(setup_callback)));
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

  main_delegate_->AfterUIThreadInitialize();
  std::move(context_initialized).Run();
}

void CefMainRunner::FinishShutdownOnUIThread(
    base::OnceClosure shutdown_on_ui_thread,
    base::WaitableEvent* uithread_shutdown_event) {
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

  static_cast<content::ContentMainRunnerImpl*>(main_runner_.get())
      ->ShutdownOnUIThread();

  std::move(shutdown_on_ui_thread).Run();
  main_delegate_->AfterUIThreadShutdown();

  if (uithread_shutdown_event)
    uithread_shutdown_event->Signal();
}

void CefMainRunner::FinalizeShutdown(base::OnceClosure finalize_shutdown) {
  main_delegate_->BeforeMainThreadShutdown();

  if (browser_runner_.get()) {
    browser_runner_->Shutdown();
    browser_runner_.reset();
  }

  if (ui_thread_.get()) {
    // Blocks until the thread has stopped.
    ui_thread_->Stop();
    ui_thread_.reset();
  }

  // Shut down the content runner.
  content::ContentMainShutdown(main_runner_.get());

  main_runner_.reset();

  std::move(finalize_shutdown).Run();
  main_delegate_->AfterMainThreadShutdown();

  main_delegate_.reset();
  g_runtime_type = RuntimeType::UNINITIALIZED;
}

// From libcef/features/runtime.h:
namespace cef {

bool IsAlloyRuntimeEnabled() {
  return g_runtime_type == RuntimeType::ALLOY;
}

bool IsChromeRuntimeEnabled() {
  return g_runtime_type == RuntimeType::CHROME;
}

}  // namespace cef
