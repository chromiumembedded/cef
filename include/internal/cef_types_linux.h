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

#if !defined(GENERATING_CEF_API_HASH)
#include "include/base/cef_build.h"
#endif

#if defined(OS_LINUX)

#include "include/internal/cef_export.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types_color.h"
#include "include/internal/cef_types_geometry.h"
#include "include/internal/cef_types_osr.h"
#include "include/internal/cef_types_runtime.h"

#define kNullCursorHandle 0
#define kNullEventHandle NULL
#define kNullWindowHandle 0

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CEF_X11)
typedef union _XEvent XEvent;
typedef struct _XDisplay XDisplay;

// Handle types.
typedef unsigned long cef_cursor_handle_t;
typedef XEvent* cef_event_handle_t;
#else
typedef void* cef_cursor_handle_t;
typedef void* cef_event_handle_t;
#endif

typedef unsigned long cef_window_handle_t;

///
/// Return the singleton X11 display shared with Chromium. The display is not
/// thread-safe and must only be accessed on the browser process UI thread.
///
#if defined(CEF_X11)
CEF_EXPORT XDisplay* cef_get_xdisplay(void);
#endif

///
/// Structure representing CefExecuteProcess arguments.
///
typedef struct _cef_main_args_t {
  int argc;
  char** argv;
} cef_main_args_t;

///
/// Class representing window information.
///
typedef struct _cef_window_info_t {
  ///
  /// Size of this structure.
  ///
  size_t size;

  ///
  /// The initial title of the window, to be set when the window is created.
  /// Some layout managers (e.g., Compiz) can look at the window title
  /// in order to decide where to place the window when it is
  /// created. When this attribute is not empty, the window title will
  /// be set before the window is mapped to the dispay. Otherwise the
  /// title will be initially empty.
  ///
  cef_string_t window_name;

  ///
  /// Initial window bounds.
  ///
  cef_rect_t bounds;

  ///
  /// Pointer for the parent window.
  ///
  cef_window_handle_t parent_window;

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
  /// Pointer for the new browser window. Only used with windowed rendering.
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
/// Structure containing the plane information of the shared texture.
/// Sync with native_pixmap_handle.h
///
typedef struct _cef_accelerated_paint_native_pixmap_plane_info_t {
  ///
  /// The strides and offsets in bytes to be used when accessing the buffers via
  /// a memory mapping. One per plane per entry. Size in bytes of the plane is
  /// necessary to map the buffers.
  ///
  uint32_t stride;
  uint64_t offset;
  uint64_t size;

  ///
  /// File descriptor for the underlying memory object (usually dmabuf).
  ///
  int fd;
} cef_accelerated_paint_native_pixmap_plane_t;

#define kAcceleratedPaintMaxPlanes 4

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
  /// Planes of the shared texture, usually file descriptors of dmabufs.
  ///
  cef_accelerated_paint_native_pixmap_plane_t
      planes[kAcceleratedPaintMaxPlanes];

  ///
  /// Plane count.
  ///
  int plane_count;

  ///
  /// Modifier could be used with EGL driver.
  ///
  uint64_t modifier;

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

#endif  // OS_LINUX

#endif  // CEF_INCLUDE_INTERNAL_CEF_TYPES_LINUX_H_
