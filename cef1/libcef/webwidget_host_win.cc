// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <commctrl.h>

#include "libcef/webwidget_host.h"
#include "libcef/browser_impl.h"
#include "libcef/cef_thread.h"

#include "base/bind.h"
#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebInputEventFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebScreenInfoFactory.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/range/range.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/rect.h"

#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))

using webkit::npapi::WebPluginGeometry;
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

namespace {

struct MessageInfo {
  UINT message;
  WPARAM wParam;
  LPARAM lParam;
};

BOOL CALLBACK SendMessageFunc(HWND hwnd, LPARAM lParam) {
  MessageInfo* info = reinterpret_cast<MessageInfo*>(lParam);
  SendMessage(hwnd, info->message, info->wParam, info->lParam);
  return TRUE;
}

// Plugins are hosted in a Chromium-created parent window so it's necessary to
// send messages directly to the child window.
void SendMessageToPlugin(HWND hwnd, UINT message, WPARAM wParam,
                           LPARAM lParam) {
  MessageInfo info = {message, wParam, lParam};
  EnumChildWindows(hwnd, SendMessageFunc, reinterpret_cast<LPARAM>(&info));
}

inline SkIRect convertToSkiaRect(const gfx::Rect& r) {
  return SkIRect::MakeLTRB(r.x(), r.y(), r.right(), r.bottom());
}

}  // namespace

/*static*/
WebWidgetHost* WebWidgetHost::Create(HWND parent_view,
                                     WebWidgetClient* client,
                                     PaintDelegate* paint_delegate) {
  WebWidgetHost* host = new WebWidgetHost();

  if (!paint_delegate) {
    // Create a window for the host.
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
                                 parent_view, NULL, GetModuleHandle(NULL),
                                 NULL);

    ui::SetWindowUserData(host->view_, host);
  } else {
    host->paint_delegate_ = paint_delegate;
  }

  host->webwidget_ = WebPopupMenu::create(client);

  return host;
}

/*static*/
static WebWidgetHost* FromWindow(HWND view) {
  return reinterpret_cast<WebWidgetHost*>(ui::GetWindowUserData(view));
}

