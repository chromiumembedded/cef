// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

// Include this first to avoid type conflict errors.
#include "base/tracked_objects.h"
#undef Status

#include <sys/sysinfo.h>
#include <X11/cursorfont.h>

#include "libcef/browser/context.h"
#include "libcef/browser/window_delegate_view.h"
#include "libcef/browser/window_x11.h"
#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/renderer_preferences.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns the number of seconds since system boot.
long GetSystemUptime() {
  struct sysinfo info;
  if (sysinfo(&info) == 0)
    return info.uptime;
  return 0;
}

// Based on ui/base/cursor/cursor_loader_x11.cc.

using blink::WebCursorInfo;

int ToCursorID(WebCursorInfo::Type type) {
  switch (type) {
    case WebCursorInfo::TypePointer:
      return XC_left_ptr;
    case WebCursorInfo::TypeCross:
      return XC_crosshair;
    case WebCursorInfo::TypeHand:
      return XC_hand2;
    case WebCursorInfo::TypeIBeam:
      return XC_xterm;
    case WebCursorInfo::TypeWait:
      return XC_watch;
    case WebCursorInfo::TypeHelp:
      return XC_question_arrow;
    case WebCursorInfo::TypeEastResize:
      return XC_right_side;
    case WebCursorInfo::TypeNorthResize:
      return XC_top_side;
    case WebCursorInfo::TypeNorthEastResize:
      return XC_top_right_corner;
    case WebCursorInfo::TypeNorthWestResize:
      return XC_top_left_corner;
    case WebCursorInfo::TypeSouthResize:
      return XC_bottom_side;
    case WebCursorInfo::TypeSouthEastResize:
      return XC_bottom_right_corner;
    case WebCursorInfo::TypeSouthWestResize:
      return XC_bottom_left_corner;
    case WebCursorInfo::TypeWestResize:
      return XC_left_side;
    case WebCursorInfo::TypeNorthSouthResize:
      return XC_sb_v_double_arrow;
    case WebCursorInfo::TypeEastWestResize:
      return XC_sb_h_double_arrow;
    case WebCursorInfo::TypeNorthEastSouthWestResize:
      return XC_left_ptr;
    case WebCursorInfo::TypeNorthWestSouthEastResize:
      return XC_left_ptr;
    case WebCursorInfo::TypeColumnResize:
      return XC_sb_h_double_arrow;
    case WebCursorInfo::TypeRowResize:
      return XC_sb_v_double_arrow;
    case WebCursorInfo::TypeMiddlePanning:
      return XC_fleur;
    case WebCursorInfo::TypeEastPanning:
      return XC_sb_right_arrow;
    case WebCursorInfo::TypeNorthPanning:
      return XC_sb_up_arrow;
    case WebCursorInfo::TypeNorthEastPanning:
      return XC_top_right_corner;
    case WebCursorInfo::TypeNorthWestPanning:
      return XC_top_left_corner;
    case WebCursorInfo::TypeSouthPanning:
      return XC_sb_down_arrow;
    case WebCursorInfo::TypeSouthEastPanning:
      return XC_bottom_right_corner;
    case WebCursorInfo::TypeSouthWestPanning:
      return XC_bottom_left_corner;
    case WebCursorInfo::TypeWestPanning:
      return XC_sb_left_arrow;
    case WebCursorInfo::TypeMove:
      return XC_fleur;
    case WebCursorInfo::TypeVerticalText:
      return XC_left_ptr;
    case WebCursorInfo::TypeCell:
      return XC_left_ptr;
    case WebCursorInfo::TypeContextMenu:
      return XC_left_ptr;
    case WebCursorInfo::TypeAlias:
      return XC_left_ptr;
    case WebCursorInfo::TypeProgress:
      return XC_left_ptr;
    case WebCursorInfo::TypeNoDrop:
      return XC_left_ptr;
    case WebCursorInfo::TypeCopy:
      return XC_left_ptr;
    case WebCursorInfo::TypeNotAllowed:
      return XC_left_ptr;
    case WebCursorInfo::TypeZoomIn:
      return XC_left_ptr;
    case WebCursorInfo::TypeZoomOut:
      return XC_left_ptr;
    case WebCursorInfo::TypeGrab:
      return XC_left_ptr;
    case WebCursorInfo::TypeGrabbing:
      return XC_left_ptr;
    case WebCursorInfo::TypeCustom:
    case WebCursorInfo::TypeNone:
      break;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

ui::PlatformCursor CefBrowserHostImpl::GetPlatformCursor(
    blink::WebCursorInfo::Type type) {
  if (type == WebCursorInfo::TypeNone) {
    if (!invisible_cursor_) {
      invisible_cursor_.reset(
          new ui::XScopedCursor(ui::CreateInvisibleCursor(),
                                gfx::GetXDisplay()));
    }
    return invisible_cursor_->get();
  } else {
    return ui::GetXCursor(ToCursorID(type));
  }
}

bool CefBrowserHostImpl::PlatformCreateWindow() {
  DCHECK(!window_x11_);
  DCHECK(!window_widget_);

  if (window_info_.width == 0)
    window_info_.width = 800;
  if (window_info_.height == 0)
    window_info_.height = 600;

  gfx::Rect rect(window_info_.x, window_info_.y,
                 window_info_.width, window_info_.height);

  // Create a new window object. It will delete itself when the associated X11
  // window is destroyed.
  window_x11_ = new CefWindowX11(this, window_info_.parent_window, rect);
  window_info_.window = window_x11_->xwindow();

  // Add a reference that will be released in the destroy handler.
  AddRef();

  SkColor background_color = SK_ColorWHITE;
  const CefSettings& settings = CefContext::Get()->settings();
  if (CefColorGetA(settings.background_color) > 0) {
    background_color = SkColorSetRGB(
        CefColorGetR(settings.background_color),
        CefColorGetG(settings.background_color),
        CefColorGetB(settings.background_color));
  }

  CefWindowDelegateView* delegate_view =
      new CefWindowDelegateView(background_color);
  delegate_view->Init(window_info_.window,
                      web_contents(),
                      gfx::Rect(gfx::Point(), rect.size()));

  window_widget_ = delegate_view->GetWidget();
  window_widget_->Show();

  window_x11_->Show();

  // As an additional requirement on Linux, we must set the colors for the
  // render widgets in webkit.
  content::RendererPreferences* prefs =
      web_contents_->GetMutableRendererPrefs();
  prefs->focus_ring_color = SkColorSetARGB(255, 229, 151, 0);
  prefs->thumb_active_color = SkColorSetRGB(244, 244, 244);
  prefs->thumb_inactive_color = SkColorSetRGB(234, 234, 234);
  prefs->track_color = SkColorSetRGB(211, 211, 211);

  prefs->active_selection_bg_color = SkColorSetRGB(30, 144, 255);
  prefs->active_selection_fg_color = SK_ColorWHITE;
  prefs->inactive_selection_bg_color = SkColorSetRGB(200, 200, 200);
  prefs->inactive_selection_fg_color = SkColorSetRGB(50, 50, 50);

  return true;
}

void CefBrowserHostImpl::PlatformCloseWindow() {
  if (window_x11_)
    window_x11_->Close();
}

void CefBrowserHostImpl::PlatformSizeTo(int width, int height) {
  if (window_x11_) {
    window_x11_->SetBounds(
        gfx::Rect(window_x11_->bounds().origin(), gfx::Size(width, height)));
  }
}

void CefBrowserHostImpl::PlatformSetFocus(bool focus) {
  if (!focus)
    return;

  if (web_contents_) {
    // Give logical focus to the RenderWidgetHostViewAura in the views
    // hierarchy. This does not change the native keyboard focus.
    web_contents_->Focus();
  }

  if (window_x11_) {
    // Give native focus to the DesktopNativeWidgetAura for the root window.
    // Needs to be done via the ::Window so that keyboard focus is assigned
    // correctly.
    window_x11_->Focus();
  }
}

CefWindowHandle CefBrowserHostImpl::PlatformGetWindowHandle() {
  return IsWindowless() ? window_info_.parent_window : window_info_.window;
}

bool CefBrowserHostImpl::PlatformViewText(const std::string& text) {
  CEF_REQUIRE_UIT();

  char buff[] = "/tmp/CEFSourceXXXXXX";
  int fd = mkstemp(buff);

  if (fd == -1)
    return false;

  FILE* srcOutput = fdopen(fd, "w+");
  if (!srcOutput)
    return false;

  if (fputs(text.c_str(), srcOutput) < 0) {
    fclose(srcOutput);
    return false;
  }

  fclose(srcOutput);

  std::string newName(buff);
  newName.append(".txt");
  if (rename(buff, newName.c_str()) != 0)
    return false;

  std::string openCommand("xdg-open ");
  openCommand += newName;

  if (system(openCommand.c_str()) != 0)
    return false;

  return true;
}

void CefBrowserHostImpl::PlatformHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // TODO(cef): Is something required here to handle shortcut keys?
}

void CefBrowserHostImpl::PlatformRunFileChooser(
    const FileChooserParams& params,
    RunFileChooserCallback callback) {
  NOTIMPLEMENTED();
  callback.Run(0, std::vector<base::FilePath>());
}

void CefBrowserHostImpl::PlatformHandleExternalProtocol(const GURL& url) {
}

void CefBrowserHostImpl::PlatformTranslateKeyEvent(
    content::NativeWebKeyboardEvent& result,
    const CefKeyEvent& key_event) {
  result.timeStampSeconds = GetSystemUptime();

  result.windowsKeyCode = key_event.windows_key_code;
  result.nativeKeyCode = key_event.native_key_code;
  result.isSystemKey = key_event.is_system_key ? 1 : 0;
  switch (key_event.type) {
  case KEYEVENT_RAWKEYDOWN:
  case KEYEVENT_KEYDOWN:
    result.type = blink::WebInputEvent::RawKeyDown;
    break;
  case KEYEVENT_KEYUP:
    result.type = blink::WebInputEvent::KeyUp;
    break;
  case KEYEVENT_CHAR:
    result.type = blink::WebInputEvent::Char;
    break;
  default:
    NOTREACHED();
  }

  result.text[0] = key_event.character;
  result.unmodifiedText[0] = key_event.unmodified_character;

  result.setKeyIdentifierFromWindowsKeyCode();

  result.modifiers |= TranslateModifiers(key_event.modifiers);
}

void CefBrowserHostImpl::PlatformTranslateClickEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    MouseButtonType type,
    bool mouseUp, int clickCount) {
  PlatformTranslateMouseEvent(result, mouse_event);

  switch (type) {
  case MBT_LEFT:
    result.type = mouseUp ? blink::WebInputEvent::MouseUp :
                            blink::WebInputEvent::MouseDown;
    result.button = blink::WebMouseEvent::ButtonLeft;
    break;
  case MBT_MIDDLE:
    result.type = mouseUp ? blink::WebInputEvent::MouseUp :
                            blink::WebInputEvent::MouseDown;
    result.button = blink::WebMouseEvent::ButtonMiddle;
    break;
  case MBT_RIGHT:
    result.type = mouseUp ? blink::WebInputEvent::MouseUp :
                            blink::WebInputEvent::MouseDown;
    result.button = blink::WebMouseEvent::ButtonRight;
    break;
  default:
    NOTREACHED();
  }

  result.clickCount = clickCount;
}

