// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_WEBWIDGET_HOST_H_
#define CEF_LIBCEF_WEBWIDGET_HOST_H_
#pragma once

#include <map>
#include <vector>

#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"
#include "include/internal/cef_types_wrappers.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextInputType.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/npapi/webplugin.h"

#if defined(OS_WIN)
#include "ui/base/win/ime_input.h"
#endif

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>  // NOLINT(build/include_order)
#endif

namespace gfx {
class Rect;
class Size;
}

namespace WebKit {
class WebWidget;
class WebWidgetClient;
class WebKeyboardEvent;
struct WebScreenInfo;
}

#if defined(OS_MACOSX)
#ifdef __OBJC__
@class NSEvent;
#else
class NSEvent;
#endif
#endif

// This class is a simple NativeView-based host for a WebWidget
class WebWidgetHost {
 public:
  class PaintDelegate {
   public:
    virtual ~PaintDelegate() {}
    virtual void Paint(bool popup, const std::vector<CefRect>& dirtyRects,
                       const void* buffer) =0;
  };

  // The new instance is deleted once the associated NativeView is destroyed.
  // The newly created window should be resized after it is created, using the
  // MoveWindow (or equivalent) function.
  static WebWidgetHost* Create(gfx::NativeView parent_view,
                               WebKit::WebWidgetClient* client,
                               PaintDelegate* paint_delegate);

  virtual ~WebWidgetHost();

  gfx::NativeView view_handle() const { return view_; }
  WebKit::WebWidget* webwidget() const { return webwidget_; }

  void InvalidateRect(const gfx::Rect& rect);

  void ScrollRect(int dx, int dy, const gfx::Rect& clip_rect);

  // Called for accelerated content like WebGL.
  void ScheduleComposite();

  // Called for requestAnimationFrame animations.
  void ScheduleAnimation();

#if defined(OS_WIN)
  void SetCursor(HCURSOR cursor);
#endif

#if defined(OS_MACOSX)
  void Paint(SkRegion& update_rgn);
#else
  void Paint();
#endif

  bool GetImage(int width, int height, void* buffer);

  void SetSize(int width, int height);
  void GetSize(int& width, int& height);

  skia::PlatformCanvas* canvas() const { return canvas_.get(); }

  WebKit::WebScreenInfo GetScreenInfo();

  WebKit::WebKeyboardEvent GetLastKeyEvent() const { return last_key_event_; }

  void SetTooltipText(const CefString& tooltip_text);

  void SendKeyEvent(cef_key_type_t type, const cef_key_info_t& keyInfo,
                    int modifiers);
  void SendMouseClickEvent(int x, int y, cef_mouse_button_type_t type,
                           bool mouseUp, int clickCount);
  void SendMouseMoveEvent(int x, int y, bool mouseLeave);
  void SendMouseWheelEvent(int x, int y, int deltaX, int deltaY);
  void SendFocusEvent(bool setFocus);
  void SendCaptureLostEvent();

  // Manage windowed plugins when window rendering is disabled.
  bool HasWindowedPlugins() { return !plugin_map_.empty(); }
  void AddWindowedPlugin(gfx::PluginWindowHandle handle);
  void RemoveWindowedPlugin(gfx::PluginWindowHandle handle);
  void MoveWindowedPlugin(const webkit::npapi::WebPluginGeometry& geometry);
  gfx::PluginWindowHandle GetWindowedPluginAt(int x, int y);

  void SetFrameRate(int frames_per_second);

  virtual bool IsTransparent() { return false; }

  void set_popup(bool popup) { popup_ = popup; }
  bool popup() { return popup_; }

  PaintDelegate* paint_delegate() { return paint_delegate_; }

 protected:
  WebWidgetHost();

  // Update the region that will be painted to the canvas by WebKit the next
  // time that Paint() is called.
  void UpdatePaintRect(const gfx::Rect& rect);

  void PaintRect(const gfx::Rect& rect);

  // Trigger the OS to invalidate/repaint the window.
  void InvalidateWindow();
  void InvalidateWindowRect(const gfx::Rect& rect);

  // When window rendering is enabled this method invalidates the client area to
  // trigger repaint via the OS. When window rendering is disabled this method
  // is used to generate CefRenderHandler::OnPaint() calls.
  void ScheduleTimer();
  void DoTimer();

#if defined(OS_WIN)
  // Per-class wndproc.  Returns true if the event should be swallowed.
  virtual bool WndProc(UINT message, WPARAM wparam, LPARAM lparam);

