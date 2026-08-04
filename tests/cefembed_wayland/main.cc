// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Minimal Wayland client that embeds a CEF browser inside its own window, the
// way OBS embeds a browser dock or Steam embeds its store view.
//
// cefclient's Linux windowing path is GTK over X11 and hands CEF an XID
// (GDK_WINDOW_XID in root_window_gtk.cc, temp_window_x11.cc), so there is no
// wl_surface to pass through it. This exists to exercise the three-argument
// CefWindowInfo::SetAsChild() end to end:
//
//   * open a wl_display and hand it to CEF before CefInitialize;
//   * create an xdg_toplevel and paint a plain host UI into it with wl_shm;
//   * create a browser as a wl_subsurface of that surface;
//   * drive both event loops from one thread.
//
// The layout mirrors the host applications this is meant to serve: a sidebar
// the client owns, and a content rectangle handed to the browser. If embedding
// fails, that rectangle stays the colour the client painted, which makes the
// failure visible rather than silent.

#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "cursor-shape-v1-client-protocol.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/wrapper/cef_helpers.h"
#include "xdg-shell-client-protocol.h"

namespace {

constexpr int kWidth = 1280;
constexpr int kHeight = 800;
constexpr int kSidebarWidth = 200;
constexpr int kHeaderHeight = 48;
// GNOME does not advertise zxdg_decoration_manager_v1, so there is no way to
// ask for server-side decorations: the client has to draw its own and drive
// moving and resizing itself. This is the grab area along the window edges.
constexpr int kResizeBorder = 12;
constexpr int kCloseButtonSize = 40;

// Smallest window this program will paint: enough for the sidebar, the close
// button and a content rectangle that is not empty.
constexpr int kMinWidth = kSidebarWidth + kCloseButtonSize + 2 * kResizeBorder;
constexpr int kMinHeight = kHeaderHeight + 2 * kResizeBorder;

// Colours are XRGB8888, matching wl_shm's default format.
constexpr uint32_t kSidebarColor = 0xff171a21;
constexpr uint32_t kHeaderColor = 0xff101822;
// Deliberately loud: if the browser subsurface never appears, this is what the
// user sees where the page should be.
constexpr uint32_t kVoidColor = 0xff240d0d;
constexpr uint32_t kCloseColor = 0xffc94d3f;
constexpr uint32_t kBorderColor = 0xff2a475e;

// One wl_shm buffer and the mapping behind it. |busy| is true from the moment
// the buffer is attached until the compositor releases it; until then neither
// the wl_buffer nor its storage may be touched.
struct Buffer {
  wl_buffer* buffer = nullptr;
  uint32_t* pixels = nullptr;
  size_t size = 0;
  int width = 0;
  int height = 0;
  bool busy = false;
};

struct Host {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  wl_compositor* compositor = nullptr;
  wl_shm* shm = nullptr;
  xdg_wm_base* wm_base = nullptr;
  wl_seat* seat = nullptr;
  wl_pointer* pointer = nullptr;
  wp_cursor_shape_manager_v1* cursor_shape_manager = nullptr;
  wp_cursor_shape_device_v1* cursor_shape_device = nullptr;
  uint32_t last_enter_serial = 0;
  uint32_t current_cursor = 0;

  // Latest pointer position over our own surface, and the serial of the last
  // button press. xdg_toplevel.move and .resize both need a serial.
  double pointer_x = 0;
  double pointer_y = 0;
  bool pointer_on_host_surface = false;

  wl_surface* surface = nullptr;
  xdg_surface* xdg_surface_obj = nullptr;
  xdg_toplevel* toplevel = nullptr;

  int width = kWidth;
  int height = kHeight;
  bool configured = false;
  bool closed = false;

  // Every buffer this program has allocated. A wl_buffer may not be destroyed
  // or its storage reused until the compositor sends wl_buffer.release; it may
  // still be reading from it to paint the current frame. A resize drag produces
  // a configure per frame, so this is the difference between a correct client
  // and one that shows torn or undefined content while being resized.
  std::vector<std::unique_ptr<Buffer>> buffers;

