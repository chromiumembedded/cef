// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <memory>

#include "include/base/cef_build.h"
#include "include/cef_version_info.h"

#if defined(OS_LINUX) && defined(CEF_X11)
#include <X11/Xlib.h>
// Definitions conflict with gtest.
#undef None
#undef Bool
#endif

#if defined(OS_POSIX)
#include <unistd.h>
#endif

#include "include/base/cef_callback.h"
#include "include/cef_api_hash.h"
#include "include/cef_app.h"
#include "include/cef_task.h"
#include "include/cef_thread.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_server.h"
#include "tests/ceftests/test_suite.h"
#include "tests/ceftests/test_util.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/browser/main_message_loop_external_pump.h"
#include "tests/shared/browser/main_message_loop_std.h"
#include "tests/shared/common/client_app_other.h"
#include "tests/shared/renderer/client_app_renderer.h"

#if defined(OS_MAC)
#include "include/wrapper/cef_library_loader.h"
#endif

#if defined(OS_WIN)
#include "include/cef_sandbox_win.h"
#include "tests/shared/browser/util_win.h"
#endif

#if defined(OS_MAC)
// Platform-specific initialization and cleanup.
extern void PlatformInit();
extern void PlatformCleanup();
#endif

namespace {

void QuitMessageLoop() {
  CEF_REQUIRE_UI_THREAD();
  client::MainMessageLoop* message_loop = client::MainMessageLoop::Get();
  if (message_loop) {
    message_loop->Quit();
  } else {
    CefQuitMessageLoop();
  }
}

void sleep(int64_t ms) {
#if defined(OS_WIN)
  Sleep(ms);
#elif defined(OS_POSIX)
  usleep(static_cast<useconds_t>(ms * 1000));
#else
#error Unsupported platform
#endif
}

// Called on the test thread.
void RunTestsOnTestThread() {
  // Run the test suite.
  CefTestSuite::GetInstance()->Run();

  // Wait for all TestHandlers to be destroyed.
  size_t loop_count = 0;
  while (true) {
    const size_t handler_count = TestHandler::GetTestHandlerCount();
    if (handler_count == 0) {
      break;
    }
    if (++loop_count == 20) {
      LOG(ERROR) << "Terminating with " << handler_count
                 << " leaked TestHandler objects";
      break;
    }
    sleep(100);
  }

  // Wait for the test server to stop, and then quit the CEF message loop.
  test_server::Stop(base::BindOnce(QuitMessageLoop));
}

// Called on the UI thread.
void ContinueOnUIThread(CefRefPtr<CefTaskRunner> test_task_runner) {
  // Run the test suite on the test thread.
  test_task_runner->PostTask(
      CefCreateClosureTask(base::BindOnce(&RunTestsOnTestThread)));
}

#if defined(OS_LINUX) && defined(CEF_X11)
int XErrorHandlerImpl(Display* display, XErrorEvent* event) {
  LOG(WARNING) << "X error received: " << "type " << event->type << ", "
               << "serial " << event->serial << ", " << "error_code "
               << static_cast<int>(event->error_code) << ", " << "request_code "
               << static_cast<int>(event->request_code) << ", " << "minor_code "
               << static_cast<int>(event->minor_code);
  return 0;
}

int XIOErrorHandlerImpl(Display* display) {
  return 0;
}
#endif  // defined(OS_LINUX) && defined(CEF_X11)

#if defined(OS_MAC)
class ScopedPlatformSetup final {
 public:
  ScopedPlatformSetup() { PlatformInit(); }
  ~ScopedPlatformSetup() { PlatformCleanup(); }
};
#endif  // defined(OS_MAC)

int RunMain(int argc,
            char* argv[],
            void* sandbox_info) {
  int exit_code;

#if CEF_API_VERSION != CEF_EXPERIMENTAL
  printf("Running with configured CEF API version %d\n", CEF_API_VERSION);
#endif

#if defined(OS_MAC)
  // Load the CEF framework library at runtime instead of linking directly
  // as required by the macOS sandbox implementation.
  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInMain()) {
    return 1;
  }
#endif

  // Create the singleton test suite object.
  CefTestSuite test_suite(argc, argv);

#if defined(OS_WIN)
  CefMainArgs main_args(client::GetCodeModuleHandle());
#else
  CefMainArgs main_args(argc, argv);
#endif

  // Create a ClientApp of the correct type.
  CefRefPtr<CefApp> app;
  client::ClientApp::ProcessType process_type =
      client::ClientApp::GetProcessType(test_suite.command_line());
  if (process_type == client::ClientApp::BrowserProcess) {
    app = new client::ClientAppBrowser();
#if !defined(OS_MAC)
  } else if (process_type == client::ClientApp::RendererProcess ||
             process_type == client::ClientApp::ZygoteProcess) {
    app = new client::ClientAppRenderer();
  } else if (process_type == client::ClientApp::OtherProcess) {
    app = new client::ClientAppOther();
  }

