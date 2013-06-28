// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/cefclient_osr_widget_gtk.h"

// This value is defined in build/common.gypi and must be undefined here
// in order for gtkglext to compile.
#undef GTK_DISABLE_SINGLE_INCLUDES

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <GL/gl.h>

#include "include/cef_runnable.h"
#include "cefclient/util.h"

namespace {

gint glarea_size_allocation(GtkWidget* widget,
                            GtkAllocation* allocation,
                            OSRWindow* window) {
  CefRefPtr<CefBrowserHost> host = window->GetBrowserHost();
  host->WasResized();
  return TRUE;
}

int get_cef_state_modifiers(guint state) {
  int modifiers = 0;
  if (state & GDK_SHIFT_MASK)
    modifiers |= EVENTFLAG_SHIFT_DOWN;
  if (state & GDK_LOCK_MASK)
    modifiers |= EVENTFLAG_CAPS_LOCK_ON;
  if (state & GDK_CONTROL_MASK)
    modifiers |= EVENTFLAG_CONTROL_DOWN;
  if (state & GDK_MOD1_MASK)
    modifiers |= EVENTFLAG_ALT_DOWN;
  if (state & GDK_BUTTON1_MASK)
    modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
  if (state & GDK_BUTTON2_MASK)
    modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
  if (state & GDK_BUTTON3_MASK)
    modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
  return modifiers;
}

gint glarea_click_event(GtkWidget* widget,
                        GdkEventButton* event,
                        OSRWindow* window) {
  CefRefPtr<CefBrowserHost> host = window->GetBrowserHost();

  CefBrowserHost::MouseButtonType button_type = MBT_LEFT;
  switch (event->button) {
    case 1:
      break;
    case 2:
      button_type = MBT_MIDDLE;
      break;
    case 3:
      button_type = MBT_RIGHT;
      break;
    default:
      // Other mouse buttons are not handled here.
      return FALSE;
  }

  CefMouseEvent mouse_event;
  mouse_event.x = event->x;
  mouse_event.y = event->y;
  window->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  mouse_event.modifiers = get_cef_state_modifiers(event->state);

  bool mouse_up = (event->type == GDK_BUTTON_RELEASE);
  if (!mouse_up)
    gtk_widget_grab_focus(widget);

  int click_count = 1;
  switch (event->type) {
    case GDK_2BUTTON_PRESS:
      click_count = 2;
      break;
    case GDK_3BUTTON_PRESS:
      click_count = 3;
      break;
    default:
      break;
  }

  host->SendMouseClickEvent(mouse_event, button_type, mouse_up, click_count);
  return TRUE;
}

gint glarea_move_event(GtkWidget* widget,
                       GdkEventMotion* event,
                       OSRWindow* window) {
  gint x, y;
  GdkModifierType state;

  if (event->is_hint) {
    gdk_window_get_pointer(event->window, &x, &y, &state);
  } else {
    x = (gint)event->x;
    y = (gint)event->y;
    state = (GdkModifierType)event->state;
  }

  CefRefPtr<CefBrowserHost> host = window->GetBrowserHost();

  CefMouseEvent mouse_event;
  mouse_event.x = x;
  mouse_event.y = y;
  window->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  mouse_event.modifiers = get_cef_state_modifiers(state);

  bool mouse_leave = (event->type == GDK_LEAVE_NOTIFY);

  host->SendMouseMoveEvent(mouse_event, mouse_leave);
  return TRUE;
}

gint glarea_scroll_event(GtkWidget* widget,
                         GdkEventScroll* event,
                         OSRWindow* window) {
  CefRefPtr<CefBrowserHost> host = window->GetBrowserHost();

  CefMouseEvent mouse_event;
  mouse_event.x = event->x;
  mouse_event.y = event->y;
  window->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  mouse_event.modifiers = get_cef_state_modifiers(event->state);

  static const int scrollbarPixelsPerGtkTick = 40;
  int deltaX = 0;
  int deltaY = 0;
  switch (event->direction) {
    case GDK_SCROLL_UP:
      deltaY = scrollbarPixelsPerGtkTick;
      break;
    case GDK_SCROLL_DOWN:
      deltaY = -scrollbarPixelsPerGtkTick;
      break;
    case GDK_SCROLL_LEFT:
      deltaX = scrollbarPixelsPerGtkTick;
      break;
    case GDK_SCROLL_RIGHT:
      deltaX = -scrollbarPixelsPerGtkTick;
      break;
  }

  host->SendMouseWheelEvent(mouse_event, deltaX, deltaY);
  return TRUE;
}

gint glarea_key_event(GtkWidget* widget,
                      GdkEventKey* event,
                      OSRWindow* window) {
  CefRefPtr<CefBrowserHost> host = window->GetBrowserHost();

  CefKeyEvent key_event;
  key_event.native_key_code = event->keyval;
  key_event.modifiers = get_cef_state_modifiers(event->state);

  if (event->type == GDK_KEY_PRESS) {
    key_event.type = KEYEVENT_RAWKEYDOWN;
    host->SendKeyEvent(key_event);
  } else {
    // Need to send both KEYUP and CHAR events.
    key_event.type = KEYEVENT_KEYUP;
    host->SendKeyEvent(key_event);
    key_event.type = KEYEVENT_CHAR;
    host->SendKeyEvent(key_event);
  }

  return TRUE;
}

gint glarea_focus_event(GtkWidget* widget,
                        GdkEventFocus* event,
                        OSRWindow* window) {
  CefRefPtr<CefBrowserHost> host = window->GetBrowserHost();
  host->SendFocusEvent(event->in == TRUE);
  return TRUE;
}

void widget_get_rect_in_screen(GtkWidget* widget, GdkRectangle* r) {
  gint x, y, w, h;
  GdkRectangle extents;

  GdkWindow* window = gtk_widget_get_parent_window(widget);

  // Get parent's left-top screen coordinates.
  gdk_window_get_root_origin(window, &x, &y);
  // Get parent's width and height.
  gdk_drawable_get_size(window, &w, &h);
  // Get parent's extents including decorations.
  gdk_window_get_frame_extents(window, &extents);

  // X and Y calculations assume that left, right and bottom border sizes are
  // all the same.
  const gint border = (extents.width - w) / 2;
  r->x = x + border + widget->allocation.x;
  r->y = y + (extents.height - h) - border + widget->allocation.y;
  r->width = widget->allocation.width;
  r->height = widget->allocation.height;
}

class ScopedGLContext {
 public:
  ScopedGLContext(GtkWidget* widget, bool swap_buffers)
      : swap_buffers_(swap_buffers) {
    GdkGLContext* glcontext = gtk_widget_get_gl_context(widget);
    gldrawable_ = gtk_widget_get_gl_drawable(widget);
    is_valid_ = gdk_gl_drawable_gl_begin(gldrawable_, glcontext);
  }

