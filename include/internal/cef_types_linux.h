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

#include "include/cef_api_hash.h"
#include "include/internal/cef_export.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types_color.h"
#include "include/internal/cef_types_geometry.h"
#include "include/internal/cef_types_osr.h"
#include "include/internal/cef_types_runtime.h"

#define kNullCursorHandle 0
#define kNullEventHandle NULL
#define kNullWindowHandle 0

#if CEF_API_ADDED(CEF_EXPERIMENTAL)
#define kNullWaylandDisplayHandle NULL
#define kNullXdgSurfaceHandle NULL
#endif

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

#if CEF_API_ADDED(CEF_EXPERIMENTAL)

///
/// Handle type for a Wayland connection (struct wl_display*).
///
typedef void* cef_wayland_display_handle_t;

///
/// Handle type for an xdg_surface (struct xdg_surface*).
///
typedef void* cef_xdg_surface_handle_t;
#endif

///
/// Return the singleton X11 display shared with Chromium. The display is not
/// thread-safe and must only be accessed on the browser process UI thread.
///
#if defined(CEF_X11)
CEF_EXPORT XDisplay* cef_get_xdisplay(void);
#endif

#if CEF_API_ADDED(CEF_EXPERIMENTAL)
///
/// Give CEF the Wayland connection (struct wl_display*) owned by the client
/// application, so that browser surfaces can be created as subsurfaces of the
/// client's surfaces. A wl_surface is a protocol object scoped to one
/// connection and wl_subcompositor_get_subsurface() requires both surfaces to
/// belong to the same one, so embedding is only possible if CEF joins the
/// client's connection instead of opening its own.
///
/// Must be called before CefInitialize. Chromium creates its Wayland connection
/// while initializing Ozone, which happens during CefInitialize and long before
/// any browser exists, so this is necessarily a process-global, one-time
/// decision rather than a per-browser one.
///
/// The client retains ownership and must keep the connection alive until after
/// CefShutdown. CEF dispatches its own protocol traffic on a dedicated
/// wl_event_queue and never calls wl_display_disconnect() on an adopted
/// connection, so the client may keep running its own event loop on the
/// default queue.
///
/// Has no effect if the active Ozone platform is not Wayland.
///
CEF_EXPORT void cef_set_wayland_display(cef_wayland_display_handle_t display);

///
/// Return the Wayland connection used by Chromium, or NULL if the active Ozone
/// platform is not Wayland. If the client provided a connection via
/// cef_set_wayland_display() then that same connection is returned. Must only
/// be accessed on the browser process UI thread.
///
CEF_EXPORT cef_wayland_display_handle_t cef_get_wayland_display(void);
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
  /// When the active Ozone platform is Wayland this is a struct wl_surface*
  /// belonging to the client application, cast to cef_window_handle_t; the
  /// browser surface becomes a wl_subsurface of it. Under X11 it is an X11
  /// Window, as before. The field has always been an opaque native parent
  /// handle whose meaning depends on the platform -- it is an NSView* on
  /// macOS and an HWND on Windows -- and Wayland is no different; the
  /// unsigned long spelling here is an artifact of X11.
  ///
  /// A Wayland surface must belong to the connection previously passed to
  /// cef_set_wayland_display(). Note that CEF cannot tell a mistaken X11
  /// Window from a valid pointer, so passing the wrong kind of handle for the
  /// active platform is undefined rather than diagnosed.
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
  /// Under Ozone/X11 this is the X11 Window of the browser, usable from any
  /// client on the connection. Under Ozone/Wayland it is not a protocol object
  /// at all: it is the gfx::AcceleratedWidget Ozone uses internally, an id
  /// meaningful only inside this process. A wl_surface cannot be handed back
  /// this way because it is scoped to the connection that created it, so an
  /// embedder that needs to address the browser's surface should keep the
  /// parent surface it passed in and position the browser through
  /// CefBrowserHost::SetWindowBounds() instead.
  ///
  cef_window_handle_t window;

  ///
  /// Optionally change the runtime style. Alloy style will always be used if
  /// |windowless_rendering_enabled| is true. See cef_runtime_style_t
  /// documentation for details.
  ///
  cef_runtime_style_t runtime_style;

#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  ///
  /// The client application's xdg_surface (struct xdg_surface*), the one
  /// |parent_window| belongs to when it names a wl_surface.
  ///
  /// Required for the browser to open menus, <select> dropdowns, tooltips and
  /// autocomplete. A wl_subsurface has no xdg_surface role, and
  /// xdg_surface.get_popup demands an xdg_surface, so popups from an embedded
  /// browser have to be anchored on the client's instead. Their positions are
  /// then computed in the client's surface coordinates.
  ///
  /// May be NULL for hosts whose toolkit does not expose the xdg_surface. The
  /// browser then renders and takes input as usual, and popups fall back to a
  /// wl_subsurface of the browser. That fallback is degraded and deliberately
  /// so: a subsurface cannot extend past the host window, so a dropdown near an
  /// edge is clipped rather than repositioned, and it holds no grab, so
  /// clicking outside does not dismiss it. Provide the xdg_surface wherever the
  /// toolkit allows it.
  ///
  /// Ignored unless the active Ozone platform is Wayland.
  ///
  cef_xdg_surface_handle_t parent_xdg_surface;
#endif
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
