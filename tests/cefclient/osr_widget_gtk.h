// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_OSR_WIDGET_GTK_H_
#define CEF_TESTS_CEFCLIENT_OSR_WIDGET_GTK_H_
#pragma once

#include "include/cef_render_handler.h"
#include "cefclient/client_handler.h"
#include "cefclient/osr_renderer.h"

namespace client {

class OSRBrowserProvider {
 public:
  virtual CefRefPtr<CefBrowser> GetBrowser() =0;

 protected:
  virtual ~OSRBrowserProvider() {}
};

class OSRWindow : public ClientHandler::RenderHandler {
 public:
  // Create a new OSRWindow instance. |browser_provider| must outlive this
  // object.
  static CefRefPtr<OSRWindow> Create(OSRBrowserProvider* browser_provider,
                                     bool transparent,
                                     bool show_update_rect,
                                     ClientWindowHandle parentView);

  static CefRefPtr<OSRWindow> From(
      CefRefPtr<ClientHandler::RenderHandler> renderHandler);

  ClientWindowHandle GetWindowHandle() const {
    return glarea_;
  }
  CefRefPtr<CefBrowserHost> GetBrowserHost() const {
    if (browser_provider_->GetBrowser())
      return browser_provider_->GetBrowser()->GetHost();
    return NULL;
  }

  // ClientHandler::RenderHandler methods
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;

  // CefRenderHandler methods
  bool GetViewRect(CefRefPtr<CefBrowser> browser,
                   CefRect& rect) OVERRIDE;
  bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                      int viewX,
                      int viewY,
                      int& screenX,
                      int& screenY) OVERRIDE;
  void OnPopupShow(CefRefPtr<CefBrowser> browser,
                   bool show) OVERRIDE;
  void OnPopupSize(CefRefPtr<CefBrowser> browser,
                   const CefRect& rect) OVERRIDE;
  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) OVERRIDE;
  void OnCursorChange(CefRefPtr<CefBrowser> browser,
                      CefCursorHandle cursor,
                      CursorType type,
                      const CefCursorInfo& custom_cursor_info) OVERRIDE;

  void Invalidate();
  bool IsOverPopupWidget(int x, int y) const;
  int GetPopupXOffset() const;
  int GetPopupYOffset() const;
  void ApplyPopupOffset(int& x, int& y) const;

 private:
  OSRWindow(OSRBrowserProvider* browser_provider,
            bool transparent,
            bool show_update_rect,
            ClientWindowHandle parentView);
  ~OSRWindow();

  void Render();
  void EnableGL();
  void DisableGL();

  OsrRenderer renderer_;
  OSRBrowserProvider* browser_provider_;
  ClientWindowHandle glarea_;
  bool gl_enabled_;

  bool painting_popup_;
  bool render_task_pending_;

  IMPLEMENT_REFCOUNTING(OSRWindow);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_OSR_WIDGET_GTK_H_
