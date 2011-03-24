// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webwidget_host.h"
#include "cef_thread.h"

#include "ui/gfx/rect.h"
#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebInputEventFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebScreenInfoFactory.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/gdi_util.h"

#include <commctrl.h>

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

BOOL CALLBACK SendMessageFunc(HWND hwnd, LPARAM lParam)
{
  MessageInfo* info = reinterpret_cast<MessageInfo*>(lParam);
  SendMessage(hwnd, info->message, info->wParam, info->lParam);
  return TRUE;
}

// Plugins are hosted in a Chromium-created parent window so it's necessary to
// send messages directly to the child window.
void SendMessageToPlugin(HWND hwnd, UINT message, WPARAM wParam,
                           LPARAM lParam)
{
  MessageInfo info = {message, wParam, lParam};
  EnumChildWindows(hwnd, SendMessageFunc, reinterpret_cast<LPARAM>(&info));
}

} // namespace

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

  InvalidateRect(gfx::Rect(damaged_rect));
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

  InvalidateRect(clip_rect);
}

void WebWidgetHost::ScheduleComposite() {
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
      popup_(false),
      track_mouse_leave_(false),
      scroll_dx_(0),
      scroll_dy_(0),
      update_task_(NULL),
      tooltip_view_(NULL),
      tooltip_showing_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
  set_painting(false);
}

