// Copyright (c) 2008 Marshall A. Greenblatt. All rights reserved.
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

#ifndef CEF_INCLUDE_INTERNAL_CEF_WIN_H_
#define CEF_INCLUDE_INTERNAL_CEF_WIN_H_
#pragma once

#include "include/internal/cef_app_win.h"
#include "include/internal/cef_types_win.h"
#include "include/internal/cef_types_wrappers.h"

// Handle types.
#define CefCursorHandle cef_cursor_handle_t
#define CefEventHandle cef_event_handle_t
#define CefWindowHandle cef_window_handle_t

///
/// Class representing CefExecuteProcess arguments.
///
class CefMainArgs : public cef_main_args_t {
 public:
  CefMainArgs() : cef_main_args_t{} {}
  CefMainArgs(const cef_main_args_t& r) : cef_main_args_t(r) {}
  explicit CefMainArgs(HINSTANCE hInstance) : cef_main_args_t{hInstance} {}
};

struct CefWindowInfoTraits {
  typedef cef_window_info_t struct_type;

  static inline void init(struct_type* s) {}

  static inline void clear(struct_type* s) {
    cef_string_clear(&s->window_name);
  }

  static inline void set(const struct_type* src,
                         struct_type* target,
                         bool copy) {
    target->ex_style = src->ex_style;
    cef_string_set(src->window_name.str, src->window_name.length,
                   &target->window_name, copy);
    target->style = src->style;
    target->bounds = src->bounds;
    target->parent_window = src->parent_window;
    target->menu = src->menu;
    target->windowless_rendering_enabled = src->windowless_rendering_enabled;
    target->shared_texture_enabled = src->shared_texture_enabled;
    target->external_begin_frame_enabled = src->external_begin_frame_enabled;
    target->window = src->window;
  }
};

///
/// Class representing window information.
///
class CefWindowInfo : public CefStructBase<CefWindowInfoTraits> {
 public:
  typedef CefStructBase<CefWindowInfoTraits> parent;

  CefWindowInfo() : parent() {}
  explicit CefWindowInfo(const cef_window_info_t& r) : parent(r) {}
  explicit CefWindowInfo(const CefWindowInfo& r) : parent(r) {}

  CefWindowInfo& operator=(const CefWindowInfo&) = default;
  CefWindowInfo& operator=(CefWindowInfo&&) = default;

  ///
  /// Create the browser as a child window.
  ///
  void SetAsChild(CefWindowHandle parent, const CefRect& windowBounds) {
    style =
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP | WS_VISIBLE;
    parent_window = parent;
    bounds = windowBounds;
  }

  ///
  /// Create the browser as a popup window.
  ///
  void SetAsPopup(CefWindowHandle parent, const CefString& windowName) {
    style =
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE;
    parent_window = parent;
    bounds.x = CW_USEDEFAULT;
    bounds.y = CW_USEDEFAULT;
    bounds.width = CW_USEDEFAULT;
    bounds.height = CW_USEDEFAULT;

    cef_string_copy(windowName.c_str(), windowName.length(), &window_name);
  }

  ///
  /// Create the browser using windowless (off-screen) rendering. No window
  /// will be created for the browser and all rendering will occur via the
  /// CefRenderHandler interface. The |parent| value will be used to identify
  /// monitor info and to act as the parent window for dialogs, context menus,
  /// etc. If |parent| is not provided then the main screen monitor will be used
  /// and some functionality that requires a parent window may not function
  /// correctly. In order to create windowless browsers the
  /// CefSettings.windowless_rendering_enabled value must be set to true.
  /// Transparent painting is enabled by default but can be disabled by setting
  /// CefBrowserSettings.background_color to an opaque value.
  ///
  void SetAsWindowless(CefWindowHandle parent) {
    windowless_rendering_enabled = TRUE;
    parent_window = parent;
  }
};

#if defined(ARCH_CPU_32_BITS)
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
int CefRunWinMainWithPreferredStackSize(wWinMainPtr wWinMain,
                                        HINSTANCE hInstance,
                                        LPWSTR lpCmdLine,
                                        int nCmdShow);
int CefRunMainWithPreferredStackSize(mainPtr main, int argc, char* argv[]);
#endif  // defined(ARCH_CPU_32_BITS)

///
/// Call during process startup to enable High-DPI support on Windows 7 or
/// newer. Older versions of Windows should be left DPI-unaware because they do
/// not support DirectWrite and GDI fonts are kerned very badly.
///
void CefEnableHighDPISupport();

///
/// Set to true before calling Windows APIs like TrackPopupMenu that enter a
/// modal message loop. Set to false after exiting the modal message loop.
///
void CefSetOSModalLoop(bool osModalLoop);

#endif  // CEF_INCLUDE_INTERNAL_CEF_WIN_H_
