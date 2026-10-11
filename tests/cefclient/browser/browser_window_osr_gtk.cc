// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/browser_window_osr_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms-compat.h>
#if defined(CEF_TESTS_USE_X11)
#include <gdk/gdkx.h>
#endif
#include <glib-object.h>
#include <gtk/gtk.h>

#include <algorithm>

#include "include/base/cef_logging.h"
#include "include/base/cef_macros.h"
#include "include/views/cef_display.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/cefclient/browser/util_gtk.h"
#include "tests/shared/browser/geometry_util.h"
#include "tests/shared/browser/main_message_loop.h"
#include "tests/shared/browser/osr_gl_linux.h"
#include "tests/shared/browser/osr_keyboard_util_gtk.h"

namespace client {

namespace {

// Static BrowserWindowOsrGtk::EventFilter needs to forward touch events
// to correct browser, so we maintain a vector of all windows.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif
std::vector<BrowserWindowOsrGtk*> g_browser_windows;
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

CefBrowserHost::DragOperationsMask GetDragOperationsMask(
    GdkDragContext* drag_context) {
  int allowed_ops = DRAG_OPERATION_NONE;
  GdkDragAction drag_action = gdk_drag_context_get_actions(drag_context);
  if (drag_action & GDK_ACTION_COPY) {
    allowed_ops |= DRAG_OPERATION_COPY;
  }
  if (drag_action & GDK_ACTION_MOVE) {
    allowed_ops |= DRAG_OPERATION_MOVE;
  }
  if (drag_action & GDK_ACTION_LINK) {
    allowed_ops |= DRAG_OPERATION_LINK;
  }
  if (drag_action & GDK_ACTION_PRIVATE) {
    allowed_ops |= DRAG_OPERATION_PRIVATE;
  }
  return static_cast<CefBrowserHost::DragOperationsMask>(allowed_ops);
}

// Returns the CSS cursor name for |type|, or nullptr for the default cursor.
const char* GetCursorName(cef_cursor_type_t type) {
  switch (type) {
    case CT_CROSS:
      return "crosshair";
    case CT_HAND:
      return "pointer";
    case CT_IBEAM:
      return "text";
    case CT_WAIT:
      return "wait";
    case CT_HELP:
      return "help";
    case CT_EASTRESIZE:
      return "e-resize";
    case CT_NORTHRESIZE:
      return "n-resize";
    case CT_NORTHEASTRESIZE:
      return "ne-resize";
    case CT_NORTHWESTRESIZE:
      return "nw-resize";
    case CT_SOUTHRESIZE:
      return "s-resize";
    case CT_SOUTHEASTRESIZE:
      return "se-resize";
    case CT_SOUTHWESTRESIZE:
      return "sw-resize";
    case CT_WESTRESIZE:
      return "w-resize";
    case CT_NORTHSOUTHRESIZE:
      return "ns-resize";
    case CT_EASTWESTRESIZE:
      return "ew-resize";
    case CT_NORTHEASTSOUTHWESTRESIZE:
      return "nesw-resize";
    case CT_NORTHWESTSOUTHEASTRESIZE:
      return "nwse-resize";
    case CT_COLUMNRESIZE:
      return "col-resize";
    case CT_ROWRESIZE:
      return "row-resize";
    case CT_MIDDLEPANNING:
    case CT_EASTPANNING:
    case CT_NORTHPANNING:
    case CT_NORTHEASTPANNING:
    case CT_NORTHWESTPANNING:
    case CT_SOUTHPANNING:
    case CT_SOUTHEASTPANNING:
    case CT_SOUTHWESTPANNING:
    case CT_WESTPANNING:
    case CT_MIDDLE_PANNING_VERTICAL:
    case CT_MIDDLE_PANNING_HORIZONTAL:
      return "all-scroll";
    case CT_MOVE:
    case CT_DND_MOVE:
      return "move";
    case CT_VERTICALTEXT:
      return "vertical-text";
    case CT_CELL:
      return "cell";
    case CT_CONTEXTMENU:
      return "context-menu";
    case CT_ALIAS:
    case CT_DND_LINK:
      return "alias";
    case CT_PROGRESS:
      return "progress";
    case CT_NODROP:
    case CT_DND_NONE:
      return "no-drop";
    case CT_COPY:
    case CT_DND_COPY:
      return "copy";
    case CT_NONE:
      return "none";
    case CT_NOTALLOWED:
      return "not-allowed";
    case CT_ZOOMIN:
      return "zoom-in";
    case CT_ZOOMOUT:
      return "zoom-out";
    case CT_GRAB:
      return "grab";
    case CT_GRABBING:
      return "grabbing";
    default:
      return nullptr;
  }
}

// Returns a new cursor reference, or nullptr for the default cursor.
GdkCursor* CreateGdkCursor(GdkDisplay* display,
                           cef_cursor_type_t type,
                           const CefCursorInfo& custom_cursor_info) {
  if (type == CT_CUSTOM) {
    const int width = custom_cursor_info.size.width;
    const int height = custom_cursor_info.size.height;
    if (!custom_cursor_info.buffer || width <= 0 || height <= 0) {
      return nullptr;
    }

    // Convert from premultiplied BGRA to non-premultiplied RGBA.
    GdkPixbuf* pixbuf =
        gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    const int stride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar* dst = gdk_pixbuf_get_pixels(pixbuf);
    const auto* src = static_cast<const uint8_t*>(custom_cursor_info.buffer);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const uint8_t* s = src + (y * width + x) * 4;
        guchar* d = dst + y * stride + x * 4;
        const uint8_t a = s[3];
        d[0] = a ? s[2] * 255 / a : 0;
        d[1] = a ? s[1] * 255 / a : 0;
        d[2] = a ? s[0] * 255 / a : 0;
        d[3] = a;
      }
    }

