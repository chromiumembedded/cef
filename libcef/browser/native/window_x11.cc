// Copyright 2014 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/window_x11.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/thread_util.h"

#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_util.h"
#include "ui/platform_window/x11/x11_topmost_window_finder.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

namespace {

const char kAtom[] = "ATOM";
const char kNetWMPid[] = "_NET_WM_PID";
const char kNetWMPing[] = "_NET_WM_PING";
const char kNetWMState[] = "_NET_WM_STATE";
const char kNetWMStateKeepAbove[] = "_NET_WM_STATE_KEEP_ABOVE";
const char kWMDeleteWindow[] = "WM_DELETE_WINDOW";
const char kWMProtocols[] = "WM_PROTOCOLS";
const char kXdndProxy[] = "XdndProxy";

x11::Window FindChild(x11::Window window) {
  auto query_tree = x11::Connection::Get()->QueryTree({window}).Sync();
  if (query_tree && query_tree->children.size() == 1U) {
    return query_tree->children[0];
  }

  return x11::Window::None;
}

x11::Window FindToplevelParent(x11::Window window) {
  x11::Window top_level_window = window;

  do {
    auto query_tree = x11::Connection::Get()->QueryTree({window}).Sync();
    if (!query_tree)
      break;

    top_level_window = window;
    if (!ui::PropertyExists(query_tree->parent, kNetWMPid) ||
        query_tree->parent == query_tree->root) {
      break;
    }

    window = query_tree->parent;
  } while (true);

  return top_level_window;
}

}  // namespace

CEF_EXPORT XDisplay* cef_get_xdisplay() {
  if (!CEF_CURRENTLY_ON(CEF_UIT))
    return nullptr;
  return gfx::GetXDisplay();
}

CefWindowX11::CefWindowX11(CefRefPtr<CefBrowserHostBase> browser,
                           x11::Window parent_xwindow,
                           const gfx::Rect& bounds,
                           const std::string& title)
    : browser_(browser),
      connection_(x11::Connection::Get()),
      parent_xwindow_(parent_xwindow),
      bounds_(bounds),
      weak_ptr_factory_(this) {
  if (parent_xwindow_ == x11::Window::None)
    parent_xwindow_ = ui::GetX11RootWindow();

  x11::VisualId visual;
  uint8_t depth;
  x11::ColorMap colormap;
  ui::XVisualManager::GetInstance()->ChooseVisualForWindow(
      /*want_argb_visual=*/false, &visual, &depth, &colormap,
      /*visual_has_alpha=*/nullptr);

  xwindow_ = connection_->GenerateId<x11::Window>();
  connection_->CreateWindow({
      .depth = depth,
      .wid = xwindow_,
      .parent = parent_xwindow_,
      .x = bounds.x(),
      .y = bounds.y(),
      .width = bounds.width(),
      .height = bounds.height(),
      .c_class = x11::WindowClass::InputOutput,
      .visual = visual,
      .background_pixel = 0,
      .border_pixel = 0,
      .override_redirect = x11::Bool32(false),
      .event_mask = x11::EventMask::FocusChange |
                    x11::EventMask::StructureNotify |
                    x11::EventMask::PropertyChange,
      .colormap = colormap,
  });

  connection_->Flush();

  DCHECK(ui::X11EventSource::HasInstance());
  ui::X11EventSource::GetInstance()->AddXEventDispatcher(this);

  std::vector<x11::Atom> protocols = {
      gfx::GetAtom(kWMDeleteWindow),
      gfx::GetAtom(kNetWMPing),
  };
  ui::SetArrayProperty(xwindow_, gfx::GetAtom(kWMProtocols), x11::Atom::ATOM,
                       protocols);

  // We need a WM_CLIENT_MACHINE value so we integrate with the desktop
  // environment.
  ui::SetStringProperty(xwindow_, x11::Atom::WM_CLIENT_MACHINE,
                        x11::Atom::STRING, net::GetHostName());

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  // XChangeProperty() expects "pid" to be long.
  static_assert(sizeof(uint32_t) >= sizeof(pid_t),
                "pid_t should not be larger than uint32_t");
  uint32_t pid = getpid();
  ui::SetProperty(xwindow_, gfx::GetAtom(kNetWMPid), x11::Atom::CARDINAL, pid);

  // Set the initial window name, if provided.
  if (!title.empty()) {
    ui::SetStringProperty(xwindow_, x11::Atom::WM_NAME, x11::Atom::STRING,
                          title);
    ui::SetStringProperty(xwindow_, x11::Atom::WM_ICON_NAME, x11::Atom::STRING,
                          title);
  }
}

