// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_WINDOW_WIN_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_WINDOW_WIN_H_
#pragma once

#include "include/base/cef_callback.h"
#include "include/base/cef_ref_counted.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/cefclient/browser/client_handler_osr.h"
#include "tests/cefclient/browser/osr_accessibility_node.h"
#include "tests/cefclient/browser/osr_dragdrop_win.h"
#include "tests/cefclient/browser/osr_render_handler_win.h"
#include "tests/cefclient/browser/osr_renderer_settings.h"

namespace client {

class OsrAccessibilityHelper;
class OsrImeHandlerWin;

// Represents the native parent window for an off-screen browser. This object
// must live on the CEF UI thread in order to handle CefRenderHandler callbacks.
// The methods of this class are thread-safe unless otherwise indicated.
class OsrWindowWin
    : public base::RefCountedThreadSafe<OsrWindowWin, CefDeleteOnUIThread>,
      public ClientHandlerOsr::OsrDelegate
#if defined(CEF_USE_ATL)
    ,
      public OsrDragEvents
#endif
{
 public:
  // This interface is implemented by the owner of the OsrWindowWin. The
  // methods of this class will be called on the main thread.
  class Delegate {
   public:
    // Called after the native window has been created.
    virtual void OnOsrNativeWindowCreated(HWND hwnd) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // |delegate| must outlive this object.
  OsrWindowWin(Delegate* delegate, const OsrRendererSettings& settings);

  // Create a new browser and native window.
  void CreateBrowser(HWND parent_hwnd,
                     const RECT& rect,
                     CefRefPtr<CefClient> handler,
                     const CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue> extra_info,
                     CefRefPtr<CefRequestContext> request_context,
                     const std::string& startup_url);

  // Show the popup window with correct parent and bounds in parent coordinates.
  void ShowPopup(HWND parent_hwnd, int x, int y, size_t width, size_t height);

  void Show();
  void Hide();
  void SetBounds(int x, int y, size_t width, size_t height);
  void SetFocus();
  void SetDeviceScaleFactor(float device_scale_factor);

  const OsrRendererSettings& settings() const { return settings_; }

 private:
  // Only allow deletion via scoped_refptr.
  friend struct CefDeleteOnThread<TID_UI>;
  friend class base::RefCountedThreadSafe<OsrWindowWin, CefDeleteOnUIThread>;

  ~OsrWindowWin();

  // Manage native window lifespan.
  void Create(HWND parent_hwnd, const RECT& rect);
  void Destroy();

  void NotifyNativeWindowCreated(HWND hwnd);

  static void RegisterOsrClass(HINSTANCE hInstance, HBRUSH background_brush);
  static LRESULT CALLBACK OsrWndProc(HWND hWnd,
                                     UINT message,
                                     WPARAM wParam,
                                     LPARAM lParam);

  // WndProc message handlers.
  void OnMouseEvent(UINT message, WPARAM wParam, LPARAM lParam);
  void OnSize();
  void OnFocus(bool setFocus);
  void OnCaptureLost();
  void OnKeyEvent(UINT message, WPARAM wParam, LPARAM lParam);
  void OnPaint();
  bool OnEraseBkgnd();
  bool OnTouchEvent(UINT message, WPARAM wParam, LPARAM lParam);

  void OnIMESetContext(UINT message, WPARAM wParam, LPARAM lParam);
  void OnIMEStartComposition();
  void OnIMEComposition(UINT message, WPARAM wParam, LPARAM lParam);
  void OnIMECancelCompositionEvent();

  // Manage popup bounds.
  bool IsOverPopupWidget(int x, int y) const;
  int GetPopupXOffset() const;
  int GetPopupYOffset() const;
  void ApplyPopupOffset(int& x, int& y) const;

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
                          void* share_handle) override;
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

#if defined(CEF_USE_ATL)
  // OsrDragEvents methods.
  CefBrowserHost::DragOperationsMask OnDragEnter(
      CefRefPtr<CefDragData> drag_data,
      CefMouseEvent ev,
      CefBrowserHost::DragOperationsMask effect) override;
  CefBrowserHost::DragOperationsMask OnDragOver(
      CefMouseEvent ev,
      CefBrowserHost::DragOperationsMask effect) override;
  void OnDragLeave() override;
  CefBrowserHost::DragOperationsMask OnDrop(
      CefMouseEvent ev,
      CefBrowserHost::DragOperationsMask effect) override;
#endif  // defined(CEF_USE_ATL)

  void EnsureRenderHandler();

  // Only accessed on the main thread.
  Delegate* delegate_;

  const OsrRendererSettings settings_;
  HWND hwnd_;
  std::unique_ptr<OsrRenderHandlerWin> render_handler_;

  // Class that encapsulates IMM32 APIs and controls IMEs attached to a window.
  std::unique_ptr<OsrImeHandlerWin> ime_handler_;

  RECT client_rect_;
  float device_scale_factor_;

  CefRefPtr<CefBrowser> browser_;

#if defined(CEF_USE_ATL)
  CComPtr<DropTargetWin> drop_target_;
  CefRenderHandler::DragOperation current_drag_op_;

  // Class that abstracts the accessibility information received from the
  // renderer.
  std::unique_ptr<OsrAccessibilityHelper> accessibility_handler_;
  IAccessible* accessibility_root_;
#endif

  bool hidden_;

  // Mouse state tracking.
  POINT last_mouse_pos_;
  POINT current_mouse_pos_;
  bool mouse_rotation_;
  bool mouse_tracking_;
  int last_click_x_;
  int last_click_y_;
  CefBrowserHost::MouseButtonType last_click_button_;
  int last_click_count_;
  double last_click_time_;
  bool last_mouse_down_on_view_;

  DISALLOW_COPY_AND_ASSIGN(OsrWindowWin);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_WINDOW_WIN_H_
