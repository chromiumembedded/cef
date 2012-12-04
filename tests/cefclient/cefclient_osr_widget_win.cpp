// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/cefclient_osr_widget_win.h"
#include "cefclient/resource.h"
#include "cefclient/util.h"

// static
CefRefPtr<OSRWindow> OSRWindow::Create(OSRBrowserProvider* browser_provider,
    bool transparent) {
  ASSERT(browser_provider);
  if (!browser_provider)
    return NULL;

  return new OSRWindow(browser_provider, transparent);
}

// static
CefRefPtr<OSRWindow> OSRWindow::From(
    CefRefPtr<ClientHandler::RenderHandler> renderHandler) {
  return static_cast<OSRWindow*>(renderHandler.get());
}

bool OSRWindow::CreateWidget(HWND hWndParent, const RECT& rect,
                             HINSTANCE hInst, LPCTSTR className) {
  ASSERT(hWnd_ == NULL && hDC_ == NULL && hRC_ == NULL);

  RegisterOSRClass(hInst, className);
  hWnd_ = ::CreateWindow(className, 0,
      WS_BORDER | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
      hWndParent, 0, hInst, 0);

  if (!hWnd_)
    return false;

  SetWindowLongPtr(hWnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

  // Reference released in OnDestroyed().
  AddRef();

  return true;
}

void OSRWindow::DestroyWidget() {
  if (IsWindow(hWnd_))
    DestroyWindow(hWnd_);
}

void OSRWindow::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  DisableGL();
}

bool OSRWindow::GetRootScreenRect(CefRefPtr<CefBrowser> browser,
                                  CefRect& rect) {
  RECT window_rect = {0};
  HWND root_window = GetAncestor(hWnd_, GA_ROOT);
  if (::GetWindowRect(root_window, &window_rect)) {
    rect = CefRect(window_rect.left,
                   window_rect.top,
                   window_rect.right - window_rect.left,
                   window_rect.bottom - window_rect.top);
    return true;
  }
  return false;
}

bool OSRWindow::GetViewRect(CefRefPtr<CefBrowser> browser,
                            CefRect& rect) {
  RECT clientRect;
  if (!::GetClientRect(hWnd_, &clientRect))
    return false;
  rect.x = rect.y = 0;
  rect.width = clientRect.right;
  rect.height = clientRect.bottom;
  return true;
}

bool OSRWindow::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                               int viewX,
                               int viewY,
                               int& screenX,
                               int& screenY) {
  if (!::IsWindow(hWnd_))
    return false;

  // Convert the point from view coordinates to actual screen coordinates.
  POINT screen_pt = {viewX, viewY};
  ClientToScreen(hWnd_, &screen_pt);
  screenX = screen_pt.x;
  screenY = screen_pt.y;
  return true;
}

void OSRWindow::OnPopupShow(CefRefPtr<CefBrowser> browser,
                            bool show) {
  renderer_.OnPopupShow(browser, show);
}

void OSRWindow::OnPopupSize(CefRefPtr<CefBrowser> browser,
                            const CefRect& rect) {
  renderer_.OnPopupSize(browser, rect);
}

void OSRWindow::OnPaint(CefRefPtr<CefBrowser> browser,
                        PaintElementType type,
                        const RectList& dirtyRects,
                        const void* buffer,
                        int width, int height) {
  if (!hDC_)
    EnableGL();

  wglMakeCurrent(hDC_, hRC_);
  renderer_.OnPaint(browser, type, dirtyRects, buffer, width, height);
  renderer_.Render();
  SwapBuffers(hDC_);
}

void OSRWindow::OnCursorChange(CefRefPtr<CefBrowser> browser,
                               CefCursorHandle cursor) {
  if (!::IsWindow(hWnd_))
    return;

  // Change the plugin window's cursor.
  SetClassLong(hWnd_, GCL_HCURSOR,
               static_cast<LONG>(reinterpret_cast<LONG_PTR>(cursor)));
  SetCursor(cursor);
}

OSRWindow::OSRWindow(OSRBrowserProvider* browser_provider, bool transparent)
    : renderer_(transparent),
      browser_provider_(browser_provider),
      hWnd_(NULL),
      hDC_(NULL),
      hRC_(NULL) {
}

OSRWindow::~OSRWindow() {
  DestroyWidget();
}

void OSRWindow::EnableGL() {
  ASSERT(CefCurrentlyOn(TID_UI));

  PIXELFORMATDESCRIPTOR pfd;
  int format;

  // Get the device context.
  hDC_ = GetDC(hWnd_);

  // Set the pixel format for the DC.
  ZeroMemory(&pfd, sizeof(pfd));
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 24;
  pfd.cDepthBits = 16;
  pfd.iLayerType = PFD_MAIN_PLANE;
  format = ChoosePixelFormat(hDC_, &pfd);
  SetPixelFormat(hDC_, format, &pfd);

  // Create and enable the render context.
  hRC_ = wglCreateContext(hDC_);
  wglMakeCurrent(hDC_, hRC_);

  renderer_.Initialize();
}

void OSRWindow::DisableGL() {
  ASSERT(CefCurrentlyOn(TID_UI));

  if (!hDC_)
    return;

  renderer_.Cleanup();

  if (IsWindow(hWnd_)) {
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC_);
    ReleaseDC(hWnd_, hDC_);
  }

  hDC_ = NULL;
  hRC_ = NULL;
}

