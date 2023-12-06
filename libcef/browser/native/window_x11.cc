// Copyright 2014 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/window_x11.h"

// Include first due to redefinition of x11::EventMask.
#include "ui/base/x/x11_util.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/thread_util.h"

#include "net/base/network_interfaces.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/visual_manager.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xinput.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

namespace {

const char kNetWMPid[] = "_NET_WM_PID";
const char kNetWMPing[] = "_NET_WM_PING";
const char kNetWMState[] = "_NET_WM_STATE";
const char kNetWMStateKeepAbove[] = "_NET_WM_STATE_KEEP_ABOVE";
const char kWMDeleteWindow[] = "WM_DELETE_WINDOW";
const char kWMProtocols[] = "WM_PROTOCOLS";
const char kXdndProxy[] = "XdndProxy";

// Return true if |window| has any property with |property_name|.
// Deleted from ui/base/x/x11_util.h in https://crrev.com/62fc260067.
bool PropertyExists(x11::Connection* connection,
                    x11::Window window,
                    x11::Atom property) {
  auto response = connection
                      ->GetProperty(x11::GetPropertyRequest{
                          .window = window,
                          .property = property,
                          .long_length = 1,
                      })
                      .Sync();
  return response && response->format;
}

// Returns true if |window| is visible.
// Deleted from ui/base/x/x11_util.h in https://crrev.com/62fc260067.
bool IsWindowVisible(x11::Connection* connection, x11::Window window) {
  auto response = connection->GetWindowAttributes({window}).Sync();
  if (!response || response->map_state != x11::MapState::Viewable) {
    return false;
  }

  // Minimized windows are not visible.
  std::vector<x11::Atom> wm_states;
  if (connection->GetArrayProperty(window, x11::GetAtom("_NET_WM_STATE"),
                                   &wm_states)) {
    x11::Atom hidden_atom = x11::GetAtom("_NET_WM_STATE_HIDDEN");
    if (base::Contains(wm_states, hidden_atom)) {
      return false;
    }
  }

  // Do not check _NET_CURRENT_DESKTOP/_NET_WM_DESKTOP since some
  // window managers (eg. i3) have per-monitor workspaces where more
  // than one workspace can be visible at once, but only one will be
  // "active".
  return true;
}

x11::Window FindChild(x11::Connection* connection, x11::Window window) {
  auto query_tree = connection->QueryTree({window}).Sync();
  if (query_tree && query_tree->children.size() == 1U) {
    return query_tree->children[0];
  }

  return x11::Window::None;
}

x11::Window FindToplevelParent(x11::Connection* connection,
                               x11::Window window) {
  x11::Window top_level_window = window;

  do {
    auto query_tree = connection->QueryTree({window}).Sync();
    if (!query_tree) {
      break;
    }

    top_level_window = window;
    if (!PropertyExists(connection, query_tree->parent,
                        x11::GetAtom(kNetWMPid)) ||
        query_tree->parent == query_tree->root) {
      break;
    }

    window = query_tree->parent;
  } while (true);

  return top_level_window;
}

}  // namespace

