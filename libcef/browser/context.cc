// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/context.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_main.h"
#include "libcef/browser/chrome_browser_process_stub.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/trace_subscriber.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/main_delegate.h"
#include "libcef/common/widevine_loader.h"
#include "libcef/renderer/content_renderer_client.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/app/content_service_manager_main_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/embedder/main.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/install_static/initialize_from_primary_module.h"
#include "components/crash/content/app/crashpad.h"
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
#include "components/crash/content/app/crash_switches.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"
#endif

namespace {

CefContext* g_context = nullptr;

#if DCHECK_IS_ON()
// When the process terminates check if CefShutdown() has been called.
class CefShutdownChecker {
 public:
  ~CefShutdownChecker() { DCHECK(!g_context) << "CefShutdown was not called"; }
} g_shutdown_checker;
#endif  // DCHECK_IS_ON()

#if defined(OS_WIN)
#if defined(ARCH_CPU_X86_64)
// VS2013 only checks the existence of FMA3 instructions, not the enabled-ness
// of them at the OS level (this is fixed in VS2015). We force off usage of
// FMA3 instructions in the CRT to avoid using that path and hitting illegal
// instructions when running on CPUs that support FMA3, but OSs that don't.
void DisableFMA3() {
  static bool disabled = false;
  if (disabled)
    return;
  disabled = true;
  _set_FMA3_enable(0);
}
#endif  // defined(ARCH_CPU_X86_64)

// Transfer state from chrome_elf.dll to the libcef.dll. Accessed when
// loading chrome://system.
void InitInstallDetails() {
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;
  install_static::InitializeFromPrimaryModule();
}

// Signal chrome_elf to initialize crash reporting, rather than doing it in
// DllMain. See https://crbug.com/656800 for details.
void InitCrashReporter() {
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;
  SignalInitializeCrashReporting();
}
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX) || defined(OS_WIN)

// Based on components/crash/content/app/run_as_crashpad_handler_win.cc
// Remove the "--type=crashpad-handler" command-line flag that will otherwise
// confuse the crashpad handler.
// Chrome uses an embedded crashpad handler on Windows only and imports this
// function via the existing "run_as_crashpad_handler" target defined in
// components/crash/content/app/BUILD.gn. CEF uses an embedded handler on both
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

bool GetColor(const cef_color_t cef_in, bool is_windowless, SkColor* sk_out) {
  // Windowed browser colors must be fully opaque.
  if (!is_windowless && CefColorGetA(cef_in) != SK_AlphaOPAQUE)
    return false;

  // Windowless browser colors may be fully transparent.
  if (is_windowless && CefColorGetA(cef_in) == SK_AlphaTRANSPARENT) {
    *sk_out = SK_ColorTRANSPARENT;
    return true;
  }

  // Ignore the alpha component.
  *sk_out = SkColorSetRGB(CefColorGetR(cef_in), CefColorGetG(cef_in),
                          CefColorGetB(cef_in));
  return true;
}

}  // namespace

int CefExecuteProcess(const CefMainArgs& args,
                      CefRefPtr<CefApp> application,
                      void* windows_sandbox_info) {
#if defined(OS_WIN)
#if defined(ARCH_CPU_X86_64)
  DisableFMA3();
#endif
  InitInstallDetails();
  InitCrashReporter();
#endif

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
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty())
    return -1;

#if defined(OS_MACOSX) || defined(OS_WIN)
  if (process_type == crash_reporter::switches::kCrashpadHandler)
    return RunAsCrashpadHandler(command_line);
#endif

  CefMainDelegate main_delegate(application);

// Execute the secondary process.
#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  if (windows_sandbox_info == nullptr) {
    content::InitializeSandboxInfo(&sandbox_info);
    windows_sandbox_info = &sandbox_info;
  }

  content::ContentMainParams params(&main_delegate);
  params.instance = args.instance;
  params.sandbox_info =
      static_cast<sandbox::SandboxInterfaceInfo*>(windows_sandbox_info);

  return content::ContentMain(params);
#else
  content::ContentMainParams params(&main_delegate);
  params.argc = args.argc;
  params.argv = const_cast<const char**>(args.argv);

  return content::ContentMain(params);
#endif
}

bool CefInitialize(const CefMainArgs& args,
                   const CefSettings& settings,
                   CefRefPtr<CefApp> application,
                   void* windows_sandbox_info) {
#if defined(OS_WIN)
#if defined(ARCH_CPU_X86_64)
  DisableFMA3();
#endif
  InitInstallDetails();
  InitCrashReporter();
#endif

  // Return true if the global context already exists.
  if (g_context)
    return true;

  if (settings.size != sizeof(cef_settings_t)) {
    NOTREACHED() << "invalid CefSettings structure size";
    return false;
  }

  g_browser_process = new ChromeBrowserProcessStub();

  // Create the new global context object.
  g_context = new CefContext();

  // Initialize the global context.
  return g_context->Initialize(args, settings, application,
                               windows_sandbox_info);
}

