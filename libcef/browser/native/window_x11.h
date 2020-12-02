// Copyright 2014 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_WINDOW_X11_H_
#define CEF_LIBCEF_BROWSER_NATIVE_WINDOW_X11_H_
#pragma once

#include <memory>

#include "include/internal/cef_ptr.h"

#include "base/memory/weak_ptr.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {
class XScopedEventSelector;
}

namespace views {
class DesktopWindowTreeHostLinux;
}

class CefBrowserHostBase;

// Object wrapper for an X11 Window.
// Based on WindowTreeHostX11 and DesktopWindowTreeHostX11.
class CefWindowX11 : public ui::PlatformEventDispatcher,
                     public ui::XEventDispatcher {
 public:
  CefWindowX11(CefRefPtr<CefBrowserHostBase> browser,
               x11::Window parent_xwindow,
               const gfx::Rect& bounds,
               const std::string& title);
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

  // ui::XEventDispatcher methods:
  void CheckCanDispatchNextPlatformEvent(x11::Event* x11_event) override;
  void PlatformEventDispatchFinished() override;
  ui::PlatformEventDispatcher* GetPlatformEventDispatcher() override;
  bool DispatchXEvent(x11::Event* x11_event) override;

  x11::Window xwindow() const { return xwindow_; }
  gfx::Rect bounds() const { return bounds_; }

  bool TopLevelAlwaysOnTop() const;

 private:
  void ContinueFocus();

  bool IsTargetedBy(const x11::Event& x11_event) const;
  void ProcessXEvent(x11::Event* xev);

  CefRefPtr<CefBrowserHostBase> browser_;

  // The display and the native X window hosting the root window.
  x11::Connection* const connection_;
  x11::Window parent_xwindow_;
  x11::Window xwindow_;

  // Events selected on |xwindow_|.
  std::unique_ptr<ui::XScopedEventSelector> xwindow_events_;

  // Is the window mapped to the screen?
  bool window_mapped_ = false;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_;

  bool focus_pending_ = false;

  // Tells if this dispatcher can process next translated event based on a
  // previous check in ::CheckCanDispatchNextPlatformEvent based on a XID
  // target.
  x11::Event* current_xevent_ = nullptr;

  // Must always be the last member.
  base::WeakPtrFactory<CefWindowX11> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefWindowX11);
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_WINDOW_X11_H_
