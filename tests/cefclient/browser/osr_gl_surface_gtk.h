// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_GL_SURFACE_GTK_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_GL_SURFACE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <memory>

namespace client {

// A GTK widget with an EGL-based OpenGL context for off-screen rendering
// output. Uses GtkGLArea when GDK's OpenGL support is EGL-based (Wayland), and
// an EGL window surface on X11 where GDK uses GLX, which cannot import
// DMA-BUFs.
//
// The toolkit drives presentation: |render| is called on the main thread with
// the context current whenever the widget needs to be drawn (new frames,
// resize, expose), and should draw the most recent frame scaled to the given
// framebuffer size. Other methods must be called with the GDK lock held. The
// context is only current between MakeCurrent() and ReleaseCurrent(), so it
// may be used from the browser UI thread and the main thread. This relies on
// GTK dispatching draw/render signals with the GDK lock held, as it does when
// the GDK lock functions are installed for multi-threaded message loop mode.
class OsrGlSurfaceGtk {
 public:
  using RenderCallback =
      std::function<void(unsigned int framebuffer, int width, int height)>;

  // Creates a surface for the default GDK display. Must be called on the main
  // thread.
  static std::unique_ptr<OsrGlSurfaceGtk> Create(RenderCallback render);

  virtual ~OsrGlSurfaceGtk() = default;

  // The widget that displays the rendered output.
  virtual GtkWidget* widget() const = 0;

  // Makes the context current for creating, updating or releasing GL
  // resources. Returns false if the context is unavailable.
  virtual bool MakeCurrent() = 0;
  virtual void ReleaseCurrent() = 0;

  // Requests that the widget be drawn with the most recent frame.
  virtual void Invalidate() = 0;

  // Releases the context and surface. The widget remains valid.
  virtual void Destroy() = 0;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_GL_SURFACE_GTK_H_
