// Copyright 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDER_HANDLER_WIN_GL_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDER_HANDLER_WIN_GL_H_
#pragma once

#include "tests/cefclient/browser/osr_render_handler_win.h"
#include "tests/cefclient/browser/osr_renderer.h"

namespace client {

class OsrRenderHandlerWinGL : public OsrRenderHandlerWin {
 public:
  OsrRenderHandlerWinGL(const OsrRendererSettings& settings, HWND hwnd);
  virtual ~OsrRenderHandlerWinGL();

  // Must be called immediately after object creation.
  void Initialize(CefRefPtr<CefBrowser> browser);

  void SetSpin(float spinX, float spinY) OVERRIDE;
  void IncrementSpin(float spinDX, float spinDY) OVERRIDE;
  bool IsOverPopupWidget(int x, int y) const OVERRIDE;
  int GetPopupXOffset() const OVERRIDE;
  int GetPopupYOffset() const OVERRIDE;
  void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) OVERRIDE;
  void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) OVERRIDE;
  void OnPaint(CefRefPtr<CefBrowser> browser,
               CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) OVERRIDE;
  void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                          CefRenderHandler::PaintElementType type,
                          const CefRenderHandler::RectList& dirtyRects,
                          void* share_handle) OVERRIDE;

 private:
  void Render() OVERRIDE;

  void EnableGL();
  void DisableGL();

  // The below members are only accessed on the UI thread.
  OsrRenderer renderer_;
  HDC hdc_;
  HGLRC hrc_;
  bool painting_popup_;

  DISALLOW_COPY_AND_ASSIGN(OsrRenderHandlerWinGL);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_RENDER_HANDLER_WIN_GL_H_