void CefBrowserHostImpl::PlatformTranslateMoveEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    bool mouseLeave) {
  PlatformTranslateMouseEvent(result, mouse_event);

  if (!mouseLeave) {
    result.type = blink::WebInputEvent::MouseMove;
    if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::ButtonLeft;
    else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::ButtonMiddle;
    else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::ButtonRight;
    else
      result.button = blink::WebMouseEvent::ButtonNone;
  } else {
    result.type = blink::WebInputEvent::MouseLeave;
    result.button = blink::WebMouseEvent::ButtonNone;
  }

  result.clickCount = 0;
}

void CefBrowserHostImpl::PlatformTranslateWheelEvent(
    blink::WebMouseWheelEvent& result,
    const CefMouseEvent& mouse_event,
    int deltaX, int deltaY) {
  result = blink::WebMouseWheelEvent();
  PlatformTranslateMouseEvent(result, mouse_event);

  result.type = blink::WebInputEvent::MouseWheel;

  static const double scrollbarPixelsPerGtkTick = 40.0;
  result.deltaX = deltaX;
  result.deltaY = deltaY;
  result.wheelTicksX = result.deltaX / scrollbarPixelsPerGtkTick;
  result.wheelTicksY = result.deltaY / scrollbarPixelsPerGtkTick;
  result.hasPreciseScrollingDeltas = true;

  // Unless the phase and momentumPhase are passed in as parameters to this
  // function, there is no way to know them
  result.phase = blink::WebMouseWheelEvent::PhaseNone;
  result.momentumPhase = blink::WebMouseWheelEvent::PhaseNone;

  if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::ButtonLeft;
  else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::ButtonMiddle;
  else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::ButtonRight;
  else
    result.button = blink::WebMouseEvent::ButtonNone;
}

