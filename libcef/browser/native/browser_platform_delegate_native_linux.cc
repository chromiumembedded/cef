// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/native/browser_platform_delegate_native_linux.h"

#include "base/no_destructor.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/native/native_widget_delegate.h"
#include "cef/libcef/browser/thread_util.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keysym_to_unicode.h"
#include "ui/gfx/font_render_params.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(SUPPORTS_OZONE_X11)
#include "cef/libcef/browser/native/window_x11.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/keycodes/keyboard_code_conversion_xkb.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#endif

CefBrowserPlatformDelegateNativeLinux::CefBrowserPlatformDelegateNativeLinux(
    const CefWindowInfo& window_info,
    SkColor background_color)
    : CefBrowserPlatformDelegateNativeAura(window_info, background_color) {
  connection_ = x11::Connection::Get();
}

#if BUILDFLAG(SUPPORTS_OZONE_X11)
void CefBrowserPlatformDelegateNativeLinux::enableKeyboardEventsForWindow(
    x11::Window target_win,
    bool enable) const {
  if (enable) {
    auto win_attr_response =
        connection_->GetWindowAttributes({target_win}).Sync();
    if (!win_attr_response) {
      return;
    }
    auto window_attrs_req = x11::ChangeWindowAttributesRequest{};
    window_attrs_req.window = target_win;
    auto event_mask = static_cast<int>(win_attr_response->your_event_mask);
    event_mask |= static_cast<int>(x11::EventMask::KeyPress);
    event_mask |= static_cast<int>(x11::EventMask::KeyRelease);
    window_attrs_req.event_mask = static_cast<x11::EventMask>(event_mask);
    connection_->ChangeWindowAttributes(window_attrs_req);
  } else {
    if (target_win != x11::Window::None) {
      auto win_attr_response =
          connection_->GetWindowAttributes({target_win}).Sync();
      if (!win_attr_response) {
        return;
      }
      auto window_attrs_req = x11::ChangeWindowAttributesRequest{};
      window_attrs_req.window = target_win;
      auto event_mask = static_cast<int>(win_attr_response->your_event_mask);
      event_mask &= ~static_cast<int>(x11::EventMask::KeyPress);
      event_mask &= ~static_cast<int>(x11::EventMask::KeyRelease);
      window_attrs_req.event_mask = static_cast<x11::EventMask>(event_mask);
      connection_->ChangeWindowAttributes(window_attrs_req);
    }
  }
}

void CefBrowserPlatformDelegateNativeLinux::BrowserCreated(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateNativeAura::BrowserCreated(browser);

  // we only enable keyboard events when it is focused, just disable them now
  const auto host_x11_win = static_cast<x11::Window>(GetHostWindowHandle());
  if (host_x11_win != x11::Window::None) {
    enableKeyboardEventsForWindow(host_x11_win, false);
  }
}
#endif

void CefBrowserPlatformDelegateNativeLinux::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateNativeAura::BrowserDestroyed(browser);

  if (host_window_created_) {
    // Release the reference added in CreateHostWindow().
    browser->Release();
  }
}

