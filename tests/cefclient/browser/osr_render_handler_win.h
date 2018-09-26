// Copyright 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDER_HANDLER_WIN_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDER_HANDLER_WIN_H_
#pragma once

#include "include/base/cef_weak_ptr.h"
#include "include/cef_render_handler.h"
#include "tests/cefclient/browser/osr_renderer_settings.h"

namespace client {

// Abstract base class for implementing OSR rendering with different backends on
// Windows. Methods are only called on the UI thread.
class OsrRenderHandlerWin {
 public:
  OsrRenderHandlerWin(const OsrRendererSettings& settings, HWND hwnd);
  virtual ~OsrRenderHandlerWin();

  void SetBrowser(CefRefPtr<CefBrowser> browser);

  // Rotate the texture based on mouse events.
  virtual void SetSpin(float spinX, float spinY) = 0;
  virtual void IncrementSpin(float spinDX, float spinDY) = 0;

  // Popup hit testing.
  virtual bool IsOverPopupWidget(int x, int y) const = 0;
  virtual int GetPopupXOffset() const = 0;
  virtual int GetPopupYOffset() const = 0;

  // CefRenderHandler callbacks.
  virtual void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) = 0;
  // |rect| must be in pixel coordinates.
  virtual void OnPopupSize(CefRefPtr<CefBrowser> browser,
                           const CefRect& rect) = 0;

  // Used when not rendering with shared textures.
  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       CefRenderHandler::PaintElementType type,
                       const CefRenderHandler::RectList& dirtyRects,
                       const void* buffer,
                       int width,
                       int height) = 0;

  // Used when rendering with shared textures.
  virtual void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                                  CefRenderHandler::PaintElementType type,
                                  const CefRenderHandler::RectList& dirtyRects,
                                  void* share_handle) = 0;

  bool send_begin_frame() const {
    return settings_.external_begin_frame_enabled;
  }
  HWND hwnd() const { return hwnd_; }

 protected:
  // Called to trigger the BeginFrame timer.
  void Invalidate();

  // Called by the BeginFrame timer.
  virtual void Render() = 0;

 private:
  void TriggerBeginFrame(uint64_t last_time_us, float delay_us);

  // The below members are only accessed on the UI thread.
  const OsrRendererSettings settings_;
  const HWND hwnd_;
  bool begin_frame_pending_;
  CefRefPtr<CefBrowser> browser_;

  // Must be the last member.
  base::WeakPtrFactory<OsrRenderHandlerWin> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OsrRenderHandlerWin);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDER_HANDLER_WIN_H_
