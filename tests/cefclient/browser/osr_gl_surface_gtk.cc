// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/osr_gl_surface_gtk.h"

#include <X11/Xlib.h>
#include <gdk/gdkx.h>

#include <utility>
#include <vector>

#include "include/base/cef_logging.h"
#include "tests/shared/browser/osr_gl_linux.h"

namespace client {

namespace {

// Uses GtkGLArea, which renders into a framebuffer that GDK composites.
// Requires GDK to use EGL so that the context can import DMA-BUFs.
class OsrGlAreaSurface : public OsrGlSurfaceGtk {
 public:
  explicit OsrGlAreaSurface(RenderCallback render)
      : widget_(gtk_gl_area_new()), render_(std::move(render)) {
    gtk_gl_area_set_required_version(GTK_GL_AREA(widget_), 3, 3);
    gtk_gl_area_set_auto_render(GTK_GL_AREA(widget_), FALSE);
    g_signal_connect(widget_, "render", G_CALLBACK(&OnRender), this);
  }

  ~OsrGlAreaSurface() override {
    g_signal_handlers_disconnect_by_data(widget_, this);
  }

  GtkWidget* widget() const override { return widget_; }

  bool MakeCurrent() override {
    if (!gl::GetApi() || !gtk_widget_get_realized(widget_)) {
      return false;
    }
    gtk_gl_area_make_current(GTK_GL_AREA(widget_));
    if (GError* error = gtk_gl_area_get_error(GTK_GL_AREA(widget_))) {
      if (!error_logged_) {
        LOG(ERROR) << "OSR: GtkGLArea error: " << error->message;
        error_logged_ = true;
      }
      return false;
    }
    return true;
  }

  void ReleaseCurrent() override { gdk_gl_context_clear_current(); }

  void Invalidate() override { gtk_gl_area_queue_render(GTK_GL_AREA(widget_)); }

  void Destroy() override {}

 private:
  // Called by GTK with the context current and the area's framebuffer bound
  // whenever the area needs to be drawn, including after resizes.
  static gboolean OnRender(GtkGLArea* area,
                           GdkGLContext* context,
                           OsrGlAreaSurface* self) {
    const gl::Api* api = gl::GetApi();
    if (!api) {
      return FALSE;
    }
    gl::GLint binding = 0;
    api->glGetIntegerv(gl::kGlFramebufferBinding, &binding);
    GtkWidget* widget = GTK_WIDGET(area);
    const int scale = gtk_widget_get_scale_factor(widget);
    self->render_(static_cast<unsigned int>(binding),
                  gtk_widget_get_allocated_width(widget) * scale,
                  gtk_widget_get_allocated_height(widget) * scale);
    return TRUE;
  }

  GtkWidget* const widget_;
  const RenderCallback render_;
  bool error_logged_ = false;
};

// Renders to an EGL window surface on the widget's native X11 window. Used
// because GDK's OpenGL support on X11 is GLX-based.
class OsrEglX11Surface : public OsrGlSurfaceGtk {
 public:
  explicit OsrEglX11Surface(RenderCallback render)
      : widget_(gtk_drawing_area_new()), render_(std::move(render)) {
    // GTK must not paint over the native window that EGL presents to.
    gtk_widget_set_double_buffered(widget_, FALSE);
    gtk_widget_set_app_paintable(widget_, TRUE);
    g_signal_connect(widget_, "realize", G_CALLBACK(&OnRealize), nullptr);
    g_signal_connect(widget_, "draw", G_CALLBACK(&OnDraw), this);
  }

  ~OsrEglX11Surface() override {
    g_signal_handlers_disconnect_by_data(widget_, this);
    ReleaseResources();
  }

  GtkWidget* widget() const override { return widget_; }

  bool MakeCurrent() override {
    if (destroyed_ || (!surface_ && !Initialize())) {
      return false;
    }
    const gl::Api* api = gl::GetApi();
    // The bound API is per thread.
    api->eglBindAPI(gl::kEglOpenGLApi);
    return api->eglMakeCurrent(display_, surface_, surface_, context_);
  }

  void ReleaseCurrent() override {
    if (display_) {
      gl::GetApi()->eglMakeCurrent(display_, gl::kNoSurface, gl::kNoSurface,
                                   gl::kNoContext);
    }
  }

  void Invalidate() override { gtk_widget_queue_draw(widget_); }

  void Destroy() override {
    // The widget may still be drawn after the browser closes. Do not recreate
    // the context.
    destroyed_ = true;
    ReleaseResources();
  }

 private:
  void ReleaseResources() {
    const gl::Api* api = gl::GetApi();
    if (!api || !display_) {
      return;
    }
    api->eglMakeCurrent(display_, gl::kNoSurface, gl::kNoSurface,
                        gl::kNoContext);
    if (surface_) {
      api->eglDestroySurface(display_, surface_);
      surface_ = gl::kNoSurface;
    }
    if (context_) {
      api->eglDestroyContext(display_, context_);
      context_ = gl::kNoContext;
    }
  }

