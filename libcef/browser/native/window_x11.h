// Copyright 2014 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_WINDOW_X11_H_
#define CEF_LIBCEF_BROWSER_NATIVE_WINDOW_X11_H_
#pragma once

#include "include/internal/cef_ptr.h"

#include "base/memory/weak_ptr.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"

namespace views {
class DesktopWindowTreeHostLinux;
}

class CefBrowserHostBase;

// Object wrapper for an X11 Window.
// Based on WindowTreeHostX11 and DesktopWindowTreeHostX11.
class CefWindowX11 : public ui::PlatformEventDispatcher,
                     public x11::EventObserver {
 public:
  CefWindowX11(CefRefPtr<CefBrowserHostBase> browser,
               x11::Window parent_xwindow,
               const gfx::Rect& bounds,
               const std::string& title);

  CefWindowX11(const CefWindowX11&) = delete;
  CefWindowX11& operator=(const CefWindowX11&) = delete;

  ~CefWindowX11() override;

  void Close();

  void Show();
  void Hide();

  void Focus();

  void SetBounds(const gfx::Rect& bounds);

  gfx::Rect GetBoundsInScreen();

  views::DesktopWindowTreeHostLinux* GetHost();

  // ui::PlatformEventDispatcher methods:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  // x11::EventObserver methods:
  void OnEvent(const x11::Event& event) override;

  x11::Window xwindow() const { return xwindow_; }
  gfx::Rect bounds() const { return bounds_; }

  bool TopLevelAlwaysOnTop() const;

 private:
  void ContinueFocus();

  void ProcessXEvent(const x11::Event& xev);

  bool IsTargetedBy(const x11::Event& xev) const;

  CefRefPtr<CefBrowserHostBase> browser_;

  // The display and the native X window hosting the root window.
  x11::Connection* const connection_;
  x11::Window parent_xwindow_;
  x11::Window xwindow_;

  // Events selected on |xwindow_|.
  x11::ScopedEventSelector xwindow_events_;

  // Is the window mapped to the screen?
  bool window_mapped_ = false;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_;

  bool focus_pending_ = false;

  // Must always be the last member.
  base::WeakPtrFactory<CefWindowX11> weak_ptr_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_WINDOW_X11_H_
