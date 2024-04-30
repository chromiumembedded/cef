// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/views/view_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include <dwmapi.h>

#include "base/win/windows_version.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace view_util {

gfx::NativeWindow GetNativeWindow(views::Widget* widget) {
  if (widget) {
    return widget->GetNativeWindow();
  }
  return nullptr;
}

gfx::NativeView GetNativeView(views::Widget* widget) {
  if (widget) {
    return widget->GetNativeView();
  }
  return nullptr;
}

CefWindowHandle GetWindowHandle(views::Widget* widget) {
  // Same implementation as views::HWNDForView() but cross-platform.
  if (widget) {
    return GetWindowHandle(widget->GetNativeWindow());
  }
  return kNullWindowHandle;
}

CefWindowHandle GetWindowHandle(gfx::NativeWindow window) {
  // |window| is an aura::Window*.
  if (window && window->GetRootWindow()) {
    return window->GetHost()->GetAcceleratedWidget();
  }
  return kNullWindowHandle;
}

views::NativeWidget* CreateNativeWidget(
    views::internal::NativeWidgetDelegate* delegate,
    CefRefPtr<CefWindow> window,
    CefWindowDelegate* window_delegate) {
  return nullptr;
}

void SetHostView(views::Widget* widget, views::View* host_view) {
  widget->GetNativeView()->SetProperty(views::kHostViewKey, host_view);
}

views::View* GetHostView(const views::Widget* widget) {
  return widget->GetNativeView()->GetProperty(views::kHostViewKey);
}

void UpdateTitlebarTheme(views::Widget* widget) {
#if BUILDFLAG(IS_WIN)
  // Value was 19 prior to Windows 10 20H1, according to
  // https://stackoverflow.com/a/70693198
  const DWORD dwAttribute =
      base::win::GetVersion() >= base::win::Version::WIN10_20H1
          ? DWMWA_USE_IMMERSIVE_DARK_MODE
          : 19;

  const HWND widget_hwnd = views::HWNDForWidget(widget);

  BOOL has_dark_titlebar = FALSE;
  DwmGetWindowAttribute(widget_hwnd, dwAttribute, &has_dark_titlebar,
                        sizeof(has_dark_titlebar));

  const BOOL dark_titlebar_enabled = ShouldUseDarkTheme(widget);
  if (has_dark_titlebar == dark_titlebar_enabled) {
    // No change required.
    return;
  }

  // From BrowserFrameViewWin::SetSystemMicaTitlebarAttributes.
  DwmSetWindowAttribute(widget_hwnd, dwAttribute, &dark_titlebar_enabled,
                        sizeof(dark_titlebar_enabled));

  // Repaint the titlebar if the Widget is visible. None of the usual NC
  // repaint techniques work with DwmSetWindowAttribute so use a workaround
  // that nudges the window width.
  // See https://stackoverflow.com/a/78269906/23991994
  if (IsWindowVisible(widget_hwnd) && !IsIconic(widget_hwnd)) {
    RECT rect = {};
    ::GetWindowRect(widget_hwnd, &rect);

    if (IsZoomed(widget_hwnd)) {
      // Window is currently maximized. Restore and then re-maximize the
      // window. The restore position is changed temporarily to make the
      // update less noticeable.
      WINDOWPLACEMENT placement = {};
      GetWindowPlacement(widget_hwnd, &placement);

      const RECT oldrect = placement.rcNormalPosition;

      placement.rcNormalPosition = rect;
      placement.rcNormalPosition.right -= 1;
      SetWindowPlacement(widget_hwnd, &placement);

      LockWindowUpdate(widget_hwnd);
      ShowWindow(widget_hwnd, SW_SHOWNORMAL);
      ShowWindow(widget_hwnd, SW_SHOWMAXIMIZED);
      LockWindowUpdate(nullptr);

      placement.rcNormalPosition = oldrect;
      SetWindowPlacement(widget_hwnd, &placement);
    } else {
      // Window is currently normal. Change and then restore the window width.
      // Use Defer functions to make the update less noticeable.
      HDWP defer = BeginDeferWindowPos(2);
      DeferWindowPos(defer, widget_hwnd, NULL, 0, 0, rect.right - rect.left - 1,
                     rect.bottom - rect.top,
                     SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
      DeferWindowPos(defer, widget_hwnd, NULL, 0, 0, rect.right - rect.left,
                     rect.bottom - rect.top,
                     SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
      LockWindowUpdate(widget_hwnd);
      EndDeferWindowPos(defer);
      LockWindowUpdate(nullptr);
    }
  }
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace view_util