  // Lives here rather than at namespace scope so that its destructor runs when
  // main() returns instead of at exit time.
  CefRefPtr<CefBrowser> browser;
};

Host* g_host = nullptr;

// --- wl_shm buffer -----------------------------------------------------------

void OnBufferRelease(void* data, wl_buffer* /*buffer*/) {
  static_cast<Buffer*>(data)->busy = false;
}

const wl_buffer_listener kBufferListener = {
    .release = &OnBufferRelease,
};

void DestroyBuffer(Buffer* buffer) {
  if (buffer->buffer) {
    wl_buffer_destroy(buffer->buffer);
  }
  if (buffer->pixels) {
    munmap(buffer->pixels, buffer->size);
  }
}

// Returns a buffer of the current window size that the compositor is not
// reading from. Reuses one when possible; a resize allocates a new one and
// drops any idle buffer of the old size.
Buffer* AcquireBuffer(Host* host) {
  const int width = host->width;
  const int height = host->height;

  // Reap what the compositor has handed back, keeping idle buffers that still
  // match the current size.
  std::erase_if(host->buffers, [&](const std::unique_ptr<Buffer>& buffer) {
    if (buffer->busy || (buffer->width == width && buffer->height == height)) {
      return false;
    }
    DestroyBuffer(buffer.get());
    return true;
  });

  for (const auto& buffer : host->buffers) {
    if (!buffer->busy && buffer->width == width && buffer->height == height) {
      return buffer.get();
    }
  }

  const int stride = width * 4;
  const int size = stride * height;

  char name[] = "/cefembed-XXXXXX";
  const int fd = memfd_create(name, MFD_CLOEXEC);
  if (fd < 0 || ftruncate(fd, size) < 0) {
    fprintf(stderr, "cefembed: failed to allocate a shm buffer\n");
    if (fd >= 0) {
      close(fd);
    }
    return nullptr;
  }

  auto* pixels = static_cast<uint32_t*>(
      mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  if (pixels == MAP_FAILED) {
    close(fd);
    return nullptr;
  }

  wl_shm_pool* pool = wl_shm_create_pool(host->shm, fd, size);
  wl_buffer* wl_buffer_obj = wl_shm_pool_create_buffer(
      pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
  close(fd);

  if (!wl_buffer_obj) {
    munmap(pixels, size);
    fprintf(stderr, "cefembed: wl_shm_pool_create_buffer failed\n");
    return nullptr;
  }

  auto buffer = std::make_unique<Buffer>();
  buffer->buffer = wl_buffer_obj;
  buffer->pixels = pixels;
  buffer->size = static_cast<size_t>(size);
  buffer->width = width;
  buffer->height = height;
  wl_buffer_add_listener(wl_buffer_obj, &kBufferListener, buffer.get());

  host->buffers.push_back(std::move(buffer));
  return host->buffers.back().get();
}

// Clipped to the buffer on both edges. The callers below derive rectangles from
// the window size, and a compositor is free to configure a window smaller than
// any minimum this program has in mind, at which point an unclipped fill writes
// past the end of the mapping.
void FillRect(uint32_t* pixels,
              int pitch,
              int rows,
              int x,
              int y,
              int w,
              int h,
              uint32_t color) {
  const int x0 = std::max(x, 0);
  const int y0 = std::max(y, 0);
  const int x1 = std::min(x + w, pitch);
  const int y1 = std::min(y + h, rows);
  for (int row = y0; row < y1; ++row) {
    for (int col = x0; col < x1; ++col) {
      pixels[row * pitch + col] = color;
    }
  }
}

// Paints the part of the window the client owns. The content rectangle is left
// in kVoidColor so that a missing browser surface is obvious.
void PaintHostChrome(Host* host) {
  // Once closing has started, any repaint would attach a buffer and remap the
  // window that the shutdown path just took off screen -- reappearing without
  // the browser inside it, since the subsurface is gone by then.
  if (host->closed) {
    return;
  }

  Buffer* buffer = AcquireBuffer(host);
  if (!buffer) {
    return;
  }
  uint32_t* pixels = buffer->pixels;
  const int w = host->width;
  const int h = host->height;

  FillRect(pixels, w, h, 0, 0, w, h, kVoidColor);
  FillRect(pixels, w, h, 0, 0, w, kHeaderHeight, kHeaderColor);
  FillRect(pixels, w, h, 0, kHeaderHeight, kSidebarWidth, h - kHeaderHeight,
           kSidebarColor);

  // Client-side decoration, such as it is: a resize border and a close button.
  FillRect(pixels, w, h, 0, 0, w, kResizeBorder, kBorderColor);
  FillRect(pixels, w, h, 0, h - kResizeBorder, w, kResizeBorder, kBorderColor);
  FillRect(pixels, w, h, 0, 0, kResizeBorder, h, kBorderColor);
  FillRect(pixels, w, h, w - kResizeBorder, 0, kResizeBorder, h, kBorderColor);
  FillRect(pixels, w, h, w - kCloseButtonSize - kResizeBorder, kResizeBorder,
           kCloseButtonSize, kCloseButtonSize - kResizeBorder, kCloseColor);

  buffer->busy = true;
  wl_surface_attach(host->surface, buffer->buffer, 0, 0);
  wl_surface_damage_buffer(host->surface, 0, 0, host->width, host->height);
  // This commit also publishes any pending wl_subsurface state for the
  // embedded browser, which is why the host has to repaint after moving it.
  wl_surface_commit(host->surface);
}

CefRect ContentBounds(const Host* host) {
  return CefRect(kSidebarWidth, kHeaderHeight, host->width - kSidebarWidth,
                 host->height - kHeaderHeight);
}

// --- Wayland listeners -------------------------------------------------------

void OnXdgSurfaceConfigure(void* data, xdg_surface* surface, uint32_t serial) {
  auto* host = static_cast<Host*>(data);
  xdg_surface_ack_configure(surface, serial);
  host->configured = true;
  PaintHostChrome(host);
}

const xdg_surface_listener kXdgSurfaceListener = {
    .configure = OnXdgSurfaceConfigure,
};

void OnToplevelConfigure(void* data,
                         xdg_toplevel*,
                         int32_t width,
                         int32_t height,
                         wl_array*) {
  auto* host = static_cast<Host*>(data);
  if (width > 0 && height > 0) {
    // A compositor may configure any size it likes, including one narrower
    // than the sidebar or shorter than the header. The chrome this program
    // paints has no layout to speak of, so refuse to go below the point where
    // the content rectangle would be empty rather than paint nonsense.
    host->width = std::max(width, kMinWidth);
    host->height = std::max(height, kMinHeight);
    if (host->browser) {
      // The embedded browser does not observe the host's configure events; the
      // host has to push the new geometry down. This is the contract change
      // that Wayland embedding forces on every embedder.
      host->browser->GetHost()->SetWindowBounds(ContentBounds(host));
    }
  }
}

void OnToplevelClose(void* data, xdg_toplevel*) {
  printf("cefembed: compositor asked the window to close\n");
  fflush(stdout);
  static_cast<Host*>(data)->closed = true;
}

const xdg_toplevel_listener kToplevelListener = {
    .configure = OnToplevelConfigure,
    .close = OnToplevelClose,
};

// Which resize edge, if any, the pointer is over. Returns 0 for "not on an
// edge", which xdg_toplevel_resize would reject anyway.
uint32_t ResizeEdgeAt(const Host* host, double x, double y) {
  const bool left = x < kResizeBorder;
  const bool right = x >= host->width - kResizeBorder;
  const bool top = y < kResizeBorder;
  const bool bottom = y >= host->height - kResizeBorder;

  if (top && left) {
    return XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
  }
  if (top && right) {
    return XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
  }
  if (bottom && left) {
    return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
  }
  if (bottom && right) {
    return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
  }
  if (top) {
    return XDG_TOPLEVEL_RESIZE_EDGE_TOP;
  }
  if (bottom) {
    return XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
  }
  if (left) {
    return XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
  }
  if (right) {
    return XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
  }
  return XDG_TOPLEVEL_RESIZE_EDGE_NONE;
}

bool IsOverCloseButton(const Host* host, double x, double y) {
  return x >= host->width - kCloseButtonSize - kResizeBorder &&
         x < host->width - kResizeBorder && y >= kResizeBorder &&
         y < kCloseButtonSize;
}

// Maps a resize edge to the cursor that advertises it. A Wayland client gets no
// cursor at all unless it sets one, so without this the border looks inert even
// though it works.
uint32_t CursorShapeForEdge(uint32_t edge) {
  switch (edge) {
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE;
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE;
    case XDG_TOPLEVEL_RESIZE_EDGE_LEFT:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE;
    case XDG_TOPLEVEL_RESIZE_EDGE_RIGHT:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE;
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE;
    case XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE;
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE;
    case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE;
    default:
      return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
  }
}

void UpdateCursor(Host* host);

void OnPointerEnter(void* data,
                    wl_pointer*,
                    uint32_t serial,
                    wl_surface* surface,
                    wl_fixed_t x,
                    wl_fixed_t y) {
  auto* host = static_cast<Host*>(data);
  // Only our own surface; the browser's subsurface is Chromium's business and
  // it receives its own pointer events directly.
  host->pointer_on_host_surface = (surface == host->surface);
  host->pointer_x = wl_fixed_to_double(x);
  host->pointer_y = wl_fixed_to_double(y);
  host->last_enter_serial = serial;
  UpdateCursor(host);
}

void OnPointerLeave(void* data, wl_pointer*, uint32_t, wl_surface*) {
  auto* host = static_cast<Host*>(data);
  host->pointer_on_host_surface = false;
}

void OnPointerMotion(void* data,
                     wl_pointer*,
                     uint32_t,
                     wl_fixed_t x,
                     wl_fixed_t y) {
  auto* host = static_cast<Host*>(data);
  host->pointer_x = wl_fixed_to_double(x);
  host->pointer_y = wl_fixed_to_double(y);
  UpdateCursor(host);
}

void OnPointerButton(void* data,
                     wl_pointer*,
                     uint32_t serial,
                     uint32_t,
                     uint32_t button,
                     uint32_t state) {
  auto* host = static_cast<Host*>(data);

  constexpr uint32_t kLeftButton = 0x110;  // BTN_LEFT
  if (button != kLeftButton || state != WL_POINTER_BUTTON_STATE_PRESSED ||
      !host->pointer_on_host_surface || host->closed) {
    return;
  }

  if (IsOverCloseButton(host, host->pointer_x, host->pointer_y)) {
    printf("cefembed: close button clicked\n");
    fflush(stdout);
    host->closed = true;
    return;
  }

  const uint32_t edge = ResizeEdgeAt(host, host->pointer_x, host->pointer_y);
  if (edge != XDG_TOPLEVEL_RESIZE_EDGE_NONE) {
    xdg_toplevel_resize(host->toplevel, host->seat, serial, edge);
    return;
  }

  // Anywhere else in the chrome we own drags the window. The browser's
  // subsurface never delivers pointer events here, so a press we can see is a
  // press on our own UI.
  if (host->pointer_y < kHeaderHeight) {
    xdg_toplevel_move(host->toplevel, host->seat, serial);
  }
}

// Every member must be filled in: libwayland aborts the process with
// "listener function for opcode N of wl_pointer is NULL" the first time the
// compositor sends an event whose slot is null. wl_seat version 5 already
// implies frame/axis_source/axis_stop/axis_discrete, and later versions add
// more, so the unused ones get no-ops rather than being left out.
void OnPointerAxis(void*, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) {}
void OnPointerFrame(void*, wl_pointer*) {}
void OnPointerAxisSource(void*, wl_pointer*, uint32_t) {}
void OnPointerAxisStop(void*, wl_pointer*, uint32_t, uint32_t) {}
void OnPointerAxisDiscrete(void*, wl_pointer*, uint32_t, int32_t) {}
void OnPointerAxisValue120(void*, wl_pointer*, uint32_t, int32_t) {}
void OnPointerAxisRelativeDirection(void*, wl_pointer*, uint32_t, uint32_t) {}

void UpdateCursor(Host* host) {
  if (!host->cursor_shape_device || !host->pointer_on_host_surface) {
    return;
  }

  uint32_t shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
  const uint32_t edge = ResizeEdgeAt(host, host->pointer_x, host->pointer_y);
  if (edge != XDG_TOPLEVEL_RESIZE_EDGE_NONE) {
    shape = CursorShapeForEdge(edge);
  } else if (IsOverCloseButton(host, host->pointer_x, host->pointer_y)) {
    shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER;
  } else if (host->pointer_y < kHeaderHeight) {
    shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE;
  }

  if (shape == host->current_cursor) {
    return;
  }
  host->current_cursor = shape;
  wp_cursor_shape_device_v1_set_shape(host->cursor_shape_device,
                                      host->last_enter_serial, shape);
}

const wl_pointer_listener kPointerListener = {
    .enter = OnPointerEnter,
    .leave = OnPointerLeave,
    .motion = OnPointerMotion,
    .button = OnPointerButton,
    .axis = OnPointerAxis,
    .frame = OnPointerFrame,
    .axis_source = OnPointerAxisSource,
    .axis_stop = OnPointerAxisStop,
    .axis_discrete = OnPointerAxisDiscrete,
    .axis_value120 = OnPointerAxisValue120,
    .axis_relative_direction = OnPointerAxisRelativeDirection,
};

void OnSeatCapabilities(void* data, wl_seat* seat, uint32_t caps) {
  auto* host = static_cast<Host*>(data);
  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !host->pointer) {
    host->pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(host->pointer, &kPointerListener, host);
    if (host->cursor_shape_manager) {
      host->cursor_shape_device = wp_cursor_shape_manager_v1_get_pointer(
          host->cursor_shape_manager, host->pointer);
    }
  } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && host->pointer) {
    // capabilities is sent again whenever the set changes, including when the
    // last mouse is unplugged. Keeping a released pointer would leave stale
    // objects behind and a cursor device pointing at nothing.
    if (host->cursor_shape_device) {
      wp_cursor_shape_device_v1_destroy(host->cursor_shape_device);
      host->cursor_shape_device = nullptr;
    }
    wl_pointer_release(host->pointer);
    host->pointer = nullptr;
    host->pointer_on_host_surface = false;
  }
}

void OnSeatName(void*, wl_seat*, const char*) {}

const wl_seat_listener kSeatListener = {
    .capabilities = OnSeatCapabilities,
    .name = OnSeatName,
};

void OnWmBasePing(void*, xdg_wm_base* wm_base, uint32_t serial) {
  xdg_wm_base_pong(wm_base, serial);
}

const xdg_wm_base_listener kWmBaseListener = {.ping = OnWmBasePing};

void OnGlobal(void* data,
              wl_registry* registry,
              uint32_t name,
              const char* interface,
              uint32_t version) {
  auto* host = static_cast<Host*>(data);
  const std::string iface(interface);
  if (iface == wl_compositor_interface.name) {
    host->compositor = static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface, 4));
  } else if (iface == wl_shm_interface.name) {
    host->shm = static_cast<wl_shm*>(
        wl_registry_bind(registry, name, &wl_shm_interface, 1));
  } else if (iface == wp_cursor_shape_manager_v1_interface.name) {
    host->cursor_shape_manager =
        static_cast<wp_cursor_shape_manager_v1*>(wl_registry_bind(
            registry, name, &wp_cursor_shape_manager_v1_interface, 1));
  } else if (iface == wl_seat_interface.name) {
    host->seat = static_cast<wl_seat*>(
        wl_registry_bind(registry, name, &wl_seat_interface, 5));
    wl_seat_add_listener(host->seat, &kSeatListener, host);
  } else if (iface == xdg_wm_base_interface.name) {
    host->wm_base = static_cast<xdg_wm_base*>(
        wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
    xdg_wm_base_add_listener(host->wm_base, &kWmBaseListener, host);
  }
}