CefWindowX11::~CefWindowX11() {
  DCHECK_EQ(xwindow_, x11::Window::None);
  DCHECK(ui::X11EventSource::HasInstance());
  ui::X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

void CefWindowX11::Close() {
  if (xwindow_ == x11::Window::None)
    return;

  ui::SendClientMessage(
      xwindow_, xwindow_, gfx::GetAtom(kWMProtocols),
      {static_cast<uint32_t>(gfx::GetAtom(kWMDeleteWindow)),
       static_cast<uint32_t>(x11::Time::CurrentTime), 0, 0, 0},
      x11::EventMask::NoEvent);

  auto host = GetHost();
  if (host)
    host->Close();
}

void CefWindowX11::Show() {
  if (xwindow_ == x11::Window::None)
    return;

  if (!window_mapped_) {
    // Before we map the window, set size hints. Otherwise, some window managers
    // will ignore toplevel XMoveWindow commands.
    ui::SizeHints size_hints;
    memset(&size_hints, 0, sizeof(size_hints));
    ui::GetWmNormalHints(xwindow_, &size_hints);
    size_hints.flags |= ui::SIZE_HINT_P_POSITION;
    size_hints.x = bounds_.x();
    size_hints.y = bounds_.y();
    ui::SetWmNormalHints(xwindow_, size_hints);

    connection_->MapWindow({xwindow_});

    // TODO(thomasanderson): Find out why this flush is necessary.
    connection_->Flush();
    window_mapped_ = true;

    // Setup the drag and drop proxy on the top level window of the application
    // to be the child of this window.
    auto child = FindChild(xwindow_);
    auto toplevel_window = FindToplevelParent(xwindow_);
    DCHECK_NE(toplevel_window, x11::Window::None);
    if (child != x11::Window::None && toplevel_window != x11::Window::None) {
      // Configure the drag&drop proxy property for the top-most window so
      // that all drag&drop-related messages will be sent to the child
      // DesktopWindowTreeHostLinux. The proxy property is referenced by
      // DesktopDragDropClientAuraX11::FindWindowFor.
      x11::Window window = x11::Window::None;
      auto dndproxy_atom = gfx::GetAtom(kXdndProxy);
      ui::GetProperty(toplevel_window, dndproxy_atom, &window);

      if (window != child) {
        // Set the proxy target for the top-most window.
        ui::SetProperty(toplevel_window, dndproxy_atom, x11::Atom::WINDOW,
                        child);
        // Do the same for the proxy target per the spec.
        ui::SetProperty(child, dndproxy_atom, x11::Atom::WINDOW, child);
      }
    }
  }
}

void CefWindowX11::Hide() {
  if (xwindow_ == x11::Window::None)
    return;

  if (window_mapped_) {
    ui::WithdrawWindow(xwindow_);
    window_mapped_ = false;
  }
}

void CefWindowX11::Focus() {
  if (xwindow_ == x11::Window::None || !window_mapped_)
    return;

  x11::Window focus_target = xwindow_;

  if (browser_.get()) {
    auto child = FindChild(xwindow_);
    if (child != x11::Window::None && ui::IsWindowVisible(child)) {
      // Give focus to the child DesktopWindowTreeHostLinux.
      focus_target = child;
    }
  }

  // Directly ask the X server to give focus to the window. Note that the call
  // would have raised an X error if the window is not mapped.
  connection_
      ->SetInputFocus(
          {x11::InputFocus::Parent, focus_target, x11::Time::CurrentTime})
      .IgnoreError();
}

void CefWindowX11::SetBounds(const gfx::Rect& bounds) {
  if (xwindow_ == x11::Window::None)
    return;

  x11::ConfigureWindowRequest req{.window = xwindow_};

  bool origin_changed = bounds_.origin() != bounds.origin();
  bool size_changed = bounds_.size() != bounds.size();

  if (size_changed) {
    req.width = bounds.width();
    req.height = bounds.height();
  }

  if (origin_changed) {
    req.x = bounds.x();
    req.y = bounds.y();
  }

  if (origin_changed || size_changed) {
    connection_->ConfigureWindow(req);
  }
}

gfx::Rect CefWindowX11::GetBoundsInScreen() {
  if (auto coords =
          connection_
              ->TranslateCoordinates({xwindow_, ui::GetX11RootWindow(), 0, 0})
              .Sync()) {
    return gfx::Rect(gfx::Point(coords->dst_x, coords->dst_y), bounds_.size());
  }

  return gfx::Rect();
}

views::DesktopWindowTreeHostLinux* CefWindowX11::GetHost() {
  if (browser_.get()) {
    auto child = FindChild(xwindow_);
    if (child != x11::Window::None) {
      return static_cast<views::DesktopWindowTreeHostLinux*>(
          views::DesktopWindowTreeHostLinux::GetHostForWidget(
              static_cast<gfx::AcceleratedWidget>(child)));
    }
  }
  return nullptr;
}

bool CefWindowX11::CanDispatchEvent(const ui::PlatformEvent& event) {
  DCHECK_NE(xwindow_, x11::Window::None);
  return !!current_xevent_;
}

uint32_t CefWindowX11::DispatchEvent(const ui::PlatformEvent& event) {
  DCHECK_NE(xwindow_, x11::Window::None);
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
  auto toplevel_window = FindToplevelParent(xwindow_);
  if (toplevel_window == x11::Window::None)
    return false;

  std::vector<x11::Atom> wm_states;
  if (ui::GetAtomArrayProperty(toplevel_window, kNetWMState, &wm_states)) {
    x11::Atom keep_above_atom = gfx::GetAtom(kNetWMStateKeepAbove);
    if (base::Contains(wm_states, keep_above_atom))
      return true;
  }
  return false;
}

bool CefWindowX11::IsTargetedBy(const x11::Event& x11_event) const {
  return x11_event.window() == xwindow_;
}

void CefWindowX11::ProcessXEvent(x11::Event* event) {
  if (auto* configure = event->As<x11::ConfigureNotifyEvent>()) {
    DCHECK_EQ(xwindow_, configure->event);
    DCHECK_EQ(xwindow_, configure->window);
    // It's possible that the X window may be resized by some other means
    // than from within Aura (e.g. the X window manager can change the
    // size). Make sure the root window size is maintained properly.
    bounds_ = gfx::Rect(configure->x, configure->y, configure->width,
                        configure->height);

    if (browser_.get()) {
      auto child = FindChild(xwindow_);
      if (child != x11::Window::None) {
        // Resize the child DesktopWindowTreeHostLinux to match this window.
        x11::ConfigureWindowRequest req{
            .window = child,
            .width = bounds_.width(),
            .height = bounds_.height(),
        };
        connection_->ConfigureWindow(req);

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
          connection_->DestroyWindow({xwindow_});
          xwindow_ = x11::Window::None;

          if (browser_) {
            // Force the browser to be destroyed and release the reference
            // added in PlatformCreateWindow().
            static_cast<AlloyBrowserHostImpl*>(browser_.get())
                ->WindowDestroyed();
          }

          delete this;
        }
      } else if (protocol == gfx::GetAtom(kNetWMPing)) {
        x11::ClientMessageEvent reply_event = *client;
        reply_event.window = parent_xwindow_;
        x11::SendEvent(reply_event, reply_event.window,
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
        auto child = FindChild(xwindow_);
        if (child != x11::Window::None) {
          // Forward the state change to the child DesktopWindowTreeHostLinux
          // window so that resource usage will be reduced while the window is
          // minimized. |atom_list| may be empty.
          std::vector<x11::Atom> atom_list;
          ui::GetAtomArrayProperty(xwindow_, kNetWMState, &atom_list);
          ui::SetAtomArrayProperty(child, kNetWMState, kAtom, atom_list);
        }
      }
    }
  }
}
