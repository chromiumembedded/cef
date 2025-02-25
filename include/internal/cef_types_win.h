// Copyright (c) 2009 Marshall A. Greenblatt. All rights reserved.
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

#ifndef CEF_INCLUDE_INTERNAL_CEF_TYPES_WIN_H_
#define CEF_INCLUDE_INTERNAL_CEF_TYPES_WIN_H_
#pragma once

#if !defined(GENERATING_CEF_API_HASH)
#include "include/base/cef_build.h"
#endif

#if defined(OS_WIN)

#if !defined(GENERATING_CEF_API_HASH)
#include <windows.h>
#endif

#include "include/internal/cef_string.h"
#include "include/internal/cef_types_color.h"
#include "include/internal/cef_types_geometry.h"
#include "include/internal/cef_types_osr.h"
#include "include/internal/cef_types_runtime.h"

#define kNullCursorHandle NULL
#define kNullEventHandle NULL
#define kNullWindowHandle NULL

#ifdef __cplusplus
extern "C" {
#endif

// Handle types.
typedef HCURSOR cef_cursor_handle_t;
typedef MSG* cef_event_handle_t;
typedef HWND cef_window_handle_t;
typedef HANDLE cef_shared_texture_handle_t;

///
/// Structure representing CefExecuteProcess arguments.
///
typedef struct _cef_main_args_t {
  HINSTANCE instance;
} cef_main_args_t;

///
/// Structure representing window information.
///
typedef struct _cef_window_info_t {
  ///
  /// Size of this structure.
  ///
  size_t size;

  // Standard parameters required by CreateWindowEx()
  DWORD ex_style;
  cef_string_t window_name;
  DWORD style;
  cef_rect_t bounds;
  cef_window_handle_t parent_window;
  HMENU menu;

  ///
  /// Set to true (1) to create the browser using windowless (off-screen)
  /// rendering. No window will be created for the browser and all rendering
  /// will occur via the CefRenderHandler interface. The |parent_window| value
  /// will be used to identify monitor info and to act as the parent window for
  /// dialogs, context menus, etc. If |parent_window| is not provided then the
  /// main screen monitor will be used and some functionality that requires a
  /// parent window may not function correctly. In order to create windowless
  /// browsers the CefSettings.windowless_rendering_enabled value must be set to
  /// true. Transparent painting is enabled by default but can be disabled by
  /// setting CefBrowserSettings.background_color to an opaque value.
  ///
  int windowless_rendering_enabled;

  ///
  /// Set to true (1) to enable shared textures for windowless rendering. Only
  /// valid if windowless_rendering_enabled above is also set to true. Currently
  /// only supported on Windows (D3D11).
  ///
  int shared_texture_enabled;

  ///
  /// Set to true (1) to enable the ability to issue BeginFrame requests from
  /// the client application by calling CefBrowserHost::SendExternalBeginFrame.
  ///
  int external_begin_frame_enabled;

  ///
  /// Handle for the new browser window. Only used with windowed rendering.
  ///
  cef_window_handle_t window;

  ///
  /// Optionally change the runtime style. Alloy style will always be used if
  /// |windowless_rendering_enabled| is true. See cef_runtime_style_t
  /// documentation for details.
  ///
  cef_runtime_style_t runtime_style;
} cef_window_info_t;

///
/// Structure containing shared texture information for the OnAcceleratedPaint
/// callback. Resources will be released to the underlying pool for reuse when
/// the callback returns from client code.
///
typedef struct _cef_accelerated_paint_info_t {
  ///
  /// Size of this structure.
  ///
  size_t size;

  ///
  /// Handle for the shared texture. The shared texture is instantiated
  /// without a keyed mutex.
  ///
  cef_shared_texture_handle_t shared_texture_handle;

  ///
  /// The pixel format of the texture.
  ///
  cef_color_type_t format;

  ///
  /// The extra common info.
  ///
  cef_accelerated_paint_info_common_t extra;
} cef_accelerated_paint_info_t;

#ifdef __cplusplus
}
#endif

#endif  // OS_WIN

#endif  // CEF_INCLUDE_INTERNAL_CEF_TYPES_WIN_H_
