// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/main_runner.h"

#include "libcef/browser/browser_message_loop.h"
#include "libcef/browser/thread_util.h"
#include "libcef/renderer/content_renderer_client.h"

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/app/content_service_manager_main_delegate.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/embedder/main.h"

#if defined(OS_WIN)
#include <Objbase.h>
#include <windows.h>
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
#include "components/crash/core/app/crash_switches.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"
#endif

namespace {

#if defined(OS_MACOSX) || defined(OS_WIN)

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

#if defined(OS_MACOSX)
  // HandlerMain on macOS uses the system version of getopt_long which expects
  // the first argument to be the program name.
  argv.insert(argv.begin(), command_line.GetProgram().value());
#endif

  std::unique_ptr<char*[]> argv_as_utf8(new char*[argv.size() + 1]);
  std::vector<std::string> storage;
  storage.reserve(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
#if defined(OS_WIN)
    storage.push_back(base::UTF16ToUTF8(argv[i]));
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

#endif  // defined(OS_MACOSX) || defined(OS_WIN)

}  // namespace

// Used to run the UI on a separate thread.
class CefUIThread : public base::PlatformThread::Delegate {
 public:
  explicit CefUIThread(base::OnceClosure setup_callback)
      : setup_callback_(std::move(setup_callback)) {}
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
      CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefUIThread::ThreadQuitHelper,
                                            base::Unretained(this)));
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
      const content::MainFunctionParams& main_function_params) {
    // Use our own browser process runner.
    browser_runner_ = content::BrowserMainRunner::Create();

    // Initialize browser process state. Uses the current thread's message loop.
    int exit_code = browser_runner_->Initialize(main_function_params);
    CHECK_EQ(exit_code, -1);
  }

 protected:
  void ThreadMain() override {
    base::PlatformThread::SetName("CefUIThread");

#if defined(OS_WIN)
    // Initializes the COM library on the current thread.
    CoInitialize(nullptr);
#endif

    start_event_.Signal();

    std::move(setup_callback_).Run();

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();

    browser_runner_->Shutdown();
    browser_runner_.reset(nullptr);

    content::BrowserTaskExecutor::Shutdown();

    // Run exit callbacks on the UI thread to avoid sequence check failures.
    base::AtExitManager::ProcessCallbacksNow();

#if defined(OS_WIN)
    // Closes the COM library on the current thread. CoInitialize must
    // be balanced by a corresponding call to CoUninitialize.
    CoUninitialize();
#endif

    run_loop_ = nullptr;
  }

  void ThreadQuitHelper() {
    DCHECK(run_loop_);
    run_loop_->QuitWhenIdle();
  }

  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
  base::OnceClosure setup_callback_;

  bool stopping_ = false;

  // The thread's handle.
  base::PlatformThreadHandle thread_;
  mutable base::Lock thread_lock_;  // Protects |thread_|.

  base::RunLoop* run_loop_ = nullptr;

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
  CefRuntimeInitialize(settings, application);

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

// static
int CefMainRunner::RunAsHelperProcess(const CefMainArgs& args,
                                      CefRefPtr<CefApp> application,
                                      void* windows_sandbox_info) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
#if defined(OS_WIN)
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

#if defined(OS_MACOSX) || defined(OS_WIN)
  if (process_type == crash_reporter::switches::kCrashpadHandler)
    return RunAsCrashpadHandler(command_line);
#endif

  std::unique_ptr<content::ContentMainDelegate> main_delegate;
  main_delegate.reset(new CefMainDelegate(/*context=*/nullptr,
                                          /*settings=*/nullptr, application));

// Execute the secondary process.
#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  if (windows_sandbox_info == nullptr) {
    content::InitializeSandboxInfo(&sandbox_info);
    windows_sandbox_info = &sandbox_info;
  }

  content::ContentMainParams params(main_delegate.get());
  params.instance = args.instance;
  params.sandbox_info =
      static_cast<sandbox::SandboxInterfaceInfo*>(windows_sandbox_info);

  return content::ContentMain(params);
#else
  content::ContentMainParams params(main_delegate.get());
  params.argc = args.argc;
  params.argv = const_cast<const char**>(args.argv);

  return content::ContentMain(params);
#endif
}

void CefMainRunner::CefRuntimeInitialize(CefSettings* settings,
                                         CefRefPtr<CefApp> app) {
  DCHECK_EQ(RuntimeType::UNINITIALIZED, runtime_type_);
  DCHECK(!main_delegate_);

  runtime_type_ = RuntimeType::CEF;
  CefMainDelegate::CefInitialize();
  main_delegate_.reset(new CefMainDelegate(this, settings, app));
}

