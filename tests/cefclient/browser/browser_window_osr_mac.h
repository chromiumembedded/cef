// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_BROWSER_WINDOW_OSR_MAC_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_BROWSER_WINDOW_OSR_MAC_H_
#pragma once

#include "tests/cefclient/browser/browser_window.h"
#include "tests/cefclient/browser/client_handler_osr.h"
#include "tests/cefclient/browser/osr_renderer.h"
#include "tests/cefclient/browser/text_input_client_osr_mac.h"

namespace client {

class BrowserWindowOsrMacImpl;

// Represents a native child window hosting a single off-screen browser
// instance. The methods of this class must be called on the main thread unless
// otherwise indicated.
class BrowserWindowOsrMac : public BrowserWindow,
                            public ClientHandlerOsr::OsrDelegate {
 public:
  // Constructor may be called on any thread.
  // |delegate| must outlive this object.
  BrowserWindowOsrMac(BrowserWindow::Delegate* delegate,
                      bool with_controls,
                      const std::string& startup_url,
                      const OsrRendererSettings& settings);
  ~BrowserWindowOsrMac();

  // BrowserWindow methods.
  void CreateBrowser(ClientWindowHandle parent_handle,
                     const CefRect& rect,
                     const CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue> extra_info,
                     CefRefPtr<CefRequestContext> request_context) override;
  void GetPopupConfig(CefWindowHandle temp_handle,
                      CefWindowInfo& windowInfo,
                      CefRefPtr<CefClient>& client,
                      CefBrowserSettings& settings) override;
  void ShowPopup(ClientWindowHandle parent_handle,
                 int x,
                 int y,
                 size_t width,
                 size_t height) override;
  void Show() override;
  void Hide() override;
  void SetBounds(int x, int y, size_t width, size_t height) override;
  void SetFocus(bool focus) override;
  void SetDeviceScaleFactor(float device_scale_factor) override;
  float GetDeviceScaleFactor() const override;
  ClientWindowHandle GetWindowHandle() const override;

  // ClientHandlerOsr::OsrDelegate methods.
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
  bool GetRootScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
  bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                      int viewX,
                      int viewY,
                      int& screenX,
                      int& screenY) override;
  bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                     CefScreenInfo& screen_info) override;
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
                          const CefAcceleratedPaintInfo& info) override;
  void OnCursorChange(CefRefPtr<CefBrowser> browser,
                      CefCursorHandle cursor,
                      cef_cursor_type_t type,
                      const CefCursorInfo& custom_cursor_info) override;
  bool StartDragging(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefDragData> drag_data,
                     CefRenderHandler::DragOperationsMask allowed_ops,
                     int x,
                     int y) override;
  void UpdateDragCursor(CefRefPtr<CefBrowser> browser,
                        CefRenderHandler::DragOperation operation) override;
  void OnImeCompositionRangeChanged(
      CefRefPtr<CefBrowser> browser,
      const CefRange& selection_range,
      const CefRenderHandler::RectList& character_bounds) override;

  void UpdateAccessibilityTree(CefRefPtr<CefValue> value) override;
  void UpdateAccessibilityLocation(CefRefPtr<CefValue> value) override;

 private:
  std::unique_ptr<BrowserWindowOsrMacImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowOsrMac);

  friend class BrowserWindowOsrMacImpl;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_BROWSER_WINDOW_OSR_MAC_H_
