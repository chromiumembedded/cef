// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/osr_widget_win.h"

#include <windowsx.h>

#include "include/base/cef_bind.h"
#include "include/base/cef_build.h"
#include "include/wrapper/cef_closure_task.h"
#include "cefclient/resource.h"
#include "cefclient/util_win.h"

namespace client {

namespace {

class ScopedGLContext {
 public:
  ScopedGLContext(HDC hdc, HGLRC hglrc, bool swap_buffers)
    : hdc_(hdc),
      swap_buffers_(swap_buffers) {
    BOOL result = wglMakeCurrent(hdc, hglrc);
    ALLOW_UNUSED_LOCAL(result);
    DCHECK(result);
  }
  ~ScopedGLContext() {
    BOOL result = wglMakeCurrent(NULL, NULL);
    DCHECK(result);
    if (swap_buffers_) {
      result = SwapBuffers(hdc_);
      DCHECK(result);
    }
  }

 private:
  const HDC hdc_;
  const bool swap_buffers_;
};

}  // namespace

// static
CefRefPtr<OSRWindow> OSRWindow::Create(
    OSRBrowserProvider* browser_provider,
    bool transparent,
    bool show_update_rect) {
  DCHECK(browser_provider);
  if (!browser_provider)
    return NULL;

  return new OSRWindow(browser_provider, transparent, show_update_rect);
}

// static
CefRefPtr<OSRWindow> OSRWindow::From(
    CefRefPtr<ClientHandler::RenderHandler> renderHandler) {
  return static_cast<OSRWindow*>(renderHandler.get());
}

bool OSRWindow::CreateWidget(HWND hWndParent, const RECT& rect,
                             HINSTANCE hInst, LPCTSTR className) {
  DCHECK(hWnd_ == NULL && hDC_ == NULL && hRC_ == NULL);

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

#if defined(CEF_USE_ATL)
  drop_target_ = DropTargetWin::Create(this, hWnd_);
  HRESULT register_res = RegisterDragDrop(hWnd_, drop_target_);
  DCHECK_EQ(register_res, S_OK);
#endif

  return true;
}

void OSRWindow::DestroyWidget() {
  if (IsWindow(hWnd_))
    DestroyWindow(hWnd_);
}

void OSRWindow::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
#if defined(CEF_USE_ATL)
  RevokeDragDrop(hWnd_);
  drop_target_ = NULL;
#endif

  DisableGL();
  ::DestroyWindow(hWnd_);
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
  if (!show) {
    renderer_.ClearPopupRects();
    browser->GetHost()->Invalidate(PET_VIEW);
  }
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
  if (painting_popup_) {
    renderer_.OnPaint(browser, type, dirtyRects, buffer, width, height);
    return;
  }
  if (!hDC_)
    EnableGL();

  {
    ScopedGLContext scoped_gl_context(hDC_, hRC_, true);
    renderer_.OnPaint(browser, type, dirtyRects, buffer, width, height);
    if (type == PET_VIEW && !renderer_.popup_rect().IsEmpty()) {
      painting_popup_ = true;
      browser->GetHost()->Invalidate(PET_POPUP);
      painting_popup_ = false;
    }
    renderer_.Render();
  }
}

void OSRWindow::OnCursorChange(CefRefPtr<CefBrowser> browser,
                               CefCursorHandle cursor,
                               CursorType type,
                               const CefCursorInfo& custom_cursor_info) {
  if (!::IsWindow(hWnd_))
    return;

  // Change the plugin window's cursor.
  SetClassLongPtr(hWnd_, GCLP_HCURSOR,
                  static_cast<LONG>(reinterpret_cast<LONG_PTR>(cursor)));
  SetCursor(cursor);
}

