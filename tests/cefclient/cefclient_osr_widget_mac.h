// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_CEFCLIENT_OSR_WIDGET_MAC_H_
#define CEF_TESTS_CEFCLIENT_CEFCLIENT_OSR_WIDGET_MAC_H_

#include "include/cef_client.h"
#include "cefclient/client_handler.h"

class ClientOSRenderer;

class OSRBrowserProvider {
 public:
  virtual CefRefPtr<CefBrowser> GetBrowser() =0;

 protected:
  virtual ~OSRBrowserProvider() {}
};

// The client OpenGL view.
@interface ClientOpenGLView : NSOpenGLView {
@public
  NSTrackingArea* tracking_area_;
  OSRBrowserProvider* browser_provider_;
  ClientOSRenderer* renderer_;
  NSPoint last_mouse_pos_;
  NSPoint cur_mouse_pos_;
  bool rotating_;

  bool was_last_mouse_down_on_view_;

  // Event monitor for scroll wheel end event.
  id endWheelMonitor_;
}

- (id)initWithFrame:(NSRect)frame andTransparency:(bool)transparency;
- (NSPoint)getClickPointForEvent:(NSEvent*)event;
- (void)getKeyEvent:(CefKeyEvent&)keyEvent forEvent:(NSEvent*)event;
- (void)getMouseEvent:(CefMouseEvent&)mouseEvent forEvent:(NSEvent*)event;
- (int)getModifiersForEvent:(NSEvent*)event;
- (BOOL)isKeyUpEvent:(NSEvent*)event;
- (BOOL)isKeyPadEvent:(NSEvent*)event;
- (CefRefPtr<CefBrowser>)getBrowser;
@end

// Handler for off-screen rendering windows.
class ClientOSRHandler : public ClientHandler::RenderHandler {
 public:
  explicit ClientOSRHandler(ClientOpenGLView* view,
                            OSRBrowserProvider* browser_provider);
  virtual ~ClientOSRHandler();

  void Disconnect();

  // ClientHandler::RenderHandler
  virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;

  // CefRenderHandler methods

  virtual bool GetViewRect(CefRefPtr<CefBrowser> browser,
                           CefRect& rect) OVERRIDE;

  virtual bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                              int viewX,
                              int viewY,
                              int& screenX,
                              int& screenY) OVERRIDE;
  virtual bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
                             CefScreenInfo& screen_info) OVERRIDE;

  virtual void OnPopupShow(CefRefPtr<CefBrowser> browser,
                           bool show) OVERRIDE;

  virtual void OnPopupSize(CefRefPtr<CefBrowser> browser,
                           const CefRect& rect) OVERRIDE;

  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirtyRects,
                       const void* buffer,
                       int width, int height) OVERRIDE;

  virtual void OnCursorChange(CefRefPtr<CefBrowser> browser,
                              CefCursorHandle cursor) OVERRIDE;

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
                                     CefWindowHandle parentView,
                                     const CefRect& frame);

  CefRefPtr<ClientHandler::RenderHandler> GetRenderHandler() {
    return render_client.get();
  }

  CefWindowHandle GetWindowHandle() { return view_; }

 private:
  OSRWindow(OSRBrowserProvider* browser_provider,
            bool transparent,
            CefWindowHandle parentView,
            const CefRect& frame);

  ~OSRWindow();

  CefRefPtr<ClientOSRHandler> render_client;
  CefWindowHandle view_;

  IMPLEMENT_REFCOUNTING(OSRWindow);
};

#endif  // CEF_TESTS_CEFCLIENT_CEFCLIENT_OSR_WIDGET_MAC_H_

