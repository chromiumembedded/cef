// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_OSR_WIDGET_MAC_H_
#define CEF_TESTS_CEFCLIENT_OSR_WIDGET_MAC_H_

#include "include/cef_client.h"
#include "cefclient/client_handler.h"

namespace client {
class OSRBrowserProvider;
class OsrRenderer;
}

// The client OpenGL view.
@interface ClientOpenGLView
    : NSOpenGLView <NSDraggingSource, NSDraggingDestination> {
@public
  NSTrackingArea* tracking_area_;
  client::OSRBrowserProvider* browser_provider_;
  client::OsrRenderer* renderer_;
  NSPoint last_mouse_pos_;
  NSPoint cur_mouse_pos_;
  bool rotating_;

  bool was_last_mouse_down_on_view_;

  // Drag and drop
  CefRefPtr<CefDragData> current_drag_data_;
  NSDragOperation current_drag_op_;
  NSDragOperation current_allowed_ops_;
  NSPasteboard* pasteboard_;
  CFStringRef fileUTI_;

  // Event monitor for scroll wheel end event.
  id endWheelMonitor_;
}

- (id)initWithFrame:(NSRect)frame andTransparency:(bool)transparency
                                andShowUpdateRect:(bool)show_update_rect;
- (NSPoint)getClickPointForEvent:(NSEvent*)event;
- (void)getKeyEvent:(CefKeyEvent&)keyEvent forEvent:(NSEvent*)event;
- (void)getMouseEvent:(CefMouseEvent&)mouseEvent forEvent:(NSEvent*)event;
- (int)getModifiersForEvent:(NSEvent*)event;
- (BOOL)isKeyUpEvent:(NSEvent*)event;
- (BOOL)isKeyPadEvent:(NSEvent*)event;
- (CefRefPtr<CefBrowser>)getBrowser;
- (BOOL)startDragging:(CefRefPtr<CefDragData>)drag_data
          allowed_ops:(NSDragOperation)ops point:(NSPoint)p;
@end

namespace client {

class OSRBrowserProvider {
 public:
  virtual CefRefPtr<CefBrowser> GetBrowser() =0;

 protected:
  virtual ~OSRBrowserProvider() {}
};

// Handler for off-screen rendering windows.
class ClientOSRHandler : public ClientHandler::RenderHandler {
 public:
  explicit ClientOSRHandler(ClientOpenGLView* view,
                            OSRBrowserProvider* browser_provider);
  ~ClientOSRHandler();

  void Disconnect();

  // ClientHandler::RenderHandler
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;

  // CefRenderHandler methods
  bool GetViewRect(CefRefPtr<CefBrowser> browser,
                   CefRect& rect) OVERRIDE;
  bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                      int viewX,
                      int viewY,
                      int& screenX,
                      int& screenY) OVERRIDE;
  bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                     CefScreenInfo& screen_info) OVERRIDE;
  void OnPopupShow(CefRefPtr<CefBrowser> browser,
                   bool show) OVERRIDE;
  void OnPopupSize(CefRefPtr<CefBrowser> browser,
                   const CefRect& rect) OVERRIDE;
  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType type,
               const RectList& dirtyRects,
               const void* buffer,
               int width, int height) OVERRIDE;
  void OnCursorChange(CefRefPtr<CefBrowser> browser,
                      CefCursorHandle cursor,
                      CursorType type,
                      const CefCursorInfo& custom_cursor_info) OVERRIDE;
  bool StartDragging(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefDragData> drag_data,
                     CefRenderHandler::DragOperationsMask allowed_ops,
                     int x, int y) OVERRIDE;
  void UpdateDragCursor(
      CefRefPtr<CefBrowser> browser,
      CefRenderHandler::DragOperation operation) OVERRIDE;

  CefWindowHandle view() { return view_; }

 private:
  void SetLoading(bool isLoading);

  ClientOpenGLView* view_;

  bool painting_popup_;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(ClientOSRHandler);
};

class OSRWindow {
 public:
  static CefRefPtr<OSRWindow> Create(OSRBrowserProvider* browser_provider,
                                     bool transparent,
                                     bool show_update_rect,
                                     CefWindowHandle parentView,
                                     const CefRect& frame);

  CefRefPtr<ClientHandler::RenderHandler> GetRenderHandler() {
    return render_client.get();
  }

  CefWindowHandle GetWindowHandle() { return view_; }

 private:
  OSRWindow(OSRBrowserProvider* browser_provider,
            bool transparent,
            bool show_update_rect,
            CefWindowHandle parentView,
            const CefRect& frame);
  ~OSRWindow();

  CefRefPtr<ClientOSRHandler> render_client;
  CefWindowHandle view_;

  IMPLEMENT_REFCOUNTING(OSRWindow);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_OSR_WIDGET_MAC_H_

