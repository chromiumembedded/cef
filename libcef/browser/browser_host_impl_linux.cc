// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#include <sys/sysinfo.h>

#include "libcef/browser/context.h"
#include "libcef/browser/window_delegate_view.h"
#include "libcef/browser/window_x11.h"
#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/renderer_preferences.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns the number of seconds since system boot.
long GetSystemUptime() {
  struct sysinfo info;
  if (sysinfo(&info) == 0)
    return info.uptime;
  return 0;
}

}  // namespace

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

CefWindowHandle CefBrowserHostImpl::PlatformGetWindowHandle() {
  return window_info_.window;
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
    const content::FileChooserParams& params,
    RunFileChooserCallback callback) {
  NOTIMPLEMENTED();
  std::vector<base::FilePath> files;
  callback.Run(files);
}

void CefBrowserHostImpl::PlatformHandleExternalProtocol(const GURL& url) {
}

void CefBrowserHostImpl::PlatformTranslateKeyEvent(
    content::NativeWebKeyboardEvent& result,
    const CefKeyEvent& key_event) {
  NOTIMPLEMENTED();
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

  // TODO(linux): Convert global{X,Y} to screen coordinates.

  // modifiers
  result.modifiers |= TranslateModifiers(mouse_event.modifiers);

  // timestamp
  result.timeStampSeconds = GetSystemUptime();
}