void CefShutdown() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!g_context->OnInitThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  // Shut down the global context. This will block until shutdown is complete.
  g_context->Shutdown();

  // Delete the global context object.
  delete g_context;
  g_context = nullptr;
}

void CefDoMessageLoopWork() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!g_context->OnInitThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void CefRunMessageLoop() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!g_context->OnInitThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  base::RunLoop run_loop;
  run_loop.Run();
}

void CefQuitMessageLoop() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!g_context->OnInitThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  // Quit the CefBrowserMessageLoop.
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

void CefSetOSModalLoop(bool osModalLoop) {
#if defined(OS_WIN)
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (CEF_CURRENTLY_ON_UIT())
    base::MessageLoopCurrent::Get()->set_os_modal_loop(osModalLoop);
  else
    CEF_POST_TASK(CEF_UIT, base::Bind(CefSetOSModalLoop, osModalLoop));
#endif  // defined(OS_WIN)
}

// CefContext

CefContext::CefContext()
    : initialized_(false), shutting_down_(false), init_thread_id_(0) {}

CefContext::~CefContext() {}

// static
CefContext* CefContext::Get() {
  return g_context;
}

bool CefContext::Initialize(const CefMainArgs& args,
                            const CefSettings& settings,
                            CefRefPtr<CefApp> application,
                            void* windows_sandbox_info) {
  init_thread_id_ = base::PlatformThread::CurrentId();
  settings_ = settings;

#if !(defined(OS_WIN) || defined(OS_LINUX))
  if (settings.multi_threaded_message_loop) {
    NOTIMPLEMENTED() << "multi_threaded_message_loop is not supported.";
    return false;
  }
#endif

#if defined(OS_WIN)
  // Signal Chrome Elf that Chrome has begun to start.
  SignalChromeElf();
#endif

  base::FilePath cache_path = base::FilePath(CefString(&settings_.cache_path));
  if (!ValidateCachePath(cache_path)) {
    // Reset to in-memory storage.
    CefString(&settings_.cache_path).clear();
    cache_path = base::FilePath();
  }
  const base::FilePath& root_cache_path =
      base::FilePath(CefString(&settings_.root_cache_path));
  if (root_cache_path.empty() && !cache_path.empty()) {
    CefString(&settings_.root_cache_path) = CefString(&settings_.cache_path);
  }

  main_delegate_.reset(new CefMainDelegate(application));
  browser_info_manager_.reset(new CefBrowserInfoManager);

  int exit_code;

  // Initialize the content runner.
  content::ContentMainParams params(main_delegate_.get());
#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  if (windows_sandbox_info == nullptr) {
    if (!settings.no_sandbox) {
      content::InitializeSandboxInfo(&sandbox_info);
    }
    windows_sandbox_info = &sandbox_info;
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

  exit_code = service_manager::MainInitialize(*sm_main_params_);
  DCHECK_LT(exit_code, 0);
  if (exit_code >= 0)
    return false;

  static_cast<ChromeBrowserProcessStub*>(g_browser_process)->Initialize();

  if (settings.multi_threaded_message_loop) {
    base::WaitableEvent uithread_startup_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    if (!main_delegate_->CreateUIThread(base::BindOnce(
            [](CefContext* context, base::WaitableEvent* event) {
              service_manager::MainRun(*context->sm_main_params_);
              event->Signal();
            },
            base::Unretained(this),
            base::Unretained(&uithread_startup_event)))) {
      return false;
    }

    initialized_ = true;

    // We need to wait until service_manager::MainRun has finished.
    uithread_startup_event.Wait();
  } else {
    initialized_ = true;
    service_manager::MainRun(*sm_main_params_);
  }

  if (CEF_CURRENTLY_ON_UIT()) {
    OnContextInitialized();
  } else {
    // Continue initialization on the UI thread.
    CEF_POST_TASK(CEF_UIT, base::Bind(&CefContext::OnContextInitialized,
                                      base::Unretained(this)));
  }

  return true;
}

void CefContext::Shutdown() {
  // Must always be called on the same thread as Initialize.
  DCHECK(OnInitThread());

  shutting_down_ = true;

  if (settings_.multi_threaded_message_loop) {
    // Events that will be used to signal when shutdown is complete. Start in
    // non-signaled mode so that the event will block.
    base::WaitableEvent uithread_shutdown_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    // Finish shutdown on the UI thread.
    CEF_POST_TASK(CEF_UIT,
                  base::Bind(&CefContext::FinishShutdownOnUIThread,
                             base::Unretained(this), &uithread_shutdown_event));

    /// Block until UI thread shutdown is complete.
    uithread_shutdown_event.Wait();

    FinalizeShutdown();
  } else {
    // Finish shutdown on the current thread, which should be the UI thread.
    FinishShutdownOnUIThread(nullptr);

    FinalizeShutdown();
  }
}