/*static*/
LRESULT CALLBACK WebWidgetHost::WndProc(HWND hwnd, UINT message, WPARAM wparam,
                                        LPARAM lparam) {
  WebWidgetHost* host = FromWindow(hwnd);
  if (host && !host->WndProc(message, wparam, lparam)) {
    switch (message) {
      case WM_PAINT: {
        // Paint to the window.
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
        // Finish the ongoing composition whenever a mouse click happens.
        // It matches IE's behavior.
        if (message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN ||
            message == WM_RBUTTONDOWN) {
          host->ime_input_.CleanupComposition(host->view_);
        }
        break;

      case WM_MOUSEWHEEL:
        {
          // Only send mouse wheel events if the cursor is over the window.
          POINT mousePt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
          RECT wndRect;
          GetWindowRect(hwnd, &wndRect);
          if (PtInRect(&wndRect, mousePt))
            host->WheelEvent(wparam, lparam);
        }
        break;

      case WM_MOUSEACTIVATE:
        if (host->popup()) {
          // Do not activate popup widgets on mouse click.
          return MA_NOACTIVATE;
        }
        break;

      case WM_CAPTURECHANGED:
      case WM_CANCELMODE:
        host->CaptureLostEvent();
        break;

      case WM_KEYDOWN:
      case WM_KEYUP:
      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP:
      case WM_CHAR:
      case WM_SYSCHAR:
        host->KeyEvent(message, wparam, lparam);
        break;

      // Necessary for text input of characters from east-asian languages. Do
      // not pass to DefWindowProc or characters will be displayed twice.
      case WM_IME_CHAR:
        host->KeyEvent(message, wparam, lparam);
        return 0;

      case WM_CREATE:
        // Call the WM_INPUTLANGCHANGE message handler to initialize
        // the input locale of a browser process.
        host->OnInputLangChange(0, 0);
        break;

      case WM_INPUTLANGCHANGE:
        host->OnInputLangChange(0, 0);
        break;

      case WM_IME_SETCONTEXT:
        {
          BOOL handled;
          LRESULT ime_retval = host->OnImeSetContext(message, wparam,
                                                     lparam, handled);
          if (handled)
            return ime_retval;
        }
        break;

      case WM_IME_STARTCOMPOSITION:
        {
          BOOL handled;
          LRESULT ime_retval = host->OnImeStartComposition(message, wparam,
                                                           lparam, handled);
          if (handled)
            return ime_retval;
        }
        break;

      case WM_IME_COMPOSITION:
        {
          BOOL handled;
          LRESULT ime_retval = host->OnImeComposition(message, wparam,
                                                      lparam, handled);
          if (handled)
            return ime_retval;
        }
        break;

      case WM_IME_ENDCOMPOSITION:
        {
          BOOL handled;
          LRESULT ime_retval = host->OnImeEndComposition(message, wparam,
                                                         lparam, handled);
          if (handled)
            return ime_retval;
        }
        break;

      case WM_SETFOCUS:
        host->SetFocus(true);
        break;

      case WM_KILLFOCUS:
        host->SetFocus(false);
        break;

      case WM_NOTIFY:
        host->OnNotify(0, reinterpret_cast<NMHDR*>(lparam));
        break;

      case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    }
  }

  return DefWindowProc(hwnd, message, wparam, lparam);
}

void WebWidgetHost::DidInvalidateRect(const gfx::Rect& damaged_rect) {
  DLOG_IF(WARNING, painting_) << "unexpected invalidation while painting";

  // If this invalidate overlaps with a pending scroll then we have to downgrade
  // to invalidating the scroll rect.
  UpdatePaintRect(damaged_rect);
  InvalidateRect(damaged_rect);

  if (!popup_ && view_ && webwidget_ && input_method_is_active_ &&
      !has_update_input_method_task_) {
    has_update_input_method_task_ = true;

    // Call UpdateInputMethod() approximately every 100ms.
    CefThread::PostDelayedTask(CefThread::UI, FROM_HERE,
        base::Bind(&WebWidgetHost::UpdateInputMethod,
            weak_factory_.GetWeakPtr()),
        100);
  }
}

void WebWidgetHost::DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect) {
  DCHECK(dx || dy);

  // Invalidate and re-paint the entire scroll rect if:
  // 1. Window rendering is disabled, or
  // 2. We're in a state where we cannot draw into the view right now, or
  // 3. The rect is being scrolled by more than the size of the view, or
  // 4. The scroll rect intersects the current paint region.
  if (!view_ || !canvas_.get() || layouting_ || painting_ ||
      abs(dx) >= clip_rect.width() || abs(dy) >= clip_rect.height() ||
      paint_rgn_.intersects(convertToSkiaRect(clip_rect))) {
    DidInvalidateRect(clip_rect);
    return;
  }

  // Scroll the canvas bitmap.
  {
    skia::ScopedPlatformPaint scoped_platform_paint(canvas_.get());
    HDC hdc = scoped_platform_paint.GetPlatformSurface();
    RECT clip_rect_win32 = clip_rect.ToRECT(), uncovered_rect = {0, 0, 0, 0};
    ScrollDC(hdc, dx, dy, NULL, &clip_rect_win32, NULL, &uncovered_rect);

    UpdatePaintRect(gfx::Rect(uncovered_rect));
  }

  // Invalidate the scroll rect. It will be drawn from the canvas bitmap on the
  // next WM_PAINT call.
  InvalidateRect(clip_rect);
}

void WebWidgetHost::Invalidate() {
  if (!webwidget_)
    return;
  WebSize size = webwidget_->size();
  InvalidateRect(gfx::Rect(0, 0, size.width, size.height));
}

void WebWidgetHost::SetCursor(HCURSOR cursor) {
  DCHECK(view_);
  SetClassLong(view_, GCL_HCURSOR,
      static_cast<LONG>(reinterpret_cast<LONG_PTR>(cursor)));
  ::SetCursor(cursor);
}

WebWidgetHost::WebWidgetHost()
    : view_(NULL),
      paint_delegate_(NULL),
      webwidget_(NULL),
      canvas_w_(0),
      canvas_h_(0),
      popup_(false),
      track_mouse_leave_(false),
      has_update_task_(false),
      has_invalidate_task_(false),
      has_update_input_method_task_(false),
      tooltip_view_(NULL),
      tooltip_showing_(false),
      ime_notification_(false),
      input_method_is_active_(false),
      layouting_(false),
      text_input_type_(WebKit::WebTextInputTypeNone),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  set_painting(false);
}

