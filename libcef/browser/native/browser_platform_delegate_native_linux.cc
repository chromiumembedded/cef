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
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/keycodes/keyboard_code_conversion_xkb.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#endif

#if BUILDFLAG(SUPPORTS_OZONE_X11)
namespace {

// Returns the display whose native (pixel) bounds contain the largest portion
// of |bounds_in_pixels|, which must be expressed in X11 root (screen) pixel
// coordinates. On X11 each display carries its exact pixel geometry via
// native_origin()/GetSizeInPixel(), so match against that directly (as
// X11ScreenOzone::DisplayBoundsInPixels() does). display::Display::bounds() is
// unsuitable here: it is in DIP, and a non-primary display's DIP origin does
// not map back to its pixel origin by scaling (under mixed DPI the two layouts
// diverge). Falls back to the primary display when there is no intersection.
display::Display GetDisplayMatchingPixelBounds(
    const gfx::Rect& bounds_in_pixels) {
  display::Screen* screen = display::Screen::Get();
  display::Display matched = screen->GetPrimaryDisplay();
  int largest_area = 0;
  for (const display::Display& display : screen->GetAllDisplays()) {
    gfx::Rect display_pixels(display.native_origin(), display.GetSizeInPixel());
    display_pixels.Intersect(bounds_in_pixels);
    const int area = display_pixels.size().GetArea();
    if (area > largest_area) {
      largest_area = area;
      matched = display;
    }
  }
  return matched;
}

}  // namespace
#endif  // BUILDFLAG(SUPPORTS_OZONE_X11)

CefBrowserPlatformDelegateNativeLinux::CefBrowserPlatformDelegateNativeLinux(
    const CefWindowInfo& window_info,
    SkColor background_color)
    : CefBrowserPlatformDelegateNativeAura(window_info, background_color) {}

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

  // |rect| (from window_info_.bounds) is in pixels and matches the size of the
  // |window_x11_| host window created above, but views::Widget bounds are in
  // DIP. Determine the device scale factor from the display the host window is
  // actually on: translate the host window's bounds to X11 root (screen)
  // coordinates and match them against each display's native pixel bounds. This
  // selects the correct display when the window is placed on a non-primary
  // monitor, instead of assuming the primary display.
  //
  // The scale is computed here rather than deferred to the child window because
  // this early in initialization the child does not yet resolve to the correct
  // display. Its geometry in root coordinates is not established yet (the
  // GeometryCache behind X11Window::GetBoundsInPixels(), which X11ScreenOzone
  // reads, has not accumulated the parent offsets), and
  // DesktopWindowTreeHostPlatform::GetRootTransform() runs before its
  // platform_window exists during CreateXWindow(), so the child falls back to
  // the primary display's scale. As with the Windows implementation, the
  // Init() bounds have origin (0,0); the child is positioned by the host
  // window.
  const gfx::Rect host_bounds_in_pixels = window_x11_->GetBoundsInScreen();
  const float device_scale_factor =
      GetDisplayMatchingPixelBounds(host_bounds_in_pixels)
          .device_scale_factor();
  const gfx::Size initial_dip_size =
      gfx::ScaleToRoundedSize(rect.size(), 1.0f / device_scale_factor);

  widget_delegate->Init(
      static_cast<gfx::AcceleratedWidget>(window_info_.window), web_contents_,
      gfx::Rect(gfx::Point(), initial_dip_size));

  window_widget_ = widget_delegate->GetWidget();

  // For the same reason, this early in initialization the child compositor is
  // sized using the primary display's scale rather than the host window's (see
  // above), which on a mixed-DPI setup leaves the web content larger or smaller
  // than the host window (see issue #3396). Pin the compositor to the host
  // window's exact pixel size (|rect|) with a size-only child configure. See
  // SetChildSizeInPixels() for why aura::WindowTreeHost::SetBoundsInPixels()
  // cannot be used here.
  window_x11_->SetChildSizeInPixels(rect.size());

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

void CefBrowserPlatformDelegateNativeLinux::SetFocus(bool setFocus) {
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