bool CefBrowserPlatformDelegateNativeLinux::CreateHostWindow() {
  DCHECK(!window_widget_);

  if (window_info_.bounds.width == 0) {
    window_info_.bounds.width = 800;
  }
  if (window_info_.bounds.height == 0) {
    window_info_.bounds.height = 600;
  }

  gfx::Rect rect(window_info_.bounds.x, window_info_.bounds.y,
                 window_info_.bounds.width, window_info_.bounds.height);

#if BUILDFLAG(SUPPORTS_OZONE_X11)
  DCHECK(!window_x11_);

  x11::Window parent_window = x11::Window::None;
  if (window_info_.parent_window != kNullWindowHandle) {
    parent_window = static_cast<x11::Window>(window_info_.parent_window);
  }

  // Create a new window object. It will delete itself when the associated X11
  // window is destroyed.
  window_x11_ =
      new CefWindowX11(browser_.get(), parent_window, rect,
                       CefString(&window_info_.window_name).ToString());
  DCHECK_NE(window_x11_->xwindow(), x11::Window::None);
  window_info_.window =
      static_cast<cef_window_handle_t>(window_x11_->xwindow());

  host_window_created_ = true;

  // Add a reference that will be released in BrowserDestroyed().
  browser_->AddRef();

  auto* widget_delegate = new CefNativeWidgetDelegate(
      GetBackgroundColor(), window_x11_->TopLevelAlwaysOnTop(),
      GetBoundsChangedCallback(), GetWidgetDeleteCallback());
  widget_delegate->Init(
      static_cast<gfx::AcceleratedWidget>(window_info_.window), web_contents_,
      gfx::Rect(gfx::Point(), rect.size()));

  window_widget_ = widget_delegate->GetWidget();
  window_widget_->Show();

  window_x11_->Show();
#endif  // BUILDFLAG(SUPPORTS_OZONE_X11)

  // As an additional requirement on Linux, we must set the colors for the
  // render widgets in webkit.
  auto prefs = web_contents_->GetMutableRendererPrefs();
  prefs->focus_ring_color = SkColorSetARGB(255, 229, 151, 0);

  prefs->active_selection_bg_color = SkColorSetRGB(30, 144, 255);
  prefs->active_selection_fg_color = SK_ColorWHITE;
  prefs->inactive_selection_bg_color = SkColorSetRGB(200, 200, 200);
  prefs->inactive_selection_fg_color = SkColorSetRGB(50, 50, 50);

  // Set font-related attributes.
  static const gfx::FontRenderParams params(
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr));
  prefs->should_antialias_text = params.antialiasing;
  prefs->use_subpixel_positioning = params.subpixel_positioning;
  prefs->hinting = params.hinting;
  prefs->use_autohinter = params.autohinter;
  prefs->use_bitmaps = params.use_bitmaps;
  prefs->subpixel_rendering = params.subpixel_rendering;

  web_contents_->SyncRendererPrefs();

  return true;
}

void CefBrowserPlatformDelegateNativeLinux::CloseHostWindow() {
#if BUILDFLAG(SUPPORTS_OZONE_X11)
  if (window_x11_) {
    window_x11_->Close();
  }
#endif
}

CefWindowHandle CefBrowserPlatformDelegateNativeLinux::GetHostWindowHandle()
    const {
  if (windowless_handler_) {
    return windowless_handler_->GetParentWindowHandle();
  }
  return window_info_.window;
}

views::Widget* CefBrowserPlatformDelegateNativeLinux::GetWindowWidget() const {
  return window_widget_;
}

#if BUILDFLAG(SUPPORTS_OZONE_X11)
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
    if (std::ranges::contains(wm_states, hidden_atom)) {
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
  if (query_tree && query_tree->children.size() >= 1U) {
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
    if (query_tree->parent == query_tree->root) {
      break;
    }
    window = query_tree->parent;
  } while (true);

  return top_level_window;
}

bool IsParentOfChildWindow(x11::Connection* connection,
                           x11::Window parent,
                           x11::Window child) {
  if (parent == child) {
    return false;
  }
  do {
    auto query_tree = connection->QueryTree({child}).Sync();
    if (!query_tree) {
      break;
    }

    if (query_tree->parent == query_tree->root) {
      return query_tree->parent == parent;
    }
    if (query_tree->parent == parent) {
      return true;
    }
    child = query_tree->parent;
  } while (true);
  return false;
}

void CefBrowserPlatformDelegateNativeLinux::chromeRuntimeBrowserFocus() {
  auto host_x11_win = static_cast<x11::Window>(GetHostWindowHandle());
  if (host_x11_win == x11::Window::None) {
    return;
  }

  if (not IsWindowVisible(connection_, host_x11_win)) {
    return;
  }

  const auto focused = connection_->GetInputFocus().Sync().reply->focus;
  x11::Window focus_target = host_x11_win;

  if (browser_.get()) {
    if (focused != host_x11_win) {
      auto top_parent = FindToplevelParent(connection_, host_x11_win);
      if (top_parent != host_x11_win) {
        connection_->MapWindow({top_parent});
      }
      // Give focus to the child DesktopWindowTreeHostLinux.
      focus_target = host_x11_win;
      connection_
          ->SetInputFocus(
              {x11::InputFocus::Parent, focus_target, x11::Time::CurrentTime})
          .IgnoreError();
      if (focused != host_x11_win) {
        // Store the focused window to restore the original state precisely.
        previously_focused_ = focused;
      }
    }
  } else if (focused != host_x11_win) {
    auto top_parent = FindToplevelParent(connection_, host_x11_win);
    if (top_parent != host_x11_win) {
      connection_->MapWindow({top_parent});
    }
    connection_
        ->SetInputFocus(
            {x11::InputFocus::Parent, host_x11_win, x11::Time::CurrentTime})
        .IgnoreError();
    // Store the focused window to restore the original state precisely.
    previously_focused_ = focused;
  }
  // enable keyboard events when the window got focus
  enableKeyboardEventsForWindow(host_x11_win, true);
}

