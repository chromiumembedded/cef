// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/window_test_runner_gtk.h"

#include <gtk/gtk.h>

#include "include/wrapper/cef_helpers.h"
#include "tests/cefclient/browser/root_window.h"
#include "tests/shared/browser/main_message_loop.h"

namespace client {
namespace window_test {

namespace {

GtkWindow* GetWindow(CefRefPtr<CefBrowser> browser) {
  scoped_refptr<RootWindow> root_window =
      RootWindow::GetForBrowser(browser->GetIdentifier());
  if (root_window) {
    GtkWindow* window = GTK_WINDOW(root_window->GetWindowHandle());
    if (!window)
      LOG(ERROR) << "No GtkWindow for browser";
    return window;
  }
  return NULL;
}

bool IsMaximized(GtkWindow* window) {
  GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
  gint state = gdk_window_get_state(gdk_window);
  return (state & GDK_WINDOW_STATE_MAXIMIZED) ? true : false;
}

}  // namespace

WindowTestRunnerGtk::WindowTestRunnerGtk() {
}

void WindowTestRunnerGtk::SetPos(CefRefPtr<CefBrowser> browser,
                                 int x, int y, int width, int height) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  GtkWindow* window = GetWindow(browser);
  if (!window)
    return;
  GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(window));

  // Make sure the window isn't minimized or maximized.
  if (IsMaximized(window))
    gtk_window_unmaximize(window);
  else
    gtk_window_present(window);

  // Retrieve information about the display that contains the window.
  GdkScreen* screen = gdk_screen_get_default();
  gint monitor = gdk_screen_get_monitor_at_window(screen, gdk_window);
  GdkRectangle rect;
  gdk_screen_get_monitor_geometry(screen, monitor, &rect);

  // Make sure the window is inside the display.
  CefRect display_rect(rect.x, rect.y, rect.width, rect.height);
  CefRect window_rect(x, y, width, height);
  ModifyBounds(display_rect, window_rect);

  gdk_window_move_resize(gdk_window, window_rect.x, window_rect.y,
                         window_rect.width, window_rect.height);
}

void WindowTestRunnerGtk::Minimize(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  GtkWindow* window = GetWindow(browser);
  if (!window)
    return;

  // Unmaximize the window before minimizing so restore behaves correctly.
  if (IsMaximized(window))
    gtk_window_unmaximize(window);

  gtk_window_iconify(window);
}

void WindowTestRunnerGtk::Maximize(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  GtkWindow* window = GetWindow(browser);
  if (!window)
    return;
  gtk_window_maximize(window);
}

void WindowTestRunnerGtk::Restore(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  GtkWindow* window = GetWindow(browser);
  if (!window)
    return;
  if (IsMaximized(window))
    gtk_window_unmaximize(window);
  else
    gtk_window_present(window);
}

}  // namespace window_test
}  // namespace client
