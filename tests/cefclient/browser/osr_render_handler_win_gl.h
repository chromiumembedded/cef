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

  void SetSpin(float spinX, float spinY) override;
  void IncrementSpin(float spinDX, float spinDY) override;
  bool IsOverPopupWidget(int x, int y) const override;
  int GetPopupXOffset() const override;
  int GetPopupYOffset() const override;
  void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override;
  void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) override;
  void OnPaint(CefRefPtr<CefBrowser> browser,
               CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) override;
  void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                          CefRenderHandler::PaintElementType type,
                          const CefRenderHandler::RectList& dirtyRects,
                          void* share_handle) override;

 private:
  void Render() override;

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