CEF_EXPORT XDisplay* cef_get_xdisplay() {
  if (!CEF_CURRENTLY_ON(CEF_UIT)) {
    return nullptr;
  }
  return x11::Connection::Get()->GetXlibDisplay();
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
  if (parent_xwindow_ == x11::Window::None) {
    parent_xwindow_ = ui::GetX11RootWindow();
  }

  x11::VisualId visual;
  uint8_t depth;
  x11::ColorMap colormap;
  connection_->GetOrCreateVisualManager().ChooseVisualForWindow(
      /*want_argb_visual=*/false, &visual, &depth, &colormap,
      /*visual_has_alpha=*/nullptr);

  xwindow_ = connection_->GenerateId<x11::Window>();

  connection_->CreateWindow({
      .depth = depth,
      .wid = xwindow_,
      .parent = parent_xwindow_,
      .x = static_cast<int16_t>(bounds.x()),
      .y = static_cast<int16_t>(bounds.y()),
      .width = static_cast<uint16_t>(bounds.width()),
      .height = static_cast<uint16_t>(bounds.height()),
      .c_class = x11::WindowClass::InputOutput,
      .visual = visual,
      .background_pixel = 0,
      .border_pixel = 0,
      .override_redirect = x11::Bool32(false),
      .colormap = colormap,
  });

  auto event_mask = x11::EventMask::FocusChange |
                    x11::EventMask::StructureNotify |
                    x11::EventMask::PropertyChange;
  xwindow_events_ = connection_->ScopedSelectEvent(xwindow_, event_mask);

  connection_->Flush();

  DCHECK(ui::X11EventSource::HasInstance());
  connection_->AddEventObserver(this);
  ui::X11EventSource::GetInstance()->AddPlatformEventDispatcher(this);

  std::vector<x11::Atom> protocols = {
      x11::GetAtom(kWMDeleteWindow),
      x11::GetAtom(kNetWMPing),
  };
  connection_->SetArrayProperty(xwindow_, x11::GetAtom(kWMProtocols),
                                x11::Atom::ATOM, protocols);

  // We need a WM_CLIENT_MACHINE value so we integrate with the desktop
  // environment.
  connection_->SetStringProperty(xwindow_, x11::Atom::WM_CLIENT_MACHINE,
                                 x11::Atom::STRING, net::GetHostName());

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  // XChangeProperty() expects "pid" to be long.
  static_assert(sizeof(uint32_t) >= sizeof(pid_t),
                "pid_t should not be larger than uint32_t");
  uint32_t pid = getpid();
  connection_->SetProperty(xwindow_, x11::GetAtom(kNetWMPid),
                           x11::Atom::CARDINAL, pid);

  // Set the initial window name, if provided.
  if (!title.empty()) {
    connection_->SetStringProperty(xwindow_, x11::Atom::WM_NAME,
                                   x11::Atom::STRING, title);
    connection_->SetStringProperty(xwindow_, x11::Atom::WM_ICON_NAME,
                                   x11::Atom::STRING, title);
  }
}

