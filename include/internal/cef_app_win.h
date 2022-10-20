// Copyright (c) 2022 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CEF_INCLUDE_INTERNAL_CEF_APP_WIN_H_
#define CEF_INCLUDE_INTERNAL_CEF_APP_WIN_H_
#pragma once

#include "include/base/cef_build.h"

#if defined(OS_WIN)

#if defined(ARCH_CPU_32_BITS)
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ARCH_CPU_32_BITS)
typedef int(APIENTRY* wWinMainPtr)(HINSTANCE hInstance,
                                   HINSTANCE hPrevInstance,
                                   LPWSTR lpCmdLine,
                                   int nCmdShow);
typedef int (*mainPtr)(int argc, char* argv[]);

///
/// Run the main thread on 32-bit Windows using a fiber with the preferred 4MiB
/// stack size. This function must be called at the top of the executable entry
/// point function (`main()` or `wWinMain()`). It is used in combination with
/// the initial stack size of 0.5MiB configured via the `/STACK:0x80000` linker
/// flag on executable targets. This saves significant memory on threads (like
/// those in the Windows thread pool, and others) whose stack size can only be
/// controlled via the linker flag.
///
/// CEF's main thread needs at least a 1.5 MiB stack size in order to avoid
/// stack overflow crashes. However, if this is set in the PE file then other
/// threads get this size as well, leading to address-space exhaustion in 32-bit
/// CEF. This function uses fibers to switch the main thread to a 4 MiB stack
/// (roughly the same effective size as the 64-bit build's 8 MiB stack) before
/// running any other code.
///
/// Choose the function variant that matches the entry point function type used
/// by the executable. Reusing the entry point minimizes confusion when
/// examining call stacks in crash reports.
///
/// If this function is already running on the fiber it will return -1
/// immediately, meaning that execution should proceed with the remainder of the
/// entry point function. Otherwise, this function will block until the entry
/// point function has completed execution on the fiber and then return a result
/// >= 0, meaning that the entry point function should return the result
/// immediately without proceeding with execution.
///
CEF_EXPORT int cef_run_winmain_with_preferred_stack_size(wWinMainPtr wWinMain,
                                                         HINSTANCE hInstance,
                                                         LPWSTR lpCmdLine,
                                                         int nCmdShow);
CEF_EXPORT int cef_run_main_with_preferred_stack_size(mainPtr main,
                                                      int argc,
                                                      char* argv[]);
#endif  // defined(ARCH_CPU_32_BITS)

///
/// Call during process startup to enable High-DPI support on Windows 7 or
/// newer. Older versions of Windows should be left DPI-unaware because they do
/// not support DirectWrite and GDI fonts are kerned very badly.
///
CEF_EXPORT void cef_enable_highdpi_support(void);

///
/// Set to true (1) before calling Windows APIs like TrackPopupMenu that enter a
/// modal message loop. Set to false (0) after exiting the modal message loop.
///
CEF_EXPORT void cef_set_osmodal_loop(int osModalLoop);

#ifdef __cplusplus
}
#endif

#endif  // defined(OS_WIN)
#endif  // CEF_INCLUDE_INTERNAL_CEF_APP_WIN_H_
