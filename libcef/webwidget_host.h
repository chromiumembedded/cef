// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _WEBWIDGET_HOST_H
#define _WEBWIDGET_HOST_H

#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextInputType.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/npapi/webplugin.h"
#include <map>

#if defined(OS_WIN)
#include "ui/base/win/ime_input.h"
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
    virtual void Paint(bool popup, const gfx::Rect& dirtyRect,
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

  void DidInvalidateRect(const gfx::Rect& rect);
  void DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect);
  void ScheduleComposite();
  void ScheduleAnimation();
#if defined(OS_WIN)
  void SetCursor(HCURSOR cursor);
#endif

  void DiscardBackingStore();
  // Allow clients to update the paint rect. For example, if we get a gdk
  // expose or WM_PAINT event, we need to update the paint rect.
  void UpdatePaintRect(const gfx::Rect& rect);

  void Paint();
  void InvalidateRect(const gfx::Rect& rect);

  bool GetImage(int width, int height, void* buffer);

  void SetSize(int width, int height);
  void GetSize(int& width, int& height);

  skia::PlatformCanvas* canvas() const { return canvas_.get(); }

  WebKit::WebScreenInfo GetScreenInfo();

  WebKit::WebKeyboardEvent GetLastKeyEvent() const { return last_key_event_; }

  void PaintRect(const gfx::Rect& rect);

  void SetTooltipText(const CefString& tooltip_text);

  void SendKeyEvent(cef_key_type_t type, int key, int modifiers, bool sysChar,
                    bool imeChar);
  void SendMouseClickEvent(int x, int y, cef_mouse_button_type_t type,
                           bool mouseUp, int clickCount);
  void SendMouseMoveEvent(int x, int y, bool mouseLeave);
  void SendMouseWheelEvent(int x, int y, int delta);
  void SendFocusEvent(bool setFocus);
  void SendCaptureLostEvent();

  // Manage windowed plugins when window rendering is disabled.
  bool HasWindowedPlugins() { return !plugin_map_.empty(); }
  void AddWindowedPlugin(gfx::PluginWindowHandle handle);
  void RemoveWindowedPlugin(gfx::PluginWindowHandle handle);
  void MoveWindowedPlugin(const webkit::npapi::WebPluginGeometry& geometry);
  gfx::PluginWindowHandle GetWindowedPluginAt(int x, int y);

  // If window rendering is disabled paint messages are generated after all
  // other pending messages have been processed.
  void DoPaint();

  void set_popup(bool popup) { popup_ = popup; }
  bool popup() { return popup_; }

  PaintDelegate* paint_delegate() { return paint_delegate_; }

 protected:
  WebWidgetHost();

#if defined(OS_WIN)
  // Per-class wndproc.  Returns true if the event should be swallowed.
  virtual bool WndProc(UINT message, WPARAM wparam, LPARAM lparam);

  void Resize(LPARAM lparam);
  virtual void MouseEvent(UINT message, WPARAM wparam, LPARAM lparam);
  void WheelEvent(WPARAM wparam, LPARAM lparam);
  void KeyEvent(UINT message, WPARAM wparam, LPARAM lparam);
  void CaptureLostEvent();
  void SetFocus(bool enable);
  void OnNotify(WPARAM wparam, NMHDR* header);

  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  LRESULT OnImeSetContext(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnImeStartComposition(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnImeComposition(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnImeEndComposition(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  void OnInputLangChange(DWORD character_set, HKL input_language_id);
  void ImeUpdateTextInputState(WebKit::WebTextInputType type, const gfx::Rect& caret_rect);
  static void UpdateInputMethod(HWND view);
#elif defined(OS_MACOSX)
  // These need to be called from a non-subclass, so they need to be public.
 public:
  void Resize(const gfx::Rect& rect);
  virtual void MouseEvent(NSEvent *);
  void WheelEvent(NSEvent *);
  void KeyEvent(NSEvent *);
  virtual void SetFocus(bool enable);
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

  // The paint delegate is used instead of the view when window rendering is
  // disabled.
  PaintDelegate* paint_delegate_;

  WebKit::WebWidget* webwidget_;
  scoped_ptr<skia::PlatformCanvas> canvas_;

  // True if this widget is a popup widget.
  bool popup_;

  // Specifies the portion of the webwidget that needs painting.
  gfx::Rect paint_rect_;

  // Specifies the portion of the webwidget that needs scrolling.
  gfx::Rect scroll_rect_;
  int scroll_dx_;
  int scroll_dy_;

  // Specifies the portion of the webwidget that has been invalidated when
  // window rendering is disabled.
  gfx::Rect update_rect_;
  CancelableTask* update_task_;

  // The map of windowed plugins that need to be drawn when window rendering is
  // disabled.
  typedef std::map<gfx::PluginWindowHandle,webkit::npapi::WebPluginGeometry>
      PluginMap;
  PluginMap plugin_map_;

#if defined(OS_WIN)
  bool track_mouse_leave_;
  std::wstring tooltip_text_;
  gfx::NativeView tooltip_view_;
  bool tooltip_showing_;

  // Wrapper class for IME input.
  ui::ImeInput ime_input_;

  // Represents whether or not this browser process is receiving status
  // messages about the focused edit control from a renderer process.
  bool ime_notification_;

  // Represents whether or not the IME of a browser process is active.
  bool input_method_is_active_;

  // Stores the current text input type received by ImeUpdateTextInputState()
  // method.
  WebKit::WebTextInputType text_input_type_;

  // Stores the current caret bounds of input focus.
  WebKit::WebRect caret_bounds_;
#endif

#if defined(TOOLKIT_USES_GTK)
  // Since GtkWindow resize is asynchronous, we have to stash the dimensions,
  // so that the backing store doesn't have to wait for sizing to take place.
  gfx::Size logical_size_;
#endif

  WebKit::WebKeyboardEvent last_key_event_;

#ifndef NDEBUG
  bool painting_;
#endif

 private:
  ScopedRunnableMethodFactory<WebWidgetHost> factory_;
};

#endif  // _WEBWIDGET_HOST_H