  virtual ~ScopedGLContext() {
    if (is_valid_) {
      gdk_gl_drawable_gl_end(gldrawable_);

      if(swap_buffers_) {
        if (gdk_gl_drawable_is_double_buffered(gldrawable_))
          gdk_gl_drawable_swap_buffers(gldrawable_);
        else
          glFlush();
      }
    }
  }

  bool IsValid() const { return is_valid_; }

 private:
  bool swap_buffers_;
  GdkGLContext* glcontext_;
  GdkGLDrawable* gldrawable_;
  bool is_valid_;
};

}  // namespace

// static
CefRefPtr<OSRWindow> OSRWindow::Create(OSRBrowserProvider* browser_provider,
                                       bool transparent,
                                       CefWindowHandle parentView) {
  ASSERT(browser_provider);
  if (!browser_provider)
    return NULL;

  return new OSRWindow(browser_provider, transparent, parentView);
}

// static
CefRefPtr<OSRWindow> OSRWindow::From(
    CefRefPtr<ClientHandler::RenderHandler> renderHandler) {
  return static_cast<OSRWindow*>(renderHandler.get());
}

void OSRWindow::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  // Disconnect all signal handlers that reference |this|.
  g_signal_handlers_disconnect_matched(glarea_, G_SIGNAL_MATCH_DATA, 0, 0,
      NULL, NULL, this);

  DisableGL();
}

bool OSRWindow::GetViewRect(CefRefPtr<CefBrowser> browser,
                            CefRect& rect) {
  if (!glarea_)
    return false;

  // The simulated screen and view rectangle are the same. This is necessary
  // for popup menus to be located and sized inside the view.
  rect.x = rect.y = 0;
  rect.width = glarea_->allocation.width;
  rect.height = glarea_->allocation.height;
  return true;
}

bool OSRWindow::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                               int viewX,
                               int viewY,
                               int& screenX,
                               int& screenY) {
  GdkRectangle screen_rect;
  widget_get_rect_in_screen(glarea_, &screen_rect);
  screenX = screen_rect.x + viewX;
  screenY = screen_rect.y + viewY;
  return true;
}