bool OSRWindow::StartDragging(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefDragData> drag_data,
                               CefRenderHandler::DragOperationsMask allowed_ops,
                               int x, int y) {
#if defined(CEF_USE_ATL)
  if (!drop_target_)
    return false;
  current_drag_op_ = DRAG_OPERATION_NONE;
  CefBrowserHost::DragOperationsMask result =
      drop_target_->StartDragging(browser, drag_data, allowed_ops, x, y);
  current_drag_op_ = DRAG_OPERATION_NONE;
  POINT pt = {};
  GetCursorPos(&pt);
  ScreenToClient(hWnd_, &pt);
  browser->GetHost()->DragSourceEndedAt(pt.x, pt.y, result);
  browser->GetHost()->DragSourceSystemDragEnded();
  return true;
#else
  // Cancel the drag. The dragging implementation requires ATL support.
  return false;
#endif
}

void OSRWindow::UpdateDragCursor(CefRefPtr<CefBrowser> browser,
                                 CefRenderHandler::DragOperation operation) {
#if defined(CEF_USE_ATL)
  current_drag_op_ = operation;
#endif
}

void OSRWindow::Invalidate() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::Bind(&OSRWindow::Invalidate, this));
    return;
  }

  // Don't post another task if the previous task is still pending.
  if (render_task_pending_)
    return;

  render_task_pending_ = true;

  // Render at 30fps.
  static const int kRenderDelay = 1000 / 30;
  CefPostDelayedTask(TID_UI, base::Bind(&OSRWindow::Render, this),
                     kRenderDelay);
}

void OSRWindow::WasHidden(bool hidden) {
  if (hidden == hidden_)
    return;
  CefRefPtr<CefBrowser> browser = browser_provider_->GetBrowser();
  if (!browser)
    return;
  browser->GetHost()->WasHidden(hidden);
  hidden_ = hidden;
}

#if defined(CEF_USE_ATL)

CefBrowserHost::DragOperationsMask
    OSRWindow::OnDragEnter(CefRefPtr<CefDragData> drag_data,
                           CefMouseEvent ev,
                           CefBrowserHost::DragOperationsMask effect) {
  browser_provider_->GetBrowser()->GetHost()->DragTargetDragEnter(
      drag_data, ev, effect);
  browser_provider_->GetBrowser()->GetHost()->DragTargetDragOver(ev, effect);
  return current_drag_op_;
}

CefBrowserHost::DragOperationsMask OSRWindow::OnDragOver(CefMouseEvent ev,
                              CefBrowserHost::DragOperationsMask effect) {
  browser_provider_->GetBrowser()->GetHost()->DragTargetDragOver(ev, effect);
  return current_drag_op_;
}

void OSRWindow::OnDragLeave() {
  browser_provider_->GetBrowser()->GetHost()->DragTargetDragLeave();
}

CefBrowserHost::DragOperationsMask
    OSRWindow::OnDrop(CefMouseEvent ev,
                      CefBrowserHost::DragOperationsMask effect) {
  browser_provider_->GetBrowser()->GetHost()->DragTargetDragOver(ev, effect);
  browser_provider_->GetBrowser()->GetHost()->DragTargetDrop(ev);
  return current_drag_op_;
}

#endif  // defined(CEF_USE_ATL)

OSRWindow::OSRWindow(OSRBrowserProvider* browser_provider,
                     bool transparent,
                     bool show_update_rect)
    : renderer_(transparent, show_update_rect),
      browser_provider_(browser_provider),
      hWnd_(NULL),
      hDC_(NULL),
      hRC_(NULL),
#if defined(CEF_USE_ATL)
      current_drag_op_(DRAG_OPERATION_NONE),
#endif
      painting_popup_(false),
      render_task_pending_(false),
      hidden_(false) {
}

OSRWindow::~OSRWindow() {
  DestroyWidget();
}

void OSRWindow::Render() {
  CEF_REQUIRE_UI_THREAD();
  if (render_task_pending_)
    render_task_pending_ = false;

  if (!hDC_)
    EnableGL();

  ScopedGLContext scoped_gl_context(hDC_, hRC_, true);
  renderer_.Render();
}

void OSRWindow::EnableGL() {
  CEF_REQUIRE_UI_THREAD();

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

  ScopedGLContext scoped_gl_context(hDC_, hRC_, false);
  renderer_.Initialize();
}