    GdkCursor* cursor = gdk_cursor_new_from_pixbuf(
        display, pixbuf, custom_cursor_info.hotspot.x,
        custom_cursor_info.hotspot.y);
    g_object_unref(pixbuf);
    return cursor;
  }

  const char* name = GetCursorName(type);
  if (!name) {
    return nullptr;
  }
  return gdk_cursor_new_from_name(display, name);
}

}  // namespace

// Calls CefBrowserHost::SendExternalBeginFrame at a fixed rate on the UI
// thread. Reference counted so that pending tasks remain valid after the
// browser window is destroyed.
class BrowserWindowOsrGtk::BeginFrameTimer
    : public base::RefCountedThreadSafe<BeginFrameTimer> {
 public:
  BeginFrameTimer(CefRefPtr<CefBrowser> browser, int frame_rate)
      : browser_(browser),
        interval_ms_(std::max(1, 1000 / std::max(1, frame_rate))) {}

  void Start() {
    CEF_REQUIRE_UI_THREAD();
    Tick();
  }

  void Stop() {
    CEF_REQUIRE_UI_THREAD();
    browser_ = nullptr;
  }

 private:
  friend class base::RefCountedThreadSafe<BeginFrameTimer>;
  ~BeginFrameTimer() = default;

  void Tick() {
    CEF_REQUIRE_UI_THREAD();
    if (!browser_) {
      return;
    }
    browser_->GetHost()->SendExternalBeginFrame();
    CefPostDelayedTask(TID_UI,
                       base::BindOnce(&BeginFrameTimer::Tick,
                                      scoped_refptr<BeginFrameTimer>(this)),
                       interval_ms_);
  }

  CefRefPtr<CefBrowser> browser_;
  const int64_t interval_ms_;
};

BrowserWindowOsrGtk::BrowserWindowOsrGtk(BrowserWindow::Delegate* delegate,
                                         bool with_controls,
                                         const std::string& startup_url,
                                         const OsrRendererSettings& settings)
    : BrowserWindow(delegate),
      settings_(settings),
      renderer_(settings.background_color, settings.show_update_rect),
      gl_enabled_(false),
      hidden_(false),
      widget_(nullptr),
      drag_trigger_event_(nullptr),
      drag_data_(nullptr),
      drag_operation_(DRAG_OPERATION_NONE),
      drag_context_(nullptr),
      drag_targets_(gtk_target_list_new(nullptr, 0)),
      drag_leave_(false),
      drag_drop_(false),
      device_scale_factor_(1.0f) {
  client_handler_ =
      new ClientHandlerOsr(this, this, with_controls, startup_url);
  g_browser_windows.push_back(this);
}

BrowserWindowOsrGtk::~BrowserWindowOsrGtk() {
  g_browser_windows.erase(
      std::find(g_browser_windows.begin(), g_browser_windows.end(), this));
  ScopedGdkThreadsEnter scoped_gdk_threads;

  if (drag_trigger_event_) {
    gdk_event_free(drag_trigger_event_);
  }
  if (drag_context_) {
    g_object_unref(drag_context_);
  }
  gtk_target_list_unref(drag_targets_);
}

void BrowserWindowOsrGtk::CreateBrowser(
    ClientWindowHandle parent_handle,
    const CefRect& rect,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  REQUIRE_MAIN_THREAD();

  // Windowless rendering requires Alloy style.
  DCHECK(delegate_->UseAlloyStyle());

  // Create the native window.
  Create(parent_handle);

  ScopedGdkThreadsEnter scoped_gdk_threads;

  // Retrieve the X11 Window ID for the GTK parent window. The parent window is
  // optional and Wayland has no equivalent.
  CefWindowHandle handle = kNullWindowHandle;
#if defined(CEF_TESTS_USE_X11)
  GtkWidget* window =
      gtk_widget_get_ancestor(GTK_WIDGET(parent_handle), GTK_TYPE_WINDOW);
  GdkWindow* gdk_window = gtk_widget_get_window(window);
  if (GDK_IS_X11_WINDOW(gdk_window)) {
    handle = GDK_WINDOW_XID(gdk_window);
    DCHECK(handle);
  }
#endif  // defined(CEF_TESTS_USE_X11)

  CefWindowInfo window_info;
  window_info.SetAsWindowless(handle);

  window_info.shared_texture_enabled = settings_.shared_texture_enabled;
  window_info.external_begin_frame_enabled =
      settings_.external_begin_frame_enabled;

  // Windowless rendering requires Alloy style.
  DCHECK_EQ(CEF_RUNTIME_STYLE_ALLOY, window_info.runtime_style);

  // Create the browser asynchronously.
  CefBrowserHost::CreateBrowser(window_info, client_handler_,
                                client_handler_->startup_url(), settings,
                                extra_info, request_context);
}

void BrowserWindowOsrGtk::GetPopupConfig(CefWindowHandle temp_handle,
                                         CefWindowInfo& windowInfo,
                                         CefRefPtr<CefClient>& client,
                                         CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();

  windowInfo.SetAsWindowless(temp_handle);

  // Windowless rendering requires Alloy style.
  DCHECK_EQ(CEF_RUNTIME_STYLE_ALLOY, windowInfo.runtime_style);

  windowInfo.shared_texture_enabled = settings_.shared_texture_enabled;
  windowInfo.external_begin_frame_enabled =
      settings_.external_begin_frame_enabled;

  client = client_handler_;
}

