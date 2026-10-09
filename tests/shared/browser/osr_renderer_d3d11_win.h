// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_SHARED_BROWSER_OSR_RENDERER_D3D11_WIN_H_
#define CEF_TESTS_SHARED_BROWSER_OSR_RENDERER_D3D11_WIN_H_
#pragma once

#include <d3d11_1.h>

#include <memory>

#include "include/cef_render_handler.h"

namespace client {

// All methods must be called on the owning thread. Paint callbacks copy their
// input into owned storage; Render can also be called without a new paint.
class OsrRendererD3D11 {
 public:
  explicit OsrRendererD3D11(cef_color_t background_color,
                            bool show_update_rect = false);
  ~OsrRendererD3D11();

  OsrRendererD3D11(const OsrRendererD3D11&) = delete;
  OsrRendererD3D11& operator=(const OsrRendererD3D11&) = delete;

  // Uses a hardware device by default. An existing device can be supplied for
  // offscreen tests, including WARP. Safe to call repeatedly.
  bool Initialize(ID3D11Device* device = nullptr);
  // Wait for submitted work and release resources. Safe to call repeatedly.
  void Cleanup();
  ID3D11Device* device() const;
  CefSize view_size() const;

  bool OnPaint(CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirty_rects,
               const void* buffer,
               int width,
               int height);
  // Reopens the NT shared handle and completes a copy of the visible image
  // before returning. Neither the handle nor its resource is retained.
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

  // Render to a BGRA8/RGBA8 UNORM target. Presentation belongs to the caller.
  // Offscreen targets exercise the same composition path without a window.
  bool Render(ID3D11Texture2D* target);

 private:
  class Impl;
  const cef_color_t background_color_;
  const bool show_update_rect_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace client

#endif  // CEF_TESTS_SHARED_BROWSER_OSR_RENDERER_D3D11_WIN_H_
