// Copyright (c) 2010 Marshall A. Greenblatt. All rights reserved.
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

#ifndef CEF_INCLUDE_INTERNAL_CEF_LINUX_H_
#define CEF_INCLUDE_INTERNAL_CEF_LINUX_H_
#pragma once

#include "include/internal/cef_types_linux.h"
#include "include/internal/cef_types_wrappers.h"

// Handle types.
#define CefCursorHandle cef_cursor_handle_t
#define CefEventHandle cef_event_handle_t
#define CefWindowHandle cef_window_handle_t
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
#define CefWaylandDisplayHandle cef_wayland_display_handle_t
#define CefXdgSurfaceHandle cef_xdg_surface_handle_t
#endif

///
/// Class representing CefExecuteProcess arguments.
///
class CefMainArgs : public cef_main_args_t {
 public:
  CefMainArgs() : cef_main_args_t{} {}
  CefMainArgs(const cef_main_args_t& r) : cef_main_args_t(r) {}
  CefMainArgs(int argc_arg, char** argv_arg)
      : cef_main_args_t{argc_arg, argv_arg} {}
};

struct CefWindowInfoTraits {
  typedef cef_window_info_t struct_type;

  static inline void init(struct_type* s) { s->size = sizeof(struct_type); }

  static inline void clear(struct_type* s) {
    cef_string_clear(&s->window_name);
  }

  static inline void set(const struct_type* src,
                         struct_type* target,
                         bool copy) {
    cef_string_set(src->window_name.str, src->window_name.length,
                   &target->window_name, copy);
    target->bounds = src->bounds;
    target->parent_window = src->parent_window;
    target->windowless_rendering_enabled = src->windowless_rendering_enabled;
    target->shared_texture_enabled = src->shared_texture_enabled;
    target->external_begin_frame_enabled = src->external_begin_frame_enabled;
    target->window = src->window;
    target->runtime_style = src->runtime_style;
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
    // |src| comes from the client and may have been compiled against an older,
    // smaller struct. Reading the member without this check is a read past the
    // end of that allocation.
    //
    // The else matters as much as the check. CefStructBase::Set() calls
    // Clear() first to "clear newer members that won't be set", but clear()
    // only frees string members; a stale pointer here would otherwise survive
    // being assigned over from an older client's struct.
    if (CEF_MEMBER_EXISTS(src, parent_xdg_surface)) {
      target->parent_xdg_surface = src->parent_xdg_surface;
    } else {
      target->parent_xdg_surface = nullptr;
    }
#endif
  }
};

///
/// Class representing window information.
///
class CefWindowInfo : public CefStructBase<CefWindowInfoTraits> {
 public:
  using base_type = CefStructBase<CefWindowInfoTraits>;
  using base_type::CefStructBase;
  using base_type::operator=;

  ///
  /// Create the browser as a child window.
  ///
  void SetAsChild(CefWindowHandle parent, const CefRect& bounds) {
    parent_window = parent;
    this->bounds = bounds;
  }

#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  ///
  /// Create the browser as a child window, additionally naming the client's
  /// xdg_surface when the active Ozone platform is Wayland.
  ///
  /// |parent| is the same opaque native parent handle the two-argument
  /// overload takes: an X11 Window under Ozone/X11, a struct wl_surface* cast
  /// to CefWindowHandle under Ozone/Wayland. A Wayland surface must belong to
  /// the connection previously passed to cef_set_wayland_display(), which must
  /// be called before CefInitialize, because wl_surface objects cannot be
  /// shared across connections. See
  /// https://github.com/chromiumembedded/cef/issues/2804.
  ///
  /// |parent_xdg| is the xdg_surface that |parent| belongs to. It is what the
  /// browser's menus, <select> dropdowns and tooltips are anchored on, since a
  /// wl_subsurface cannot be an xdg_popup parent. Passing NULL still leaves the
  /// browser able to render and take input, with popups falling back to a
  /// degraded wl_subsurface form; see cef_window_info_t::parent_xdg_surface.
  /// It is ignored under X11, where the two-argument overload is all that is
  /// needed.
  ///
  void SetAsChild(CefWindowHandle parent,
                  CefXdgSurfaceHandle parent_xdg,
                  const CefRect& bounds) {
    parent_window = parent;
    parent_xdg_surface = parent_xdg;
    this->bounds = bounds;
  }
#endif

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
    windowless_rendering_enabled = true;
    parent_window = parent;
    runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  }
};

#endif  // CEF_INCLUDE_INTERNAL_CEF_LINUX_H_
