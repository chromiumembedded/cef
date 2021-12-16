// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/browser_platform_delegate_native_linux.h"

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/context.h"
#include "libcef/browser/native/menu_runner_linux.h"
#include "libcef/browser/native/window_delegate_view.h"
#include "libcef/browser/thread_util.h"

#include "base/no_destructor.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/keycodes/keyboard_code_conversion_xkb.h"
#include "ui/events/keycodes/keysym_to_unicode.h"
#include "ui/gfx/font_render_params.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(OZONE_PLATFORM_X11)
#include "libcef/browser/native/window_x11.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#endif

CefBrowserPlatformDelegateNativeLinux::CefBrowserPlatformDelegateNativeLinux(
    const CefWindowInfo& window_info,
    SkColor background_color)
    : CefBrowserPlatformDelegateNativeAura(window_info, background_color),
      host_window_created_(false),
      window_widget_(nullptr) {}

void CefBrowserPlatformDelegateNativeLinux::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateNative::BrowserDestroyed(browser);

  if (host_window_created_) {
    // Release the reference added in CreateHostWindow().
    browser->Release();
  }
}

bool CefBrowserPlatformDelegateNativeLinux::CreateHostWindow() {
  DCHECK(!window_widget_);

  if (window_info_.bounds.width == 0)
    window_info_.bounds.width = 800;
  if (window_info_.bounds.height == 0)
    window_info_.bounds.height = 600;

  gfx::Rect rect(window_info_.bounds.x, window_info_.bounds.y,
                 window_info_.bounds.width, window_info_.bounds.height);

#if BUILDFLAG(OZONE_PLATFORM_X11)
  DCHECK(!window_x11_);

  x11::Window parent_window = x11::Window::None;
  if (window_info_.parent_window != kNullWindowHandle) {
    parent_window = static_cast<x11::Window>(window_info_.parent_window);
  }

  // Create a new window object. It will delete itself when the associated X11
  // window is destroyed.
  window_x11_ =
      new CefWindowX11(browser_, parent_window, rect,
                       CefString(&window_info_.window_name).ToString());
  DCHECK_NE(window_x11_->xwindow(), x11::Window::None);
  window_info_.window =
      static_cast<cef_window_handle_t>(window_x11_->xwindow());

  host_window_created_ = true;

  // Add a reference that will be released in BrowserDestroyed().
  browser_->AddRef();

  CefWindowDelegateView* delegate_view = new CefWindowDelegateView(
      GetBackgroundColor(), window_x11_->TopLevelAlwaysOnTop(),
      GetBoundsChangedCallback());
  delegate_view->Init(static_cast<gfx::AcceleratedWidget>(window_info_.window),
                      web_contents_, gfx::Rect(gfx::Point(), rect.size()));

  window_widget_ = delegate_view->GetWidget();
  window_widget_->Show();

  window_x11_->Show();
#endif  // BUILDFLAG(OZONE_PLATFORM_X11)

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
#if BUILDFLAG(OZONE_PLATFORM_X11)
  if (window_x11_)
    window_x11_->Close();
#endif
}

CefWindowHandle CefBrowserPlatformDelegateNativeLinux::GetHostWindowHandle()
    const {
  if (windowless_handler_)
    return windowless_handler_->GetParentWindowHandle();
  return window_info_.window;
}

views::Widget* CefBrowserPlatformDelegateNativeLinux::GetWindowWidget() const {
  return window_widget_;
}

void CefBrowserPlatformDelegateNativeLinux::SetFocus(bool setFocus) {
  if (!setFocus)
    return;

  if (web_contents_) {
    // Give logical focus to the RenderWidgetHostViewAura in the views
    // hierarchy. This does not change the native keyboard focus.
    web_contents_->Focus();
  }

#if BUILDFLAG(OZONE_PLATFORM_X11)
  if (window_x11_) {
    // Give native focus to the DesktopNativeWidgetAura for the root window.
    // Needs to be done via the ::Window so that keyboard focus is assigned
    // correctly.
    window_x11_->Focus();
  }
#endif  // BUILDFLAG(OZONE_PLATFORM_X11)
}

void CefBrowserPlatformDelegateNativeLinux::NotifyMoveOrResizeStarted() {
  // Call the parent method to dismiss any existing popups.
  CefBrowserPlatformDelegateNative::NotifyMoveOrResizeStarted();

  if (!web_contents_)
    return;

#if BUILDFLAG(OZONE_PLATFORM_X11)
  if (!window_x11_)
    return;

  views::DesktopWindowTreeHostLinux* tree_host = window_x11_->GetHost();
  if (!tree_host)
    return;

  // Explicitly set the screen bounds so that WindowTreeHost::*Screen()
  // methods return the correct results.
  const gfx::Rect& bounds = window_x11_->GetBoundsInScreen();
  tree_host->set_screen_bounds(bounds);

  // Send updated screen rectangle information to the renderer process so that
  // popups are displayed in the correct location.
  content::RenderWidgetHostImpl::From(
      web_contents_->GetRenderViewHost()->GetWidget())
      ->SendScreenRects();
#endif  // BUILDFLAG(OZONE_PLATFORM_X11)
}

