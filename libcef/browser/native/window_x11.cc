// Copyright 2014 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/window_x11.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>

#include "libcef/browser/thread_util.h"

#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/platform_window/x11/x11_topmost_window_finder.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

namespace {

// Restore Xlib constants that were #undef'ed by gen/ui/gfx/x/xproto.h.
constexpr int CopyFromParent = 0;
constexpr int InputOutput = 1;

const char kAtom[] = "ATOM";
const char kWMDeleteWindow[] = "WM_DELETE_WINDOW";
const char kWMProtocols[] = "WM_PROTOCOLS";
const char kNetWMName[] = "_NET_WM_NAME";
const char kNetWMPid[] = "_NET_WM_PID";
const char kNetWMPing[] = "_NET_WM_PING";
const char kNetWMState[] = "_NET_WM_STATE";
const char kXdndProxy[] = "XdndProxy";
const char kUTF8String[] = "UTF8_STRING";

// See https://crbug.com/1066670#c57 for background.
inline x11::Window ToX11Window(::Window window) {
  return static_cast<x11::Window>(window);
}
inline ::Window ToWindow(x11::Window window) {
  return static_cast<::Window>(window);
}

// See https://crbug.com/1066670#c29 for background.
inline ::Atom GetAtom(const std::string& atom_name) {
  return static_cast<::Atom>(gfx::GetAtom(atom_name));
}

::Window FindChild(::Display* display, ::Window window) {
  ::Window root;
  ::Window parent;
  ::Window* children;
  ::Window child_window = x11::None;
  unsigned int nchildren;
  if (XQueryTree(display, window, &root, &parent, &children, &nchildren) &&
      nchildren == 1) {
    child_window = children[0];
    XFree(children);
  }
  return child_window;
}

::Window FindToplevelParent(::Display* display, ::Window window) {
  ::Window top_level_window = window;
  ::Window root = x11::None;
  ::Window parent = x11::None;
  ::Window* children = nullptr;
  unsigned int nchildren = 0;
  // Enumerate all parents of "window" to find the highest level window
  // that either:
  //   - has a parent that does not contain the _NET_WM_PID property
  //   - has a parent that is the root window.
  while (XQueryTree(display, window, &root, &parent, &children, &nchildren)) {
    if (children) {
      XFree(children);
    }

    top_level_window = window;
    if (!ui::PropertyExists(ToX11Window(parent), kNetWMPid) || parent == root) {
      break;
    }
    window = parent;
  }
  return top_level_window;
}

}  // namespace

CEF_EXPORT XDisplay* cef_get_xdisplay() {
  if (!CEF_CURRENTLY_ON(CEF_UIT))
    return nullptr;
  return gfx::GetXDisplay();
}

CefWindowX11::CefWindowX11(CefRefPtr<AlloyBrowserHostImpl> browser,
                           ::Window parent_xwindow,
                           const gfx::Rect& bounds,
                           const std::string& title)
    : browser_(browser),
      xdisplay_(gfx::GetXDisplay()),
      parent_xwindow_(parent_xwindow),
      xwindow_(0),
      window_mapped_(false),
      bounds_(bounds),
      focus_pending_(false),
      weak_ptr_factory_(this) {
  if (parent_xwindow_ == x11::None)
    parent_xwindow_ = DefaultRootWindow(xdisplay_);

  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = x11::None;
  swa.override_redirect = false;
  xwindow_ = XCreateWindow(xdisplay_, parent_xwindow_, bounds.x(), bounds.y(),
                           bounds.width(), bounds.height(),
                           0,               // border width
                           CopyFromParent,  // depth
                           InputOutput,
                           nullptr,  // visual
                           CWBackPixmap | CWOverrideRedirect, &swa);
  CHECK(xwindow_);

  DCHECK(ui::X11EventSource::HasInstance());
  ui::X11EventSource::GetInstance()->AddXEventDispatcher(this);

  long event_mask = FocusChangeMask | StructureNotifyMask | PropertyChangeMask;
  XSelectInput(xdisplay_, xwindow_, event_mask);
  XFlush(xdisplay_);

  // TODO(erg): We currently only request window deletion events. We also
  // should listen for activation events and anything else that GTK+ listens
  // for, and do something useful.
  ::Atom protocols[2];
  protocols[0] = GetAtom(kWMDeleteWindow);
  protocols[1] = GetAtom(kNetWMPing);
  XSetWMProtocols(xdisplay_, xwindow_, protocols, 2);

  // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
  // the desktop environment.
  XSetWMProperties(xdisplay_, xwindow_, NULL, NULL, NULL, 0, NULL, NULL, NULL);

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  // XChangeProperty() expects "pid" to be long.
  static_assert(sizeof(long) >= sizeof(pid_t),
                "pid_t should not be larger than long");
  long pid = getpid();
  XChangeProperty(xdisplay_, xwindow_, GetAtom(kNetWMPid), XA_CARDINAL, 32,
                  PropModeReplace, reinterpret_cast<unsigned char*>(&pid), 1);

  // Set the initial window name, if provided.
  if (!title.empty()) {
    XChangeProperty(xdisplay_, xwindow_, GetAtom(kNetWMName),
                    GetAtom(kUTF8String), 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(title.c_str()),
                    title.size());
  }
}