void OSRWindow::OnPopupShow(CefRefPtr<CefBrowser> browser,
                            bool show) {
  if (!show) {
    CefRect dirty_rect = renderer_.popup_rect();
    renderer_.ClearPopupRects();
    browser->GetHost()->Invalidate(dirty_rect, PET_VIEW);
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

  if (!gl_enabled_)
    EnableGL();

  ScopedGLContext scoped_gl_context(glarea_, true);
  if (!scoped_gl_context.IsValid())
    return;

  renderer_.OnPaint(browser, type, dirtyRects, buffer, width, height);
  if (type == PET_VIEW && !renderer_.popup_rect().IsEmpty()) {
    painting_popup_ = true;
    CefRect client_popup_rect(0, 0,
                              renderer_.popup_rect().width,
                              renderer_.popup_rect().height);
    browser->GetHost()->Invalidate(client_popup_rect, PET_POPUP);
    painting_popup_ = false;
  }
  renderer_.Render();
}

void OSRWindow::OnCursorChange(CefRefPtr<CefBrowser> browser,
                               CefCursorHandle cursor) {
  GtkWidget* window = gtk_widget_get_toplevel(glarea_);
  GdkWindow* gdk_window = gtk_widget_get_window(window);
  if (cursor->type == GDK_LAST_CURSOR)
    cursor = NULL;
  gdk_window_set_cursor(gdk_window, cursor);
}

void OSRWindow::Invalidate() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, NewCefRunnableMethod(this, &OSRWindow::Invalidate));
    return;
  }

  // Don't post another task if the previous task is still pending.
  if (render_task_pending_)
    return;

  render_task_pending_ = true;

  // Render at 30fps.
  static const int kRenderDelay = 1000 / 30;
  CefPostDelayedTask(TID_UI, NewCefRunnableMethod(this, &OSRWindow::Render),
                     kRenderDelay);
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

OSRWindow::OSRWindow(OSRBrowserProvider* browser_provider,
                     bool transparent,
                     CefWindowHandle parentView)
    : renderer_(transparent),
      browser_provider_(browser_provider),
      gl_enabled_(false),
      painting_popup_(false),
      render_task_pending_(false) {
  glarea_ = gtk_drawing_area_new();
  ASSERT(glarea_);

  GdkGLConfig* glconfig = gdk_gl_config_new_by_mode(
      static_cast<GdkGLConfigMode>(GDK_GL_MODE_RGB |
                                   GDK_GL_MODE_DEPTH |
                                   GDK_GL_MODE_DOUBLE));
  ASSERT(glconfig);

  gtk_widget_set_gl_capability(glarea_, glconfig, NULL, TRUE,
                               GDK_GL_RGBA_TYPE);

  gtk_widget_set_can_focus(glarea_, TRUE);

  g_signal_connect(G_OBJECT(glarea_), "size_allocate",
                   G_CALLBACK(glarea_size_allocation), this);

  gtk_widget_set_events(glarea_,
                        GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK |
                        GDK_KEY_PRESS_MASK |
                        GDK_KEY_RELEASE_MASK |
                        GDK_ENTER_NOTIFY_MASK |
                        GDK_LEAVE_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK |
                        GDK_POINTER_MOTION_HINT_MASK |
                        GDK_SCROLL_MASK |
                        GDK_FOCUS_CHANGE_MASK);
  g_signal_connect(G_OBJECT(glarea_), "button_press_event",
                   G_CALLBACK(glarea_click_event), this);
  g_signal_connect(G_OBJECT(glarea_), "button_release_event",
                   G_CALLBACK(glarea_click_event), this);
  g_signal_connect(G_OBJECT(glarea_), "key_press_event",
                   G_CALLBACK(glarea_key_event), this);
  g_signal_connect(G_OBJECT(glarea_), "key_release_event",
                   G_CALLBACK(glarea_key_event), this);
  g_signal_connect(G_OBJECT(glarea_), "enter_notify_event",
                   G_CALLBACK(glarea_move_event), this);
  g_signal_connect(G_OBJECT(glarea_), "leave_notify_event",
                   G_CALLBACK(glarea_move_event), this);
  g_signal_connect(G_OBJECT(glarea_), "motion_notify_event",
                   G_CALLBACK(glarea_move_event), this);
  g_signal_connect(G_OBJECT(glarea_), "scroll_event",
                   G_CALLBACK(glarea_scroll_event), this);
  g_signal_connect(G_OBJECT(glarea_), "focus_in_event",
                   G_CALLBACK(glarea_focus_event), this);
  g_signal_connect(G_OBJECT(glarea_), "focus_out_event",
                   G_CALLBACK(glarea_focus_event), this);

  gtk_container_add(GTK_CONTAINER(parentView), glarea_);
}

OSRWindow::~OSRWindow() {
}

void OSRWindow::Render() {
  ASSERT(CefCurrentlyOn(TID_UI));
  if (render_task_pending_)
    render_task_pending_ = false;

  if (!gl_enabled_)
    EnableGL();

  ScopedGLContext scoped_gl_context(glarea_, true);
  if (!scoped_gl_context.IsValid())
    return;

  renderer_.Render();
}

void OSRWindow::EnableGL() {
  ASSERT(CefCurrentlyOn(TID_UI));
  if (gl_enabled_)
    return;

  ScopedGLContext scoped_gl_context(glarea_, false);
  if (!scoped_gl_context.IsValid())
    return;

  renderer_.Initialize();

  gl_enabled_ = true;
}

void OSRWindow::DisableGL() {
  ASSERT(CefCurrentlyOn(TID_UI));

  if (!gl_enabled_)
    return;

  ScopedGLContext scoped_gl_context(glarea_, false);
  if (!scoped_gl_context.IsValid())
    return;

  renderer_.Cleanup();

  gl_enabled_ = false;
}

