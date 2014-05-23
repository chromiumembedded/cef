// Copyright 2014 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/window_x11.h"
#include "libcef/browser/thread_util.h"

#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "ui/events/platform/x11/x11_event_source.h"

namespace {

const char* kAtomsToCache[] = {
  "WM_DELETE_WINDOW",
  "WM_PROTOCOLS",
  "_NET_WM_PING",
  "_NET_WM_PID",
  NULL
};

::Window FindEventTarget(const base::NativeEvent& xev) {
  ::Window target = xev->xany.window;
  if (xev->type == GenericEvent)
    target = static_cast<XIDeviceEvent*>(xev->xcookie.data)->event;
  return target;
}

::Window FindChild(::Display* display, ::Window window) {
  ::Window root;
  ::Window parent;
  ::Window* children;
  unsigned int nchildren;
  if (XQueryTree(display, window, &root, &parent, &children, &nchildren)) {
    DCHECK_EQ(1U, nchildren);
    return children[0];
  }
  return None;
}

}  // namespace

CEF_EXPORT XDisplay* cef_get_xdisplay() {
  if (!CEF_CURRENTLY_ON(CEF_UIT))
    return NULL;
  return gfx::GetXDisplay();
}

CefWindowX11::CefWindowX11(CefRefPtr<CefBrowserHostImpl> browser,
                           ::Window parent_xwindow,
                           const gfx::Rect& bounds)
  : browser_(browser),
    xdisplay_(gfx::GetXDisplay()),
    parent_xwindow_(parent_xwindow),
    xwindow_(0),
    window_mapped_(false),
    bounds_(bounds),
    atom_cache_(xdisplay_, kAtomsToCache) {
  if (parent_xwindow_ == None)
    parent_xwindow_ = DefaultRootWindow(xdisplay_);

  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = None;
  swa.override_redirect = false;
  xwindow_ = XCreateWindow(
      xdisplay_, parent_xwindow_,
      bounds.x(), bounds.y(), bounds.width(), bounds.height(),
      0,               // border width
      CopyFromParent,  // depth
      InputOutput,
      CopyFromParent,  // visual
      CWBackPixmap | CWOverrideRedirect,
      &swa);

  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);

  long event_mask = FocusChangeMask | StructureNotifyMask | PropertyChangeMask;
  XSelectInput(xdisplay_, xwindow_, event_mask);
  XFlush(xdisplay_);

  // TODO(erg): We currently only request window deletion events. We also
  // should listen for activation events and anything else that GTK+ listens
  // for, and do something useful.
  ::Atom protocols[2];
  protocols[0] = atom_cache_.GetAtom("WM_DELETE_WINDOW");
  protocols[1] = atom_cache_.GetAtom("_NET_WM_PING");
  XSetWMProtocols(xdisplay_, xwindow_, protocols, 2);

  // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
  // the desktop environment.
  XSetWMProperties(xdisplay_, xwindow_, NULL, NULL, NULL, 0, NULL, NULL, NULL);

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  // XChangeProperty() expects "pid" to be long.
  COMPILE_ASSERT(sizeof(long) >= sizeof(pid_t), pid_t_bigger_than_long);
  long pid = getpid();
  XChangeProperty(xdisplay_,
                  xwindow_,
                  atom_cache_.GetAtom("_NET_WM_PID"),
                  XA_CARDINAL,
                  32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&pid), 1);

  // Allow subclasses to create and cache additional atoms.
  atom_cache_.allow_uncached_atoms();
}

CefWindowX11::~CefWindowX11() {
  DCHECK(!xwindow_);
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

void CefWindowX11::Close() {
  XEvent ev = {0};
  ev.xclient.type = ClientMessage;
  ev.xclient.window = xwindow_;
  ev.xclient.message_type = atom_cache_.GetAtom("WM_PROTOCOLS");
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = atom_cache_.GetAtom("WM_DELETE_WINDOW");
  ev.xclient.data.l[1] = CurrentTime;
  XSendEvent(xdisplay_, xwindow_, False, NoEventMask, &ev);
}

void CefWindowX11::Show() {
  if (xwindow_ == None)
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

    // We now block until our window is mapped. Some X11 APIs will crash and
    // burn if passed |xwindow_| before the window is mapped, and XMapWindow is
    // asynchronous.
    if (ui::X11EventSource::GetInstance())
      ui::X11EventSource::GetInstance()->BlockUntilWindowMapped(xwindow_);
    window_mapped_ = true;
  }
}

