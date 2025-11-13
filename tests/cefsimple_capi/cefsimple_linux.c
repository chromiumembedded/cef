// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_app.h"

#if defined(CEF_X11)
#include <X11/Xlib.h>
#endif

#include "include/capi/cef_app_capi.h"
#include "include/cef_api_hash.h"
#include "tests/cefsimple_capi/simple_utils.h"

#if defined(CEF_X11)
static int XErrorHandlerImpl(Display* display, XErrorEvent* event) {
  // X error handler - just return to avoid terminating the application.
  return 0;
}

static int XIOErrorHandlerImpl(Display* display) {
  return 0;
}
#endif  // defined(CEF_X11)

// Entry point function for all processes.
int main(int argc, char* argv[]) {
  // Configure the CEF API version. This must be called before any other CEF
  // API functions.
  cef_api_hash(CEF_API_VERSION, 0);

  // Provide CEF with command-line arguments.
  cef_main_args_t main_args = {};
  main_args.argc = argc;
  main_args.argv = argv;

  // Create the application instance (with 1 reference).
  simple_app_t* app = simple_app_create();
  CHECK(app);

  // Add reference before cef_execute_process. Both cef_execute_process and
  // cef_initialize will take ownership of a reference, so we need 2 total.
  app->app.base.add_ref(&app->app.base);

  // CEF applications have multiple sub-processes (render, GPU, etc) that share
  // the same executable. This function checks the command-line and, if this is
  // a sub-process, executes the appropriate logic.
  int exit_code = cef_execute_process(&main_args, &app->app, NULL);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    // cef_execute_process took ownership of one reference.
    // Release only the additional reference we added.
    app->app.base.release(&app->app.base);
    return exit_code;
  }

#if defined(CEF_X11)
  // Install xlib error handlers so that the application won't be terminated
  // on non-fatal errors.
  XSetErrorHandler(XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

  // Specify CEF global settings here.
  cef_settings_t settings = {};
  settings.size = sizeof(cef_settings_t);

// When generating projects with CMake the CEF_USE_SANDBOX value will be defined
// automatically. Pass -DUSE_SANDBOX=OFF to the CMake command-line to disable
// use of the sandbox.
#if !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = 1;
#endif

  // Initialize the CEF browser process. May return false if initialization
  // fails or if early exit is desired (for example, due to process singleton
  // relaunch behavior).
  if (!cef_initialize(&main_args, &settings, &app->app, NULL)) {
    // cef_initialize took ownership of the remaining app reference so we don't
    // need to release any.
    return cef_get_exit_code();
  }

  // Run the CEF message loop. This will block until cef_quit_message_loop() is
  // called.
  cef_run_message_loop();

  // Shut down CEF.
  cef_shutdown();

  // Note: We DON'T release the app here. The 2 total references have been
  // given to cef_execute_process and cef_initialize. If cef_initialize
  // succeeded the final reference will be released during cef_shutdown.

  return 0;
}
