// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_HANDLER_OSR_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_HANDLER_OSR_H_
#pragma once

#include "tests/cefclient/browser/client_handler.h"

namespace client {

// Client handler implementation for windowless browsers. There will only ever
// be one browser per handler instance.
class ClientHandlerOsr : public ClientHandler,
                         public CefAccessibilityHandler,
                         public CefRenderHandler {
 public:
  // Implement this interface to receive notification of ClientHandlerOsr
  // events. The methods of this class will be called on the CEF UI thread.
  class OsrDelegate {
   public:
    // These methods match the CefLifeSpanHandler interface.
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) = 0;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) = 0;

    // These methods match the CefRenderHandler interface.
    virtual bool GetRootScreenRect(CefRefPtr<CefBrowser> browser,
                                   CefRect& rect) = 0;
    virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) = 0;
    virtual bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                                int viewX,
                                int viewY,
                                int& screenX,
                                int& screenY) = 0;
    virtual bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                               CefScreenInfo& screen_info) = 0;
    virtual void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) = 0;
    virtual void OnPopupSize(CefRefPtr<CefBrowser> browser,
                             const CefRect& rect) = 0;
    virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                         CefRenderHandler::PaintElementType type,
                         const CefRenderHandler::RectList& dirtyRects,
                         const void* buffer,
                         int width,
                         int height) = 0;
    virtual void OnAcceleratedPaint(
        CefRefPtr<CefBrowser> browser,
        CefRenderHandler::PaintElementType type,
        const CefRenderHandler::RectList& dirtyRects,
        void* share_handle) {}
    virtual bool StartDragging(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefDragData> drag_data,
                               CefRenderHandler::DragOperationsMask allowed_ops,
                               int x,
                               int y) = 0;
    virtual void UpdateDragCursor(
        CefRefPtr<CefBrowser> browser,
        CefRenderHandler::DragOperation operation) = 0;
    virtual void OnImeCompositionRangeChanged(
        CefRefPtr<CefBrowser> browser,
        const CefRange& selection_range,
        const CefRenderHandler::RectList& character_bounds) = 0;

    // These methods match the CefDisplayHandler interface.
    virtual void OnCursorChange(CefRefPtr<CefBrowser> browser,
                                CefCursorHandle cursor,
                                cef_cursor_type_t type,
                                const CefCursorInfo& custom_cursor_info) = 0;

    virtual void UpdateAccessibilityTree(CefRefPtr<CefValue> value) = 0;
    virtual void UpdateAccessibilityLocation(CefRefPtr<CefValue> value) = 0;

   protected:
    virtual ~OsrDelegate() {}
  };

  ClientHandlerOsr(Delegate* delegate,
                   OsrDelegate* osr_delegate,
                   const std::string& startup_url);

  // This object may outlive the OsrDelegate object so it's necessary for the
  // OsrDelegate to detach itself before destruction.
  void DetachOsrDelegate();

  // CefClient methods.
  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
  CefRefPtr<CefAccessibilityHandler> GetAccessibilityHandler() override {
    return this;
  }

  // CefLifeSpanHandler methods.
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefRenderHandler methods.
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
                          void* share_handle) override;
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

  // CefDisplayHandler methods.
  bool OnCursorChange(CefRefPtr<CefBrowser> browser,
                      CefCursorHandle cursor,
                      cef_cursor_type_t type,
                      const CefCursorInfo& custom_cursor_info) override;

  // CefAccessibilityHandler methods.
  void OnAccessibilityTreeChange(CefRefPtr<CefValue> value) override;
  void OnAccessibilityLocationChange(CefRefPtr<CefValue> value) override;

 private:
  // Only accessed on the UI thread.
  OsrDelegate* osr_delegate_;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(ClientHandlerOsr);
  DISALLOW_COPY_AND_ASSIGN(ClientHandlerOsr);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_HANDLER_OSR_H_
