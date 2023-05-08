// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_build.h"

#if defined(OS_WIN)

#include "include/base/cef_compiler_specific.h"
#include "include/base/cef_logging.h"
#include "include/cef_api_hash.h"
#include "include/internal/cef_win.h"

#if defined(ARCH_CPU_32_BITS)
NO_SANITIZE("cfi-icall")
int CefRunWinMainWithPreferredStackSize(wWinMainPtr wWinMain,
                                        HINSTANCE hInstance,
                                        LPWSTR lpCmdLine,
                                        int nCmdShow) {
  CHECK(wWinMain && hInstance);

  const char* api_hash = cef_api_hash(0);
  if (strcmp(api_hash, CEF_API_HASH_PLATFORM)) {
    // The libcef API hash does not match the current header API hash.
    DCHECK(false);
    return 0;
  }

  return cef_run_winmain_with_preferred_stack_size(wWinMain, hInstance,
                                                   lpCmdLine, nCmdShow);
}

int CefRunMainWithPreferredStackSize(mainPtr main, int argc, char* argv[]) {
  CHECK(main);

  const char* api_hash = cef_api_hash(0);
  if (strcmp(api_hash, CEF_API_HASH_PLATFORM)) {
    // The libcef API hash does not match the current header API hash.
    DCHECK(false);
    return 0;
  }

  return cef_run_main_with_preferred_stack_size(main, argc, argv);
}
#endif  // defined(ARCH_CPU_32_BITS)

NO_SANITIZE("cfi-icall") void CefSetOSModalLoop(bool osModalLoop) {
  cef_set_osmodal_loop(osModalLoop);
}

#endif  // defined(OS_WIN)
