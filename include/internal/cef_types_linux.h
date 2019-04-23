// Copyright (c) 2014 Marshall A. Greenblatt. All rights reserved.
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

#ifndef CEF_INCLUDE_INTERNAL_CEF_TYPES_LINUX_H_
#define CEF_INCLUDE_INTERNAL_CEF_TYPES_LINUX_H_
#pragma once

#include "include/base/cef_build.h"
#include "include/cef_config.h"

#if defined(OS_LINUX)

#if defined(CEF_X11)
typedef union _XEvent XEvent;
typedef struct _XDisplay XDisplay;
#endif

#include "include/internal/cef_export.h"
#include "include/internal/cef_string.h"

// Handle types.
#if defined(CEF_X11)
#define cef_cursor_handle_t unsigned long
#define cef_event_handle_t XEvent*
#else
#define cef_cursor_handle_t void*
#define cef_event_handle_t void*
#endif

#define cef_window_handle_t unsigned long

#define kNullCursorHandle 0
#define kNullEventHandle NULL
#define kNullWindowHandle 0

#ifdef __cplusplus
extern "C" {
#endif

///
// Return the singleton X11 display shared with Chromium. The display is not
// thread-safe and must only be accessed on the browser process UI thread.
///
#if defined(CEF_X11)
CEF_EXPORT XDisplay* cef_get_xdisplay();
#endif

///
// Structure representing CefExecuteProcess arguments.
///
typedef struct _cef_main_args_t {
  int argc;
  char** argv;
} cef_main_args_t;

///
// Class representing window information.
///
typedef struct _cef_window_info_t {
  ///
  // The initial title of the window, to be set when the window is created.
  // Some layout managers (e.g., Compiz) can look at the window title
  // in order to decide where to place the window when it is
  // created. When this attribute is not empty, the window title will
  // be set before the window is mapped to the dispay. Otherwise the
  // title will be initially empty.
  ///
  cef_string_t window_name;

  unsigned int x;
  unsigned int y;
  unsigned int width;
  unsigned int height;

  ///
  // Pointer for the parent window.
  ///
  cef_window_handle_t parent_window;

  ///
  // Set to true (1) to create the browser using windowless (off-screen)
  // rendering. No window will be created for the browser and all rendering will
  // occur via the CefRenderHandler interface. The |parent_window| value will be
  // used to identify monitor info and to act as the parent window for dialogs,
  // context menus, etc. If |parent_window| is not provided then the main screen
  // monitor will be used and some functionality that requires a parent window
  // may not function correctly. In order to create windowless browsers the
  // CefSettings.windowless_rendering_enabled value must be set to true.
  // Transparent painting is enabled by default but can be disabled by setting
  // CefBrowserSettings.background_color to an opaque value.
  ///
  int windowless_rendering_enabled;

  ///
  // Set to true (1) to enable shared textures for windowless rendering. Only
  // valid if windowless_rendering_enabled above is also set to true. Currently
  // only supported on Windows (D3D11).
  ///
  int shared_texture_enabled;

  ///
  // Set to true (1) to enable the ability to issue BeginFrame requests from the
  // client application by calling CefBrowserHost::SendExternalBeginFrame.
  ///
  int external_begin_frame_enabled;

  ///
  // Pointer for the new browser window. Only used with windowed rendering.
  ///
  cef_window_handle_t window;
} cef_window_info_t;

#ifdef __cplusplus
}
#endif

#endif  // OS_LINUX

#endif  // CEF_INCLUDE_INTERNAL_CEF_TYPES_LINUX_H_
