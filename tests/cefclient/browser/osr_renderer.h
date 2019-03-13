// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDERER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDERER_H_
#pragma once

#include "include/cef_browser.h"
#include "include/cef_render_handler.h"
#include "tests/cefclient/browser/osr_renderer_settings.h"

namespace client {

class OsrRenderer {
 public:
  explicit OsrRenderer(const OsrRendererSettings& settings);
  ~OsrRenderer();

  // Initialize the OpenGL environment.
  void Initialize();

  // Clean up the OpenGL environment.
  void Cleanup();

  // Render to the screen.
  void Render();

  // Forwarded from CefRenderHandler callbacks.
  void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show);
  // |rect| must be in pixel coordinates.
  void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect);
  void OnPaint(CefRefPtr<CefBrowser> browser,
               CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirtyRects,
               const void* buffer,
               int width,
               int height);

  // Apply spin.
  void SetSpin(float spinX, float spinY);
  void IncrementSpin(float spinDX, float spinDY);

  int GetViewWidth() const { return view_width_; }
  int GetViewHeight() const { return view_height_; }

  CefRect popup_rect() const { return popup_rect_; }
  CefRect original_popup_rect() const { return original_popup_rect_; }

  void ClearPopupRects();

 private:
  CefRect GetPopupRectInWebView(const CefRect& original_rect);

  inline bool IsTransparent() const {
    return CefColorGetA(settings_.background_color) == 0;
  }

  const OsrRendererSettings settings_;
  bool initialized_;
  unsigned int texture_id_;
  int view_width_;
  int view_height_;
  CefRect popup_rect_;
  CefRect original_popup_rect_;
  float spin_x_;
  float spin_y_;
  CefRect update_rect_;

  DISALLOW_COPY_AND_ASSIGN(OsrRenderer);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDERER_H_