CefWindowX11::~CefWindowX11() {
  DCHECK(!xwindow_);
  DCHECK(ui::X11EventSource::HasInstance());
  ui::X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

void CefWindowX11::Close() {
  XEvent ev = {0};
  ev.xclient.type = ClientMessage;
  ev.xclient.window = xwindow_;
  ev.xclient.message_type = GetAtom(kWMProtocols);
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = GetAtom(kWMDeleteWindow);
  ev.xclient.data.l[1] = x11::CurrentTime;
  XSendEvent(xdisplay_, xwindow_, false, NoEventMask, &ev);

  auto host = GetHost();
  if (host)
    host->Close();
}

void CefWindowX11::Show() {
  if (xwindow_ == x11::None)
    return;

  if (!window_mapped_) {
    // Before we map the window, set size hints. Otherwise, some window managers
    // will ignore toplevel XMoveWindow commands.
    XSizeHints size_hints;
    size_hints.flags = PPosition | PWinGravity;
    size_hints.x = bounds_.x();
    size_hints.y = bounds_.y();
    // Set StaticGravity so that the window position is not affected by the
    // frame width when running with window manager.
    size_hints.win_gravity = StaticGravity;
    XSetWMNormalHints(xdisplay_, xwindow_, &size_hints);

    XMapWindow(xdisplay_, xwindow_);

    // TODO(thomasanderson): Find out why this flush is necessary.
    XFlush(xdisplay_);
    window_mapped_ = true;

    // Setup the drag and drop proxy on the top level window of the application
    // to be the child of this window.
    ::Window child = FindChild(xdisplay_, xwindow_);
    ::Window toplevel_window = FindToplevelParent(xdisplay_, xwindow_);
    DCHECK(toplevel_window);
    if (child && toplevel_window) {
      // Configure the drag&drop proxy property for the top-most window so
      // that all drag&drop-related messages will be sent to the child
      // DesktopWindowTreeHostLinux. The proxy property is referenced by
      // DesktopDragDropClientAuraX11::FindWindowFor.
      x11::Window window = x11::Window::None;
      ui::GetProperty(ToX11Window(toplevel_window), gfx::GetAtom(kXdndProxy),
                      &window);
      ::Window proxy_target = ToWindow(window);

      if (proxy_target != child) {
        // Set the proxy target for the top-most window.
        XChangeProperty(xdisplay_, toplevel_window, GetAtom(kXdndProxy),
                        XA_WINDOW, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&child), 1);
        // Do the same for the proxy target per the spec.
        XChangeProperty(xdisplay_, child, GetAtom(kXdndProxy), XA_WINDOW, 32,
                        PropModeReplace,
                        reinterpret_cast<unsigned char*>(&child), 1);
      }
    }
  }
}

void CefWindowX11::Hide() {
  if (xwindow_ == x11::None)
    return;

  if (window_mapped_) {
    XWithdrawWindow(xdisplay_, xwindow_, 0);
    window_mapped_ = false;
  }
}

void CefWindowX11::Focus() {
  if (xwindow_ == x11::None || !window_mapped_)
    return;

  if (browser_.get()) {
    ::Window child = FindChild(xdisplay_, xwindow_);
    if (child && ui::IsWindowVisible(ToX11Window(child))) {
      // Give focus to the child DesktopWindowTreeHostLinux.
      XSetInputFocus(xdisplay_, child, RevertToParent, x11::CurrentTime);
    }
  } else {
    XSetInputFocus(xdisplay_, xwindow_, RevertToParent, x11::CurrentTime);
  }
}

