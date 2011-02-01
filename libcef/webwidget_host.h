// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _WEBWIDGET_HOST_H
#define _WEBWIDGET_HOST_H

#include "include/cef_string.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "gfx/native_widget_types.h"
#include "gfx/rect.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"

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
  // The new instance is deleted once the associated NativeView is destroyed.
  // The newly created window should be resized after it is created, using the
  // MoveWindow (or equivalent) function.
  static WebWidgetHost* Create(gfx::NativeView parent_view,
                               WebKit::WebWidgetClient* client);

#if defined(OS_MACOSX)
  static void HandleEvent(gfx::NativeView view, NSEvent* event);
#endif

  gfx::NativeView view_handle() const { return view_; }
  WebKit::WebWidget* webwidget() const { return webwidget_; }

  void DidInvalidateRect(const gfx::Rect& rect);
  void DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect);
  void ScheduleComposite();
#if defined(OS_WIN)
  void SetCursor(HCURSOR cursor);
#endif

  void DiscardBackingStore();
  // Allow clients to update the paint rect. For example, if we get a gdk
  // expose or WM_PAINT event, we need to update the paint rect.
  void UpdatePaintRect(const gfx::Rect& rect);
  void Paint();

  skia::PlatformCanvas* canvas() const { return canvas_.get(); }

  WebKit::WebScreenInfo GetScreenInfo();

  WebKit::WebKeyboardEvent GetLastKeyEvent() const { return last_key_event_; }

  void PaintRect(const gfx::Rect& rect);

  void SetTooltipText(const CefString& tooltip_text);

  void set_popup(bool popup) { popup_ = popup; }
  bool popup() { return popup_; }

 protected:
  WebWidgetHost();
  ~WebWidgetHost();

#if defined(OS_WIN)
  // Per-class wndproc.  Returns true if the event should be swallowed.
  virtual bool WndProc(UINT message, WPARAM wparam, LPARAM lparam);

  void Resize(LPARAM lparam);
  void MouseEvent(UINT message, WPARAM wparam, LPARAM lparam);
  void WheelEvent(WPARAM wparam, LPARAM lparam);
  void KeyEvent(UINT message, WPARAM wparam, LPARAM lparam);
  void CaptureLostEvent();
  void SetFocus(bool enable);
  void OnNotify(WPARAM wparam, NMHDR* header);

  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
#elif defined(OS_MACOSX)
  // These need to be called from a non-subclass, so they need to be public.
 public:
  void Resize(const gfx::Rect& rect);
  void MouseEvent(NSEvent *);
  void WheelEvent(NSEvent *);
  void KeyEvent(NSEvent *);
  void SetFocus(bool enable);
 protected:
#elif defined(TOOLKIT_USES_GTK)
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
  void WindowDestroyed();
  void Resize(const gfx::Size& size);
#endif

#if defined(OS_WIN)
  void TrackMouseLeave(bool enable);
#endif

  void ResetScrollRect();

  void set_painting(bool value) {
#ifndef NDEBUG
    painting_ = value;
#endif
  }

  void EnsureTooltip();
  void ResetTooltip();

  gfx::NativeView view_;
  WebKit::WebWidget* webwidget_;
  scoped_ptr<skia::PlatformCanvas> canvas_;

  // True if this widget is a popup widget.
  bool popup_;

  // specifies the portion of the webwidget that needs painting
  gfx::Rect paint_rect_;

  // specifies the portion of the webwidget that needs scrolling
  gfx::Rect scroll_rect_;
  int scroll_dx_;
  int scroll_dy_;

#if defined(OS_WIN)
  bool track_mouse_leave_;
  std::wstring tooltip_text_;
#endif

#if defined(TOOLKIT_USES_GTK)
  // Since GtkWindow resize is asynchronous, we have to stash the dimensions,
  // so that the backing store doesn't have to wait for sizing to take place.
  gfx::Size logical_size_;
#endif

  WebKit::WebKeyboardEvent last_key_event_;
  gfx::NativeView tooltip_view_;
  bool tooltip_showing_;

#ifndef NDEBUG
  bool painting_;
#endif
};

#endif  // _WEBWIDGET_HOST_H
