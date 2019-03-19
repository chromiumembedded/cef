// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_BROWSER_WINDOW_OSR_GTK_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_BROWSER_WINDOW_OSR_GTK_H_
#pragma once

#include "include/base/cef_lock.h"

#include "tests/cefclient/browser/browser_window.h"
#include "tests/cefclient/browser/client_handler_osr.h"
#include "tests/cefclient/browser/osr_renderer.h"

namespace client {

// Represents a native child window hosting a single off-screen browser
// instance. The methods of this class must be called on the main thread unless
// otherwise indicated.
class BrowserWindowOsrGtk : public BrowserWindow,
                            public ClientHandlerOsr::OsrDelegate {
 public:
  typedef void* CefXIDeviceEvent;

  // Constructor may be called on any thread.
  // |delegate| must outlive this object.
  BrowserWindowOsrGtk(BrowserWindow::Delegate* delegate,
                      const std::string& startup_url,
                      const OsrRendererSettings& settings);

  // Called from RootWindowGtk::CreateRootWindow before CreateBrowser.
  void set_xdisplay(XDisplay* xdisplay);

  // BrowserWindow methods.
  void CreateBrowser(ClientWindowHandle parent_handle,
                     const CefRect& rect,
                     const CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue> extra_info,
                     CefRefPtr<CefRequestContext> request_context) OVERRIDE;
  void GetPopupConfig(CefWindowHandle temp_handle,
                      CefWindowInfo& windowInfo,
                      CefRefPtr<CefClient>& client,
                      CefBrowserSettings& settings) OVERRIDE;
  void ShowPopup(ClientWindowHandle parent_handle,
                 int x,
                 int y,
                 size_t width,
                 size_t height) OVERRIDE;
  void Show() OVERRIDE;
  void Hide() OVERRIDE;
  void SetBounds(int x, int y, size_t width, size_t height) OVERRIDE;
  void SetFocus(bool focus) OVERRIDE;
  void SetDeviceScaleFactor(float device_scale_factor) OVERRIDE;
  float GetDeviceScaleFactor() const OVERRIDE;
  ClientWindowHandle GetWindowHandle() const OVERRIDE;

  // ClientHandlerOsr::OsrDelegate methods.
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;
  bool GetRootScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect) OVERRIDE;
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) OVERRIDE;
  bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                      int viewX,
                      int viewY,
                      int& screenX,
                      int& screenY) OVERRIDE;
  bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                     CefScreenInfo& screen_info) OVERRIDE;
  void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) OVERRIDE;
  void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) OVERRIDE;
  void OnPaint(CefRefPtr<CefBrowser> browser,
               CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirtyRects,
               const void* buffer,
               int width,
               int height) OVERRIDE;
  void OnCursorChange(CefRefPtr<CefBrowser> browser,
                      CefCursorHandle cursor,
                      CefRenderHandler::CursorType type,
                      const CefCursorInfo& custom_cursor_info) OVERRIDE;
  bool StartDragging(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefDragData> drag_data,
                     CefRenderHandler::DragOperationsMask allowed_ops,
                     int x,
                     int y) OVERRIDE;
  void UpdateDragCursor(CefRefPtr<CefBrowser> browser,
                        CefRenderHandler::DragOperation operation) OVERRIDE;
  void OnImeCompositionRangeChanged(
      CefRefPtr<CefBrowser> browser,
      const CefRange& selection_range,
      const CefRenderHandler::RectList& character_bounds) OVERRIDE;
  void UpdateAccessibilityTree(CefRefPtr<CefValue> value) OVERRIDE;
  void UpdateAccessibilityLocation(CefRefPtr<CefValue> value) OVERRIDE;

 private:
  ~BrowserWindowOsrGtk();

  // Create the GTK GlArea.
  void Create(ClientWindowHandle parent_handle);

  // Signal handlers for the GTK GlArea.
  static gint SizeAllocation(GtkWidget* widget,
                             GtkAllocation* allocation,
                             BrowserWindowOsrGtk* self);
  static gint ClickEvent(GtkWidget* widget,
                         GdkEventButton* event,
                         BrowserWindowOsrGtk* self);
  static gint KeyEvent(GtkWidget* widget,
                       GdkEventKey* event,
                       BrowserWindowOsrGtk* self);
  static gint MoveEvent(GtkWidget* widget,
                        GdkEventMotion* event,
                        BrowserWindowOsrGtk* self);
  static gint ScrollEvent(GtkWidget* widget,
                          GdkEventScroll* event,
                          BrowserWindowOsrGtk* self);
  static gint FocusEvent(GtkWidget* widget,
                         GdkEventFocus* event,
                         BrowserWindowOsrGtk* self);

  void TouchEvent(CefXIDeviceEvent event);
  void RegisterTouch();

  bool IsOverPopupWidget(int x, int y) const;
  int GetPopupXOffset() const;
  int GetPopupYOffset() const;
  void ApplyPopupOffset(int& x, int& y) const;

  void EnableGL();
  void DisableGL();

  // Drag & drop
  void RegisterDragDrop();
  void UnregisterDragDrop();
  void DragReset();
  static void DragBegin(GtkWidget* widget,
                        GdkDragContext* drag_context,
                        BrowserWindowOsrGtk* self);
  static void DragDataGet(GtkWidget* widget,
                          GdkDragContext* drag_context,
                          GtkSelectionData* data,
                          guint info,
                          guint time,
                          BrowserWindowOsrGtk* self);
  static void DragEnd(GtkWidget* widget,
                      GdkDragContext* drag_context,
                      BrowserWindowOsrGtk* self);
  static gboolean DragMotion(GtkWidget* widget,
                             GdkDragContext* drag_context,
                             gint x,
                             gint y,
                             guint time,
                             BrowserWindowOsrGtk* self);
  static void DragLeave(GtkWidget* widget,
                        GdkDragContext* drag_context,
                        guint time,
                        BrowserWindowOsrGtk* self);
  static gboolean DragFailed(GtkWidget* widget,
                             GdkDragContext* drag_context,
                             GtkDragResult result,
                             BrowserWindowOsrGtk* self);
  static gboolean DragDrop(GtkWidget* widget,
                           GdkDragContext* drag_context,
                           gint x,
                           gint y,
                           guint time,
                           BrowserWindowOsrGtk* self);
  static void DragDataReceived(GtkWidget* widget,
                               GdkDragContext* drag_context,
                               gint x,
                               gint y,
                               GtkSelectionData* data,
                               guint info,
                               guint time,
                               BrowserWindowOsrGtk* self);
  static GdkFilterReturn EventFilter(GdkXEvent* gdk_xevent,
                                     GdkEvent* event,
                                     gpointer data);
  static void InitializeXinput(XDisplay* xdisplay);

  XDisplay* xdisplay_;

  // Members only accessed on the UI thread.
  OsrRenderer renderer_;
  bool gl_enabled_;
  bool painting_popup_;

  // Members only accessed on the main thread.
  bool hidden_;

  // Members protected by the GDK global lock.
  ClientWindowHandle glarea_;

  // Drag & drop
  GdkEvent* drag_trigger_event_;  // mouse event, a possible trigger for drag
  CefRefPtr<CefDragData> drag_data_;
  CefRenderHandler::DragOperation drag_operation_;
  GdkDragContext* drag_context_;
  GtkTargetList* drag_targets_;
  bool drag_leave_;
  bool drag_drop_;

  mutable base::Lock lock_;

  // Access to these members must be protected by |lock_|.
  float device_scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowOsrGtk);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_BROWSER_WINDOW_OSR_GTK_H_
