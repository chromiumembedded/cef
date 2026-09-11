// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_SHARED_BROWSER_OSR_RENDERER_METAL_H_
#define CEF_TESTS_SHARED_BROWSER_OSR_RENDERER_METAL_H_
#pragma once

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <memory>

#include "include/cef_render_handler.h"

namespace client {

// All methods must be called on the owning thread. Paint callbacks copy their
// input into owned storage; Render can also be called without a new paint.
class OsrRendererMetal {
 public:
  explicit OsrRendererMetal(cef_color_t background_color,
                            bool show_update_rect = false);
  ~OsrRendererMetal();

  OsrRendererMetal(const OsrRendererMetal&) = delete;
  OsrRendererMetal& operator=(const OsrRendererMetal&) = delete;

  bool Initialize();
  // Wait for submitted work and release GPU resources. Safe to call repeatedly.
  void Cleanup();
  id<MTLDevice> device() const;

  bool OnPaint(CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirty_rects,
               const void* buffer,
               int width,
               int height);
  bool OnAcceleratedPaint(CefRenderHandler::PaintElementType type,
                          const CefRenderHandler::RectList& dirty_rects,
                          const CefAcceleratedPaintInfo& info);

  void OnPopupShow(bool show);
  // The rectangle is in browser image pixels, not Cocoa points.
  void OnPopupSize(const CefRect& rect);
  CefRect popup_rect() const;
  CefRect original_popup_rect() const;

  void SetSpin(float spin_x, float spin_y);
  void IncrementSpin(float spin_dx, float spin_dy);

  // Render to a BGRA8Unorm target, optionally presenting its drawable. The
  // returned command buffer has been committed. Offscreen targets can be used
  // to validate the same composition path without a window.
  id<MTLCommandBuffer> Render(id<MTLTexture> target,
                              id<CAMetalDrawable> drawable = nil);

 private:
  class Impl;
  const cef_color_t background_color_;
  const bool show_update_rect_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace client

#endif  // CEF_TESTS_SHARED_BROWSER_OSR_RENDERER_METAL_H_
