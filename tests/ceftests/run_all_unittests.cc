// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_build.h"
#include "include/cef_config.h"

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
#include "include/cef_app.h"
#include "include/cef_task.h"
#include "include/cef_thread.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_server.h"
#include "tests/ceftests/test_suite.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/browser/main_message_loop_external_pump.h"
#include "tests/shared/browser/main_message_loop_std.h"
#include "tests/shared/common/client_app_other.h"
#include "tests/shared/renderer/client_app_renderer.h"

#if defined(OS_MAC)
#include "include/wrapper/cef_library_loader.h"
#endif

// When generating projects with CMake the CEF_USE_SANDBOX value will be defined
// automatically if using the required compiler version. Pass -DUSE_SANDBOX=OFF
// to the CMake command-line to disable use of the sandbox.
#if defined(OS_WIN) && defined(CEF_USE_SANDBOX)
#include "include/cef_sandbox_win.h"

// The cef_sandbox.lib static library may not link successfully with all VS
// versions.
#pragma comment(lib, "cef_sandbox.lib")
#endif

namespace {

void QuitMessageLoop() {
  CEF_REQUIRE_UI_THREAD();
  client::MainMessageLoop* message_loop = client::MainMessageLoop::Get();
  if (message_loop)
    message_loop->Quit();
  else
    CefQuitMessageLoop();
}

void sleep(int64 ms) {
#if defined(OS_WIN)
  Sleep(ms);
#elif defined(OS_POSIX)
  usleep(ms * 1000);
#else
#error Unsupported platform
#endif
}

// Called on the test thread.
void RunTestsOnTestThread() {
  // Run the test suite.
  CefTestSuite::GetInstance()->Run();

  // Wait for all browsers to exit.
  while (TestHandler::HasBrowser())
    sleep(100);

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
  LOG(WARNING) << "X error received: "
               << "type " << event->type << ", "
               << "serial " << event->serial << ", "
               << "error_code " << static_cast<int>(event->error_code) << ", "
               << "request_code " << static_cast<int>(event->request_code)
               << ", "
               << "minor_code " << static_cast<int>(event->minor_code);
  return 0;
}

int XIOErrorHandlerImpl(Display* display) {
  return 0;
}
#endif  // defined(OS_LINUX) && defined(CEF_X11)

}  // namespace

int main(int argc, char* argv[]) {
#if defined(OS_MAC)
  // Load the CEF framework library at runtime instead of linking directly
  // as required by the macOS sandbox implementation.
  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInMain())
    return 1;
#endif

  // Create the singleton test suite object.
  CefTestSuite test_suite(argc, argv);

#if defined(OS_WIN)
  if (test_suite.command_line()->HasSwitch("enable-high-dpi-support")) {
    // Enable High-DPI support on Windows 7 and newer.
    CefEnableHighDPISupport();
  }

  CefMainArgs main_args(::GetModuleHandle(nullptr));
#else
  CefMainArgs main_args(argc, argv);
#endif

  void* windows_sandbox_info = nullptr;

#if defined(OS_WIN) && defined(CEF_USE_SANDBOX)
  // Manages the life span of the sandbox information object.
  CefScopedSandboxInfo scoped_sandbox;
  windows_sandbox_info = scoped_sandbox.sandbox_info();
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
  int exit_code = CefExecuteProcess(main_args, app, windows_sandbox_info);
  if (exit_code >= 0)
    return exit_code;
#else
  } else {
    // On OS X this executable is only used for the main process.
    NOTREACHED();
  }
#endif

  CefSettings settings;

#if !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = true;
#endif

  client::ClientAppBrowser::PopulateSettings(test_suite.command_line(),
                                             settings);
  test_suite.GetSettings(settings);

#if defined(OS_MAC)
  // Platform-specific initialization.
  extern void PlatformInit();
  PlatformInit();
#endif

#if defined(OS_LINUX) && defined(CEF_X11)
  // Install xlib error handlers so that the application won't be terminated
  // on non-fatal errors.
  XSetErrorHandler(XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

  // Create the MessageLoop.
  std::unique_ptr<client::MainMessageLoop> message_loop;
  if (!settings.multi_threaded_message_loop) {
    if (settings.external_message_pump)
      message_loop = client::MainMessageLoopExternalPump::Create();
    else
      message_loop.reset(new client::MainMessageLoopStd);
  }

  // Initialize CEF.
  CefInitialize(main_args, settings, app, windows_sandbox_info);

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
    if (!thread)
      return 1;

    // Start the tests from the UI thread so that any pending UI tasks get a
    // chance to execute first.
    CefPostTask(TID_UI,
                base::BindOnce(&ContinueOnUIThread, thread->GetTaskRunner()));

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

  // Destroy the MessageLoop.
  message_loop.reset(nullptr);

#if defined(OS_MAC)
  // Platform-specific cleanup.
  extern void PlatformCleanup();
  PlatformCleanup();
#endif

  return retval;
}