CefWindowX11::~CefWindowX11() {
  DCHECK_EQ(xwindow_, x11::Window::None);
  DCHECK(ui::X11EventSource::HasInstance());
  connection_->RemoveEventObserver(this);
  ui::X11EventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

void CefWindowX11::Close() {
  if (xwindow_ == x11::Window::None) {
    return;
  }

  ui::SendClientMessage(
      xwindow_, xwindow_, x11::GetAtom(kWMProtocols),
      {static_cast<uint32_t>(x11::GetAtom(kWMDeleteWindow)),
       static_cast<uint32_t>(x11::Time::CurrentTime), 0, 0, 0},
      x11::EventMask::NoEvent);

  auto host = GetHost();
  if (host) {
    host->Close();
  }
}

void CefWindowX11::Show() {
  if (xwindow_ == x11::Window::None) {
    return;
  }

  if (!window_mapped_) {
    // Before we map the window, set size hints. Otherwise, some window managers
    // will ignore toplevel XMoveWindow commands.
    x11::SizeHints size_hints;
    memset(&size_hints, 0, sizeof(size_hints));
    connection_->GetWmNormalHints(xwindow_, &size_hints);
    size_hints.flags |= x11::SIZE_HINT_P_POSITION;
    size_hints.x = bounds_.x();
    size_hints.y = bounds_.y();
    connection_->SetWmNormalHints(xwindow_, size_hints);

    connection_->MapWindow({xwindow_});

    // TODO(thomasanderson): Find out why this flush is necessary.
    connection_->Flush();
    window_mapped_ = true;

    // Setup the drag and drop proxy on the top level window of the application
    // to be the child of this window.
    auto child = FindChild(connection_, xwindow_);
    auto toplevel_window = FindToplevelParent(connection_, xwindow_);
    DCHECK_NE(toplevel_window, x11::Window::None);
    if (child != x11::Window::None && toplevel_window != x11::Window::None) {
      // Configure the drag&drop proxy property for the top-most window so
      // that all drag&drop-related messages will be sent to the child
      // DesktopWindowTreeHostLinux. The proxy property is referenced by
      // DesktopDragDropClientAuraX11::FindWindowFor.
      x11::Window window = x11::Window::None;
      auto dndproxy_atom = x11::GetAtom(kXdndProxy);
      connection_->GetPropertyAs(toplevel_window, dndproxy_atom, &window);

      if (window != child) {
        // Set the proxy target for the top-most window.
        connection_->SetProperty(toplevel_window, dndproxy_atom,
                                 x11::Atom::WINDOW, child);
        // Do the same for the proxy target per the spec.
        connection_->SetProperty(child, dndproxy_atom, x11::Atom::WINDOW,
                                 child);
      }
    }
  }
}

void CefWindowX11::Hide() {
  if (xwindow_ == x11::Window::None) {
    return;
  }

  if (window_mapped_) {
    connection_->WithdrawWindow(xwindow_);
    window_mapped_ = false;
  }
}

void CefWindowX11::Focus() {
  if (xwindow_ == x11::Window::None || !window_mapped_) {
    return;
  }

  x11::Window focus_target = xwindow_;

  if (browser_.get()) {
    auto child = FindChild(connection_, xwindow_);
    if (child != x11::Window::None && IsWindowVisible(connection_, child)) {
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
  if (xwindow_ == x11::Window::None) {
    return;
  }

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
    auto child = FindChild(connection_, xwindow_);
    if (child != x11::Window::None) {
      return static_cast<views::DesktopWindowTreeHostLinux*>(
          views::DesktopWindowTreeHostLinux::GetHostForWidget(
              static_cast<gfx::AcceleratedWidget>(child)));
    }
  }
  return nullptr;
}

bool CefWindowX11::CanDispatchEvent(const ui::PlatformEvent& event) {
  auto* dispatching_event = connection_->dispatching_event();
  return dispatching_event && IsTargetedBy(*dispatching_event);
}

uint32_t CefWindowX11::DispatchEvent(const ui::PlatformEvent& event) {
  DCHECK_NE(xwindow_, x11::Window::None);
  DCHECK(event);

  auto* current_xevent = connection_->dispatching_event();
  ProcessXEvent(*current_xevent);
  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

void CefWindowX11::OnEvent(const x11::Event& event) {
  if (!IsTargetedBy(event)) {
    return;
  }
  ProcessXEvent(event);
}

void CefWindowX11::ContinueFocus() {
  if (!focus_pending_) {
    return;
  }
  if (browser_.get()) {
    browser_->SetFocus(true);
  }
  focus_pending_ = false;
}

bool CefWindowX11::TopLevelAlwaysOnTop() const {
  auto toplevel_window = FindToplevelParent(connection_, xwindow_);
  if (toplevel_window == x11::Window::None) {
    return false;
  }

  std::vector<x11::Atom> wm_states;
  if (connection_->GetArrayProperty(toplevel_window, x11::GetAtom(kNetWMState),
                                    &wm_states)) {
    x11::Atom keep_above_atom = x11::GetAtom(kNetWMStateKeepAbove);
    if (base::Contains(wm_states, keep_above_atom)) {
      return true;
    }
  }
  return false;
}

void CefWindowX11::ProcessXEvent(const x11::Event& event) {
  if (auto* configure = event.As<x11::ConfigureNotifyEvent>()) {
    DCHECK_EQ(xwindow_, configure->window);
    // It's possible that the X window may be resized by some other means
    // than from within Aura (e.g. the X window manager can change the
    // size). Make sure the root window size is maintained properly.
    bounds_ = gfx::Rect(configure->x, configure->y, configure->width,
                        configure->height);

    if (browser_.get()) {
      auto child = FindChild(connection_, xwindow_);
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
  } else if (auto* client = event.As<x11::ClientMessageEvent>()) {
    if (client->type == x11::GetAtom(kWMProtocols)) {
      x11::Atom protocol = static_cast<x11::Atom>(client->data.data32[0]);
      if (protocol == x11::GetAtom(kWMDeleteWindow)) {
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
      } else if (protocol == x11::GetAtom(kNetWMPing)) {
        x11::ClientMessageEvent reply_event = *client;
        reply_event.window = parent_xwindow_;
        connection_->SendEvent(reply_event, reply_event.window,
                               x11::EventMask::SubstructureNotify |
                                   x11::EventMask::SubstructureRedirect);
      }
    }
  } else if (auto* focus = event.As<x11::FocusEvent>()) {
    if (focus->opcode == x11::FocusEvent::In) {
      // This message is received first followed by a "_NET_ACTIVE_WINDOW"
      // message sent to the root window. When X11DesktopHandler handles the
      // "_NET_ACTIVE_WINDOW" message it will erroneously mark the WebView
      // (hosted in a DesktopWindowTreeHostLinux) as unfocused. Use a delayed
      // task here to restore the WebView's focus state.
      if (!focus_pending_) {
        focus_pending_ = true;
        CEF_POST_DELAYED_TASK(CEF_UIT,
                              base::BindOnce(&CefWindowX11::ContinueFocus,
                                             weak_ptr_factory_.GetWeakPtr()),
                              100);
      }
    } else {
      // Cancel the pending focus change if some other window has gained focus
      // while waiting for the async task to run. Otherwise we can get stuck in
      // a focus change loop.
      if (focus_pending_) {
        focus_pending_ = false;
      }
    }
  } else if (auto* property = event.As<x11::PropertyNotifyEvent>()) {
    const auto& wm_state_atom = x11::GetAtom(kNetWMState);
    if (property->atom == wm_state_atom) {
      // State change event like minimize/maximize.
      if (browser_.get()) {
        auto child = FindChild(connection_, xwindow_);
        if (child != x11::Window::None) {
          // Forward the state change to the child DesktopWindowTreeHostLinux
          // window so that resource usage will be reduced while the window is
          // minimized. |atom_list| may be empty.
          std::vector<x11::Atom> atom_list;
          connection_->GetArrayProperty(xwindow_, wm_state_atom, &atom_list);
          connection_->SetArrayProperty(child, wm_state_atom, x11::Atom::ATOM,
                                        atom_list);
        }
      }
    }
  }
}

bool CefWindowX11::IsTargetedBy(const x11::Event& xev) const {
  if (auto* button = xev.As<x11::ButtonEvent>()) {
    return button->event == xwindow_;
  }
  if (auto* key = xev.As<x11::KeyEvent>()) {
    return key->event == xwindow_;
  }
  if (auto* motion = xev.As<x11::MotionNotifyEvent>()) {
    return motion->event == xwindow_;
  }
  if (auto* xievent = xev.As<x11::Input::DeviceEvent>()) {
    return xievent->event == xwindow_;
  }
  if (auto* motion = xev.As<x11::MotionNotifyEvent>()) {
    return motion->event == xwindow_;
  }
  if (auto* crossing = xev.As<x11::CrossingEvent>()) {
    return crossing->event == xwindow_;
  }
  if (auto* expose = xev.As<x11::ExposeEvent>()) {
    return expose->window == xwindow_;
  }
  if (auto* focus = xev.As<x11::FocusEvent>()) {
    return focus->event == xwindow_;
  }
  if (auto* configure = xev.As<x11::ConfigureNotifyEvent>()) {
    return configure->window == xwindow_;
  }
  if (auto* crossing_input = xev.As<x11::Input::CrossingEvent>()) {
    return crossing_input->event == xwindow_;
  }
  if (auto* map = xev.As<x11::MapNotifyEvent>()) {
    return map->window == xwindow_;
  }
  if (auto* unmap = xev.As<x11::UnmapNotifyEvent>()) {
    return unmap->window == xwindow_;
  }
  if (auto* client = xev.As<x11::ClientMessageEvent>()) {
    return client->window == xwindow_;
  }
  if (auto* property = xev.As<x11::PropertyNotifyEvent>()) {
    return property->window == xwindow_;
  }
  if (auto* selection = xev.As<x11::SelectionNotifyEvent>()) {
    return selection->requestor == xwindow_;
  }
  if (auto* visibility = xev.As<x11::VisibilityNotifyEvent>()) {
    return visibility->window == xwindow_;
  }
  return false;
}
