// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_CEFCLIENT_OSR_WIDGET_WIN_H_
#define CEF_TESTS_CEFCLIENT_CEFCLIENT_OSR_WIDGET_WIN_H_
#pragma once

#include "include/cef_render_handler.h"
#include "tests/cefclient/client_handler.h"
#include "tests/cefclient/osrenderer.h"

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
                                     bool transparent);

  static CefRefPtr<OSRWindow> From(
      CefRefPtr<ClientHandler::RenderHandler> renderHandler);

  // Create the underlying window.
  bool CreateWidget(HWND hWndParent, const RECT& rect,
                    HINSTANCE hInst, LPCTSTR className);

  // Destroy the underlying window.
  void DestroyWidget();

  HWND hwnd() const {
    return hWnd_;
  }

  // ClientHandler::RenderHandler methods
  virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;

  // CefRenderHandler methods
  virtual bool GetRootScreenRect(CefRefPtr<CefBrowser> browser,
                                 CefRect& rect) OVERRIDE;
  virtual bool GetViewRect(CefRefPtr<CefBrowser> browser,
                           CefRect& rect) OVERRIDE;
  virtual bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                              int viewX,
                              int viewY,
                              int& screenX,
                              int& screenY) OVERRIDE;
  virtual void OnPopupShow(CefRefPtr<CefBrowser> browser,
                           bool show) OVERRIDE;
  virtual void OnPopupSize(CefRefPtr<CefBrowser> browser,
                           const CefRect& rect) OVERRIDE;
  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirtyRects,
                       const void* buffer,
                       int width,
                       int height) OVERRIDE;
  virtual void OnCursorChange(CefRefPtr<CefBrowser> browser,
                              CefCursorHandle cursor) OVERRIDE;

 private:
  OSRWindow(OSRBrowserProvider* browser_provider, bool transparent);
  virtual ~OSRWindow();

  void EnableGL();
  void DisableGL();
  void OnDestroyed();
  static ATOM RegisterOSRClass(HINSTANCE hInstance, LPCTSTR className);
  static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                                  LPARAM lParam);

  ClientOSRenderer renderer_;
  OSRBrowserProvider* browser_provider_;
  HWND hWnd_;
  HDC hDC_;
  HGLRC hRC_;

  IMPLEMENT_REFCOUNTING(OSRWindow);
};

#endif  // CEF_TESTS_CEFCLIENT_CEFCLIENT_OSR_WIDGET_WIN_H_
