// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webwidget_host.h"

#include "gfx/rect.h"
#include "base/logging.h"
#include "base/win_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/win/WebInputEventFactory.h"
#include "third_party/WebKit/WebKit/chromium/public/win/WebScreenInfoFactory.h"

#include <commctrl.h>

using WebKit::WebInputEvent;
using WebKit::WebInputEventFactory;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebPopupMenu;
using WebKit::WebScreenInfo;
using WebKit::WebScreenInfoFactory;
using WebKit::WebSize;
using WebKit::WebWidget;
using WebKit::WebWidgetClient;

static const wchar_t kWindowClassName[] = L"WebWidgetHost";

/*static*/
WebWidgetHost* WebWidgetHost::Create(HWND parent_view,
                                     WebWidgetClient* client) {
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
                               parent_view, NULL, GetModuleHandle(NULL), NULL);

  win_util::SetWindowUserData(host->view_, host);

  host->webwidget_ = WebPopupMenu::create(client);

  return host;
}

/*static*/
static WebWidgetHost* FromWindow(HWND view) {
  return reinterpret_cast<WebWidgetHost*>(win_util::GetWindowUserData(view));
}

/*static*/
LRESULT CALLBACK WebWidgetHost::WndProc(HWND hwnd, UINT message, WPARAM wparam,
                                        LPARAM lparam) {
  WebWidgetHost* host = FromWindow(hwnd);
  if (host && !host->WndProc(message, wparam, lparam)) {
    switch (message) {
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
        host->KeyEvent(message, wparam, lparam);
        break;

      case WM_SETFOCUS:
        host->SetFocus(true);
        break;

      case WM_KILLFOCUS:
        host->SetFocus(false);
        break;

      case WM_NOTIFY:
        host->OnNotify(0, (NMHDR*)lparam);
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
  if (dx != 0 && dy != 0) {
    // We only support uni-directional scroll
    DidScrollRect(0, dy, clip_rect);
    dy = 0;
  }

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

void WebWidgetHost::ScheduleComposite() {
  if (!webwidget_)
    return;
  WebSize size = webwidget_->size();
  gfx::Rect rect(0, 0, size.width, size.height);
  RECT r = rect.ToRECT();
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
      scroll_dy_(0),
      tooltip_view_(NULL),
      tooltip_showing_(false) {
  set_painting(false);
}

WebWidgetHost::~WebWidgetHost() {
  win_util::SetWindowUserData(view_, 0);

  TrackMouseLeave(false);
  ResetTooltip();
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
  webwidget_->layout();

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

WebScreenInfo WebWidgetHost::GetScreenInfo() {
  return WebScreenInfoFactory::screenInfo(view_);
}

void WebWidgetHost::Resize(LPARAM lparam) {
  // Force an entire re-paint.  TODO(darin): Maybe reuse this memory buffer.
  DiscardBackingStore();

  webwidget_->resize(WebSize(LOWORD(lparam), HIWORD(lparam)));
  EnsureTooltip();
}

void WebWidgetHost::MouseEvent(UINT message, WPARAM wparam, LPARAM lparam) {
  const WebMouseEvent& event = WebInputEventFactory::mouseEvent(
      view_, message, wparam, lparam);
  webwidget_->handleInputEvent(event);
  switch (event.type) {
    case WebInputEvent::MouseMove:
      TrackMouseLeave(true);
      break;
    case WebInputEvent::MouseLeave:
      TrackMouseLeave(false);
      break;
    case WebInputEvent::MouseDown:
      SetCapture(view_);
      // This mimics a temporary workaround in RenderWidgetHostViewWin 
      // for bug 765011 to get focus when the mouse is clicked. This 
      // happens after the mouse down event is sent to the renderer 
      // because normally Windows does a WM_SETFOCUS after WM_LBUTTONDOWN.
      ::SetFocus(view_);
      break;
    case WebInputEvent::MouseUp:
      if (GetCapture() == view_)
        ReleaseCapture();
      break;
  }
}

void WebWidgetHost::WheelEvent(WPARAM wparam, LPARAM lparam) {
  const WebMouseWheelEvent& event = WebInputEventFactory::mouseWheelEvent(
      view_, WM_MOUSEWHEEL, wparam, lparam);
  webwidget_->handleInputEvent(event);
}

void WebWidgetHost::KeyEvent(UINT message, WPARAM wparam, LPARAM lparam) {
  const WebKeyboardEvent& event = WebInputEventFactory::keyboardEvent(
      view_, message, wparam, lparam);
  last_key_event_ = event;
  webwidget_->handleInputEvent(event);
}

void WebWidgetHost::CaptureLostEvent() {
  webwidget_->mouseCaptureLost();
}

void WebWidgetHost::SetFocus(bool enable) {
  webwidget_->setFocus(enable);
}

void WebWidgetHost::OnNotify(WPARAM wparam, NMHDR* header) {
  if (tooltip_view_ == NULL)
    return;

  switch (header->code) {
    case TTN_GETDISPINFO: 
      {
        NMTTDISPINFOW* tooltip_info = reinterpret_cast<NMTTDISPINFOW*>(header);
        tooltip_info->szText[0] = L'\0';
        tooltip_info->lpszText = const_cast<wchar_t*>(tooltip_text_.c_str());
        ::SendMessage(tooltip_view_, TTM_SETMAXTIPWIDTH, 0, 1024);
      }
      break;

    case TTN_POP:
      tooltip_showing_ = false;
      break;

    case TTN_SHOW:
      tooltip_showing_ = true;
      break;
  }
}

void WebWidgetHost::SetTooltipText(const std::wstring& new_tooltip_text) {
  if (new_tooltip_text != tooltip_text_) {
    tooltip_text_ = new_tooltip_text;

    // Need to check if the tooltip is already showing so that we don't
    // immediately show the tooltip with no delay when we move the mouse from
    // a region with no tooltip to a region with a tooltip.
    if (::IsWindow(tooltip_view_) && tooltip_showing_) {
      ::SendMessage(tooltip_view_, TTM_POP, 0, 0);
      ::SendMessage(tooltip_view_, TTM_POPUP, 0, 0);
    }
  }
  else {
    // Make sure the tooltip gets closed after TTN_POP gets sent. For some
    // reason this doesn't happen automatically, so moving the mouse around
    // within the same link/image/etc doesn't cause the tooltip to re-appear.
    if (!tooltip_showing_) {
      if (::IsWindow(tooltip_view_))
        ::SendMessage(tooltip_view_, TTM_POP, 0, 0);
    }
  }
}

void WebWidgetHost::EnsureTooltip() {
  UINT message = TTM_NEWTOOLRECT;

  TOOLINFO ti;
  ti.cbSize = sizeof(ti);
  ti.hwnd = view_handle();
  ti.uId = 0;
  if (!::IsWindow(tooltip_view_)) {
    message = TTM_ADDTOOL;
    tooltip_view_ = CreateWindowEx(
        WS_EX_TRANSPARENT,
        TOOLTIPS_CLASS, L"tooltip_view_", TTS_NOPREFIX, 0, 0, 0, 0, view_handle(), NULL,
        NULL, NULL);
    ti.uFlags = TTF_SUBCLASS;
    ti.lpszText = LPSTR_TEXTCALLBACK;
  }

  GetClientRect(view_handle(), &ti.rect);
  SendMessage(tooltip_view_, message, NULL, reinterpret_cast<LPARAM>(&ti));
}

void WebWidgetHost::ResetTooltip() {
  if (::IsWindow(tooltip_view_))
    ::DestroyWindow(tooltip_view_);
  tooltip_view_ = NULL;
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
  webwidget_->paint(canvas_.get(), rect);
  set_painting(false);
}