void CefBrowserPlatformDelegateNativeLinux::SizeTo(int width, int height) {
#if BUILDFLAG(OZONE_PLATFORM_X11)
  if (window_x11_) {
    window_x11_->SetBounds(
        gfx::Rect(window_x11_->bounds().origin(), gfx::Size(width, height)));
  }
#endif  // BUILDFLAG(OZONE_PLATFORM_X11)
}

gfx::Point CefBrowserPlatformDelegateNativeLinux::GetScreenPoint(
    const gfx::Point& view) const {
  if (windowless_handler_)
    return windowless_handler_->GetParentScreenPoint(view);

#if BUILDFLAG(OZONE_PLATFORM_X11)
  if (!window_x11_)
    return view;

  // We can't use aura::Window::GetBoundsInScreen on Linux because it will
  // return bounds from DesktopWindowTreeHostLinux which in our case is relative
  // to the parent window instead of the root window (screen).
  const gfx::Rect& bounds_in_screen = window_x11_->GetBoundsInScreen();
  return gfx::Point(bounds_in_screen.x() + view.x(),
                    bounds_in_screen.y() + view.y());
#else  // !BUILDFLAG(OZONE_PLATFORM_X11)
  return gfx::Point();
#endif
}

void CefBrowserPlatformDelegateNativeLinux::ViewText(const std::string& text) {
  char buff[] = "/tmp/CEFSourceXXXXXX";
  int fd = mkstemp(buff);

  if (fd == -1)
    return;

  FILE* srcOutput = fdopen(fd, "w+");
  if (!srcOutput)
    return;

  if (fputs(text.c_str(), srcOutput) < 0) {
    fclose(srcOutput);
    return;
  }

  fclose(srcOutput);

  std::string newName(buff);
  newName.append(".txt");
  if (rename(buff, newName.c_str()) != 0)
    return;

  std::string openCommand("xdg-open ");
  openCommand += newName;

  int result = system(openCommand.c_str());
  ALLOW_UNUSED_LOCAL(result);
}

bool CefBrowserPlatformDelegateNativeLinux::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // TODO(cef): Is something required here to handle shortcut keys?
  return false;
}

// static
void CefBrowserPlatformDelegate::HandleExternalProtocol(const GURL& url) {}

CefEventHandle CefBrowserPlatformDelegateNativeLinux::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  // TODO(cef): We need to return an XEvent* from this method, but
  // |event.os_event->native_event()| now returns a ui::Event* instead.
  // See https://crbug.com/965991.
  return nullptr;
}

std::unique_ptr<CefMenuRunner>
CefBrowserPlatformDelegateNativeLinux::CreateMenuRunner() {
  return base::WrapUnique(new CefMenuRunnerLinux);
}

gfx::Point CefBrowserPlatformDelegateNativeLinux::GetDialogPosition(
    const gfx::Size& size) {
  const gfx::Size& max_size = GetMaximumDialogSize();
  return gfx::Point((max_size.width() - size.width()) / 2,
                    (max_size.height() - size.height()) / 2);
}

gfx::Size CefBrowserPlatformDelegateNativeLinux::GetMaximumDialogSize() {
  return GetWindowWidget()->GetWindowBoundsInScreen().size();
}

ui::KeyEvent CefBrowserPlatformDelegateNativeLinux::TranslateUiKeyEvent(
    const CefKeyEvent& key_event) const {
  int flags = TranslateUiEventModifiers(key_event.modifiers);
  ui::KeyboardCode key_code =
      static_cast<ui::KeyboardCode>(key_event.windows_key_code);
  ui::DomCode dom_code =
      ui::KeycodeConverter::NativeKeycodeToDomCode(key_event.native_key_code);
  int keysym = ui::XKeysymForWindowsKeyCode(
      key_code, !!(key_event.modifiers & EVENTFLAG_SHIFT_DOWN));
  char16_t character = ui::GetUnicodeCharacterFromXKeySym(keysym);
  base::TimeTicks time_stamp = GetEventTimeStamp();

  if (key_event.type == KEYEVENT_CHAR) {
    return ui::KeyEvent(character, key_code, dom_code, flags, time_stamp);
  }

  ui::EventType type = ui::ET_UNKNOWN;
  switch (key_event.type) {
    case KEYEVENT_RAWKEYDOWN:
    case KEYEVENT_KEYDOWN:
      type = ui::ET_KEY_PRESSED;
      break;
    case KEYEVENT_KEYUP:
      type = ui::ET_KEY_RELEASED;
      break;
    default:
      NOTREACHED();
  }

  ui::DomKey dom_key = ui::XKeySymToDomKey(keysym, character);
  return ui::KeyEvent(type, key_code, dom_code, flags, dom_key, time_stamp);
}

content::NativeWebKeyboardEvent
CefBrowserPlatformDelegateNativeLinux::TranslateWebKeyEvent(
    const CefKeyEvent& key_event) const {
  ui::KeyEvent ui_event = TranslateUiKeyEvent(key_event);
  if (key_event.type == KEYEVENT_CHAR) {
    return content::NativeWebKeyboardEvent(ui_event, key_event.character);
  }
  return content::NativeWebKeyboardEvent(ui_event);
}