bool CefContext::OnInitThread() {
  return (base::PlatformThread::CurrentId() == init_thread_id_);
}

SkColor CefContext::GetBackgroundColor(
    const CefBrowserSettings* browser_settings,
    cef_state_t windowless_state) const {
  bool is_windowless = windowless_state == STATE_ENABLED
                           ? true
                           : (windowless_state == STATE_DISABLED
                                  ? false
                                  : !!settings_.windowless_rendering_enabled);

  // Default to opaque white if no acceptable color values are found.
  SkColor sk_color = SK_ColorWHITE;

  if (!browser_settings ||
      !GetColor(browser_settings->background_color, is_windowless, &sk_color)) {
    GetColor(settings_.background_color, is_windowless, &sk_color);
  }
  return sk_color;
}

CefTraceSubscriber* CefContext::GetTraceSubscriber() {
  CEF_REQUIRE_UIT();
  if (shutting_down_)
    return nullptr;
  if (!trace_subscriber_.get())
    trace_subscriber_.reset(new CefTraceSubscriber());
  return trace_subscriber_.get();
}

void CefContext::PopulateRequestContextSettings(
    CefRequestContextSettings* settings) {
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();
  CefString(&settings->cache_path) = CefString(&settings_.cache_path);
  settings->persist_session_cookies =
      settings_.persist_session_cookies ||
      command_line->HasSwitch(switches::kPersistSessionCookies);
  settings->persist_user_preferences =
      settings_.persist_user_preferences ||
      command_line->HasSwitch(switches::kPersistUserPreferences);
  settings->ignore_certificate_errors =
      settings_.ignore_certificate_errors ||
      command_line->HasSwitch(switches::kIgnoreCertificateErrors);
  CefString(&settings->accept_language_list) =
      CefString(&settings_.accept_language_list);
}

bool CefContext::ValidateCachePath(const base::FilePath& cache_path) {
  if (cache_path.empty())
    return true;

  const base::FilePath& root_cache_path =
      base::FilePath(CefString(&settings_.root_cache_path));
  if (!root_cache_path.empty() && root_cache_path != cache_path &&
      !root_cache_path.IsParent(cache_path)) {
    LOG(ERROR) << "The cache_path directory (" << cache_path.value()
               << ") is not a child of the root_cache_path directory ("
               << root_cache_path.value() << ")";
    return false;
  }

  base::ThreadRestrictions::ScopedAllowIO allow_io;
  if (!base::DirectoryExists(cache_path) &&
      !base::CreateDirectory(cache_path)) {
    LOG(ERROR) << "The cache_path directory (" << cache_path.value()
               << ") could not be created.";
    return false;
  }

  return true;
}

void CefContext::AddObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.AddObserver(observer);
}

void CefContext::RemoveObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.RemoveObserver(observer);
}

bool CefContext::HasObserver(Observer* observer) const {
  CEF_REQUIRE_UIT();
  return observers_.HasObserver(observer);
}

void CefContext::OnContextInitialized() {
  CEF_REQUIRE_UIT();

  static_cast<ChromeBrowserProcessStub*>(g_browser_process)
      ->OnContextInitialized();

#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_LIBRARY_CDMS)
  CefWidevineLoader::GetInstance()->OnContextInitialized();
#endif

  // Notify the handler.
  CefRefPtr<CefApp> app = CefContentClient::Get()->application();
  if (app.get()) {
    CefRefPtr<CefBrowserProcessHandler> handler =
        app->GetBrowserProcessHandler();
    if (handler.get())
      handler->OnContextInitialized();
  }
}

void CefContext::FinishShutdownOnUIThread(
    base::WaitableEvent* uithread_shutdown_event) {
  CEF_REQUIRE_UIT();

  browser_info_manager_->DestroyAllBrowsers();

  for (auto& observer : observers_)
    observer.OnContextDestroyed();

  if (trace_subscriber_.get())
    trace_subscriber_.reset(nullptr);

  static_cast<ChromeBrowserProcessStub*>(g_browser_process)->Shutdown();

  ui::ResourceBundle::GetSharedInstance().CleanupOnUIThread();

  sm_main_delegate_->ShutdownOnUIThread();

  if (uithread_shutdown_event)
    uithread_shutdown_event->Signal();
}

void CefContext::FinalizeShutdown() {
  if (content::RenderProcessHost::run_renderer_in_process()) {
    // Blocks until RenderProcess cleanup is complete.
    CefContentRendererClient::Get()->RunSingleProcessCleanup();
  }

  // Shut down the browser runner or UI thread.
  main_delegate_->ShutdownBrowser();

  // Shut down the content runner.
  service_manager::MainShutdown(*sm_main_params_);

  browser_info_manager_.reset(nullptr);
  sm_main_params_.reset(nullptr);
  sm_main_delegate_.reset(nullptr);
  main_delegate_.reset(nullptr);

  delete g_browser_process;
  g_browser_process = nullptr;
}
