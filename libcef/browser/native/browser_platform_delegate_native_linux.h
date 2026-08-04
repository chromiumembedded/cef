// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_LINUX_H_
#define CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_LINUX_H_

#include "base/memory/raw_ptr.h"
#include "cef/libcef/browser/native/browser_platform_delegate_native_aura.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/gfx/native_ui_types.h"

namespace gfx {
class Rect;
}

#if BUILDFLAG(SUPPORTS_OZONE_X11)
class CefWindowX11;
#endif

// Windowed browser implementation for Linux.
class CefBrowserPlatformDelegateNativeLinux
    : public CefBrowserPlatformDelegateNativeAura {
 public:
  CefBrowserPlatformDelegateNativeLinux(const CefWindowInfo& window_info,
                                        SkColor background_color);

  // CefBrowserPlatformDelegate methods:
  void BrowserDestroyed(CefBrowserHostBase* browser) override;
  bool CreateHostWindow() override;
  void CloseHostWindow() override;
  CefWindowHandle GetHostWindowHandle() const override;
  views::Widget* GetWindowWidget() const override;
  void SetFocus(bool setFocus) override;
  void NotifyMoveOrResizeStarted() override;
  void SizeTo(int width, int height) override;
  void SetHostBounds(const gfx::Rect& bounds) override;
  void ViewText(const std::string& text) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  CefEventHandle GetEventHandle(
      const input::NativeWebKeyboardEvent& event) const override;

  // CefBrowserPlatformDelegateNativeAura methods:
  ui::KeyEvent TranslateUiKeyEvent(const CefKeyEvent& key_event) const override;
  input::NativeWebKeyboardEvent TranslateWebKeyEvent(
      const CefKeyEvent& key_event) const override;

 private:
#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
  // Creates the browser window as a wl_subsurface of the client's surface,
  // which must have been provided via the three-argument
  // CefWindowInfo::SetAsChild().
  bool CreateWaylandHostWindow(const gfx::Rect& rect);

  // Runs |clear_widget| and then completes the browser teardown. Bound as the
  // widget delete callback because on Wayland nothing else signals that the
  // host window is gone.
  void WaylandWidgetDeleted(base::OnceClosure clear_widget);
#endif

  // True if the host window has been created.
  bool host_window_created_ = false;

#if BUILDFLAG(SUPPORTS_OZONE_X11)
  raw_ptr<CefWindowX11> window_x11_ = nullptr;
#endif

#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
  // Names the client's wl_surface for the duration of the browser. Released in
  // BrowserDestroyed().
  gfx::AcceleratedWidget wayland_parent_widget_ = gfx::kNullAcceleratedWidget;
#endif

  base::WeakPtrFactory<CefBrowserPlatformDelegateNativeLinux> linux_weak_factory_{
      this};
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_LINUX_H_