void CefBrowserPlatformDelegateNativeLinux::chromeRuntimeBrowserUnfocus() {
  auto host_x11_win = static_cast<x11::Window>(GetHostWindowHandle());
  if (host_x11_win == x11::Window::None) {
    return;
  }

  if (not IsWindowVisible(connection_, host_x11_win)) {
    return;
  }

  auto focused = x11::Window::None;
  focused = connection_->GetInputFocus().Sync().reply->focus;
  if (focused == x11::Window::None) {
    return;
  }

  auto toplevel = FindToplevelParent(connection_, host_x11_win);
  if (toplevel == host_x11_win) {
    return;
  }

  x11::Window child = x11::Window::None;
  if (browser_.get()) {
    child = FindChild(connection_, host_x11_win);
  }
  auto old_focused = x11::Window::None;
  if (focused == host_x11_win || focused == child) {
    old_focused = focused;
    // Our window or child window  still has keyboard focus. Return it back to
    // the original window so that GUI toolkits can receive keyboard events
    // again.
    if (previously_focused_ != x11::Window::None &&
        IsParentOfChildWindow(connection_, toplevel, previously_focused_)) {
      // GTK+ may have a special "focus window" for keyboard events. It must be
      // a child of the toplevel though.
      connection_
          ->SetInputFocus({x11::InputFocus::Parent, previously_focused_,
                           x11::Time::CurrentTime})
          .IgnoreError();
    } else {
      // Otherwise, the tolevel window is the best focus candidate we have.
      connection_
          ->SetInputFocus(
              {x11::InputFocus::Parent, toplevel, x11::Time::CurrentTime})
          .IgnoreError();
    }
  }

  if (old_focused != x11::Window::None) {
    // disable keyboard events when lost focus
    enableKeyboardEventsForWindow(old_focused, false);
  }
}
#endif

void CefBrowserPlatformDelegateNativeLinux::SetFocus(bool setFocus) {
  if (!setFocus) {
#if BUILDFLAG(SUPPORTS_OZONE_X11)
    chromeRuntimeBrowserUnfocus();
    return;
#endif
  }

  if (web_contents_) {
    // Give logical focus to the RenderWidgetHostViewAura in the views
    // hierarchy. This does not change the native keyboard focus.
    web_contents_->Focus();
  }

#if BUILDFLAG(SUPPORTS_OZONE_X11)
  chromeRuntimeBrowserFocus();
#endif  // BUILDFLAG(SUPPORTS_OZONE_X11)
}

void CefBrowserPlatformDelegateNativeLinux::NotifyMoveOrResizeStarted() {
  // Call the parent method to dismiss any existing popups.
  CefBrowserPlatformDelegateNativeAura::NotifyMoveOrResizeStarted();

  if (!web_contents_) {
    return;
  }

#if BUILDFLAG(SUPPORTS_OZONE_X11)
  if (!window_x11_) {
    return;
  }

  views::DesktopWindowTreeHostLinux* tree_host = window_x11_->GetHost();
  if (!tree_host) {
    return;
  }

  // Explicitly set the screen bounds so that WindowTreeHost::*Screen()
  // methods return the correct results.
  const gfx::Rect& bounds = window_x11_->GetBoundsInScreen();
  tree_host->set_screen_bounds(bounds);

  // Send updated screen rectangle information to the renderer process so that
  // popups are displayed in the correct location.
  content::RenderWidgetHostImpl::From(
      web_contents_->GetRenderViewHost()->GetWidget())
      ->SendScreenRects();
#endif  // BUILDFLAG(SUPPORTS_OZONE_X11)
}