void OSRWindow::DisableGL() {
  CEF_REQUIRE_UI_THREAD();

  if (!hDC_)
    return;

  {
    ScopedGLContext scoped_gl_context(hDC_, hRC_, false);
    renderer_.Cleanup();
  }

  if (IsWindow(hWnd_)) {
    // wglDeleteContext will make the context not current before deleting it.
    BOOL result = wglDeleteContext(hRC_);
    ALLOW_UNUSED_LOCAL(result);
    DCHECK(result);
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

bool OSRWindow::IsOverPopupWidget(int x, int y) const {
  const CefRect& rc = renderer_.popup_rect();
  int popup_right = rc.x + rc.width;
  int popup_bottom = rc.y + rc.height;
  return (x >= rc.x) && (x < popup_right) &&
         (y >= rc.y) && (y < popup_bottom);
}

int OSRWindow::GetPopupXOffset() const {
  return renderer_.original_popup_rect().x - renderer_.popup_rect().x;
}

int OSRWindow::GetPopupYOffset() const {
  return renderer_.original_popup_rect().y - renderer_.popup_rect().y;
}

void OSRWindow::ApplyPopupOffset(int& x, int& y) const {
  if (IsOverPopupWidget(x, y)) {
    x += GetPopupXOffset();
    y += GetPopupYOffset();
  }
}

// Plugin window procedure.
// static
LRESULT CALLBACK OSRWindow::WndProc(HWND hWnd, UINT message,
                                    WPARAM wParam, LPARAM lParam) {
  static POINT lastMousePos, curMousePos;
  static bool mouseRotation = false;
  static bool mouseTracking = false;

  static int lastClickX = 0;
  static int lastClickY = 0;
  static CefBrowserHost::MouseButtonType lastClickButton = MBT_LEFT;
  static int gLastClickCount = 0;
  static double gLastClickTime = 0;

  static bool gLastMouseDownOnView = false;

  OSRWindow* window =
      reinterpret_cast<OSRWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
  CefRefPtr<CefBrowserHost> browser;
  if (window && window->browser_provider_->GetBrowser().get())
    browser = window->browser_provider_->GetBrowser()->GetHost();

  LONG currentTime = 0;
  bool cancelPreviousClick = false;

  if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
      message == WM_MBUTTONDOWN || message == WM_MOUSEMOVE ||
      message == WM_MOUSELEAVE) {
    currentTime = GetMessageTime();
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);
    cancelPreviousClick =
        (abs(lastClickX - x) > (GetSystemMetrics(SM_CXDOUBLECLK) / 2))
        || (abs(lastClickY - y) > (GetSystemMetrics(SM_CYDOUBLECLK) / 2))
        || ((currentTime - gLastClickTime) > GetDoubleClickTime());
    if (cancelPreviousClick &&
        (message == WM_MOUSEMOVE || message == WM_MOUSELEAVE)) {
      gLastClickCount = 0;
      lastClickX = 0;
      lastClickY = 0;
      gLastClickTime = 0;
    }
  }

  switch (message) {
  case WM_DESTROY:
    if (window)
      window->OnDestroyed();
    return 0;

  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN: {
    SetCapture(hWnd);
    SetFocus(hWnd);
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);
    if (wParam & MK_SHIFT) {
      // Start rotation effect.
      lastMousePos.x = curMousePos.x = x;
      lastMousePos.y = curMousePos.y = y;
      mouseRotation = true;
    } else {
      CefBrowserHost::MouseButtonType btnType =
          (message == WM_LBUTTONDOWN ? MBT_LEFT : (
           message == WM_RBUTTONDOWN ? MBT_RIGHT : MBT_MIDDLE));
      if (!cancelPreviousClick && (btnType == lastClickButton)) {
        ++gLastClickCount;
      } else {
        gLastClickCount = 1;
        lastClickX = x;
        lastClickY = y;
      }
      gLastClickTime = currentTime;
      lastClickButton = btnType;

      if (browser.get()) {
        CefMouseEvent mouse_event;
        mouse_event.x = x;
        mouse_event.y = y;
        gLastMouseDownOnView = !window->IsOverPopupWidget(x, y);
        window->ApplyPopupOffset(mouse_event.x, mouse_event.y);
        mouse_event.modifiers = GetCefMouseModifiers(wParam);
        browser->SendMouseClickEvent(mouse_event, btnType, false,
                                     gLastClickCount);
      }
    }
    break;
  }

  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
  case WM_MBUTTONUP:
    if (GetCapture() == hWnd)
      ReleaseCapture();
    if (mouseRotation) {
      // End rotation effect.
      mouseRotation = false;
      window->renderer_.SetSpin(0, 0);
      window->Invalidate();
    } else {
      int x = GET_X_LPARAM(lParam);
      int y = GET_Y_LPARAM(lParam);
      CefBrowserHost::MouseButtonType btnType =
          (message == WM_LBUTTONUP ? MBT_LEFT : (
           message == WM_RBUTTONUP ? MBT_RIGHT : MBT_MIDDLE));
      if (browser.get()) {
        CefMouseEvent mouse_event;
        mouse_event.x = x;
        mouse_event.y = y;
        if (gLastMouseDownOnView &&
            window->IsOverPopupWidget(x, y) &&
            (window->GetPopupXOffset() || window->GetPopupYOffset())) {
          break;
        }
        window->ApplyPopupOffset(mouse_event.x, mouse_event.y);
        mouse_event.modifiers = GetCefMouseModifiers(wParam);
        browser->SendMouseClickEvent(mouse_event, btnType, true,
                                     gLastClickCount);
      }
    }
    break;

  case WM_MOUSEMOVE: {
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);
    if (mouseRotation) {
      // Apply rotation effect.
      curMousePos.x = x;
      curMousePos.y = y;
      window->renderer_.IncrementSpin((curMousePos.x - lastMousePos.x),
        (curMousePos.y - lastMousePos.y));
      lastMousePos.x = curMousePos.x;
      lastMousePos.y = curMousePos.y;
      window->Invalidate();
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
        CefMouseEvent mouse_event;
        mouse_event.x = x;
        mouse_event.y = y;
        window->ApplyPopupOffset(mouse_event.x, mouse_event.y);
        mouse_event.modifiers = GetCefMouseModifiers(wParam);
        browser->SendMouseMoveEvent(mouse_event, false);
      }
    }
    break;
  }

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
    if (browser.get()) {
      // Determine the cursor position in screen coordinates.
      POINT p;
      ::GetCursorPos(&p);
      ::ScreenToClient(hWnd, &p);

      CefMouseEvent mouse_event;
      mouse_event.x = p.x;
      mouse_event.y = p.y;
      mouse_event.modifiers = GetCefMouseModifiers(wParam);
      browser->SendMouseMoveEvent(mouse_event, true);
    }
    break;

  case WM_MOUSEWHEEL:
    if (browser.get()) {
      POINT screen_point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      HWND scrolled_wnd = ::WindowFromPoint(screen_point);
      if (scrolled_wnd != hWnd) {
        break;
      }
      ScreenToClient(hWnd, &screen_point);
      int delta = GET_WHEEL_DELTA_WPARAM(wParam);

      CefMouseEvent mouse_event;
      mouse_event.x = screen_point.x;
      mouse_event.y = screen_point.y;
      window->ApplyPopupOffset(mouse_event.x, mouse_event.y);
      mouse_event.modifiers = GetCefMouseModifiers(wParam);

      browser->SendMouseWheelEvent(mouse_event,
                                   IsKeyDown(VK_SHIFT) ? delta : 0,
                                   !IsKeyDown(VK_SHIFT) ? delta : 0);
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
    event.modifiers = GetCefKeyboardModifiers(wParam, lParam);
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
    if (browser.get())
      browser->Invalidate(PET_VIEW);
    return 0;
  }

  case WM_ERASEBKGND:
    return 0;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}

}  // namespace client