  // Execute the secondary process, if any.
  exit_code = CefExecuteProcess(main_args, app, sandbox_info);
  if (exit_code >= 0) {
    return exit_code;
  }
#else
  } else {
    // On MacOS this executable is only used for the main process.
    NOTREACHED();
  }
#endif

  CefSettings settings;

#if defined(OS_WIN)
  if (!sandbox_info) {
    settings.no_sandbox = true;
  }
#elif !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = true;
#endif

  client::ClientAppBrowser::PopulateSettings(test_suite.command_line(),
                                             settings);
  test_suite.GetSettings(settings);

#if defined(OS_MAC)
  ScopedPlatformSetup scoped_platform_setup;
#endif

#if defined(OS_LINUX) && defined(CEF_X11)
  // Install xlib error handlers so that the application won't be terminated
  // on non-fatal errors.
  XSetErrorHandler(XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

  // Initialize CEF.
  if (!CefInitialize(main_args, settings, app, sandbox_info)) {
    exit_code = CefGetExitCode();
    LOG(ERROR) << "CefInitialize exited with code " << exit_code;
    return exit_code;
  }

  // Log the current configuration.
  LOG(WARNING)
      << "Using " << (UseAlloyStyleBrowserGlobal() ? "Alloy" : "Chrome")
      << " style browser; "
      << (UseViewsGlobal()
              ? (std::string(UseAlloyStyleWindowGlobal() ? "Alloy" : "Chrome") +
                 " style window; ")
              : "")
      << (UseViewsGlobal() ? "Views" : "Native") << "-hosted (not a warning)";

  std::unique_ptr<client::MainMessageLoop> message_loop;

  // Initialize the testing framework.
  test_suite.InitMainProcess();

  int retval;

  if (settings.multi_threaded_message_loop) {
    // Run the test suite on the main thread.
    retval = test_suite.Run();

    // Wait for the test server to stop.
    CefRefPtr<CefWaitableEvent> event =
        CefWaitableEvent::CreateWaitableEvent(true, false);
    test_server::Stop(base::BindOnce(&CefWaitableEvent::Signal, event));
    event->Wait();
  } else {
    // Create and start the test thread.
    CefRefPtr<CefThread> thread = CefThread::CreateThread("test_thread");
    if (!thread) {
      LOG(ERROR) << "test_thread creation failed";
      return 1;
    }

    // Start the tests from the UI thread so that any pending UI tasks get a
    // chance to execute first.
    CefPostTask(TID_UI,
                base::BindOnce(&ContinueOnUIThread, thread->GetTaskRunner()));

    // Create the CEF message loop.
    if (settings.external_message_pump) {
      message_loop = client::MainMessageLoopExternalPump::Create();
    } else {
      message_loop = std::make_unique<client::MainMessageLoopStd>();
    }

    // Run the CEF message loop.
    message_loop->Run();

    // The test suite has completed.
    retval = test_suite.retval();

    // Terminate the test thread.
    thread->Stop();
    thread = nullptr;
  }

  // Shut down CEF.
  CefShutdown();

  test_suite.DeleteTempDirectories();

  // Destroy the CEF message loop, if any.
  message_loop.reset(nullptr);

  return retval;
}

}  // namespace

#if defined(OS_WIN) && defined(CEF_USE_BOOTSTRAP)

// Entry point called by bootstrapc.exe when built as a DLL.
CEF_BOOTSTRAP_EXPORT int RunConsoleMain(int argc,
                                        char* argv[],
                                        void* sandbox_info,
                                        cef_version_info_t* /*version_info*/) {
  return ::RunMain(argc, argv, sandbox_info);
}

#else  // !(defined(OS_WIN) && defined(CEF_USE_BOOTSTRAP))

// Program entry point function.
NO_STACK_PROTECTOR
int main(int argc, char* argv[]) {
#if defined(OS_WIN) && defined(ARCH_CPU_32_BITS)
  // Run the main thread on 32-bit Windows using a fiber with the preferred 4MiB
  // stack size. This function must be called at the top of the executable entry
  // point function (`main()` or `wWinMain()`). It is used in combination with
  // the initial stack size of 0.5MiB configured via the `/STACK:0x80000` linker
  // flag on executable targets. This saves significant memory on threads (like
  // those in the Windows thread pool, and others) whose stack size can only be
  // controlled via the linker flag.
  int exit_code = CefRunMainWithPreferredStackSize(main, argc, argv);
  if (exit_code >= 0) {
    // The fiber has completed so return here.
    return exit_code;
  }
#endif

  void* sandbox_info = nullptr;

#if defined(OS_WIN) && defined(CEF_USE_SANDBOX)
  // Manages the life span of the sandbox information object.
  CefScopedSandboxInfo scoped_sandbox;
  sandbox_info = scoped_sandbox.sandbox_info();
#endif

  return ::RunMain(argc, argv, sandbox_info);
}

#endif  // !(defined(OS_WIN) && defined(CEF_USE_BOOTSTRAP))
