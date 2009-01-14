// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "webwidget_host.h"

#include "base/gfx/platform_canvas.h"
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/win_util.h"
#include "webkit/glue/webinputevent.h"
#include "webkit/glue/webwidget.h"

static const wchar_t kWindowClassName[] = L"WebWidgetHost";

/*static*/
WebWidgetHost* WebWidgetHost::Create(gfx::NativeWindow parent_window,
                                     WebWidgetDelegate* delegate) {
  WebWidgetHost* host = new WebWidgetHost();

  static bool registered_class = false;
  if (!registered_class) {
    WNDCLASSEX wcex = {0};
    wcex.cbSize        = sizeof(wcex);
    wcex.style         = CS_DBLCLKS;
    wcex.lpfnWndProc   = WebWidgetHost::WndProc;
    wcex.hInstance     = GetModuleHandle(NULL);
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = kWindowClassName;
    RegisterClassEx(&wcex);
    registered_class = true;
  }

  host->view_ = CreateWindowEx(WS_EX_TOOLWINDOW,
                               kWindowClassName, kWindowClassName, WS_POPUP,
                               0, 0, 0, 0,
                               parent_window, NULL, GetModuleHandle(NULL), NULL);

  win_util::SetWindowUserData(host->view_, host);

  host->webwidget_ = WebWidget::Create(delegate);

  return host;
}

/*static*/
WebWidgetHost* WebWidgetHost::FromWindow(gfx::NativeWindow hwnd) {
  return reinterpret_cast<WebWidgetHost*>(win_util::GetWindowUserData(hwnd));
}

/*static*/
LRESULT CALLBACK WebWidgetHost::WndProc(HWND hwnd, UINT message, WPARAM wparam,
                                        LPARAM lparam) {
  WebWidgetHost* host = FromWindow(hwnd);
  if (host && !host->WndProc(message, wparam, lparam)) {
    switch (message) {
      case WM_DESTROY:
        delete host;
        break;

      case WM_PAINT: {
        RECT rect;
        if (GetUpdateRect(hwnd, &rect, FALSE)) {
          host->UpdatePaintRect(gfx::Rect(rect));
        }
        host->Paint();
        return 0;
      }

      case WM_ERASEBKGND:
        // Do nothing here to avoid flashing, the background will be erased
        // during painting.
        return 0;

      case WM_SIZE:
        host->Resize(lparam);
        return 0;

      case WM_MOUSEMOVE:
      case WM_MOUSELEAVE:
      case WM_LBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_MBUTTONUP:
      case WM_RBUTTONUP:
      case WM_LBUTTONDBLCLK:
      case WM_MBUTTONDBLCLK:
      case WM_RBUTTONDBLCLK:
        host->MouseEvent(message, wparam, lparam);
        break;

      case WM_MOUSEWHEEL:
        host->WheelEvent(wparam, lparam);
        break;

      case WM_CAPTURECHANGED:
      case WM_CANCELMODE:
        host->CaptureLostEvent();
        break;

      // TODO(darin): add WM_SYSKEY{DOWN/UP} to capture ALT key actions
      case WM_KEYDOWN:
      case WM_KEYUP:
      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP:
      case WM_CHAR:
      case WM_SYSCHAR:
      case WM_IME_CHAR:
        host->KeyEvent(message, wparam, lparam);
        break;

      case WM_SETFOCUS:
        host->SetFocus(true);
        break;

      case WM_KILLFOCUS:
        host->SetFocus(false);
        break;
    }
  }

  return DefWindowProc(hwnd, message, wparam, lparam);;
}

void WebWidgetHost::DidInvalidateRect(const gfx::Rect& damaged_rect) {
  DLOG_IF(WARNING, painting_) << "unexpected invalidation while painting";

  // If this invalidate overlaps with a pending scroll, then we have to
  // downgrade to invalidating the scroll rect.
  if (damaged_rect.Intersects(scroll_rect_)) {
    paint_rect_ = paint_rect_.Union(scroll_rect_);
    ResetScrollRect();
  }
  paint_rect_ = paint_rect_.Union(damaged_rect);

  RECT r = damaged_rect.ToRECT();
  InvalidateRect(view_, &r, FALSE);
}

void WebWidgetHost::DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect) {
  DCHECK(dx || dy);

  // If we already have a pending scroll operation or if this scroll operation
  // intersects the existing paint region, then just failover to invalidating.
  if (!scroll_rect_.IsEmpty() || paint_rect_.Intersects(clip_rect)) {
    paint_rect_ = paint_rect_.Union(scroll_rect_);
    ResetScrollRect();
    paint_rect_ = paint_rect_.Union(clip_rect);
  }

  // We will perform scrolling lazily, when requested to actually paint.
  scroll_rect_ = clip_rect;
  scroll_dx_ = dx;
  scroll_dy_ = dy;

  RECT r = clip_rect.ToRECT();
  InvalidateRect(view_, &r, FALSE);
}

void WebWidgetHost::SetCursor(HCURSOR cursor) {
  SetClassLong(view_, GCL_HCURSOR,
      static_cast<LONG>(reinterpret_cast<LONG_PTR>(cursor)));
  ::SetCursor(cursor);
}

void WebWidgetHost::DiscardBackingStore() {
  canvas_.reset();
}

WebWidgetHost::WebWidgetHost()
    : view_(NULL),
      webwidget_(NULL),
      track_mouse_leave_(false),
      scroll_dx_(0),
      scroll_dy_(0) {
  set_painting(false);
}