void BrowserWindowOsrGtk::ShowPopup(ClientWindowHandle parent_handle,
                                    int x,
                                    int y,
                                    size_t width,
                                    size_t height) {
  REQUIRE_MAIN_THREAD();
  DCHECK(browser_.get());

  // Create the native window.
  Create(parent_handle);

  // Send resize notification so the compositor is assigned the correct
  // viewport size and begins rendering.
  browser_->GetHost()->WasResized();

  Show();
}

void BrowserWindowOsrGtk::Show() {
  REQUIRE_MAIN_THREAD();

  if (hidden_) {
    // Set the browser as visible.
    browser_->GetHost()->WasHidden(false);
    hidden_ = false;
  }

  // Give focus to the browser.
  browser_->GetHost()->SetFocus(true);
}

void BrowserWindowOsrGtk::Hide() {
  REQUIRE_MAIN_THREAD();

  if (!browser_) {
    return;
  }

  // Remove focus from the browser.
  browser_->GetHost()->SetFocus(false);

  if (!hidden_) {
    // Set the browser as hidden.
    browser_->GetHost()->WasHidden(true);
    hidden_ = true;
  }
}

void BrowserWindowOsrGtk::SetBounds(int x, int y, size_t width, size_t height) {
  REQUIRE_MAIN_THREAD();
  // Nothing to do here. GTK will take care of positioning in the container.
}

void BrowserWindowOsrGtk::SetFocus(bool focus) {
  REQUIRE_MAIN_THREAD();
  if (widget_ && focus) {
    gtk_widget_grab_focus(widget_);
  }
}

void BrowserWindowOsrGtk::SetDeviceScaleFactor(float device_scale_factor) {
  REQUIRE_MAIN_THREAD();
  {
    base::AutoLock lock_scope(lock_);
    if (device_scale_factor == device_scale_factor_) {
      return;
    }

    // Apply some sanity checks.
    if (device_scale_factor < 0.5f || device_scale_factor > 4.0f) {
      return;
    }

    device_scale_factor_ = device_scale_factor;
  }

  if (browser_) {
    browser_->GetHost()->NotifyScreenInfoChanged();
    browser_->GetHost()->WasResized();
  }
}

float BrowserWindowOsrGtk::GetDeviceScaleFactor() const {
  REQUIRE_MAIN_THREAD();
  base::AutoLock lock_scope(lock_);
  return device_scale_factor_;
}

ClientWindowHandle BrowserWindowOsrGtk::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();
  return widget_;
}

void BrowserWindowOsrGtk::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  if (settings_.external_begin_frame_enabled) {
    // The client drives frame production, so frames are only produced while
    // the timer is running.
    begin_frame_timer_ = base::MakeRefCounted<BeginFrameTimer>(
        browser, settings_.begin_frame_rate);
    begin_frame_timer_->Start();
  }
}

void BrowserWindowOsrGtk::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  if (begin_frame_timer_) {
    begin_frame_timer_->Stop();
    begin_frame_timer_ = nullptr;
  }

  // Detach |this| from the ClientHandlerOsr.
  auto handler = ClientHandlerOsr::GetForClient(client_handler_);
  CHECK(handler);
  handler->DetachOsrDelegate();

  ScopedGdkThreadsEnter scoped_gdk_threads;

  UnregisterDragDrop();

  // Disconnect all signal handlers that reference |this|.
  g_signal_handlers_disconnect_matched(widget_, G_SIGNAL_MATCH_DATA, 0, 0,
                                       nullptr, nullptr, this);

  DisableGL();
}

bool BrowserWindowOsrGtk::GetRootScreenRect(CefRefPtr<CefBrowser> browser,
                                            CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  if (!settings_.real_screen_bounds) {
    return false;
  }

  if (!widget_) {
    return false;
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  GtkWidget* toplevel = gtk_widget_get_toplevel(widget_);

  // Convert to DIP coordinates.
  rect = DeviceToLogical(
      GetWindowBounds(GTK_WINDOW(toplevel), /*include_frame=*/true),
      device_scale_factor);
  return true;
}

void BrowserWindowOsrGtk::GetViewRect(CefRefPtr<CefBrowser> browser,
                                      CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  if (!widget_) {
    // Never return an empty rectangle.
    rect.width = rect.height = 1;
    return;
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  GtkAllocation allocation = {};
  gtk_widget_get_allocation(widget_, &allocation);

  // Convert to DIP coordinates.
  rect = DeviceToLogical(
      {allocation.x, allocation.y, allocation.width, allocation.height},
      device_scale_factor);
  if (rect.width == 0) {
    rect.width = 1;
  }
  if (rect.height == 0) {
    rect.height = 1;
  }
  if (!settings_.real_screen_bounds) {
    rect.x = rect.y = 0;
  }
}

bool BrowserWindowOsrGtk::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                                         int viewX,
                                         int viewY,
                                         int& screenX,
                                         int& screenY) {
  CEF_REQUIRE_UI_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  // Get the widget position in the window.
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget_, &allocation);

  // Convert from view DIP coordinates to window (pixel) coordinates.
  screenX = allocation.x + LogicalToDevice(viewX, device_scale_factor);
  screenY = allocation.y + LogicalToDevice(viewY, device_scale_factor);
  return true;
}