void CefWindowX11::Hide() {
  if (xwindow_ == None)
    return;

  if (window_mapped_) {
    XWithdrawWindow(xdisplay_, xwindow_, 0);
    window_mapped_ = false;
  }
}

void CefWindowX11::SetBounds(const gfx::Rect& bounds) {
  if (xwindow_ == None)
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

bool CefWindowX11::CanDispatchEvent(const ui::PlatformEvent& event) {
  ::Window target = FindEventTarget(event);
   return target == xwindow_;
}

uint32_t CefWindowX11::DispatchEvent(const ui::PlatformEvent& event) {
  XEvent* xev = event;
  switch (xev->type) {
    case ConfigureNotify: {
      DCHECK_EQ(xwindow_, xev->xconfigure.event);
      DCHECK_EQ(xwindow_, xev->xconfigure.window);
      // It's possible that the X window may be resized by some other means
      // than from within Aura (e.g. the X window manager can change the
      // size). Make sure the root window size is maintained properly.
      gfx::Rect bounds(xev->xconfigure.x, xev->xconfigure.y,
          xev->xconfigure.width, xev->xconfigure.height);
      bounds_ = bounds;

      ::Window child = FindChild(xdisplay_, xwindow_);
      if (child) {
        // Resize the child DesktopWindowTreeHostX11 to match this window.
        XWindowChanges changes = {0};
        changes.width = bounds.width();
        changes.height = bounds.height();
        XConfigureWindow(xdisplay_, child, CWHeight | CWWidth, &changes);
      }
      break;
    }
    case ClientMessage: {
      Atom message_type = static_cast<Atom>(xev->xclient.data.l[0]);
      if (message_type == atom_cache_.GetAtom("WM_DELETE_WINDOW")) {
        // We have received a close message from the window manager.
        if (browser_->destruction_state() <=
            CefBrowserHostImpl::DESTRUCTION_STATE_PENDING) {
          if (browser_->destruction_state() ==
              CefBrowserHostImpl::DESTRUCTION_STATE_NONE) {
            // Request that the browser close.
            browser_->CloseBrowser(false);
          }

          // Cancel the close.
        } else {
          // Allow the close.
          XDestroyWindow(xdisplay_, xwindow_);
        }
      } else if (message_type == atom_cache_.GetAtom("_NET_WM_PING")) {
        XEvent reply_event = *xev;
        reply_event.xclient.window = parent_xwindow_;

        XSendEvent(xdisplay_,
                   reply_event.xclient.window,
                   False,
                   SubstructureRedirectMask | SubstructureNotifyMask,
                   &reply_event);
        XFlush(xdisplay_);
      }
      break;
    }
    case DestroyNotify:
      xwindow_ = None;

      // Force the browser to be destroyed and release the reference added
      // in PlatformCreateWindow().
      browser_->WindowDestroyed();

      delete this;
      break;
    case FocusIn:
      // This message is recieved first followed by a "_NET_ACTIVE_WINDOW"
      // message sent to the root window. When X11DesktopHandler handles the
      // "_NET_ACTIVE_WINDOW" message it will erroneously mark the WebView
      // (hosted in a DesktopWindowTreeHostX11) as unfocused. Use a delayed
      // task here to restore the WebView's focus state.
      CEF_POST_DELAYED_TASK(CEF_UIT,
          base::Bind(&CefBrowserHostImpl::OnSetFocus, browser_,
                     FOCUS_SOURCE_SYSTEM),
          100);
      break;
  }

  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