WebWidgetHost::~WebWidgetHost() {
  TrackMouseLeave(false);
  ResetTooltip();

  if (view_) {
    ui::SetWindowUserData(view_, 0);
    ui::SetWindowProc(view_, DefWindowProc);
    view_ = NULL;
  }
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

void WebWidgetHost::Paint() {
  int width, height;
  GetSize(width, height);
  gfx::Rect client_rect(width, height);

  // Damaged rectangle used for drawing when window rendering is disabled.
  SkRegion damaged_rgn;
  if (!view_ && !redraw_rect_.IsEmpty()) {
    // At a minimum we need to send the delegate the rectangle that was
    // requested by calling CefBrowser::InvalidateRect().
    damaged_rgn.setRect(convertToSkiaRect(redraw_rect_));
    redraw_rect_ = gfx::Rect();
  }

  if (view_ && !webwidget_->isAcceleratedCompositingActive()) {
    // Number of pixels that the canvas is allowed to differ from the client
    // area.
    const int kCanvasGrowSize = 128;

    if (!canvas_.get() ||
        canvas_w_ < client_rect.width() ||
        canvas_h_ < client_rect.height() ||
        canvas_w_ > client_rect.width() + kCanvasGrowSize * 2 ||
        canvas_h_ > client_rect.height() + kCanvasGrowSize * 2) {
      paint_rgn_.setRect(convertToSkiaRect(client_rect));

      // Resize the canvas to be within a reasonable size of the client area.
      canvas_w_ = client_rect.width() + kCanvasGrowSize;
      canvas_h_ = client_rect.height() + kCanvasGrowSize;
      canvas_.reset(new skia::PlatformCanvas(canvas_w_, canvas_h_, true));
    }
  } else if (!canvas_.get() || canvas_w_ != client_rect.width() ||
             canvas_h_ != client_rect.height()) {
    paint_rgn_.setRect(convertToSkiaRect(client_rect));

    // The canvas must be the exact size of the client area.
    canvas_w_ = client_rect.width();
    canvas_h_ = client_rect.height();
    canvas_.reset(new skia::PlatformCanvas(canvas_w_, canvas_h_, true));
  }

  webwidget_->animate(0.0);

  // This may result in more invalidation.
  layouting_ = true;
  webwidget_->layout();
  layouting_ = false;

  // Paint the canvas if necessary. Allow painting to generate extra rects the
  // first time we call it. This is necessary because some WebCore rendering
  // objects update their layout only when painted.
  for (int i = 0; i < 2; ++i) {
    SkRegion draw_rgn;
    draw_rgn.swap(paint_rgn_);

    // Draw each dirty rect in the region.
    SkRegion::Cliperator iterator(draw_rgn, convertToSkiaRect(client_rect));
    for (; !iterator.done(); iterator.next()) {
      const SkIRect& r = iterator.rect();
      gfx::Rect paint_rect =
          gfx::Rect(r.left(), r.top(), r.width(), r.height());
      PaintRect(paint_rect);

      if (!view_)
        damaged_rgn.op(convertToSkiaRect(paint_rect), SkRegion::kUnion_Op);
    }

    if (paint_rgn_.isEmpty())
      break;
  }

  DCHECK(paint_rgn_.isEmpty());

  if (!view_ && plugin_map_.size() > 0) {
    typedef std::list<const WebPluginGeometry*> PluginList;
    PluginList visible_plugins;

    // Identify the visible plugins.
    PluginMap::const_iterator it = plugin_map_.begin();
    for (; it != plugin_map_.end(); ++it) {
      if (it->second.visible && client_rect.Intersects(it->second.window_rect))
        visible_plugins.push_back(&it->second);
    }

    if (!visible_plugins.empty()) {
      skia::ScopedPlatformPaint scoped_platform_paint(canvas_.get());
      HDC drawDC = scoped_platform_paint.GetPlatformSurface();
      HRGN oldRGN, newRGN;
      POINT oldViewport;

      // Paint the plugin windows.
      PluginList::const_iterator it = visible_plugins.begin();
      for (; it != visible_plugins.end(); ++it) {
        const WebPluginGeometry* geom = *(it);

        oldRGN = CreateRectRgn(0, 0, 1, 1);
        GetClipRgn(drawDC, oldRGN);

        // Only paint inside the clip region.
        newRGN = CreateRectRgn(geom->clip_rect.x(),
                               geom->clip_rect.y(),
                               geom->clip_rect.right(),
                               geom->clip_rect.bottom());
        gfx::SubtractRectanglesFromRegion(newRGN, geom->cutout_rects);
        OffsetRgn(newRGN, geom->window_rect.x(), geom->window_rect.y());
        SelectClipRgn(drawDC, newRGN);

        // Change the viewport origin to the plugin window origin.
        SetViewportOrgEx(drawDC, geom->window_rect.x(), geom->window_rect.y(),
                         &oldViewport);

        SendMessageToPlugin(geom->window, WM_PRINT,
            reinterpret_cast<WPARAM>(drawDC),
            PRF_OWNED | PRF_ERASEBKGND | PRF_CLIENT | PRF_NONCLIENT);

        SetViewportOrgEx(drawDC, oldViewport.x, oldViewport.y, NULL);
        SelectClipRgn(drawDC, oldRGN);

        if (!view_) {
          damaged_rgn.op(convertToSkiaRect(geom->window_rect),
              SkRegion::kUnion_Op);
        }

        DeleteObject(oldRGN);
        DeleteObject(newRGN);
      }
    }
  }

  if (view_) {
    // Paint to the window.
    PAINTSTRUCT ps;
    BeginPaint(view_, &ps);
    skia::DrawToNativeContext(canvas_.get(), ps.hdc, ps.rcPaint.left,
                              ps.rcPaint.top, &ps.rcPaint);
    EndPaint(view_, &ps);

    // Draw children
    UpdateWindow(view_);
  } else {
    // Paint to the delegate.
    DCHECK(paint_delegate_);
    const SkBitmap& bitmap = canvas_->getDevice()->accessBitmap(false);
    DCHECK(bitmap.config() == SkBitmap::kARGB_8888_Config);
    const void* pixels = bitmap.getPixels();

    std::vector<CefRect> damaged_rects;
    SkRegion::Cliperator iterator(damaged_rgn, convertToSkiaRect(client_rect));
    for (; !iterator.done(); iterator.next()) {
      const SkIRect& r = iterator.rect();
      damaged_rects.push_back(
          CefRect(r.left(), r.top(), r.width(), r.height()));
    }

    paint_delegate_->Paint(popup_, damaged_rects, pixels);
  }

  // Used with scheduled invalidation to maintain a consistent frame rate.
  paint_last_call_ = base::TimeTicks::Now();
  if (has_invalidate_task_)
    has_invalidate_task_ = false;
}

void WebWidgetHost::InvalidateRect(const gfx::Rect& rect) {
  if (rect.IsEmpty())
    return;

  if (view_) {
    // Let the window handle painting.
    RECT r = rect.ToRECT();
    ::InvalidateRect(view_, &r, FALSE);
  } else {
    // Don't post a paint task if this invalidation occurred during layout or if
    // a paint task is already pending. Paint() will be called by DoPaint().
    if (!layouting_ && !has_update_task_) {
      has_update_task_ = true;
      CefThread::PostTask(CefThread::UI, FROM_HERE,
          base::Bind(&WebWidgetHost::DoPaint, weak_factory_.GetWeakPtr()));
    }
  }
}

bool WebWidgetHost::GetImage(int width, int height, void* buffer) {
  const SkBitmap& bitmap = canvas_->getDevice()->accessBitmap(false);
  DCHECK(bitmap.config() == SkBitmap::kARGB_8888_Config);

  if (width == canvas_->getDevice()->width() &&
      height == canvas_->getDevice()->height()) {
    // The specified width and height values are the same as the canvas size.
    // Return the existing canvas contents.
    const void* pixels = bitmap.getPixels();
    memcpy(buffer, pixels, width * height * 4);
    return true;
  }

  // Create a new canvas of the requested size.
  scoped_ptr<skia::PlatformCanvas> new_canvas(
      new skia::PlatformCanvas(width, height, true));

  new_canvas->writePixels(bitmap, 0, 0);
  const SkBitmap& new_bitmap = new_canvas->getDevice()->accessBitmap(false);
  DCHECK(new_bitmap.config() == SkBitmap::kARGB_8888_Config);

  // Return the new canvas contents.
  const void* pixels = new_bitmap.getPixels();
  memcpy(buffer, pixels, width * height * 4);
  return true;
}

WebScreenInfo WebWidgetHost::GetScreenInfo() {
  return WebScreenInfoFactory::screenInfo(view_);
}

void WebWidgetHost::Resize(LPARAM lparam) {
  SetSize(LOWORD(lparam), HIWORD(lparam));
}

void WebWidgetHost::MouseEvent(UINT message, WPARAM wparam, LPARAM lparam) {
  const WebMouseEvent& event = WebInputEventFactory::mouseEvent(
      view_, message, wparam, lparam);
  switch (event.type) {
    case WebInputEvent::MouseMove:
      TrackMouseLeave(true);
      break;
    case WebInputEvent::MouseLeave:
      TrackMouseLeave(false);
      break;
    case WebInputEvent::MouseDown:
      if (!popup()) {
        SetCapture(view_);

        if (::GetFocus() != view_) {
          // Set focus to this  window.
          HWND parent_hwnd = ::GetParent(view_);
          if (parent_hwnd) {
            CefRefPtr<CefBrowserImpl> browser =
                static_cast<CefBrowserImpl*>(
                    ui::GetWindowUserData(parent_hwnd));
            if (browser.get()) {
              // This mimics a temporary workaround in RenderWidgetHostViewWin
              // for bug 765011 to get focus when the mouse is clicked. This
              // happens after the mouse down event is sent to the renderer
              // because normally Windows does a WM_SETFOCUS after
              // WM_LBUTTONDOWN.
              browser->SetFocus(true);
            }
          }
        }
      }
      break;
    case WebInputEvent::MouseUp:
      if (!popup()) {
        if (GetCapture() == view_)
          ReleaseCapture();
      }
      break;
  }
  webwidget_->handleInputEvent(event);
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

void WebWidgetHost::SetTooltipText(const CefString& tooltip_text) {
  if (!view_)
    return;

  std::wstring new_tooltip_text(tooltip_text);
  if (new_tooltip_text != tooltip_text_) {
    tooltip_text_ = new_tooltip_text;

    // Need to check if the tooltip is already showing so that we don't
    // immediately show the tooltip with no delay when we move the mouse from
    // a region with no tooltip to a region with a tooltip.
    if (::IsWindow(tooltip_view_) && tooltip_showing_) {
      ::SendMessage(tooltip_view_, TTM_POP, 0, 0);
      ::SendMessage(tooltip_view_, TTM_POPUP, 0, 0);
    }
  } else {
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
  if (!view_)
    return;

  UINT message = TTM_NEWTOOLRECT;

  TOOLINFO ti;
  ti.cbSize = sizeof(ti);
  ti.hwnd = view_;
  ti.uId = 0;
  if (!::IsWindow(tooltip_view_)) {
    message = TTM_ADDTOOL;
    tooltip_view_ = CreateWindowEx(
        WS_EX_TRANSPARENT,
        TOOLTIPS_CLASS, L"tooltip_view_", TTS_NOPREFIX, 0, 0, 0, 0,
        view_, NULL,
        NULL, NULL);
    ti.uFlags = TTF_SUBCLASS;
    ti.lpszText = LPSTR_TEXTCALLBACK;
  }

  GetClientRect(view_, &ti.rect);
  SendMessage(tooltip_view_, message, NULL, reinterpret_cast<LPARAM>(&ti));
}

void WebWidgetHost::ResetTooltip() {
  if (!view_)
    return;

  if (::IsWindow(tooltip_view_))
    ::DestroyWindow(tooltip_view_);
  tooltip_view_ = NULL;
}

void WebWidgetHost::TrackMouseLeave(bool track) {
  if (!view_)
    return;

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

void WebWidgetHost::PaintRect(const gfx::Rect& rect) {
#ifndef NDEBUG
  DCHECK(!painting_);
#endif
  DCHECK(canvas_.get());

  if (!popup() && ((WebKit::WebView*)webwidget_)->isTransparent()) {
    // When using transparency mode clear the rectangle before painting.
    SkPaint clearpaint;
    clearpaint.setARGB(0, 0, 0, 0);
    clearpaint.setXfermodeMode(SkXfermode::kClear_Mode);

    SkRect skrc;
    skrc.set(rect.x(), rect.y(), rect.right(), rect.bottom());
    canvas_->drawRect(skrc, clearpaint);
  }

  set_painting(true);
  webwidget_->paint(canvas_.get(), rect);
  set_painting(false);
}

void WebWidgetHost::SendKeyEvent(cef_key_type_t type,
                                 const cef_key_info_t& keyInfo,
                                 int modifiers) {
  UINT message = 0;
  WPARAM wparam = keyInfo.key;
  LPARAM lparam = modifiers;

  if (type == KT_KEYUP) {
    if (keyInfo.sysChar)
      message = WM_SYSKEYUP;
    else if (keyInfo.imeChar)
      message = WM_IME_KEYUP;
    else
      message = WM_KEYUP;
  } else if (type == KT_KEYDOWN) {
    if (keyInfo.sysChar)
      message = WM_SYSKEYDOWN;
    else if (keyInfo.imeChar)
      message = WM_IME_KEYDOWN;
    else
      message = WM_KEYDOWN;
  } else if (type == KT_CHAR) {
    if (keyInfo.sysChar)
      message = WM_SYSCHAR;
    else if (keyInfo.imeChar)
      message = WM_IME_CHAR;
    else
      message = WM_CHAR;
  }

  if (message == 0) {
    NOTREACHED();
    return;
  }

  const WebKeyboardEvent& event = WebInputEventFactory::keyboardEvent(
      NULL, message, wparam, lparam);
  last_key_event_ = event;

  webwidget_->handleInputEvent(event);
}

void WebWidgetHost::SendMouseClickEvent(int x, int y,
                                        cef_mouse_button_type_t type,
                                        bool mouseUp, int clickCount) {
  DCHECK(clickCount >=1 && clickCount <= 2);

  UINT message = 0;
  WPARAM wparam = 0;
  LPARAM lparam = MAKELPARAM(x, y);

  if (type == MBT_LEFT) {
    if (mouseUp)
      message = (clickCount == 1?WM_LBUTTONUP:WM_LBUTTONDBLCLK);
    else
      message = WM_LBUTTONDOWN;
  } else if (type == MBT_MIDDLE) {
    if (mouseUp)
      message = (clickCount == 1?WM_MBUTTONUP:WM_MBUTTONDBLCLK);
    else
      message = WM_MBUTTONDOWN;
  }  else if (type == MBT_RIGHT) {
    if (mouseUp)
      message = (clickCount == 1?WM_RBUTTONUP:WM_RBUTTONDBLCLK);
    else
      message = WM_RBUTTONDOWN;
  }

  if (message == 0) {
    NOTREACHED();
    return;
  }

  if (GetKeyState(VK_CONTROL) & 0x8000)
    wparam |= MK_CONTROL;
  if (GetKeyState(VK_SHIFT) & 0x8000)
    wparam |= MK_SHIFT;
  if (GetKeyState(VK_LBUTTON) & 0x8000)
    wparam |= MK_LBUTTON;
  if (GetKeyState(VK_MBUTTON) & 0x8000)
    wparam |= MK_MBUTTON;
  if (GetKeyState(VK_RBUTTON) & 0x8000)
    wparam |= MK_RBUTTON;

  gfx::PluginWindowHandle plugin = GetWindowedPluginAt(x, y);
  if (plugin) {
    SendMessageToPlugin(plugin, message, wparam, lparam);
  } else {
    const WebMouseEvent& event = WebInputEventFactory::mouseEvent(NULL, message,
        wparam, lparam);
    webwidget_->handleInputEvent(event);
  }
}

void WebWidgetHost::SendMouseMoveEvent(int x, int y, bool mouseLeave) {
  UINT message;
  WPARAM wparam = 0;
  LPARAM lparam = 0;

  if (mouseLeave) {
    message = WM_MOUSELEAVE;
  } else {
    message = WM_MOUSEMOVE;
    lparam = MAKELPARAM(x, y);
  }

  if (GetKeyState(VK_CONTROL) & 0x8000)
    wparam |= MK_CONTROL;
  if (GetKeyState(VK_SHIFT) & 0x8000)
    wparam |= MK_SHIFT;
  if (GetKeyState(VK_LBUTTON) & 0x8000)
    wparam |= MK_LBUTTON;
  if (GetKeyState(VK_MBUTTON) & 0x8000)
    wparam |= MK_MBUTTON;
  if (GetKeyState(VK_RBUTTON) & 0x8000)
    wparam |= MK_RBUTTON;

  gfx::PluginWindowHandle plugin = GetWindowedPluginAt(x, y);
  if (plugin) {
    SendMessageToPlugin(plugin, message, wparam, lparam);
  } else {
    const WebMouseEvent& event = WebInputEventFactory::mouseEvent(NULL, message,
      wparam, lparam);
    webwidget_->handleInputEvent(event);
  }
}

void WebWidgetHost::SendMouseWheelEvent(int x, int y, int deltaX, int deltaY) {
  WPARAM wparam = MAKEWPARAM(0, deltaY);
  LPARAM lparam = MAKELPARAM(x, y);

  if (GetKeyState(VK_CONTROL) & 0x8000)
    wparam |= MK_CONTROL;
  if (GetKeyState(VK_SHIFT) & 0x8000)
    wparam |= MK_SHIFT;
  if (GetKeyState(VK_LBUTTON) & 0x8000)
    wparam |= MK_LBUTTON;
  if (GetKeyState(VK_MBUTTON) & 0x8000)
    wparam |= MK_MBUTTON;
  if (GetKeyState(VK_RBUTTON) & 0x8000)
    wparam |= MK_RBUTTON;

  gfx::PluginWindowHandle plugin = GetWindowedPluginAt(x, y);
  if (plugin) {
    SendMessageToPlugin(plugin, WM_MOUSEWHEEL, wparam, lparam);
  } else {
    const WebMouseWheelEvent& event = WebInputEventFactory::mouseWheelEvent(
        NULL, WM_MOUSEWHEEL, wparam, lparam);
    webwidget_->handleInputEvent(event);
  }
}

void WebWidgetHost::SendFocusEvent(bool setFocus) {
  SetFocus(setFocus);
}

void WebWidgetHost::SendCaptureLostEvent() {
  CaptureLostEvent();
}

LRESULT WebWidgetHost::OnImeSetContext(UINT message, WPARAM wparam,
                                       LPARAM lparam, BOOL& handled) {
  if (!webwidget_)
    return 0;

  // We need status messages about the focused input control from a
  // renderer process when:
  //   * the current input context has IMEs, and;
  //   * an application is activated.
  // This seems to tell we should also check if the current input context has
  // IMEs before sending a request, however, this WM_IME_SETCONTEXT is
  // fortunately sent to an application only while the input context has IMEs.
  // Therefore, we just start/stop status messages according to the activation
  // status of this application without checks.
  bool activated = (wparam == TRUE);
  if (webwidget_) {
    input_method_is_active_ = activated;
    ime_notification_ = activated;
  }

  if (ime_notification_)
    ime_input_.CreateImeWindow(view_);

  ime_input_.CleanupComposition(view_);
  ime_input_.SetImeWindowStyle(view_, message, wparam, lparam, &handled);
  return 0;
}

LRESULT WebWidgetHost::OnImeStartComposition(UINT message, WPARAM wparam,
                                             LPARAM lparam, BOOL& handled) {
  if (!webwidget_)
    return 0;

  // Reset the composition status and create IME windows.
  ime_input_.CreateImeWindow(view_);
  ime_input_.ResetComposition(view_);
  // We have to prevent WTL from calling ::DefWindowProc() because the function
  // calls ::ImmSetCompositionWindow() and ::ImmSetCandidateWindow() to
  // over-write the position of IME windows.
  handled = TRUE;
  return 0;
}

LRESULT WebWidgetHost::OnImeComposition(UINT message, WPARAM wparam,
                                        LPARAM lparam, BOOL& handled) {
  if (!webwidget_)
    return 0;

  // At first, update the position of the IME window.
  ime_input_.UpdateImeWindow(view_);

  // ui::CompositionUnderline should be identical to
  // WebKit::WebCompositionUnderline, so that we can do reinterpret_cast safely.
  COMPILE_ASSERT(sizeof(ui::CompositionUnderline) ==
                 sizeof(WebKit::WebCompositionUnderline),
                 ui_CompositionUnderline__WebKit_WebCompositionUnderline_diff);

  // Retrieve the result string and its attributes of the ongoing composition
  // and send it to a renderer process.
  ui::CompositionText composition;
  if (ime_input_.GetResult(view_, lparam, &composition.text)) {
    webwidget_->setComposition(composition.text,
                               std::vector<WebKit::WebCompositionUnderline>(),
                               0, 0);
    webwidget_->confirmComposition();
    ime_input_.ResetComposition(view_);
    // Fall though and try reading the composition string.
    // Japanese IMEs send a message containing both GCS_RESULTSTR and
    // GCS_COMPSTR, which means an ongoing composition has been finished
    // by the start of another composition.
  }
  // Retrieve the composition string and its attributes of the ongoing
  // composition and send it to a renderer process.
  if (ime_input_.GetComposition(view_, lparam, &composition)) {
    // TODO(suzhe): due to a bug of webkit, we can't use selection range with
    // composition string. See: https://bugs.webkit.org/show_bug.cgi?id=37788
    composition.selection = ui::Range(composition.selection.end());

    // TODO(suzhe): convert both renderer_host and renderer to use
    // ui::CompositionText.
    const std::vector<WebKit::WebCompositionUnderline>& underlines =
        reinterpret_cast<const std::vector<WebKit::WebCompositionUnderline>&>(
            composition.underlines);
    webwidget_->setComposition(
        composition.text, underlines,
        composition.selection.start(), composition.selection.end());
  }
  // We have to prevent WTL from calling ::DefWindowProc() because we do not
  // want for the IMM (Input Method Manager) to send WM_IME_CHAR messages.
  handled = TRUE;
  return 0;
}

LRESULT WebWidgetHost::OnImeEndComposition(UINT message, WPARAM wparam,
                                           LPARAM lparam, BOOL& handled) {
  if (!webwidget_)
    return 0;

  if (ime_input_.is_composing()) {
    // A composition has been ended while there is an ongoing composition,
    // i.e. the ongoing composition has been canceled.
    // We need to reset the composition status both of the ImeInput object and
    // of the renderer process.
    ime_input_.CancelIME(view_);
    ime_input_.ResetComposition(view_);
  }
  ime_input_.DestroyImeWindow(view_);
  // Let WTL call ::DefWindowProc() and release its resources.
  handled = FALSE;
  return 0;
}

void WebWidgetHost::OnInputLangChange(DWORD character_set,
                                      HKL input_language_id) {
  // Send the given Locale ID to the ImeInput object and retrieves whether
  // or not the current input context has IMEs.
  // If the current input context has IMEs, a browser process has to send a
  // request to a renderer process that it needs status messages about
  // the focused edit control from the renderer process.
  // On the other hand, if the current input context does not have IMEs, the
  // browser process also has to send a request to the renderer process that
  // it does not need the status messages any longer.
  // To minimize the number of this notification request, we should check if
  // the browser process is actually retrieving the status messages (this
  // state is stored in ime_notification_) and send a request only if the
  // browser process has to update this status, its details are listed below:
  // * If a browser process is not retrieving the status messages,
  //   (i.e. ime_notification_ == false),
  //   send this request only if the input context does have IMEs,
  //   (i.e. ime_status == true);
  //   When it successfully sends the request, toggle its notification status,
  //   (i.e.ime_notification_ = !ime_notification_ = true).
  // * If a browser process is retrieving the status messages
  //   (i.e. ime_notification_ == true),
  //   send this request only if the input context does not have IMEs,
  //   (i.e. ime_status == false).
  //   When it successfully sends the request, toggle its notification status,
  //   (i.e.ime_notification_ = !ime_notification_ = false).
  // To analyze the above actions, we can optimize them into the ones
  // listed below:
  // 1 Sending a request only if ime_status_ != ime_notification_, and;
  // 2 Copying ime_status to ime_notification_ if it sends the request
  //   successfully (because Action 1 shows ime_status = !ime_notification_.)
  bool ime_status = ime_input_.SetInputLanguage();
  if (ime_status != ime_notification_ && webwidget_) {
    input_method_is_active_ = ime_status;
    ime_notification_ = ime_status;
  }
}

void WebWidgetHost::ImeUpdateTextInputState(WebKit::WebTextInputType type,
                                            const gfx::Rect& caret_rect) {
  if (text_input_type_ != type) {
    text_input_type_ = type;
    if (type == WebKit::WebTextInputTypeText)
      ime_input_.EnableIME(view_);
    else
      ime_input_.DisableIME(view_);
  }

  // Only update caret position if the input method is enabled.
  if (type == WebKit::WebTextInputTypeText)
    ime_input_.UpdateCaretRect(view_, caret_rect);
}

void WebWidgetHost::UpdateInputMethod() {
  REQUIRE_UIT();

  has_update_input_method_task_ = false;

  if (!input_method_is_active_ || !webwidget_)
    return;

  WebKit::WebTextInputType new_type = webwidget_->textInputType();
  WebKit::WebRect new_caret_bounds;

  WebKit::WebRect startRect, endRect;
  if (webwidget_->selectionBounds(startRect, endRect))
    new_caret_bounds = endRect;

  // Only sends text input type and caret bounds to the browser process if they
  // are changed.
  if (text_input_type_ != new_type || caret_bounds_ != new_caret_bounds) {
    text_input_type_ = new_type;
    caret_bounds_ = new_caret_bounds;
    ImeUpdateTextInputState(new_type, new_caret_bounds);
  }
}