void CefBrowserPlatformDelegateNativeLinux::SizeTo(int width, int height) {
#if BUILDFLAG(SUPPORTS_OZONE_X11)
  if (window_x11_) {
    window_x11_->SetBounds(
        gfx::Rect(window_x11_->bounds().origin(), gfx::Size(width, height)));
  }
#endif  // BUILDFLAG(SUPPORTS_OZONE_X11)
}

void CefBrowserPlatformDelegateNativeLinux::ViewText(const std::string& text) {
  char buff[] = "/tmp/CEFSourceXXXXXX";
  int fd = mkstemp(buff);

  if (fd == -1) {
    return;
  }

  FILE* srcOutput = fdopen(fd, "w+");
  if (!srcOutput) {
    return;
  }

  if (fputs(text.c_str(), srcOutput) < 0) {
    fclose(srcOutput);
    return;
  }

  fclose(srcOutput);

  std::string newName(buff);
  newName.append(".txt");
  if (rename(buff, newName.c_str()) != 0) {
    return;
  }

  std::string openCommand("xdg-open ");
  openCommand += newName;

  [[maybe_unused]] int result = system(openCommand.c_str());
}

bool CefBrowserPlatformDelegateNativeLinux::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  // TODO(cef): Is something required here to handle shortcut keys?
  return false;
}

CefEventHandle CefBrowserPlatformDelegateNativeLinux::GetEventHandle(
    const input::NativeWebKeyboardEvent& event) const {
  // TODO(cef): We need to return an XEvent* from this method, but
  // |event.os_event->native_event()| now returns a ui::Event* instead.
  // See https://crbug.com/965991.
  return nullptr;
}

ui::KeyEvent CefBrowserPlatformDelegateNativeLinux::TranslateUiKeyEvent(
    const CefKeyEvent& key_event) const {
  int flags = TranslateUiEventModifiers(key_event.modifiers);
  ui::KeyboardCode key_code =
      static_cast<ui::KeyboardCode>(key_event.windows_key_code);
  ui::DomCode dom_code =
      ui::KeycodeConverter::NativeKeycodeToDomCode(key_event.native_key_code);

#if BUILDFLAG(SUPPORTS_OZONE_X11)
  int keysym = ui::XKeysymForWindowsKeyCode(
      key_code, !!(key_event.modifiers & EVENTFLAG_SHIFT_DOWN));
  char16_t character = ui::GetUnicodeCharacterFromXKeySym(keysym);
#else
  char16_t character = key_event.character;
#endif

  base::TimeTicks time_stamp = GetEventTimeStamp();

  if (key_event.type == KEYEVENT_CHAR) {
    return ui::KeyEvent::FromCharacter(character, key_code, dom_code, flags,
                                       time_stamp);
  }

  ui::EventType type = ui::EventType::kUnknown;
  switch (key_event.type) {
    case KEYEVENT_RAWKEYDOWN:
    case KEYEVENT_KEYDOWN:
      type = ui::EventType::kKeyPressed;
      break;
    case KEYEVENT_KEYUP:
      type = ui::EventType::kKeyReleased;
      break;
    default:
      DCHECK(false);
  }

#if BUILDFLAG(SUPPORTS_OZONE_X11)
  ui::DomKey dom_key = ui::XKeySymToDomKey(keysym, character);
#else
  ui::DomKey dom_key = ui::DomKey::NONE;
#endif

  return ui::KeyEvent(type, key_code, dom_code, flags, dom_key, time_stamp);
}

input::NativeWebKeyboardEvent
CefBrowserPlatformDelegateNativeLinux::TranslateWebKeyEvent(
    const CefKeyEvent& key_event) const {
  ui::KeyEvent ui_event = TranslateUiKeyEvent(key_event);
  if (key_event.type == KEYEVENT_CHAR) {
    return input::NativeWebKeyboardEvent(ui_event, key_event.character);
  }
  return input::NativeWebKeyboardEvent(ui_event);
}