void OSRWindow::OnDestroyed() {
  SetWindowLongPtr(hWnd_, GWLP_USERDATA, 0L);
  hWnd_ = NULL;
  Release();
}

ATOM OSRWindow::RegisterOSRClass(HINSTANCE hInstance, LPCTSTR className) {
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style         = CS_OWNDC;
  wcex.lpfnWndProc   = &OSRWindow::WndProc;
  wcex.cbClsExtra    = 0;
  wcex.cbWndExtra    = 0;
  wcex.hInstance     = hInstance;
  wcex.hIcon         = NULL;
  wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.lpszMenuName  = NULL;
  wcex.lpszClassName = className;
  wcex.hIconSm       = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
  return RegisterClassEx(&wcex);
}

// Plugin window procedure.
// static
LRESULT CALLBACK OSRWindow::WndProc(HWND hWnd, UINT message,
                                    WPARAM wParam, LPARAM lParam) {
  static POINT lastMousePos, curMousePos;
  static bool mouseRotation = false;
  static bool mouseTracking = false;

  OSRWindow* window =
      reinterpret_cast<OSRWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
  CefRefPtr<CefBrowserHost> browser;
  if (window && window->browser_provider_->GetBrowser().get())
    browser = window->browser_provider_->GetBrowser()->GetHost();

  switch (message) {
  case WM_DESTROY:
    if (window)
      window->OnDestroyed();
    return 0;

  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
    SetCapture(hWnd);
    SetFocus(hWnd);
    if (wParam & MK_SHIFT) {
      // Start rotation effect.
      lastMousePos.x = curMousePos.x = LOWORD(lParam);
      lastMousePos.y = curMousePos.y = HIWORD(lParam);
      mouseRotation = true;
    } else {
      if (browser.get()) {
        browser->SendMouseClickEvent(LOWORD(lParam), HIWORD(lParam),
            (message == WM_LBUTTONDOWN?MBT_LEFT:MBT_RIGHT), false, 1);
      }
    }
    break;

  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
    if (GetCapture() == hWnd)
      ReleaseCapture();
    if (mouseRotation) {
      // End rotation effect.
      mouseRotation = false;
      window->renderer_.SetSpin(0, 0);
    } else {
      if (browser.get()) {
        browser->SendMouseClickEvent(LOWORD(lParam), HIWORD(lParam),
            (message == WM_LBUTTONUP?MBT_LEFT:MBT_RIGHT), true, 1);
      }
    }
    break;

  case WM_MOUSEMOVE:
    if (mouseRotation) {
      // Apply rotation effect.
      curMousePos.x = LOWORD(lParam);
      curMousePos.y = HIWORD(lParam);
      window->renderer_.IncrementSpin((curMousePos.x - lastMousePos.x),
        (curMousePos.y - lastMousePos.y));
      lastMousePos.x = curMousePos.x;
      lastMousePos.y = curMousePos.y;
    } else {
      if (!mouseTracking) {
        // Start tracking mouse leave. Required for the WM_MOUSELEAVE event to
        // be generated.
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
        mouseTracking = true;
      }
      if (browser.get()) {
        browser->SendMouseMoveEvent(LOWORD(lParam), HIWORD(lParam), false);
      }
    }
    break;

  case WM_MOUSELEAVE:
    if (mouseTracking) {
      // Stop tracking mouse leave.
      TRACKMOUSEEVENT tme;
      tme.cbSize = sizeof(TRACKMOUSEEVENT);
      tme.dwFlags = TME_LEAVE & TME_CANCEL;
      tme.hwndTrack = hWnd;
      TrackMouseEvent(&tme);
      mouseTracking = false;
    }
    if (browser.get())
      browser->SendMouseMoveEvent(0, 0, true);
    break;

  case WM_MOUSEWHEEL:
    if (browser.get()) {
      browser->SendMouseWheelEvent(LOWORD(lParam), HIWORD(lParam),
                                   0, GET_WHEEL_DELTA_WPARAM(wParam));
    }
    break;

  case WM_SIZE:
    if (browser.get())
      browser->WasResized();
    break;

  case WM_SETFOCUS:
  case WM_KILLFOCUS:
    if (browser.get())
      browser->SendFocusEvent(message == WM_SETFOCUS);
    break;

  case WM_CAPTURECHANGED:
  case WM_CANCELMODE:
    if (!mouseRotation) {
      if (browser.get())
        browser->SendCaptureLostEvent();
    }
    break;
  case WM_SYSCHAR:
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP:
  case WM_CHAR: {
    CefKeyEvent event;
    event.windows_key_code = wParam;
    event.native_key_code = lParam;
    event.is_system_key = message == WM_SYSCHAR ||
                          message == WM_SYSKEYDOWN ||
                          message == WM_SYSKEYUP;

    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
      event.type = KEYEVENT_RAWKEYDOWN;
    else if (message == WM_KEYUP || message == WM_SYSKEYUP)
      event.type = KEYEVENT_KEYUP;
    else
      event.type = KEYEVENT_CHAR;

    if (browser.get())
      browser->SendKeyEvent(event);
    break;
  }

  case WM_PAINT: {
    PAINTSTRUCT ps;
    RECT rc;
    BeginPaint(hWnd, &ps);
    rc = ps.rcPaint;
    EndPaint(hWnd, &ps);
    if (browser.get()) {
      browser->Invalidate(CefRect(rc.left,
                                  rc.top,
                                  rc.right - rc.left,
                                  rc.bottom - rc.top));
    }
    return 0;
  }

  case WM_ERASEBKGND:
    return 0;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}