void CefBrowserHostImpl::PlatformTranslateMouseEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event) {
  // position
  result.x = mouse_event.x;
  result.y = mouse_event.y;
  result.windowX = result.x;
  result.windowY = result.y;
  result.globalX = result.x;
  result.globalY = result.y;

  if (IsWindowless()) {
    CefRefPtr<CefRenderHandler> handler = client_->GetRenderHandler();
    if (handler.get()) {
      handler->GetScreenPoint(this, result.x, result.y, result.globalX,
                              result.globalY);
    }
  } else if (window_x11_) {
    const gfx::Point& origin = window_x11_->bounds().origin();
    result.globalX += origin.x();
    result.globalY += origin.y();
  }

  // modifiers
  result.modifiers |= TranslateModifiers(mouse_event.modifiers);

  // timestamp
  result.timeStampSeconds = GetSystemUptime();
}

void CefBrowserHostImpl::PlatformNotifyMoveOrResizeStarted() {
  if (IsWindowless())
    return;

  if (!window_x11_)
    return;

  views::DesktopWindowTreeHostX11* tree_host = window_x11_->GetHost();
  if (!tree_host)
    return;

  // Explicitly set the screen bounds so that WindowTreeHost::*Screen()
  // methods return the correct results.
  const gfx::Rect& bounds = window_x11_->GetBoundsInScreen();
  tree_host->set_screen_bounds(bounds);

  // Send updated screen rectangle information to the renderer process so that
  // popups are displayed in the correct location.
  content::RenderWidgetHostImpl::From(web_contents()->GetRenderViewHost())->
      SendScreenRects();
}

