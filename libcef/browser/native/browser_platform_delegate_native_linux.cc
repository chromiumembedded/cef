// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/native/browser_platform_delegate_native_linux.h"

#include "base/no_destructor.h"
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/native/native_widget_delegate.h"
#include "cef/libcef/browser/native/ozone_util_linux.h"
#include "cef/libcef/browser/thread_util.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
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

#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
#include "ui/ozone/platform/wayland/wayland_embedder.h"
#endif

CefBrowserPlatformDelegateNativeLinux::CefBrowserPlatformDelegateNativeLinux(
    const CefWindowInfo& window_info,
    SkColor background_color)
    : CefBrowserPlatformDelegateNativeAura(window_info, background_color) {}

void CefBrowserPlatformDelegateNativeLinux::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateNativeAura::BrowserDestroyed(browser);

#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
  if (wayland_parent_widget_ != gfx::kNullAcceleratedWidget) {
    ui::UnregisterForeignWaylandSurface(wayland_parent_widget_);
    wayland_parent_widget_ = gfx::kNullAcceleratedWidget;
  }
#endif

  if (host_window_created_) {
    // Release the reference added in CreateHostWindow().
    browser->Release();
  }
}

bool CefBrowserPlatformDelegateNativeLinux::CreateHostWindow() {
  DCHECK(!window_widget_);

  // A single libcef binary is typically built with both the X11 and Wayland
  // Ozone platforms compiled in, so BUILDFLAG(SUPPORTS_OZONE_X11) only tells us
  // that the X11 code exists -- not that it is usable. Creating a CefWindowX11
  // while Ozone/Wayland is active produces an X11 (or XWayland) window that is
  // not parented to the client's wl_surface, and then feeds its XID to
  // CefNativeWidgetDelegate::Init() as a gfx::AcceleratedWidget. Ozone/Wayland
  // resolves accelerated widgets through WaylandWindowManager, which has never
  // heard of that value, and the first event dispatch crashes in
  // WaylandScreen::GetPreferredScaleFactorForAcceleratedWidget().
  //
  // Fail explicitly instead. See
  // https://github.com/chromiumembedded/cef/issues/2804.
  if (!cef_ozone::SupportsNativeWindowedBrowsers()) {
    LOG(ERROR) << "Windowed browsers are not supported on the \""
               << cef_ozone::PlatformName()
               << "\" Ozone platform. Use windowless rendering "
                  "(CefWindowInfo::SetAsWindowless) or the Views framework "
                  "(CefBrowserView) instead. See "
                  "https://github.com/chromiumembedded/cef/issues/2804";
    return false;
  }

  if (window_info_.bounds.width == 0) {
    window_info_.bounds.width = 800;
  }
  if (window_info_.bounds.height == 0) {
    window_info_.bounds.height = 600;
  }

  gfx::Rect rect(window_info_.bounds.x, window_info_.bounds.y,
                 window_info_.bounds.width, window_info_.bounds.height);

#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
  if (cef_ozone::IsWayland()) {
    if (!CreateWaylandHostWindow(rect)) {
      return false;
    }
  }
#endif  // BUILDFLAG(SUPPORTS_OZONE_WAYLAND)

#if BUILDFLAG(SUPPORTS_OZONE_X11)
  if (cef_ozone::IsX11()) {
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
  }
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

#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
bool CefBrowserPlatformDelegateNativeLinux::CreateWaylandHostWindow(
    const gfx::Rect& rect) {
  // Under Ozone/Wayland parent_window names a wl_surface rather than an X11
  // Window. There is no way to tell a mistaken XID from a valid pointer, so a
  // client that passes the wrong kind of handle for the active platform gets
  // undefined behaviour rather than a diagnostic; all that can be checked is
  // that something was passed at all.
  auto* parent_surface =
      reinterpret_cast<wl_surface*>(window_info_.parent_window);
  if (!parent_surface) {
    LOG(ERROR) << "A windowed browser on Wayland needs the client's wl_surface "
                  "in CefWindowInfo::parent_window; it is null";
    return false;
  }
  // |parent_xdg_surface| is deliberately not required. Toolkits differ in
  // whether they expose the xdg_surface at all -- winit, for one, does not --
  // and refusing to create the browser would put embedding out of reach for
  // those hosts entirely. Menus then fall back to a wl_subsurface, which is
  // degraded but usable; see the warning below and cef_types_linux.h.
  LOG_IF(WARNING, !window_info_.parent_xdg_surface)
      << "No xdg_surface was provided in CefWindowInfo::parent_xdg_surface; "
         "menus, tooltips and <select> dropdowns will fall back to a "
         "wl_subsurface, which cannot extend past this window or be dismissed "
         "by clicking outside it";

  if (!ui::GetExternalWaylandDisplay()) {
    LOG(ERROR) << "cef_set_wayland_display() must be called before "
                  "CefInitialize() to embed a browser into a Wayland surface; "
                  "a wl_surface cannot be shared across connections";
    return false;
  }

  // Name the client's surface with a gfx::AcceleratedWidget so that it can
  // travel through views::Widget::InitParams::parent_widget, which is the only
  // way views knows how to express a parent. WaylandWindow::Create() resolves
  // it back into the surface and builds a WaylandEmbeddedWindow.
  wayland_parent_widget_ = ui::RegisterForeignWaylandSurface(
      parent_surface,
      static_cast<xdg_surface*>(window_info_.parent_xdg_surface));
  if (wayland_parent_widget_ == gfx::kNullAcceleratedWidget) {
    LOG(ERROR) << "Failed to register the parent wl_surface with Ozone";
    return false;
  }

  auto* widget_delegate = new CefNativeWidgetDelegate(
      GetBackgroundColor(), /*always_on_top=*/false, GetBoundsChangedCallback(),
      // The widget going away is what completes the browser teardown here, the
      // way WM_NCDESTROY does on Windows and CefWindowX11 does on X11. Without
      // it CloseBrowser() never finishes.
      base::BindOnce(
          &CefBrowserPlatformDelegateNativeLinux::WaylandWidgetDeleted,
          linux_weak_factory_.GetWeakPtr(), GetWidgetDeleteCallback()));
  widget_delegate->Init(wayland_parent_widget_, web_contents_, rect);

  window_widget_ = widget_delegate->GetWidget();

  // Report back what was created. Unlike X11, |window| is not a server-global
  // id here: it is the gfx::AcceleratedWidget that Ozone uses internally, and
  // it is only meaningful inside this process.
  wl_surface* surface = nullptr;
  if (auto* window = window_widget_->GetNativeWindow()) {
    if (auto* host = window->GetHost()) {
      const gfx::AcceleratedWidget widget = host->GetAcceleratedWidget();
      window_info_.window = static_cast<cef_window_handle_t>(widget);
      surface = ui::GetWaylandSurfaceForWidget(widget);
    }
  }

  // Everything that can fail has to fail before the two lines below. They are
  // undone by BrowserDestroyed(), which is not called for a browser that never
  // finished being created, so a later bail-out would leak the reference and
  // leave the client's surface registered forever.
  if (!surface) {
    LOG(ERROR) << "The browser window was created but has no wl_surface";
    window_widget_->CloseNow();
    window_widget_ = nullptr;
    ui::UnregisterForeignWaylandSurface(wayland_parent_widget_);
    wayland_parent_widget_ = gfx::kNullAcceleratedWidget;
    return false;
  }

  host_window_created_ = true;

  // Add a reference that will be released in BrowserDestroyed().
  browser_->AddRef();

  window_widget_->Show();

  VLOG(1) << "Embedded browser surface " << surface << " into parent surface "
          << parent_surface;
  return true;
}

void CefBrowserPlatformDelegateNativeLinux::WaylandWidgetDeleted(
    base::OnceClosure clear_widget) {
  std::move(clear_widget).Run();

  // Only meaningful once the host window was fully created. A widget torn down
  // by a failed CreateWaylandHostWindow() has no reference to release and no
  // browser to destroy; the Windows path guards the same case by checking
  // |browser| for null.
  if (host_window_created_ && browser_) {
    // Force the browser to be destroyed. This results in a call to
    // BrowserDestroyed() that releases the reference added above.
    AlloyBrowserHostImpl::FromBaseChecked(browser_.get())->WindowDestroyed();
  }
}
#endif  // BUILDFLAG(SUPPORTS_OZONE_WAYLAND)

void CefBrowserPlatformDelegateNativeLinux::CloseHostWindow() {
#if BUILDFLAG(SUPPORTS_OZONE_X11)
  if (window_x11_) {
    window_x11_->Close();
    return;
  }
#endif

  // On X11 the close travels through the window manager: CefWindowX11 sends
  // itself WM_DELETE_WINDOW and destroys the browser when it comes back.
  // Wayland has no equivalent for a wl_subsurface -- xdg_toplevel.close does
  // not apply, and the compositor has nothing to close -- so the widget has to
  // be closed directly. Without this CloseBrowser() never completes; it waits
  // for a host window teardown that nothing will ever signal, and a later
  // CefShutdown() then destroys WaylandConnection underneath a browser that is
  // still alive.
  // IsClosed() because CloseBrowser() can arrive more than once -- a second
  // Close() on a widget already tearing down would run the delete callback
  // twice, and with it WindowDestroyed().
  if (window_widget_ && !window_widget_->IsClosed()) {
    window_widget_->Close();
  }
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

void CefBrowserPlatformDelegateNativeLinux::SetFocus(bool setFocus) {
#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
  // Handled ahead of the early return below, which predates embedding: under
  // X11 the window manager takes focus away by itself, so blur needed nothing
  // here. An embedded browser has no window the compositor knows about, and
  // both directions have to be driven explicitly -- otherwise a host with two
  // browsers can ask for focus but never give it up.
  if (cef_ozone::IsWayland() && window_widget_) {
    if (setFocus) {
      window_widget_->Activate();
    } else {
      window_widget_->Deactivate();
    }
  }
#endif  // BUILDFLAG(SUPPORTS_OZONE_WAYLAND)

  if (!setFocus) {
    return;
  }

  if (web_contents_) {
    // Give logical focus to the RenderWidgetHostViewAura in the views
    // hierarchy. This does not change the native keyboard focus.
    web_contents_->Focus();
  }

#if BUILDFLAG(SUPPORTS_OZONE_X11)
  if (window_x11_) {
    // Give native focus to the DesktopNativeWidgetAura for the root window.
    // Needs to be done via the ::Window so that keyboard focus is assigned
    // correctly.
    window_x11_->Focus();
    return;
  }
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

void CefBrowserPlatformDelegateNativeLinux::SetHostBounds(
    const gfx::Rect& bounds) {
#if BUILDFLAG(SUPPORTS_OZONE_X11)
  if (window_x11_) {
    window_x11_->SetBounds(bounds);
    return;
  }
#endif

  // On Wayland the browser's wl_subsurface receives no configure events, so the
  // client application is the only source of layout information. Route it
  // through the Widget, which reaches WaylandEmbeddedWindow::SetBoundsInDIP()
  // and hence wl_subsurface.set_position.
  if (window_widget_) {
    window_widget_->SetBounds(bounds);
  }
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

  // The X11 keysym tables are only meaningful when Ozone/X11 is active. Under
  // any other Ozone platform the client-supplied character is authoritative.
  char16_t character = key_event.character;
  bool have_keysym = false;
  int keysym = 0;
#if BUILDFLAG(SUPPORTS_OZONE_X11)
  if (cef_ozone::IsX11()) {
    keysym = ui::XKeysymForWindowsKeyCode(
        key_code, !!(key_event.modifiers & EVENTFLAG_SHIFT_DOWN));
    character = ui::GetUnicodeCharacterFromXKeySym(keysym);
    have_keysym = true;
  }
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

  ui::DomKey dom_key = ui::DomKey::NONE;
#if BUILDFLAG(SUPPORTS_OZONE_X11)
  if (have_keysym) {
    dom_key = ui::XKeySymToDomKey(keysym, character);
  }
#endif
  if (dom_key == ui::DomKey::NONE && character != 0) {
    dom_key = ui::DomKey::FromCharacter(character);
  }

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