WebWidgetHost::~WebWidgetHost() {
  win_util::SetWindowUserData(view_, 0);

  TrackMouseLeave(false);

  webwidget_->Close();
  webwidget_->Release();
}

bool WebWidgetHost::WndProc(UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
  case WM_ACTIVATE:
    if (wparam == WA_INACTIVE) {
      PostMessage(view_, WM_CLOSE, 0, 0);
      return true;
    }
    break;
  }

  return false;
}

void WebWidgetHost::UpdatePaintRect(const gfx::Rect& rect) {
  paint_rect_ = paint_rect_.Union(rect);
}

void WebWidgetHost::Paint() {
  RECT r;
  GetClientRect(view_, &r);
  gfx::Rect client_rect(r);
  
  // Allocate a canvas if necessary
  if (!canvas_.get()) {
    ResetScrollRect();
    paint_rect_ = client_rect;
    canvas_.reset(new skia::PlatformCanvas(
        paint_rect_.width(), paint_rect_.height(), true));
  }

  // This may result in more invalidation
  webwidget_->Layout();

  // Scroll the canvas if necessary
  scroll_rect_ = client_rect.Intersect(scroll_rect_);
  if (!scroll_rect_.IsEmpty()) {
    HDC hdc = canvas_->getTopPlatformDevice().getBitmapDC();

    RECT damaged_rect, r = scroll_rect_.ToRECT();
    ScrollDC(hdc, scroll_dx_, scroll_dy_, NULL, &r, NULL, &damaged_rect);

    PaintRect(gfx::Rect(damaged_rect));
  }
  ResetScrollRect();

  // Paint the canvas if necessary.  Allow painting to generate extra rects the
  // first time we call it.  This is necessary because some WebCore rendering
  // objects update their layout only when painted.
  for (int i = 0; i < 2; ++i) {
    paint_rect_ = client_rect.Intersect(paint_rect_);
    if (!paint_rect_.IsEmpty()) {
      gfx::Rect rect(paint_rect_);
      paint_rect_ = gfx::Rect();

      DLOG_IF(WARNING, i == 1) << "painting caused additional invalidations";
      PaintRect(rect);
    }
  }
  DCHECK(paint_rect_.IsEmpty());

  // Paint to the screen
  PAINTSTRUCT ps;
  BeginPaint(view_, &ps);
  canvas_->getTopPlatformDevice().drawToHDC(ps.hdc,
                                            ps.rcPaint.left,
                                            ps.rcPaint.top,
                                            &ps.rcPaint);
  EndPaint(view_, &ps);

  // Draw children
  UpdateWindow(view_);
}

void WebWidgetHost::Resize(LPARAM lparam) {
  // Force an entire re-paint.  TODO(darin): Maybe reuse this memory buffer.
  DiscardBackingStore();

  webwidget_->Resize(gfx::Size(LOWORD(lparam), HIWORD(lparam)));
}

void WebWidgetHost::MouseEvent(UINT message, WPARAM wparam, LPARAM lparam) {
  WebMouseEvent event(view_, message, wparam, lparam);
  switch (event.type) {
    case WebInputEvent::MOUSE_MOVE:
      TrackMouseLeave(true);
      break;
    case WebInputEvent::MOUSE_LEAVE:
      TrackMouseLeave(false);
      break;
    case WebInputEvent::MOUSE_DOWN:
      SetCapture(view_);
      break;
    case WebInputEvent::MOUSE_UP:
      if (GetCapture() == view_)
        ReleaseCapture();
      break;
  }
  webwidget_->HandleInputEvent(&event);

  if (event.type == WebInputEvent::MOUSE_DOWN) {
    // This mimics a temporary workaround in RenderWidgetHostViewWin 
    // for bug 765011 to get focus when the mouse is clicked. This 
    // happens after the mouse down event is sent to the renderer 
    // because normally Windows does a WM_SETFOCUS after WM_LBUTTONDOWN.
    ::SetFocus(view_);
  }
}

void WebWidgetHost::WheelEvent(WPARAM wparam, LPARAM lparam) {
  WebMouseWheelEvent event(view_, WM_MOUSEWHEEL, wparam, lparam);
  webwidget_->HandleInputEvent(&event);
}

void WebWidgetHost::KeyEvent(UINT message, WPARAM wparam, LPARAM lparam) {
  WebKeyboardEvent event(view_, message, wparam, lparam);
  webwidget_->HandleInputEvent(&event);
}

void WebWidgetHost::CaptureLostEvent() {
  webwidget_->MouseCaptureLost();
}

void WebWidgetHost::SetFocus(bool enable) {
  webwidget_->SetFocus(enable);
}

void WebWidgetHost::TrackMouseLeave(bool track) {
  if (track == track_mouse_leave_)
    return;
  track_mouse_leave_ = track;

  DCHECK(view_);

  TRACKMOUSEEVENT tme;
  tme.cbSize = sizeof(TRACKMOUSEEVENT);
  tme.dwFlags = TME_LEAVE;
  if (!track_mouse_leave_)
    tme.dwFlags |= TME_CANCEL;
  tme.hwndTrack = view_;

  TrackMouseEvent(&tme);
}

void WebWidgetHost::ResetScrollRect() {
  scroll_rect_ = gfx::Rect();
  scroll_dx_ = 0;
  scroll_dy_ = 0;
}

void WebWidgetHost::PaintRect(const gfx::Rect& rect) {
#ifndef NDEBUG
  DCHECK(!painting_);
#endif
  DCHECK(canvas_.get());

  set_painting(true);
  webwidget_->Paint(canvas_.get(), rect);
  set_painting(false);
}