  virtual void MouseEvent(UINT message, WPARAM wparam, LPARAM lparam);
  void WheelEvent(WPARAM wparam, LPARAM lparam);
  virtual void KeyEvent(UINT message, WPARAM wparam, LPARAM lparam);
  void CaptureLostEvent();
  void SetFocus(bool enable);
  void OnNotify(WPARAM wparam, NMHDR* header);

  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  LRESULT OnImeSetContext(UINT message, WPARAM wparam, LPARAM lparam,
      BOOL& handled);
  LRESULT OnImeStartComposition(UINT message, WPARAM wparam, LPARAM lparam,
      BOOL& handled);
  LRESULT OnImeComposition(UINT message, WPARAM wparam, LPARAM lparam,
      BOOL& handled);
  LRESULT OnImeEndComposition(UINT message, WPARAM wparam, LPARAM lparam,
      BOOL& handled);
  void OnInputLangChange(DWORD character_set, HKL input_language_id);
  void UpdateImeInputState();
  void ToggleImeTimer();
#elif defined(OS_MACOSX)
  // These need to be called from a non-subclass, so they need to be public.
 public:
  virtual void MouseEvent(NSEvent* event);
  void WheelEvent(NSEvent* event);
  virtual void KeyEvent(NSEvent* event);
  virtual void SetFocus(bool enable);

 protected:
#elif defined(TOOLKIT_GTK)

 public:
  // ---------------------------------------------------------------------------
  // This is needed on Linux because the GtkWidget creation is the same between
  // both web view hosts and web widget hosts. The Windows code manages this by
  // reusing the WndProc function (static, above). However, GTK doesn't use a
  // single big callback function like that so we have a static function that
  // sets up a GtkWidget correctly.
  //   parent: a GtkBox to pack the new widget at the end of
  //   host: a pointer to a WebWidgetHost (or subclass thereof)
  // ---------------------------------------------------------------------------
  static gfx::NativeView CreateWidget(gfx::NativeView parent_view,
                                      WebWidgetHost* host);
  virtual void KeyEvent(GdkEventKey* event);
#endif

#if defined(OS_WIN)
  void TrackMouseLeave(bool enable);
#endif

  void set_painting(bool value) {
    painting_ = value;
  }

  void EnsureTooltip();
  void ResetTooltip();

  gfx::NativeView view_;

  // The paint delegate is used instead of the view when window rendering is
  // disabled.
  PaintDelegate* paint_delegate_;

  WebKit::WebWidget* webwidget_;
  scoped_ptr<skia::PlatformCanvas> canvas_;
  int canvas_w_;
  int canvas_h_;

  // True if this widget is a popup widget.
  bool popup_;

  // Specifies the portion of the webwidget that needs painting.
  // TODO(cef): Update the Linux port to use regions instead of rectangles.
#if defined(OS_WIN) || defined(OS_MACOSX)
  SkRegion paint_rgn_;
#else
  gfx::Rect paint_rect_;
#endif

  base::OneShotTimer<WebWidgetHost> timer_;
  base::TimeTicks timer_last_;
  bool timer_executing_;
  bool timer_wanted_;

  int64 frame_delay_;

#if defined(OS_WIN)
  // Used to call UpdateImeInputState() while IME is active.
  base::RepeatingTimer<WebWidgetHost> ime_timer_;
#endif

  // The map of windowed plugins that need to be drawn when window rendering is
  // disabled.
  typedef std::map<gfx::PluginWindowHandle, webkit::npapi::WebPluginGeometry>
      PluginMap;
  PluginMap plugin_map_;

#if defined(OS_WIN)
  bool track_mouse_leave_;
  std::wstring tooltip_text_;
  gfx::NativeView tooltip_view_;
  bool tooltip_showing_;

  // Wrapper class for IME input.
  ui::ImeInput ime_input_;

  // Represents whether or not this browser process is receiving status messages
  // about the focused edit control from a renderer process.
  bool ime_notification_;

  // Stores the current text input type.
  WebKit::WebTextInputType text_input_type_;

  // Stores the current caret bounds of input focus.
  WebKit::WebRect caret_bounds_;
#endif  // OS_WIN

#if defined(OS_MACOSX)
  int mouse_modifiers_;
  WebKit::WebMouseEvent::Button mouse_button_down_;
#endif

  WebKit::WebKeyboardEvent last_key_event_;

  bool painting_;
  bool layouting_;

  static const int kDefaultFrameRate;
  static const int kMaxFrameRate;
};

#endif  // CEF_LIBCEF_WEBWIDGET_HOST_H_