  static void OnRealize(GtkWidget* widget, gpointer) {
    gdk_window_ensure_native(gtk_widget_get_window(widget));
  }

  // Called by GTK whenever the widget needs to be drawn, including after
  // resizes and exposes. The native window's contents are not retained.
  static gboolean OnDraw(GtkWidget* widget,
                         cairo_t* cr,
                         OsrEglX11Surface* self) {
    if (!self->MakeCurrent()) {
      return TRUE;
    }
    GdkWindow* window = gtk_widget_get_window(widget);
    const int scale = gdk_window_get_scale_factor(window);
    self->render_(0, gdk_window_get_width(window) * scale,
                  gdk_window_get_height(window) * scale);
    gl::GetApi()->eglSwapBuffers(self->display_, self->surface_);
    self->ReleaseCurrent();
    return TRUE;
  }

  bool Initialize() {
    if (failed_) {
      return false;
    }
    // The native window exists once the widget is realized.
    GdkWindow* window = gtk_widget_get_window(widget_);
    if (!window) {
      return false;
    }
    // Only attempt initialization once.
    failed_ = true;
    const gl::Api* api = gl::GetApi();
    if (!api || !InitializeResources(api, window)) {
      ReleaseResources();
      return false;
    }
    failed_ = false;
    return true;
  }

  bool InitializeResources(const gl::Api* api, GdkWindow* window) {
    Display* xdisplay = GDK_WINDOW_XDISPLAY(window);
    display_ =
        api->eglGetPlatformDisplay(gl::kEglPlatformX11, xdisplay, nullptr);
    if (!display_ || !api->eglInitialize(display_, nullptr, nullptr)) {
      LOG(ERROR) << "OSR: Failed to initialize EGL for X11";
      display_ = gl::kNoDisplay;
      return false;
    }

    const gl::EGLint attributes[] = {gl::kEglSurfaceType,
                                     gl::kEglWindowBit,
                                     gl::kEglRenderableType,
                                     gl::kEglOpenGLBit,
                                     gl::kEglRedSize,
                                     8,
                                     gl::kEglGreenSize,
                                     8,
                                     gl::kEglBlueSize,
                                     8,
                                     gl::kEglNone};
    gl::EGLint count = 0;
    api->eglChooseConfig(display_, attributes, nullptr, 0, &count);
    std::vector<gl::EGLConfig> configs(count);
    if (count <= 0 || !api->eglChooseConfig(display_, attributes,
                                            configs.data(), count, &count)) {
      LOG(ERROR) << "OSR: No suitable EGL config";
      return false;
    }
    // The config must match the visual of the window that it presents to.
    const VisualID visual_id = XVisualIDFromVisual(
        gdk_x11_visual_get_xvisual(gdk_window_get_visual(window)));
    gl::EGLConfig config = configs[0];
    for (auto candidate : configs) {
      gl::EGLint native_visual = 0;
      if (api->eglGetConfigAttrib(display_, candidate, gl::kEglNativeVisualId,
                                  &native_visual) &&
          static_cast<VisualID>(native_visual) == visual_id) {
        config = candidate;
        break;
      }
    }

    context_ = gl::CreateContext(display_, config);
    if (!context_) {
      return false;
    }
    surface_ = api->eglCreateWindowSurface(display_, config,
                                           GDK_WINDOW_XID(window), nullptr);
    if (!surface_) {
      LOG(ERROR) << "OSR: eglCreateWindowSurface failed: 0x" << std::hex
                 << api->eglGetError();
      return false;
    }
    if (!api->eglMakeCurrent(display_, surface_, surface_, context_)) {
      return false;
    }
    // Never block the main thread on vsync.
    api->eglSwapInterval(display_, 0);
    if (!gl::SupportsDmaBufImport(display_)) {
      LOG(WARNING) << "OSR: EGL display does not support DMA-BUF import";
    }
    return true;
  }

  GtkWidget* const widget_;
  const RenderCallback render_;
  gl::EGLDisplay display_ = gl::kNoDisplay;
  gl::EGLContext context_ = gl::kNoContext;
  gl::EGLSurface surface_ = gl::kNoSurface;
  bool failed_ = false;
  bool destroyed_ = false;
};

}  // namespace

// static
std::unique_ptr<OsrGlSurfaceGtk> OsrGlSurfaceGtk::Create(
    RenderCallback render) {
  if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    return std::make_unique<OsrEglX11Surface>(std::move(render));
  }
  return std::make_unique<OsrGlAreaSurface>(std::move(render));
}

}  // namespace client