WebWidgetHost::~WebWidgetHost() {
  if (view_)
    ui::SetWindowUserData(view_, 0);

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

void WebWidgetHost::Paint() {
  if (canvas_.get() && paint_rect_.IsEmpty())
    return;

  int width, height;
  GetSize(width, height);
  gfx::Rect client_rect(width, height);
  gfx::Rect damaged_rect;

  // Allocate a canvas if necessary
  if (!canvas_.get()) {
    ResetScrollRect();
    paint_rect_ = client_rect;
    canvas_.reset(new skia::PlatformCanvas(
        paint_rect_.width(), paint_rect_.height(), true));
  }

  webwidget_->animate();

  // This may result in more invalidation
  webwidget_->layout();

  // Scroll the canvas if necessary
  scroll_rect_ = client_rect.Intersect(scroll_rect_);
  if (!scroll_rect_.IsEmpty()) {
    HDC hdc = canvas_->getTopPlatformDevice().getBitmapDC();

    RECT damaged_scroll_rect, r = scroll_rect_.ToRECT();
    ScrollDC(hdc, scroll_dx_, scroll_dy_, NULL, &r, NULL, &damaged_scroll_rect);

    PaintRect(gfx::Rect(damaged_scroll_rect));
  }
  ResetScrollRect();

  // Paint the canvas if necessary.  Allow painting to generate extra rects the
  // first time we call it.  This is necessary because some WebCore rendering
  // objects update their layout only when painted.
  for (int i = 0; i < 2; ++i) {
    paint_rect_ = client_rect.Intersect(paint_rect_);
    damaged_rect = damaged_rect.Union(paint_rect_);
    if (!paint_rect_.IsEmpty()) {
      gfx::Rect rect(paint_rect_);
      paint_rect_ = gfx::Rect();

      DLOG_IF(WARNING, i == 1) << "painting caused additional invalidations";
      PaintRect(rect);
    }
  }
  DCHECK(paint_rect_.IsEmpty());

  if (plugin_map_.size() > 0) {
    typedef std::list<const WebPluginGeometry*> PluginList;
    PluginList visible_plugins;
    
    // Identify the visible plugins.
    PluginMap::const_iterator it = plugin_map_.begin();
    for(; it != plugin_map_.end(); ++it) {
      if (it->second.visible && client_rect.Intersects(it->second.window_rect))
        visible_plugins.push_back(&it->second);
    }

    if (!visible_plugins.empty()) {
      HDC drawDC = canvas_->beginPlatformPaint();
      HRGN oldRGN, newRGN;
      POINT oldViewport;

      // Paint the plugin windows.
      PluginList::const_iterator it = visible_plugins.begin();
      for(; it != visible_plugins.end(); ++it) {
        const WebPluginGeometry* geom = *(it);
        
        oldRGN = CreateRectRgn(0,0,1,1);
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

        damaged_rect = damaged_rect.Union(geom->window_rect);
      }

      canvas_->endPlatformPaint();

      // Make sure the damaged rectangle is inside the client rectangle.
      damaged_rect = damaged_rect.Intersect(client_rect);
    }
  }

  if (view_) {
    // Paint to the window.
    PAINTSTRUCT ps;
    BeginPaint(view_, &ps);
    canvas_->getTopPlatformDevice().drawToHDC(ps.hdc,
                                              ps.rcPaint.left,
                                              ps.rcPaint.top,
                                              &ps.rcPaint);
    EndPaint(view_, &ps);

    // Draw children
    UpdateWindow(view_);
  } else {
    // Paint to the delegate.
    DCHECK(paint_delegate_);
    const SkBitmap& bitmap =
        canvas_->getTopPlatformDevice().accessBitmap(false);
    DCHECK(bitmap.config() == SkBitmap::kARGB_8888_Config);
    const void* pixels = bitmap.getPixels();
    paint_delegate_->Paint(popup_, damaged_rect, pixels);
  }
}

void WebWidgetHost::InvalidateRect(const gfx::Rect& rect)
{
  if (rect.IsEmpty())
    return;
  
  if (view_) {
    // Let the window handle painting.
    RECT r = {rect.x(), rect.y(), rect.x() + rect.width(),
              rect.y() + rect.height()};
    ::InvalidateRect(view_, &r, FALSE);
  } else {
    // The update rectangle will be painted by DoPaint().
    update_rect_ = update_rect_.Union(rect);
    if (!update_task_) {
      update_task_ = factory_.NewRunnableMethod(&WebWidgetHost::DoPaint);
      CefThread::PostTask(CefThread::UI, FROM_HERE, update_task_);
    }
  }
}

bool WebWidgetHost::GetImage(int width, int height, void* buffer)
{
  if (!canvas_.get())
    return false;

  DCHECK(width == canvas_->getTopPlatformDevice().width());
  DCHECK(height == canvas_->getTopPlatformDevice().height());

  const SkBitmap& bitmap = canvas_->getTopPlatformDevice().accessBitmap(false);
  DCHECK(bitmap.config() == SkBitmap::kARGB_8888_Config);
  const void* pixels = bitmap.getPixels();
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
        // This mimics a temporary workaround in RenderWidgetHostViewWin 
        // for bug 765011 to get focus when the mouse is clicked. This 
        // happens after the mouse down event is sent to the renderer 
        // because normally Windows does a WM_SETFOCUS after WM_LBUTTONDOWN.
        ::SetFocus(view_);
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

void WebWidgetHost::SendKeyEvent(cef_key_type_t type, int key, int modifiers,
                                 bool sysChar, bool imeChar)
{
  UINT message = 0;
  WPARAM wparam = key;
  LPARAM lparam = modifiers;

  if (type == KT_KEYUP) {
    if (sysChar)
      message = WM_SYSKEYUP;
    else if(imeChar)
      message = WM_IME_KEYUP;
    else
      message = WM_KEYUP;
  } else if(type == KT_KEYDOWN) {
    if (sysChar)
      message = WM_SYSKEYDOWN;
    else if(imeChar)
      message = WM_IME_KEYDOWN;
    else
      message = WM_KEYDOWN;
  } else if(type == KT_CHAR) {
    if (sysChar)
      message = WM_SYSCHAR;
    else if(imeChar)
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
                                        bool mouseUp, int clickCount)
{
  DCHECK(clickCount >=1 && clickCount <= 2);
  
  UINT message = 0;
  WPARAM wparam = 0;
  LPARAM lparam = MAKELPARAM(x, y);

  if (type == MBT_LEFT) {
    if (mouseUp)
      message = (clickCount==1?WM_LBUTTONUP:WM_LBUTTONDBLCLK);
    else
      message = WM_LBUTTONDOWN;
  } else if (type == MBT_MIDDLE) {
    if (mouseUp)
      message = (clickCount==1?WM_MBUTTONUP:WM_MBUTTONDBLCLK);
    else
      message = WM_MBUTTONDOWN;
  }  else if (type == MBT_RIGHT) {
    if (mouseUp)
      message = (clickCount==1?WM_RBUTTONUP:WM_RBUTTONDBLCLK);
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

void WebWidgetHost::SendMouseMoveEvent(int x, int y, bool mouseLeave)
{
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

void WebWidgetHost::SendMouseWheelEvent(int x, int y, int delta)
{
  WPARAM wparam = MAKEWPARAM(0, delta);
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

void WebWidgetHost::SendFocusEvent(bool setFocus)
{
  SetFocus(setFocus);
}

void WebWidgetHost::SendCaptureLostEvent()
{
  CaptureLostEvent();
}