void OnGlobalRemove(void*, wl_registry*, uint32_t) {}

const wl_registry_listener kRegistryListener = {
    .global = OnGlobal,
    .global_remove = OnGlobalRemove,
};

// --- CEF client --------------------------------------------------------------

class EmbedClient : public CefClient, public CefLifeSpanHandler {
 public:
  // DISALLOW_COPY_AND_ASSIGN declares a constructor, which suppresses the
  // implicit default one.
  EmbedClient() = default;

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    g_host->browser = browser;
    printf("cefembed: browser created\n");

    // wl_subsurface.set_position is part of *our* surface's double-buffered
    // state, so the browser's placement does not take effect until we commit.
    // Nothing else would prompt us to, since the host UI has not changed.
    if (g_host && g_host->surface) {
      wl_surface_commit(g_host->surface);
      wl_display_flush(g_host->display);
    }
  }

  void OnBeforeClose(CefRefPtr<CefBrowser>) override {
    CEF_REQUIRE_UI_THREAD();
    g_host->browser = nullptr;
  }

 private:
  IMPLEMENT_REFCOUNTING(EmbedClient);
  DISALLOW_COPY_AND_ASSIGN(EmbedClient);
};

bool CreateHostWindow(Host* host) {
  host->display = wl_display_connect(nullptr);
  if (!host->display) {
    fprintf(stderr,
            "cefembed: no Wayland compositor (is WAYLAND_DISPLAY set?)\n");
    return false;
  }

  host->registry = wl_display_get_registry(host->display);
  wl_registry_add_listener(host->registry, &kRegistryListener, host);
  wl_display_roundtrip(host->display);

  if (!host->compositor || !host->shm || !host->wm_base || !host->seat) {
    fprintf(stderr,
            "cefembed: compositor is missing wl_compositor, wl_shm, "
            "xdg_wm_base or wl_seat\n");
    return false;
  }

  host->surface = wl_compositor_create_surface(host->compositor);
  host->xdg_surface_obj =
      xdg_wm_base_get_xdg_surface(host->wm_base, host->surface);
  xdg_surface_add_listener(host->xdg_surface_obj, &kXdgSurfaceListener, host);
  host->toplevel = xdg_surface_get_toplevel(host->xdg_surface_obj);
  xdg_toplevel_add_listener(host->toplevel, &kToplevelListener, host);
  xdg_toplevel_set_title(host->toplevel, "CEF embedded in a Wayland client");
  xdg_toplevel_set_app_id(host->toplevel, "org.cef.cefembed_wayland");
  // Tell the compositor as well, so the window cannot be dragged below what
  // this program is willing to paint. OnToplevelConfigure() clamps regardless,
  // since a compositor is free to ignore this.
  xdg_toplevel_set_min_size(host->toplevel, kMinWidth, kMinHeight);

  wl_surface_commit(host->surface);
  while (!host->configured && wl_display_dispatch(host->display) != -1) {
  }

  return host->configured;
}

