// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//

#include <cstddef>

#include "include/base/cef_build.h"
#include "include/cef_api_hash.h"
#include "include/cef_version.h"

#if defined(OS_WIN)
#include "include/internal/cef_win.h"
#endif

CEF_EXPORT int cef_version_info(int entry) {
  switch (entry) {
    case 0:
      return CEF_VERSION_MAJOR;
    case 1:
      return CEF_VERSION_MINOR;
    case 2:
      return CEF_VERSION_PATCH;
    case 3:
      return CEF_COMMIT_NUMBER;
    case 4:
      return CHROME_VERSION_MAJOR;
    case 5:
      return CHROME_VERSION_MINOR;
    case 6:
      return CHROME_VERSION_BUILD;
    case 7:
      return CHROME_VERSION_PATCH;
    default:
      return 0;
  }
}

CEF_EXPORT const char* cef_api_hash(int entry) {
  switch (entry) {
    case 0:
      return CEF_API_HASH_PLATFORM;
    case 1:
      return CEF_API_HASH_UNIVERSAL;
    case 2:
      return CEF_COMMIT_HASH;
    default:
      return NULL;
  }
}

#if defined(OS_WIN)

#if defined(ARCH_CPU_32_BITS)
CEF_EXPORT int cef_run_winmain_with_preferred_stack_size(wWinMainPtr wWinMain,
                                                         HINSTANCE hInstance,
                                                         LPWSTR lpCmdLine,
                                                         int nCmdShow) {
  return CefRunWinMainWithPreferredStackSize(wWinMain, hInstance, lpCmdLine,
                                             nCmdShow);
}

CEF_EXPORT int cef_run_main_with_preferred_stack_size(mainPtr main,
                                                      int argc,
                                                      char* argv[]) {
  return CefRunMainWithPreferredStackSize(main, argc, argv);
}
#endif  // defined(ARCH_CPU_32_BITS)

CEF_EXPORT void cef_set_osmodal_loop(int osModalLoop) {
  CefSetOSModalLoop(osModalLoop ? true : false);
}

#endif  // defined(OS_WIN)