void CefWindowX11::SetBounds(const gfx::Rect& bounds) {
  if (xwindow_ == x11::None)
    return;

  bool origin_changed = bounds_.origin() != bounds.origin();
  bool size_changed = bounds_.size() != bounds.size();
  XWindowChanges changes = {0};
  unsigned value_mask = 0;

  if (size_changed) {
    changes.width = bounds.width();
    changes.height = bounds.height();
    value_mask = CWHeight | CWWidth;
  }

  if (origin_changed) {
    changes.x = bounds.x();
    changes.y = bounds.y();
    value_mask |= CWX | CWY;
  }

  if (value_mask)
    XConfigureWindow(xdisplay_, xwindow_, value_mask, &changes);
}

gfx::Rect CefWindowX11::GetBoundsInScreen() {
  int x, y;
  Window child;
  if (XTranslateCoordinates(xdisplay_, xwindow_, DefaultRootWindow(xdisplay_),
                            0, 0, &x, &y, &child)) {
    return gfx::Rect(gfx::Point(x, y), bounds_.size());
  }
  return gfx::Rect();
}

views::DesktopWindowTreeHostLinux* CefWindowX11::GetHost() {
  if (browser_.get()) {
    ::Window child = FindChild(xdisplay_, xwindow_);
    if (child) {
      return static_cast<views::DesktopWindowTreeHostLinux*>(
          views::DesktopWindowTreeHostLinux::GetHostForWidget(
              static_cast<gfx::AcceleratedWidget>(child)));
    }
  }
  return nullptr;
}

bool CefWindowX11::CanDispatchEvent(const ui::PlatformEvent& event) {
  DCHECK_NE(xwindow_, x11::None);
  return !!current_xevent_;
}