// Reads the default queue without stealing events from Chromium's queue. The
// prepare_read/read_events pair is the only safe way to share a connection
// between two dispatchers.
//
// The poll timeout matters: wl_display_read_events() blocks until something
// arrives, so without it CefDoMessageLoopWork() would only run when the
// compositor happened to send us an event. CEF is configured with
// external_message_pump and needs to be serviced on its own schedule.
void PumpWayland(Host* host, int timeout_ms) {
  while (wl_display_prepare_read(host->display) != 0) {
    wl_display_dispatch_pending(host->display);
  }
  wl_display_flush(host->display);

  pollfd pfd = {wl_display_get_fd(host->display), POLLIN, 0};
  if (poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & POLLIN)) {
    wl_display_read_events(host->display);
  } else {
    wl_display_cancel_read(host->display);
  }
  wl_display_dispatch_pending(host->display);

  // A protocol error kills the connection silently otherwise: libwayland stops
  // sending and every later request is dropped, which looks like a freeze.
  if (const int err = wl_display_get_error(host->display)) {
    uint32_t id = 0;
    const wl_interface* interface = nullptr;
    const uint32_t code =
        wl_display_get_protocol_error(host->display, &interface, &id);
    fprintf(stderr,
            "cefembed: wayland connection failed: errno=%d protocol_error=%u "
            "interface=%s id=%u\n",
            err, code, interface ? interface->name : "(none)", id);
    host->closed = true;
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  // Select the Wayland backend explicitly. Ozone otherwise picks X11 wherever
  // DISPLAY is set, and the wl_surface this program puts in parent_window would
  // then be read as an X11 Window -- undefined behaviour that CEF has no way to
  // diagnose. The switch has to be on the command line rather than in
  // CefSettings because subprocesses inherit argv, and it is added only when
  // the caller did not pick a platform, so that the failing case stays testable
  // with an explicit --ozone-platform=x11.
  std::vector<char*> args(argv, argv + argc);
  static char kOzoneWayland[] = "--ozone-platform=wayland";
  const bool has_platform_switch =
      std::any_of(args.begin(), args.end(), [](const char* arg) {
        return std::string_view(arg).starts_with("--ozone-platform");
      });
  if (!has_platform_switch) {
    args.push_back(kOzoneWayland);
  }
  argc = static_cast<int>(args.size());
  argv = args.data();

  CefMainArgs main_args(argc, argv);

  const int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
  if (exit_code >= 0) {
    return exit_code;
  }

  Host host;
  g_host = &host;
  if (!CreateHostWindow(&host)) {
    return 1;
  }

  // Must happen before CefInitialize: Chromium connects to the compositor while
  // initializing Ozone, and a wl_surface cannot be shared across connections.
  cef_set_wayland_display(host.display);

  CefSettings settings;
  settings.no_sandbox = true;
  settings.external_message_pump = true;
  CefString(&settings.root_cache_path).FromASCII("/tmp/cefembed_wayland");

  if (!CefInitialize(main_args, settings, nullptr, nullptr)) {
    fprintf(stderr, "cefembed: CefInitialize failed\n");
    return 1;
  }

  std::string url = "https://store.steampowered.com/";
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg.rfind("--url=", 0) == 0) {
      url = arg.substr(6);
    }
  }

  CefWindowInfo window_info;
  window_info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  // parent_window carries the wl_surface here, the same field an X11 embedder
  // fills with an X11 Window. The xdg_surface is what popups anchor on.
  window_info.SetAsChild(reinterpret_cast<CefWindowHandle>(host.surface),
                         host.xdg_surface_obj, ContentBounds(&host));

  CefBrowserSettings browser_settings;
  CefRefPtr<EmbedClient> client(new EmbedClient());
  if (!CefBrowserHost::CreateBrowser(window_info, client, url, browser_settings,
                                     nullptr, nullptr)) {
    fprintf(stderr, "cefembed: CreateBrowser failed; see the log above\n");
    CefShutdown();
    return 1;
  }

  while (!host.closed) {
    CefDoMessageLoopWork();
    PumpWayland(&host, /*timeout_ms=*/8);
  }
  printf("cefembed: leaving the event loop\n");
  fflush(stdout);

  // CreateBrowser() is asynchronous: |browser| only appears in
  // OnAfterCreated(). Closing the window before that callback arrives -- easy
  // to do, since the page takes a moment to load -- would otherwise fall past
  // the CloseBrowser() below and shut CEF down with a browser still being
  // built, which is the same teardown-under-a-live-browser crash the rest of
  // this path exists to avoid. Wait for creation to land first.
  for (int i = 0; i < 400 && !g_host->browser; ++i) {
    CefDoMessageLoopWork();
    PumpWayland(&host, /*timeout_ms=*/5);
  }

  if (!g_host->browser) {
    fprintf(stderr,
            "cefembed: the browser never finished being created; leaving "
            "without CefShutdown() rather than shutting down over a pending "
            "creation\n");
    return 2;
  }

  {
    g_host->browser->GetHost()->CloseBrowser(true);
    // Both loops have to keep running here. CEF tears the browser window down
    // through Wayland requests and replies, so pumping only
    // CefDoMessageLoopWork leaves the window half-alive; CefShutdown then
    // destroys WaylandConnection underneath it, and ~WaylandPointer dispatches
    // a final pointer-leave into a window whose WaylandWindowManager is already
    // gone.
    for (int i = 0; i < 400 && g_host->browser; ++i) {
      CefDoMessageLoopWork();
      PumpWayland(&host, /*timeout_ms=*/5);
    }
    if (g_host->browser) {
      // Going into CefShutdown() anyway would destroy WaylandConnection under
      // a live browser, which is a crash rather than a diagnostic. Leaving the
      // process without shutting CEF down is the lesser evil, and the exit
      // code makes the failure visible to whatever ran this.
      fprintf(stderr,
              "cefembed: browser did not close in time; leaving without "
              "CefShutdown() rather than tearing the connection down "
              "underneath it\n");
      return 2;
    }
  }

  // Only now: unmapping earlier stops the subsurface receiving frames, and the
  // browser's teardown never finishes. Attaching a null buffer takes the window
  // off screen without destroying anything, so the rest of CefShutdown() -- the
  // slow part -- happens with nothing left to look at.
  wl_surface_attach(host.surface, nullptr, 0, 0);
  wl_surface_commit(host.surface);
  wl_display_flush(host.display);

  CefShutdown();

  // After the null attach above the compositor is reading none of them, so
  // this is the one point where every buffer can go regardless of |busy|.
  for (const auto& buffer : host.buffers) {
    DestroyBuffer(buffer.get());
  }
  host.buffers.clear();

  // The compositor would reclaim all of this when the connection drops, but a
  // sample that leaks its own protocol objects is a poor thing to copy from.
  if (host.cursor_shape_device) {
    wp_cursor_shape_device_v1_destroy(host.cursor_shape_device);
  }
  if (host.cursor_shape_manager) {
    wp_cursor_shape_manager_v1_destroy(host.cursor_shape_manager);
  }
  if (host.pointer) {
    wl_pointer_release(host.pointer);
  }
  if (host.seat) {
    wl_seat_release(host.seat);
  }
  if (host.toplevel) {
    xdg_toplevel_destroy(host.toplevel);
  }
  if (host.xdg_surface_obj) {
    xdg_surface_destroy(host.xdg_surface_obj);
  }
  if (host.surface) {
    wl_surface_destroy(host.surface);
  }
  if (host.wm_base) {
    xdg_wm_base_destroy(host.wm_base);
  }
  if (host.shm) {
    wl_shm_destroy(host.shm);
  }
  if (host.compositor) {
    wl_compositor_destroy(host.compositor);
  }
  if (host.registry) {
    wl_registry_destroy(host.registry);
  }
  if (host.display) {
    wl_display_disconnect(host.display);
  }

  return 0;
}
