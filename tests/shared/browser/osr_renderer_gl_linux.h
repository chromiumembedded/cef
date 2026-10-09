// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_SHARED_BROWSER_OSR_RENDERER_GL_LINUX_H_
#define CEF_TESTS_SHARED_BROWSER_OSR_RENDERER_GL_LINUX_H_
#pragma once

#include <memory>

#include "include/cef_render_handler.h"

namespace client {

// OpenGL renderer for off-screen rendering on Linux. Requires a current desktop
// OpenGL 3.3 core profile context created via EGL for all methods that access
// GL state. Accelerated paints are imported via the current EGL display.
class OsrRendererGl {
 public:
  explicit OsrRendererGl(cef_color_t background_color,
                         bool show_update_rect = false);
  ~OsrRendererGl();

  OsrRendererGl(const OsrRendererGl&) = delete;
  OsrRendererGl& operator=(const OsrRendererGl&) = delete;

  // Create GL resources. Safe to call repeatedly.
  bool Initialize();
  // Release GL resources. Must be called with the same
  // context current before destruction. Safe to call repeatedly.
  void Cleanup();

  bool OnPaint(CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirty_rects,
               const void* buffer,
               int width,
               int height);
  // Imports the DMA-BUF and copies its contents before returning. No
  // reference to the buffer or its file descriptors is retained.
  bool OnAcceleratedPaint(CefRenderHandler::PaintElementType type,
                          const CefRenderHandler::RectList& dirty_rects,
                          const CefAcceleratedPaintInfo& info);

  void OnPopupShow(bool show);
  // The rectangle is in browser image pixels.
  void OnPopupSize(const CefRect& rect);
  CefRect popup_rect() const;
  CefRect original_popup_rect() const;

  void SetSpin(float spin_x, float spin_y);
  void IncrementSpin(float spin_dx, float spin_dy);

  // Render to |framebuffer| (0 for the default framebuffer) with a size of
  // |width| x |height| pixels. The browser image is scaled to fit.
  bool Render(unsigned int framebuffer, int width, int height);

 private:
  class Impl;
  const cef_color_t background_color_;
  const bool show_update_rect_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace client

#endif  // CEF_TESTS_SHARED_BROWSER_OSR_RENDERER_GL_LINUX_H_
