// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_OSRENDERER_H_
#define CEF_TESTS_CEFCLIENT_OSRENDERER_H_

#include "include/cef.h"

class ClientOSRenderer {
 public:
  // The context must outlive this object.
  explicit ClientOSRenderer(bool transparent);
  virtual ~ClientOSRenderer();

  // Initialize the OpenGL environment.
  void Initialize();

  // Clean up the OpenGL environment.
  void Cleanup();

  // Set the size of the viewport.
  void SetSize(int width, int height);

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
               const void* buffer);

  // Apply spin.
  void SetSpin(float spinX, float spinY);
  void IncrementSpin(float spinDX, float spinDY);

  // Retrieve the pixel value from the view buffer. |x| and |y| are relative to
  // the upper-left corner of the view.
  bool GetPixelValue(int x, int y, unsigned char& r, unsigned char& g,
                     unsigned char& b, unsigned char& a);

  bool IsTransparent() { return transparent_; }

 private:
  void SetBufferSize(int width, int height, bool view);
  void SetRGBA(const void* src, int width, int height, bool view);
  void SetRGB(const void* src, int width, int height, bool view);
  void DrawViewBuffer(int max_width, int max_height);
 
  bool transparent_;
  bool initialized_;
  unsigned int texture_id_;
  unsigned char* view_buffer_;
  int view_buffer_size_;
  unsigned char* popup_buffer_;
  int popup_buffer_size_;
  CefRect popup_rect_;
  int view_width_;
  int view_height_;
  float spin_x_;
  float spin_y_;
};

#endif  // CEF_TESTS_CEFCLIENT_OSR_RENDERER_H_

