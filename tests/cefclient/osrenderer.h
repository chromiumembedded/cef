// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_OSRENDERER_H_
#define CEF_TESTS_CEFCLIENT_OSRENDERER_H_
#pragma once

#include "include/cef_browser.h"
#include "include/cef_render_handler.h"

class ClientOSRenderer {
 public:
  // The context must outlive this object.
  explicit ClientOSRenderer(bool transparent);
  virtual ~ClientOSRenderer();

  // Initialize the OpenGL environment.
  void Initialize();

  // Clean up the OpenGL environment.
  void Cleanup();

  // Render to the screen.
  void Render();

  // Forwarded from CefRenderHandler callbacks.
  void OnPopupShow(CefRefPtr<CefBrowser> browser,
                   bool show);
  void OnPopupSize(CefRefPtr<CefBrowser> browser,
                   const CefRect& rect);
  void OnPaint(CefRefPtr<CefBrowser> browser,
               CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirtyRects,
               const void* buffer, int width, int height);

  // Apply spin.
  void SetSpin(float spinX, float spinY);
  void IncrementSpin(float spinDX, float spinDY);

  bool IsTransparent() { return transparent_; }

  int GetViewWidth() { return view_width_; }
  int GetViewHeight() { return view_height_; }

 private:
  bool transparent_;
  bool initialized_;
  unsigned int texture_id_;
  int view_width_;
  int view_height_;
  CefRect popup_rect_;
  float spin_x_;
  float spin_y_;
};

#endif  // CEF_TESTS_CEFCLIENT_OSRENDERER_H_

