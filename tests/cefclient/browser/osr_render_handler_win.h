// Copyright 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDER_HANDLER_WIN_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDER_HANDLER_WIN_H_
#pragma once

#include <dxgi.h>
#include <wrl/client.h>

#include "include/base/cef_weak_ptr.h"
#include "include/cef_render_handler.h"
#include "tests/cefclient/browser/osr_renderer_settings.h"
#include "tests/shared/browser/osr_renderer_d3d11_win.h"

namespace client {

// Owns Windows presentation and begin-frame timing, delegating image storage
// and composition to the shared D3D11 renderer. Both CPU and accelerated paints
// use the same renderer. Methods are only called on the UI thread.
class OsrRenderHandlerWin {
 public:
  OsrRenderHandlerWin(const OsrRendererSettings& settings, HWND hwnd);
  ~OsrRenderHandlerWin();

  OsrRenderHandlerWin(const OsrRenderHandlerWin&) = delete;
  OsrRenderHandlerWin& operator=(const OsrRenderHandlerWin&) = delete;

  bool Initialize(CefRefPtr<CefBrowser> browser, int width, int height);
  void SetBrowser(CefRefPtr<CefBrowser> browser);

  // Rotate the texture based on mouse events.
  void SetSpin(float spinX, float spinY);
  void IncrementSpin(float spinDX, float spinDY);

  // Popup hit testing.
  bool IsOverPopupWidget(int x, int y) const;
  int GetPopupXOffset() const;
  int GetPopupYOffset() const;

  // CefRenderHandler callbacks.
  void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show);
  // |rect| must be in pixel coordinates.
  void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect);

  // Used when not rendering with shared textures.
  void OnPaint(CefRefPtr<CefBrowser> browser,
               CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirtyRects,
               const void* buffer,
               int width,
               int height);

  // Used when rendering with shared textures.
  void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                          CefRenderHandler::PaintElementType type,
                          const CefRenderHandler::RectList& dirtyRects,
                          const CefAcceleratedPaintInfo& info);

  // Redraw the last owned image on native exposure or from the BeginFrame
  // timer. This must not request another CEF paint or access its live
  // framebuffer.
  void Render();

  bool send_begin_frame() const {
    return settings_.external_begin_frame_enabled;
  }
  HWND hwnd() const { return hwnd_; }

 private:
  // Called to trigger the BeginFrame timer.
  void Invalidate();

  void TriggerBeginFrame(uint64_t last_time_us, float delay_us);

  // The below members are only accessed on the UI thread.
  const OsrRendererSettings settings_;
  const HWND hwnd_;
  bool begin_frame_pending_ = false;
  CefRefPtr<CefBrowser> browser_;

  OsrRendererD3D11 renderer_;
  Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain_;
  int width_ = 0;
  int height_ = 0;

  // Must be the last member.
  base::WeakPtrFactory<OsrRenderHandlerWin> weak_factory_;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDER_HANDLER_WIN_H_
