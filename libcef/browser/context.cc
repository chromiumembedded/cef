// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/context.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_main.h"
#include "libcef/browser/browser_message_loop.h"
#include "libcef/browser/chrome_browser_process_stub.h"
#include "libcef/browser/component_updater/cef_component_updater_configurator.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/trace_subscriber.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/main_delegate.h"
#include "libcef/renderer/content_renderer_client.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/configurator.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

namespace {

CefContext* g_context = NULL;

// Force shutdown when the process terminates if a context currently exists and
// CefShutdown() has not been explicitly called.
class CefForceShutdown {
 public:
  ~CefForceShutdown() {
    if (g_context) {
      g_context->Shutdown();
      delete g_context;
      g_context = NULL;
    }
  }
} g_force_shutdown;

#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
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
#endif  // WIN && ARCH_CPU_X86_64

}  // namespace

int CefExecuteProcess(const CefMainArgs& args,
                      CefRefPtr<CefApp> application,
                      void* windows_sandbox_info) {
#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
  DisableFMA3();
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

  CefMainDelegate main_delegate(application);

  // Execute the secondary process.
#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  if (windows_sandbox_info == NULL) {
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
#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
  DisableFMA3();
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
  g_context = NULL;
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

  CefBrowserMessageLoop::current()->DoMessageLoopIteration();
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

  CefBrowserMessageLoop::current()->RunMessageLoop();
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

  CefBrowserMessageLoop::current()->QuitWhenIdle();
}

void CefSetOSModalLoop(bool osModalLoop) {
#if defined(OS_WIN)
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (CEF_CURRENTLY_ON_UIT())
    base::MessageLoop::current()->set_os_modal_loop(osModalLoop);
  else
    CEF_POST_TASK(CEF_UIT, base::Bind(CefSetOSModalLoop, osModalLoop));
#endif  // defined(OS_WIN)
}


// CefContext

CefContext::CefContext()
  : initialized_(false),
    shutting_down_(false),
    init_thread_id_(0) {
}

CefContext::~CefContext() {
}

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

#if !defined(OS_WIN)
  if (settings.multi_threaded_message_loop) {
    NOTIMPLEMENTED() << "multi_threaded_message_loop is not supported.";
    return false;
  }
#endif

  main_delegate_.reset(new CefMainDelegate(application));
  main_runner_.reset(content::ContentMainRunner::Create());
  browser_info_manager_.reset(new CefBrowserInfoManager);

  int exit_code;

  // Initialize the content runner.
#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  if (windows_sandbox_info == NULL) {
    content::InitializeSandboxInfo(&sandbox_info);
    windows_sandbox_info = &sandbox_info;
    settings_.no_sandbox = true;
  }

  content::ContentMainParams params(main_delegate_.get());
  params.instance = args.instance;
  params.sandbox_info =
      static_cast<sandbox::SandboxInterfaceInfo*>(windows_sandbox_info);

  exit_code = main_runner_->Initialize(params);
#else
  content::ContentMainParams params(main_delegate_.get());
  params.argc = args.argc;
  params.argv = const_cast<const char**>(args.argv);

  exit_code = main_runner_->Initialize(params);
#endif

  DCHECK_LT(exit_code, 0);
  if (exit_code >= 0)
    return false;

  // Run the process. Results in a call to CefMainDelegate::RunProcess() which
  // will create the browser runner and message loop without blocking.
  exit_code = main_runner_->Run();

  initialized_ = true;

  if (CEF_CURRENTLY_ON_UIT()) {
    OnContextInitialized();
  } else {
    // Continue initialization on the UI thread.
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefContext::OnContextInitialized, base::Unretained(this)));
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
    base::WaitableEvent uithread_shutdown_event(false, false);

    // Finish shutdown on the UI thread.
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefContext::FinishShutdownOnUIThread,
                   base::Unretained(this),
                   &uithread_shutdown_event));

    /// Block until UI thread shutdown is complete.
    uithread_shutdown_event.Wait();

    FinalizeShutdown();
  } else {
    // Finish shutdown on the current thread, which should be the UI thread.
    FinishShutdownOnUIThread(NULL);

    FinalizeShutdown();
  }
}

bool CefContext::OnInitThread() {
  return (base::PlatformThread::CurrentId() == init_thread_id_);
}

CefTraceSubscriber* CefContext::GetTraceSubscriber() {
  CEF_REQUIRE_UIT();
  if (shutting_down_)
    return NULL;
  if (!trace_subscriber_.get())
    trace_subscriber_.reset(new CefTraceSubscriber());
  return trace_subscriber_.get();
}

component_updater::ComponentUpdateService*
CefContext::component_updater() {
  if (!component_updater_.get()) {
    CEF_REQUIRE_UIT_RETURN(NULL);
    scoped_refptr<update_client::Configurator> configurator =
        component_updater::MakeCefComponentUpdaterConfigurator(
            base::CommandLine::ForCurrentProcess(),
            CefContentBrowserClient::Get()->browser_context()->
                request_context().get());
    // Creating the component updater does not do anything, components
    // need to be registered and Start() needs to be called.
    component_updater_.reset(component_updater::ComponentUpdateServiceFactory(
                                 configurator).release());
  }
  return component_updater_.get();
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

void RegisterComponentsForUpdate() {
  component_updater::ComponentUpdateService* cus =
      CefContext::Get()->component_updater();

  // Registration can be before or after cus->Start() so it is ok to post
  // a task to the UI thread to do registration once you done the necessary
  // file IO to know you existing component version.
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWidevineCdm)) {
    RegisterWidevineCdmComponent(cus);
  }
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
}

void CefContext::OnContextInitialized() {
  CEF_REQUIRE_UIT();

  // Must be created after the NotificationService.
  print_job_manager_.reset(new printing::PrintJobManager());

  bool io_was_allowed = base::ThreadRestrictions::SetIOAllowed(true);
  RegisterComponentsForUpdate();
  base::ThreadRestrictions::SetIOAllowed(io_was_allowed);

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

  // Wait for the pending print jobs to finish. Don't do this later, since
  // this might cause a nested message loop to run, and we don't want pending
  // tasks to run once teardown has started.
  print_job_manager_->Shutdown();
  print_job_manager_.reset(NULL);

  browser_info_manager_->DestroyAllBrowsers();

  if (trace_subscriber_.get())
    trace_subscriber_.reset(NULL);

  if (component_updater_.get())
    component_updater_.reset(NULL);

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
  main_runner_->Shutdown();

  browser_info_manager_.reset(NULL);
  main_runner_.reset(NULL);
  main_delegate_.reset(NULL);
}
