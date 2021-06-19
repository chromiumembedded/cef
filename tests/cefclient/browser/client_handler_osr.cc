// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/client_handler_osr.h"

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

namespace client {

ClientHandlerOsr::ClientHandlerOsr(Delegate* delegate,
                                   OsrDelegate* osr_delegate,
                                   const std::string& startup_url)
    : ClientHandler(delegate, true, startup_url), osr_delegate_(osr_delegate) {
  DCHECK(osr_delegate_);
}

void ClientHandlerOsr::DetachOsrDelegate() {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI,
                base::BindOnce(&ClientHandlerOsr::DetachOsrDelegate, this));
    return;
  }

  DCHECK(osr_delegate_);
  osr_delegate_ = nullptr;
}

void ClientHandlerOsr::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  if (osr_delegate_)
    osr_delegate_->OnAfterCreated(browser);
  ClientHandler::OnAfterCreated(browser);
}

void ClientHandlerOsr::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  if (osr_delegate_)
    osr_delegate_->OnBeforeClose(browser);
  ClientHandler::OnBeforeClose(browser);
}

bool ClientHandlerOsr::GetRootScreenRect(CefRefPtr<CefBrowser> browser,
                                         CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_)
    return false;
  return osr_delegate_->GetRootScreenRect(browser, rect);
}

void ClientHandlerOsr::GetViewRect(CefRefPtr<CefBrowser> browser,
                                   CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_) {
    // Never return an empty rectangle.
    rect.width = rect.height = 1;
    return;
  }
  osr_delegate_->GetViewRect(browser, rect);
}

bool ClientHandlerOsr::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                                      int viewX,
                                      int viewY,
                                      int& screenX,
                                      int& screenY) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_)
    return false;
  return osr_delegate_->GetScreenPoint(browser, viewX, viewY, screenX, screenY);
}

bool ClientHandlerOsr::GetScreenInfo(CefRefPtr<CefBrowser> browser,
                                     CefScreenInfo& screen_info) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_)
    return false;
  return osr_delegate_->GetScreenInfo(browser, screen_info);
}

void ClientHandlerOsr::OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_)
    return;
  return osr_delegate_->OnPopupShow(browser, show);
}

void ClientHandlerOsr::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                   const CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_)
    return;
  return osr_delegate_->OnPopupSize(browser, rect);
}

void ClientHandlerOsr::OnPaint(CefRefPtr<CefBrowser> browser,
                               PaintElementType type,
                               const RectList& dirtyRects,
                               const void* buffer,
                               int width,
                               int height) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_)
    return;
  osr_delegate_->OnPaint(browser, type, dirtyRects, buffer, width, height);
}

void ClientHandlerOsr::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirtyRects,
    void* share_handle) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_)
    return;
  osr_delegate_->OnAcceleratedPaint(browser, type, dirtyRects, share_handle);
}

bool ClientHandlerOsr::StartDragging(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDragData> drag_data,
    CefRenderHandler::DragOperationsMask allowed_ops,
    int x,
    int y) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_)
    return false;
  return osr_delegate_->StartDragging(browser, drag_data, allowed_ops, x, y);
}

void ClientHandlerOsr::UpdateDragCursor(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::DragOperation operation) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_)
    return;
  osr_delegate_->UpdateDragCursor(browser, operation);
}

void ClientHandlerOsr::OnImeCompositionRangeChanged(
    CefRefPtr<CefBrowser> browser,
    const CefRange& selection_range,
    const CefRenderHandler::RectList& character_bounds) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_)
    return;
  osr_delegate_->OnImeCompositionRangeChanged(browser, selection_range,
                                              character_bounds);
}

void ClientHandlerOsr::OnAccessibilityTreeChange(CefRefPtr<CefValue> value) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_)
    return;
  osr_delegate_->UpdateAccessibilityTree(value);
}

bool ClientHandlerOsr::OnCursorChange(CefRefPtr<CefBrowser> browser,
                                      CefCursorHandle cursor,
                                      cef_cursor_type_t type,
                                      const CefCursorInfo& custom_cursor_info) {
  CEF_REQUIRE_UI_THREAD();
  if (ClientHandler::OnCursorChange(browser, cursor, type,
                                    custom_cursor_info)) {
    return true;
  }
  if (osr_delegate_) {
    osr_delegate_->OnCursorChange(browser, cursor, type, custom_cursor_info);
  }
  return true;
}

void ClientHandlerOsr::OnAccessibilityLocationChange(
    CefRefPtr<CefValue> value) {
  CEF_REQUIRE_UI_THREAD();
  if (!osr_delegate_)
    return;
  osr_delegate_->UpdateAccessibilityLocation(value);
}

}  // namespace client