bool BrowserWindowOsrGtk::GetScreenInfo(CefRefPtr<CefBrowser> browser,
                                        CefScreenInfo& screen_info) {
  CEF_REQUIRE_UI_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  screen_info.device_scale_factor = device_scale_factor;

  if (settings_.real_screen_bounds) {
    CefRect root_rect;
    GetRootScreenRect(browser, root_rect);

    auto display = CefDisplay::GetDisplayMatchingBounds(
        root_rect, /*input_pixel_coords=*/false);
    screen_info.rect = display->GetBounds();
    screen_info.available_rect = display->GetWorkArea();
  } else {
    CefRect view_rect;
    GetViewRect(browser, view_rect);

    // Keep HTML select popups inside the view rectangle.
    screen_info.rect = view_rect;
    screen_info.available_rect = view_rect;
  }

  return true;
}

void BrowserWindowOsrGtk::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                      bool show) {
  CEF_REQUIRE_UI_THREAD();

  {
    // The renderer is also accessed on the main thread when drawing.
    ScopedGdkThreadsEnter scoped_gdk_threads;
    renderer_.OnPopupShow(show);
  }
  if (!show) {
    browser->GetHost()->Invalidate(PET_VIEW);
  }
}

void BrowserWindowOsrGtk::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                      const CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  // The renderer is also accessed on the main thread when drawing.
  ScopedGdkThreadsEnter scoped_gdk_threads;
  renderer_.OnPopupSize(LogicalToDevice(rect, device_scale_factor));
}

