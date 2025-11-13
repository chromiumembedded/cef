// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/capi/cef_app_capi.h"
#include "include/cef_api_hash.h"
#include "include/wrapper/cef_library_loader.h"

// When generating projects with CMake the CEF_USE_SANDBOX value will be defined
// automatically. Pass -DUSE_SANDBOX=OFF to the CMake command-line to disable
// use of the sandbox.
#if defined(CEF_USE_SANDBOX)
#include "include/cef_sandbox_mac.h"
#endif

// Entry point function for sub-processes.
int main(int argc, char* argv[]) {
#if defined(CEF_USE_SANDBOX)
  // Initialize the macOS sandbox for this helper process.
  void* sandbox_context = cef_scoped_sandbox_initialize(argc, argv);
  if (!sandbox_context) {
    return 1;
  }
#endif

  // Load the CEF framework library at runtime instead of linking directly
  // as required by the macOS sandbox implementation.
  void* library_loader = cef_scoped_library_loader_create(/*helper=*/1);
  if (!library_loader) {
#if defined(CEF_USE_SANDBOX)
    cef_scoped_sandbox_destroy(sandbox_context);
#endif
    return 1;
  }

  // Configure the CEF API version. This must be called before any other CEF
  // API functions.
  cef_api_hash(CEF_API_VERSION, 0);

  // Provide CEF with command-line arguments.
  cef_main_args_t main_args = {};
  main_args.argc = argc;
  main_args.argv = argv;

  // Execute the sub-process.
  int result = cef_execute_process(&main_args, NULL, NULL);

  // Unload the CEF framework library.
  cef_unload_library();

#if defined(CEF_USE_SANDBOX)
  // Destroy the sandbox context.
  cef_scoped_sandbox_destroy(sandbox_context);
#endif

  return result;
}