int CefMainRunner::ContentMainInitialize(const CefMainArgs& args,
                                         void* windows_sandbox_info,
                                         int* no_sandbox) {
  DCHECK(main_delegate_);

  // Initialize the content runner.
  content::ContentMainParams params(main_delegate_.get());
#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  if (windows_sandbox_info == nullptr) {
    windows_sandbox_info = &sandbox_info;
    *no_sandbox = true;
  }

  params.instance = args.instance;
  params.sandbox_info =
      static_cast<sandbox::SandboxInterfaceInfo*>(windows_sandbox_info);
#else
  params.argc = args.argc;
  params.argv = const_cast<const char**>(args.argv);
#endif

  sm_main_delegate_.reset(
      new content::ContentServiceManagerMainDelegate(params));
  sm_main_params_.reset(
      new service_manager::MainParams(sm_main_delegate_.get()));

#if defined(OS_POSIX) && !defined(OS_ANDROID)
  sm_main_params_->argc = params.argc;
  sm_main_params_->argv = params.argv;
#endif

  return service_manager::MainInitialize(*sm_main_params_);
}

bool CefMainRunner::ContentMainRun(bool* initialized,
                                   base::OnceClosure context_initialized) {
  if (IsCefRuntime())
    CefMainDelegate::MainThreadInitialize();

  if (multi_threaded_message_loop_) {
    base::WaitableEvent uithread_startup_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    if (!CreateUIThread(base::BindOnce(
            [](CefMainRunner* runner, base::WaitableEvent* event) {
              service_manager::MainRun(*runner->sm_main_params_);
              event->Signal();
            },
            base::Unretained(this),
            base::Unretained(&uithread_startup_event)))) {
      return false;
    }

    *initialized = true;

    // We need to wait until service_manager::MainRun has finished.
    uithread_startup_event.Wait();
  } else {
    *initialized = true;
    service_manager::MainRun(*sm_main_params_);
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

void CefMainRunner::PreCreateMainMessageLoop() {
  if (external_message_pump_) {
    InitExternalMessagePumpFactoryForUI();
  }
}

int CefMainRunner::RunMainProcess(
    const content::MainFunctionParams& main_function_params) {
  if (!multi_threaded_message_loop_) {
    // Use our own browser process runner.
    browser_runner_ = content::BrowserMainRunner::Create();

    // Initialize browser process state. Results in a call to
    // CefBrowserMain::PreMainMessageLoopStart() which creates the UI message
    // loop.
    int exit_code = browser_runner_->Initialize(main_function_params);
    if (exit_code >= 0)
      return exit_code;
  } else {
    // Running on the separate UI thread.
    DCHECK(ui_thread_);
    ui_thread_->InitializeBrowserRunner(main_function_params);
  }

  return 0;
}

bool CefMainRunner::CreateUIThread(base::OnceClosure setup_callback) {
  DCHECK(!ui_thread_);

  ui_thread_.reset(new CefUIThread(std::move(setup_callback)));
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
  if (IsCefRuntime())
    CefMainDelegate::UIThreadInitialize();
  std::move(context_initialized).Run();
}

void CefMainRunner::FinishShutdownOnUIThread(
    base::OnceClosure shutdown_on_ui_thread,
    base::WaitableEvent* uithread_shutdown_event) {
  CEF_REQUIRE_UIT();

  sm_main_delegate_->ShutdownOnUIThread();

  std::move(shutdown_on_ui_thread).Run();
  if (IsCefRuntime())
    CefMainDelegate::UIThreadShutdown();

  if (uithread_shutdown_event)
    uithread_shutdown_event->Signal();
}

void CefMainRunner::FinalizeShutdown(base::OnceClosure finalize_shutdown) {
  if (content::RenderProcessHost::run_renderer_in_process() && IsCefRuntime()) {
    // Blocks until RenderProcess cleanup is complete.
    CefContentRendererClient::Get()->RunSingleProcessCleanup();
  }

  if (browser_runner_.get()) {
    browser_runner_->Shutdown();
    browser_runner_.reset(nullptr);
  }

  if (ui_thread_.get()) {
    // Blocks until the thread has stopped.
    ui_thread_->Stop();
    ui_thread_.reset();
  }

  // Shut down the content runner.
  service_manager::MainShutdown(*sm_main_params_);

  sm_main_params_.reset(nullptr);
  sm_main_delegate_.reset(nullptr);

  std::move(finalize_shutdown).Run();
  if (IsCefRuntime())
    CefMainDelegate::MainThreadShutdown();

  main_delegate_.reset(nullptr);
  runtime_type_ = RuntimeType::UNINITIALIZED;
}