void BrowserWindowOsrGtk::OnPaint(CefRefPtr<CefBrowser> browser,
                                  CefRenderHandler::PaintElementType type,
                                  const CefRenderHandler::RectList& dirtyRects,
                                  const void* buffer,
                                  int width,
                                  int height) {
  CEF_REQUIRE_UI_THREAD();

  if (width <= 2 && height <= 2) {
    // Ignore really small buffer sizes while the widget is starting up.
    return;
  }

  if (!EnableGL()) {
    return;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;
  if (!surface_->MakeCurrent()) {
    return;
  }
  const bool updated =
      renderer_.OnPaint(type, dirtyRects, buffer, width, height);
  surface_->ReleaseCurrent();
  if (updated) {
    surface_->Invalidate();
  }
}

void BrowserWindowOsrGtk::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirtyRects,
    const CefAcceleratedPaintInfo& info) {
  CEF_REQUIRE_UI_THREAD();

  if (!EnableGL()) {
    return;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;
  if (!surface_->MakeCurrent()) {
    return;
  }
  // Copies the frame before returning, as CEF reuses the buffer.
  const bool updated = renderer_.OnAcceleratedPaint(type, dirtyRects, info);
  surface_->ReleaseCurrent();
  if (updated) {
    surface_->Invalidate();
  }
}

void BrowserWindowOsrGtk::RenderSurface(unsigned int framebuffer,
                                        int width,
                                        int height) {
  REQUIRE_MAIN_THREAD();

  // Draw the most recent frame scaled to the surface, including while waiting
  // for a frame at a new size. Called with the GDK lock held.
  if (!gl_enabled_ || !renderer_.Render(framebuffer, width, height)) {
    const gl::Api* api = gl::GetApi();
    if (api) {
      api->glBindFramebuffer(gl::kGlFramebuffer, framebuffer);
      api->glClearColor(0, 0, 0, 0);
      api->glClear(gl::kGlColorBufferBit);
    }
  }
}

void BrowserWindowOsrGtk::OnCursorChange(
    CefRefPtr<CefBrowser> browser,
    CefCursorHandle cursor,
    cef_cursor_type_t type,
    const CefCursorInfo& custom_cursor_info) {
  CEF_REQUIRE_UI_THREAD();

  ScopedGdkThreadsEnter scoped_gdk_threads;

  GdkWindow* gdk_window = gtk_widget_get_window(widget_);
  if (!gdk_window) {
    return;
  }

  GdkCursor* gdk_cursor = CreateGdkCursor(gdk_window_get_display(gdk_window),
                                          type, custom_cursor_info);
  gdk_window_set_cursor(gdk_window, gdk_cursor);
  if (gdk_cursor) {
    g_object_unref(gdk_cursor);
  }
}

bool BrowserWindowOsrGtk::StartDragging(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDragData> drag_data,
    CefRenderHandler::DragOperationsMask allowed_ops,
    int x,
    int y) {
  CEF_REQUIRE_UI_THREAD();

  if (!drag_data->HasImage()) {
    LOG(ERROR) << "Drag image representation not available";
    return false;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  DragReset();
  drag_data_ = drag_data;

  // Begin drag.
  if (drag_trigger_event_) {
    LOG(ERROR) << "Dragging started, but last mouse event is missing";
    DragReset();
    return false;
  }
  drag_context_ = gtk_drag_begin(widget_, drag_targets_, GDK_ACTION_COPY,
                                 1,  // left mouse button
                                 drag_trigger_event_);
  if (!drag_context_) {
    LOG(ERROR) << "GTK drag begin failed";
    DragReset();
    return false;
  }
  g_object_ref(drag_context_);

  // Send drag enter event.
  CefMouseEvent ev;
  ev.x = x;
  ev.y = y;
  ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
  browser->GetHost()->DragTargetDragEnter(drag_data, ev, allowed_ops);

  return true;
}

void BrowserWindowOsrGtk::UpdateDragCursor(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::DragOperation operation) {
  CEF_REQUIRE_UI_THREAD();
  drag_operation_ = operation;
}

void BrowserWindowOsrGtk::OnImeCompositionRangeChanged(
    CefRefPtr<CefBrowser> browser,
    const CefRange& selection_range,
    const CefRenderHandler::RectList& character_bounds) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrGtk::UpdateAccessibilityTree(CefRefPtr<CefValue> value) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrGtk::UpdateAccessibilityLocation(
    CefRefPtr<CefValue> value) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrGtk::Create(ClientWindowHandle parent_handle) {
  REQUIRE_MAIN_THREAD();
  DCHECK(!widget_);

  ScopedGdkThreadsEnter scoped_gdk_threads;

  surface_ = OsrGlSurfaceGtk::Create(
      [this](unsigned int framebuffer, int width, int height) {
        RenderSurface(framebuffer, width, height);
      });
  widget_ = surface_->widget();
  DCHECK(widget_);

  gtk_widget_set_can_focus(widget_, TRUE);

  g_signal_connect(G_OBJECT(widget_), "size_allocate",
                   G_CALLBACK(&BrowserWindowOsrGtk::SizeAllocation), this);

  gtk_widget_set_events(
      widget_, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                   GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
                   GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                   GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
                   GDK_SCROLL_MASK | GDK_FOCUS_CHANGE_MASK);
  g_signal_connect(G_OBJECT(widget_), "button_press_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::ClickEvent), this);
  g_signal_connect(G_OBJECT(widget_), "button_release_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::ClickEvent), this);
  g_signal_connect(G_OBJECT(widget_), "key_press_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::KeyEvent), this);
  g_signal_connect(G_OBJECT(widget_), "key_release_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::KeyEvent), this);
  g_signal_connect(G_OBJECT(widget_), "enter_notify_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::MoveEvent), this);
  g_signal_connect(G_OBJECT(widget_), "leave_notify_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::MoveEvent), this);
  g_signal_connect(G_OBJECT(widget_), "motion_notify_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::MoveEvent), this);
  g_signal_connect(G_OBJECT(widget_), "scroll_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::ScrollEvent), this);
  g_signal_connect(G_OBJECT(widget_), "focus_in_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::FocusEvent), this);
  g_signal_connect(G_OBJECT(widget_), "focus_out_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::FocusEvent), this);
  g_signal_connect(G_OBJECT(widget_), "touch-event",
                   G_CALLBACK(&BrowserWindowOsrGtk::TouchEvent), this);

  RegisterDragDrop();

  gtk_widget_set_vexpand(widget_, TRUE);
  gtk_grid_attach(GTK_GRID(parent_handle), widget_, 0, 3, 1, 1);

  // Make the GlArea visible in the parent container.
  gtk_widget_show_all(parent_handle);
}

// static
gint BrowserWindowOsrGtk::SizeAllocation(GtkWidget* widget,
                                         GtkAllocation* allocation,
                                         BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();
  if (self->browser_.get()) {
    // Results in a call to GetViewRect().
    self->browser_->GetHost()->WasResized();
  }
  return TRUE;
}

// static
gint BrowserWindowOsrGtk::ClickEvent(GtkWidget* widget,
                                     GdkEventButton* event,
                                     BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (!self->browser_.get()) {
    return TRUE;
  }

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

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

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  CefMouseEvent mouse_event;
  mouse_event.x = event->x;
  mouse_event.y = event->y;
  self->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  DeviceToLogical(mouse_event, device_scale_factor);
  mouse_event.modifiers = GetCefStateModifiers(event->state);

  bool mouse_up = (event->type == GDK_BUTTON_RELEASE);
  if (!mouse_up) {
    gtk_widget_grab_focus(widget);
  }

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

  // Save mouse event that can be a possible trigger for drag.
  if (!self->drag_context_ && button_type == MBT_LEFT) {
    if (self->drag_trigger_event_) {
      gdk_event_free(self->drag_trigger_event_);
    }
    self->drag_trigger_event_ =
        gdk_event_copy(reinterpret_cast<GdkEvent*>(event));
  }

  return TRUE;
}

// static
gint BrowserWindowOsrGtk::KeyEvent(GtkWidget* widget,
                                   GdkEventKey* event,
                                   BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (!self->browser_.get()) {
    return TRUE;
  }

#if CEF_API_ADDED(CEF_NEXT)
  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  // GTK OSR supports X11 and Wayland. Keep all toolkit objects and
  // source keymap interpretation here; only the portable payload crosses CEF.
#if defined(CEF_TESTS_USE_X11)
  if (event->type == GDK_KEY_RELEASE &&
      GDK_IS_X11_DISPLAY(gdk_window_get_display(event->window))) {
    // Only X11 represents repeat with a synthetic release/press pair. Wayland
    // emits repeated presses, and every release must clear the pressed state.
    GdkEvent* next = gdk_event_peek();
    const bool repeat_release =
        next && next->type == GDK_KEY_PRESS &&
        next->key.window == event->window &&
        next->key.hardware_keycode == event->hardware_keycode &&
        next->key.time == event->time;
    if (next) {
      gdk_event_free(next);
    }
    if (repeat_release) {
      return TRUE;
    }
  }
#endif  // defined(CEF_TESTS_USE_X11)
  bool repeat = false;
  {
    base::AutoLock lock(self->lock_);
    if (event->type == GDK_KEY_PRESS) {
      repeat = !self->pressed_keycodes_.insert(event->hardware_keycode).second;
    } else {
      self->pressed_keycodes_.erase(event->hardware_keycode);
    }
  }
  std::vector<CefKeyboardInputEvent> events;
  if (!KeyboardInputEventsFromGdkEvent(event, repeat, events)) {
    return FALSE;
  }
  for (const auto& input : events) {
    host->SendKeyboardEvent(input);
  }
#else
  // Supporting the legacy keyboard API here would require maintaining copies
  // of Chromium's key mapping tables. Use the unified API instead.
  NOTIMPLEMENTED();
#endif  // CEF_API_ADDED(CEF_NEXT)
  return TRUE;
}

// static
gint BrowserWindowOsrGtk::MoveEvent(GtkWidget* widget,
                                    GdkEventMotion* event,
                                    BrowserWindowOsrGtk* self) {
  if (!self->browser_.get()) {
    return TRUE;
  }

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  gint x, y;
  GdkModifierType state;

  if (event->is_hint) {
    gdk_window_get_pointer(event->window, &x, &y, &state);
  } else {
    x = (gint)event->x;
    y = (gint)event->y;
    state = (GdkModifierType)event->state;
    if (x == 0 && y == 0) {
      // Invalid coordinates of (0,0) appear from time to time in
      // enter-notify-event and leave-notify-event events. Sending them may
      // cause StartDragging to never get called, so just ignore these.
      return TRUE;
    }
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  CefMouseEvent mouse_event;
  mouse_event.x = x;
  mouse_event.y = y;
  self->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  DeviceToLogical(mouse_event, device_scale_factor);
  mouse_event.modifiers = GetCefStateModifiers(state);

  bool mouse_leave = (event->type == GDK_LEAVE_NOTIFY);
  host->SendMouseMoveEvent(mouse_event, mouse_leave);

  // Save mouse event that can be a possible trigger for drag.
  if (!self->drag_context_ &&
      (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)) {
    if (self->drag_trigger_event_) {
      gdk_event_free(self->drag_trigger_event_);
    }
    self->drag_trigger_event_ =
        gdk_event_copy(reinterpret_cast<GdkEvent*>(event));
  }

  return TRUE;
}

// static
gint BrowserWindowOsrGtk::ScrollEvent(GtkWidget* widget,
                                      GdkEventScroll* event,
                                      BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (!self->browser_.get()) {
    return TRUE;
  }

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  CefMouseEvent mouse_event;
  mouse_event.x = event->x;
  mouse_event.y = event->y;
  self->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  DeviceToLogical(mouse_event, device_scale_factor);
  mouse_event.modifiers = GetCefStateModifiers(event->state);
  mouse_event.modifiers |= EVENTFLAG_PRECISION_SCROLLING_DELTA;

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
    case GDK_SCROLL_SMOOTH:
      NOTIMPLEMENTED();
      break;
  }

  host->SendMouseWheelEvent(mouse_event, deltaX, deltaY);
  return TRUE;
}

// static
gint BrowserWindowOsrGtk::FocusEvent(GtkWidget* widget,
                                     GdkEventFocus* event,
                                     BrowserWindowOsrGtk* self) {
  // May be called on the main thread and the UI thread.
  if (!event->in) {
    base::AutoLock lock(self->lock_);
    self->pressed_keycodes_.clear();
  }
  if (self->browser_.get()) {
    self->browser_->GetHost()->SetFocus(event->in == TRUE);
  }
  return TRUE;
}

// static
gboolean BrowserWindowOsrGtk::TouchEvent(GtkWidget* widget,
                                         GdkEventTouch* event,
                                         BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (!self->browser_.get()) {
    return TRUE;
  }

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  CefTouchEvent touch_event;
  switch (event->type) {
    case GDK_TOUCH_BEGIN:
      touch_event.type = CEF_TET_PRESSED;
      break;
    case GDK_TOUCH_UPDATE:
      touch_event.type = CEF_TET_MOVED;
      break;
    case GDK_TOUCH_END:
      touch_event.type = CEF_TET_RELEASED;
      break;
    default:
      return TRUE;
  }

  touch_event.x = event->x;
  touch_event.y = event->y;
  touch_event.radius_x = 0;
  touch_event.radius_y = 0;
  touch_event.rotation_angle = 0;
  touch_event.pressure = 0;
  DeviceToLogical(touch_event, device_scale_factor);
  touch_event.modifiers = GetCefStateModifiers(event->state);

  host->SendTouchEvent(touch_event);
  return TRUE;
}

bool BrowserWindowOsrGtk::IsOverPopupWidget(int x, int y) const {
  const CefRect& rc = renderer_.popup_rect();
  int popup_right = rc.x + rc.width;
  int popup_bottom = rc.y + rc.height;
  return (x >= rc.x) && (x < popup_right) && (y >= rc.y) && (y < popup_bottom);
}

int BrowserWindowOsrGtk::GetPopupXOffset() const {
  return renderer_.original_popup_rect().x - renderer_.popup_rect().x;
}

int BrowserWindowOsrGtk::GetPopupYOffset() const {
  return renderer_.original_popup_rect().y - renderer_.popup_rect().y;
}

void BrowserWindowOsrGtk::ApplyPopupOffset(int& x, int& y) const {
  if (IsOverPopupWidget(x, y)) {
    x += GetPopupXOffset();
    y += GetPopupYOffset();
  }
}

bool BrowserWindowOsrGtk::EnableGL() {
  CEF_REQUIRE_UI_THREAD();

  if (gl_enabled_) {
    return true;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;
  if (!surface_ || !surface_->MakeCurrent()) {
    return false;
  }

  gl_enabled_ = renderer_.Initialize();
  surface_->ReleaseCurrent();
  return gl_enabled_;
}

void BrowserWindowOsrGtk::DisableGL() {
  CEF_REQUIRE_UI_THREAD();

  if (!gl_enabled_) {
    return;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;
  if (surface_->MakeCurrent()) {
    renderer_.Cleanup();
    surface_->ReleaseCurrent();
  }
  surface_->Destroy();

  gl_enabled_ = false;
}

void BrowserWindowOsrGtk::RegisterDragDrop() {
  REQUIRE_MAIN_THREAD();

  ScopedGdkThreadsEnter scoped_gdk_threads;

  // Succession of CEF d&d calls:
  // 1. DragTargetDragEnter
  // 2. DragTargetDragOver
  // 3. DragTargetDragLeave - optional
  // 4. DragSourceSystemDragEnded - optional, to cancel dragging
  // 5. DragTargetDrop
  // 6. DragSourceEndedAt
  // 7. DragSourceSystemDragEnded

  // Succession of GTK d&d events:
  // 1. drag-begin-event, drag-data-get
  // 2. drag-motion
  // 3. drag-leave
  // 4. drag-failed
  // 5. drag-drop, drag-data-received
  // 6. 7. drag-end-event

  // Using gtk_drag_begin in StartDragging instead of calling
  // gtk_drag_source_set here. Doing so because when using gtk_drag_source_set
  // then StartDragging is being called very late, about ten DragMotion events
  // after DragBegin, and drag icon can be set only when beginning drag.
  // Default values for drag threshold are set to 8 pixels in both GTK and
  // Chromium, but doesn't work as expected.
  // --OFF--
  // gtk_drag_source_set(widget_, GDK_BUTTON1_MASK, nullptr, 0,
  // GDK_ACTION_COPY);

  // Source widget events.
  g_signal_connect(G_OBJECT(widget_), "drag_begin",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragBegin), this);
  g_signal_connect(G_OBJECT(widget_), "drag_data_get",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragDataGet), this);
  g_signal_connect(G_OBJECT(widget_), "drag_end",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragEnd), this);

  // Destination widget and its events.
  gtk_drag_dest_set(widget_, (GtkDestDefaults)0, (GtkTargetEntry*)nullptr, 0,
                    (GdkDragAction)GDK_ACTION_COPY);
  g_signal_connect(G_OBJECT(widget_), "drag_motion",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragMotion), this);
  g_signal_connect(G_OBJECT(widget_), "drag_leave",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragLeave), this);
  g_signal_connect(G_OBJECT(widget_), "drag_failed",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragFailed), this);
  g_signal_connect(G_OBJECT(widget_), "drag_drop",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragDrop), this);
  g_signal_connect(G_OBJECT(widget_), "drag_data_received",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragDataReceived), this);
}

void BrowserWindowOsrGtk::UnregisterDragDrop() {
  ScopedGdkThreadsEnter scoped_gdk_threads;
  gtk_drag_dest_unset(widget_);
  // Drag events are unregistered in OnBeforeClose by calling
  // g_signal_handlers_disconnect_matched.
}

void BrowserWindowOsrGtk::DragReset() {
  if (drag_trigger_event_) {
    gdk_event_free(drag_trigger_event_);
    drag_trigger_event_ = nullptr;
  }
  drag_data_ = nullptr;
  drag_operation_ = DRAG_OPERATION_NONE;
  if (drag_context_) {
    g_object_unref(drag_context_);
    drag_context_ = nullptr;
  }
  drag_leave_ = false;
  drag_drop_ = false;
}

// static
void BrowserWindowOsrGtk::DragBegin(GtkWidget* widget,
                                    GdkDragContext* drag_context,
                                    BrowserWindowOsrGtk* self) {
  // Load drag icon.
  if (!self->drag_data_->HasImage()) {
    LOG(ERROR) << "Failed to set drag icon, drag image not available";
    return;
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  int pixel_width = 0;
  int pixel_height = 0;
  CefRefPtr<CefBinaryValue> image_binary =
      self->drag_data_->GetImage()->GetAsPNG(device_scale_factor, true,
                                             pixel_width, pixel_height);
  if (!image_binary) {
    LOG(ERROR) << "Failed to set drag icon, drag image error";
    return;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  size_t image_size = image_binary->GetSize();
  guint8* image_buffer = (guint8*)malloc(image_size);  // must free
  image_binary->GetData((void*)image_buffer, image_size, 0);
  GdkPixbufLoader* loader = nullptr;  // must unref
  GError* error = nullptr;            // must free
  GdkPixbuf* pixbuf = nullptr;        // owned by loader
  gboolean success = FALSE;
  loader = gdk_pixbuf_loader_new_with_type("png", &error);
  if (error == nullptr && loader) {
    success =
        gdk_pixbuf_loader_write(loader, image_buffer, image_size, nullptr);
    if (success) {
      success = gdk_pixbuf_loader_close(loader, nullptr);
      if (success) {
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pixbuf) {
          CefPoint image_hotspot = self->drag_data_->GetImageHotspot();
          int hotspot_x = image_hotspot.x;
          int hotspot_y = image_hotspot.y;
          gtk_drag_set_icon_pixbuf(drag_context, pixbuf, hotspot_x, hotspot_y);
        } else {
          LOG(ERROR) << "Failed to set drag icon, pixbuf error";
        }
      } else {
        LOG(ERROR) << "Failed to set drag icon, loader close error";
      }
    } else {
      LOG(ERROR) << "Failed to set drag icon, loader write error";
    }
  } else {
    LOG(ERROR) << "Failed to set drag icon, loader creation error";
  }
  if (loader) {
    g_object_unref(loader);  // unref
  }
  if (error) {
    g_error_free(error);  // free
  }
  free(image_buffer);  // free
}

// static
void BrowserWindowOsrGtk::DragDataGet(GtkWidget* widget,
                                      GdkDragContext* drag_context,
                                      GtkSelectionData* data,
                                      guint info,
                                      guint time,
                                      BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();
  // No drag targets are set so this callback is never called.
}

// static
void BrowserWindowOsrGtk::DragEnd(GtkWidget* widget,
                                  GdkDragContext* drag_context,
                                  BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (self->browser_) {
    // Sometimes there is DragEnd event generated without prior DragDrop.
    // Maybe related to drag-leave bug described in comments in DragLeave.
    if (!self->drag_drop_) {
      // Real coordinates not available.
      self->browser_->GetHost()->DragSourceEndedAt(-1, -1,
                                                   self->drag_operation_);
    }
    self->browser_->GetHost()->DragSourceSystemDragEnded();
  }

  self->DragReset();
}

// static
gboolean BrowserWindowOsrGtk::DragMotion(GtkWidget* widget,
                                         GdkDragContext* drag_context,
                                         gint x,
                                         gint y,
                                         guint time,
                                         BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  // MoveEvent is never called during drag & drop, so must call
  // SendMouseMoveEvent here.
  CefMouseEvent mouse_event;
  mouse_event.x = x;
  mouse_event.y = y;
  mouse_event.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
  self->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  DeviceToLogical(mouse_event, device_scale_factor);
  if (self->browser_) {
    bool mouse_leave = self->drag_leave_;
    self->browser_->GetHost()->SendMouseMoveEvent(mouse_event, mouse_leave);
  }

  // Mouse event.
  CefMouseEvent ev;
  ev.x = x;
  ev.y = y;
  ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;

  CefBrowserHost::DragOperationsMask allowed_ops =
      GetDragOperationsMask(drag_context);

  // Send drag enter event if needed.
  if (self->drag_leave_ && self->browser_) {
    self->browser_->GetHost()->DragTargetDragEnter(self->drag_data_, ev,
                                                   allowed_ops);
  }

  // Send drag over event.
  if (self->browser_) {
    self->browser_->GetHost()->DragTargetDragOver(ev, allowed_ops);
  }

  // Update GTK drag status.
  if (widget == self->widget_) {
    gdk_drag_status(drag_context, GDK_ACTION_COPY, time);
    if (self->drag_leave_) {
      self->drag_leave_ = false;
    }
    return TRUE;
  } else {
    LOG(WARNING) << "Invalid drag destination widget";
    gdk_drag_status(drag_context, (GdkDragAction)0, time);
    return FALSE;
  }
}

// static
void BrowserWindowOsrGtk::DragLeave(GtkWidget* widget,
                                    GdkDragContext* drag_context,
                                    guint time,
                                    BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  // There is no drag-enter event in GTK. The first drag-motion event
  // after drag-leave will be a drag-enter event.

  // There seems to be a bug during GTK drop, drag-leave event is generated
  // just before drag-drop. A solution is to call DragTargetDragEnter
  // and DragTargetDragOver in DragDrop when drag_leave_ is true.

  // Send drag leave event.
  if (self->browser_) {
    self->browser_->GetHost()->DragTargetDragLeave();
  }

  self->drag_leave_ = true;
}

// static
gboolean BrowserWindowOsrGtk::DragFailed(GtkWidget* widget,
                                         GdkDragContext* drag_context,
                                         GtkDragResult result,
                                         BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  // Send drag end coordinates and system drag ended event.
  if (self->browser_) {
    // Real coordinates not available.
    self->browser_->GetHost()->DragSourceEndedAt(-1, -1, self->drag_operation_);
    self->browser_->GetHost()->DragSourceSystemDragEnded();
  }

  self->DragReset();
  return TRUE;
}

// static
gboolean BrowserWindowOsrGtk::DragDrop(GtkWidget* widget,
                                       GdkDragContext* drag_context,
                                       gint x,
                                       gint y,
                                       guint time,
                                       BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  // Finish GTK drag.
  gtk_drag_finish(drag_context, TRUE, FALSE, time);

  // Mouse event.
  CefMouseEvent ev;
  ev.x = x;
  ev.y = y;
  ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;

  CefBrowserHost::DragOperationsMask allowed_ops =
      GetDragOperationsMask(drag_context);

  // Send drag enter/over events if needed (read comment in DragLeave).
  if (self->drag_leave_ && self->browser_) {
    self->browser_->GetHost()->DragTargetDragEnter(self->drag_data_, ev,
                                                   allowed_ops);
    self->browser_->GetHost()->DragTargetDragOver(ev, allowed_ops);
  }

  // Send drag drop event.
  if (self->browser_) {
    self->browser_->GetHost()->DragTargetDrop(ev);
  }

  // Send drag end coordinates.
  if (self->browser_) {
    self->browser_->GetHost()->DragSourceEndedAt(x, y, self->drag_operation_);
  }

  self->drag_drop_ = true;
  return TRUE;
}

// static
void BrowserWindowOsrGtk::DragDataReceived(GtkWidget* widget,
                                           GdkDragContext* drag_context,
                                           gint x,
                                           gint y,
                                           GtkSelectionData* data,
                                           guint info,
                                           guint time,
                                           BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();
  // This callback is never called because DragDrop does not call
  // gtk_drag_get_data, as only dragging inside web view is supported.
}

}  // namespace client