uint32_t CefWindowX11::DispatchEvent(const ui::PlatformEvent& event) {
  DCHECK_NE(xwindow_, x11::None);
  DCHECK(event);
  DCHECK(current_xevent_);

  ProcessXEvent(current_xevent_);
  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

// Called by X11EventSourceLibevent to determine whether this XEventDispatcher
// implementation is able to process the next translated event sent by it.
void CefWindowX11::CheckCanDispatchNextPlatformEvent(x11::Event* x11_event) {
  current_xevent_ = IsTargetedBy(*x11_event) ? x11_event : nullptr;
}

void CefWindowX11::PlatformEventDispatchFinished() {
  current_xevent_ = nullptr;
}

ui::PlatformEventDispatcher* CefWindowX11::GetPlatformEventDispatcher() {
  return this;
}

bool CefWindowX11::DispatchXEvent(x11::Event* x11_event) {
  if (!IsTargetedBy(*x11_event))
    return false;
  ProcessXEvent(x11_event);
  return true;
}

void CefWindowX11::ContinueFocus() {
  if (!focus_pending_)
    return;
  if (browser_.get())
    browser_->SetFocus(true);
  focus_pending_ = false;
}

bool CefWindowX11::TopLevelAlwaysOnTop() const {
  ::Window toplevel_window = FindToplevelParent(xdisplay_, xwindow_);

  Atom state_atom = GetAtom("_NET_WM_STATE");
  Atom state_keep_above = GetAtom("_NET_WM_STATE_KEEP_ABOVE");
  Atom* states;

  Atom actual_type;
  int actual_format;
  unsigned long num_items;
  unsigned long bytes_after;

  XGetWindowProperty(xdisplay_, toplevel_window, state_atom, 0, 1024,
                     x11::False, XA_ATOM, &actual_type, &actual_format,
                     &num_items, &bytes_after,
                     reinterpret_cast<unsigned char**>(&states));

  bool always_on_top = false;

  for (unsigned long i = 0; i < num_items; ++i) {
    if (states[i] == state_keep_above) {
      always_on_top = true;
      break;
    }
  }

  XFree(states);

  return always_on_top;
}

bool CefWindowX11::IsTargetedBy(const x11::Event& x11_event) const {
  return ToWindow(x11_event.window()) == xwindow_;
}

void CefWindowX11::ProcessXEvent(x11::Event* event) {
  if (auto* configure = event->As<x11::ConfigureNotifyEvent>()) {
    DCHECK_EQ(xwindow_, ToWindow(configure->event));
    DCHECK_EQ(xwindow_, ToWindow(configure->window));
    // It's possible that the X window may be resized by some other means
    // than from within Aura (e.g. the X window manager can change the
    // size). Make sure the root window size is maintained properly.
    bounds_ = gfx::Rect(configure->x, configure->y, configure->width,
                        configure->height);

    if (browser_.get()) {
      ::Window child = FindChild(xdisplay_, xwindow_);
      if (child) {
        // Resize the child DesktopWindowTreeHostLinux to match this window.
        XWindowChanges changes = {0};
        changes.width = bounds_.width();
        changes.height = bounds_.height();
        XConfigureWindow(xdisplay_, child, CWHeight | CWWidth, &changes);

        browser_->NotifyMoveOrResizeStarted();
      }
    }
  } else if (auto* client = event->As<x11::ClientMessageEvent>()) {
    if (client->type == gfx::GetAtom(kWMProtocols)) {
      x11::Atom protocol = static_cast<x11::Atom>(client->data.data32[0]);
      if (protocol == gfx::GetAtom(kWMDeleteWindow)) {
        // We have received a close message from the window manager.
        if (!browser_ || browser_->TryCloseBrowser()) {
          // Allow the close.
          XDestroyWindow(xdisplay_, xwindow_);

          xwindow_ = x11::None;

          if (browser_.get()) {
            // Force the browser to be destroyed and release the reference
            // added in PlatformCreateWindow().
            browser_->WindowDestroyed();
          }

          delete this;
        }
      } else if (protocol == gfx::GetAtom(kNetWMPing)) {
        x11::ClientMessageEvent reply_event = *client;
        reply_event.window = ToX11Window(parent_xwindow_);
        ui::SendEvent(reply_event, reply_event.window,
                      x11::EventMask::SubstructureNotify |
                          x11::EventMask::SubstructureRedirect);
      }
    }
  } else if (auto* focus = event->As<x11::FocusEvent>()) {
    if (focus->opcode == x11::FocusEvent::In) {
      // This message is received first followed by a "_NET_ACTIVE_WINDOW"
      // message sent to the root window. When X11DesktopHandler handles the
      // "_NET_ACTIVE_WINDOW" message it will erroneously mark the WebView
      // (hosted in a DesktopWindowTreeHostLinux) as unfocused. Use a delayed
      // task here to restore the WebView's focus state.
      if (!focus_pending_) {
        focus_pending_ = true;
        CEF_POST_DELAYED_TASK(CEF_UIT,
                              base::Bind(&CefWindowX11::ContinueFocus,
                                         weak_ptr_factory_.GetWeakPtr()),
                              100);
      }
    } else {
      // Cancel the pending focus change if some other window has gained focus
      // while waiting for the async task to run. Otherwise we can get stuck in
      // a focus change loop.
      if (focus_pending_)
        focus_pending_ = false;
    }
  } else if (auto* property = event->As<x11::PropertyNotifyEvent>()) {
    if (property->atom == gfx::GetAtom(kNetWMState)) {
      // State change event like minimize/maximize.
      if (browser_.get()) {
        ::Window child = FindChild(xdisplay_, xwindow_);
        if (child) {
          // Forward the state change to the child DesktopWindowTreeHostLinux
          // window so that resource usage will be reduced while the window is
          // minimized.
          std::vector<x11::Atom> atom_list;
          if (ui::GetAtomArrayProperty(ToX11Window(xwindow_), kNetWMState,
                                       &atom_list) &&
              !atom_list.empty()) {
            ui::SetAtomArrayProperty(ToX11Window(child), kNetWMState, "ATOM",
                                     atom_list);
          } else {
            // Set an empty list of property values to pass the check in
            // DesktopWindowTreeHostLinux::OnWMStateUpdated().
            XChangeProperty(xdisplay_, child,
                            GetAtom(kNetWMState),  // name
                            GetAtom(kAtom),        // type
                            32,  // size in bits of items in 'value'
                            PropModeReplace, NULL,
                            0);  // num items
          }
        }
      }
    }
  }
}
