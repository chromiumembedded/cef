// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#undef Success     // Definition conflicts with cef_message_router.h
#undef RootWindow  // Definition conflicts with root_window.h

#include <stdlib.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "include/base/cef_logging.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/cefclient/browser/main_context_impl.h"
#include "tests/cefclient/browser/main_message_loop_multithreaded_gtk.h"
#include "tests/cefclient/browser/test_runner.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/browser/main_message_loop_external_pump.h"
#include "tests/shared/browser/main_message_loop_std.h"
#include "tests/shared/common/client_app_other.h"
#include "tests/shared/common/client_switches.h"
#include "tests/shared/renderer/client_app_renderer.h"

namespace client {
namespace {

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

void TerminationSignalHandler(int signatl) {
  LOG(ERROR) << "Received termination signal: " << signatl;
  MainContext::Get()->GetRootWindowManager()->CloseAllWindows(true);
}

NO_STACK_PROTECTOR
int RunMain(int argc, char* argv[]) {
  // Create a copy of |argv| on Linux because Chromium mangles the value
  // internally (see issue #620).
  CefScopedArgArray scoped_arg_array(argc, argv);
  char** argv_copy = scoped_arg_array.array();

  CefMainArgs main_args(argc, argv);

  // Parse command-line arguments.
  CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
  command_line->InitFromArgv(argc, argv);

  // Create a ClientApp of the correct type.
  CefRefPtr<CefApp> app;
  ClientApp::ProcessType process_type = ClientApp::GetProcessType(command_line);
  if (process_type == ClientApp::BrowserProcess) {
    app = new ClientAppBrowser();
  } else if (process_type == ClientApp::RendererProcess ||
             process_type == ClientApp::ZygoteProcess) {
    // On Linux the zygote process is used to spawn other process types. Since
    // we don't know what type of process it will be give it the renderer
    // client.
    app = new ClientAppRenderer();
  } else if (process_type == ClientApp::OtherProcess) {
    app = new ClientAppOther();
  }

  // Execute the secondary process, if any.
  int exit_code = CefExecuteProcess(main_args, app, nullptr);
  if (exit_code >= 0) {
    return exit_code;
  }

  // Create the main context object.
  auto context = std::make_unique<MainContextImpl>(command_line, true);

  CefSettings settings;

// When generating projects with CMake the CEF_USE_SANDBOX value will be defined
// automatically. Pass -DUSE_SANDBOX=OFF to the CMake command-line to disable
// use of the sandbox.
#if !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = true;
#endif

  // Populate the settings based on command line arguments.
  context->PopulateSettings(&settings);

  // Create the main message loop object.
  std::unique_ptr<MainMessageLoop> message_loop;
  if (settings.multi_threaded_message_loop) {
    message_loop.reset(new MainMessageLoopMultithreadedGtk);
  } else if (settings.external_message_pump) {
    message_loop = MainMessageLoopExternalPump::Create();
  } else {
    message_loop.reset(new MainMessageLoopStd);
  }

  // Initialize the CEF browser process. May return false if initialization
  // fails or if early exit is desired (for example, due to process singleton
  // relaunch behavior).
  if (!context->Initialize(main_args, settings, app, nullptr)) {
    return CefGetExitCode();
  }

  // Force Gtk to use Xwayland (in case a Wayland compositor is being used).
  gdk_set_allowed_backends("x11");

  // The Chromium sandbox requires that there only be a single thread during
  // initialization. Therefore initialize GTK after CEF.
  gtk_init(&argc, &argv_copy);

  // Install xlib error handlers so that the application won't be terminated
  // on non-fatal errors. Must be done after initializing GTK.
  XSetErrorHandler(XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);

  // Install a signal handler so we clean up after ourselves.
  signal(SIGINT, TerminationSignalHandler);
  signal(SIGTERM, TerminationSignalHandler);

  // Register scheme handlers.
  test_runner::RegisterSchemeHandlers();

  auto window_config = std::make_unique<RootWindowConfig>();
  window_config->always_on_top =
      command_line->HasSwitch(switches::kAlwaysOnTop);
  window_config->with_osr =
      settings.windowless_rendering_enabled ? true : false;

  // Create the first window.
  context->GetRootWindowManager()->CreateRootWindow(std::move(window_config));

  // Run the message loop. This will block until Quit() is called.
  int result = message_loop->Run();

  // Shut down CEF.
  context->Shutdown();

  // Release objects in reverse order of creation.
  message_loop.reset();
  context.reset();

  return result;
}

}  // namespace
}  // namespace client

// Program entry point function.
NO_STACK_PROTECTOR
int main(int argc, char* argv[]) {
  return client::RunMain(argc, argv);
}
