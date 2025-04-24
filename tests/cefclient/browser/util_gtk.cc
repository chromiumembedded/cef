// Copyright (c) 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/util_gtk.h"

#include <gdk/gdk.h>

namespace client {

base::PlatformThreadId ScopedGdkThreadsEnter::locked_thread_ =
    kInvalidPlatformThreadId;

ScopedGdkThreadsEnter::ScopedGdkThreadsEnter() {
  // The GDK lock is not reentrant, so don't try to lock again if the current
  // thread already holds it.
  base::PlatformThreadId current_thread = base::PlatformThread::CurrentId();
  take_lock_ = current_thread != locked_thread_;

  if (take_lock_) {
    gdk_threads_enter();
    locked_thread_ = current_thread;
  }
}

ScopedGdkThreadsEnter::~ScopedGdkThreadsEnter() {
  if (take_lock_) {
    locked_thread_ = kInvalidPlatformThreadId;
    gdk_threads_leave();
  }
}

CefRect GetWindowBounds(GtkWindow* window, bool include_frame) {
  GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(window));

  gint x = 0;
  gint y = 0;
  gdk_window_get_origin(gdk_window, &x, &y);

  gint width = 0;
  gint height = 0;
  gdk_window_get_geometry(gdk_window, nullptr, nullptr, &width, &height);

  if (include_frame) {
    GdkRectangle frame_rect = {};
    gdk_window_get_frame_extents(gdk_window, &frame_rect);

    // This calculation assumes that all added frame height is at the top of the
    // window, which may be incorrect for some window managers.
    y -= frame_rect.height - height;
  }

  return {x, y, width, height};
}

bool IsWindowMaximized(GtkWindow* window) {
  GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
  gint state = gdk_window_get_state(gdk_window);
  return (state & GDK_WINDOW_STATE_MAXIMIZED) ? true : false;
}

void MinimizeWindow(GtkWindow* window) {
  // Unmaximize the window before minimizing so restore behaves correctly.
  if (IsWindowMaximized(window)) {
    gtk_window_unmaximize(window);
  }

  gtk_window_iconify(window);
}

void MaximizeWindow(GtkWindow* window) {
  gtk_window_maximize(window);
}

void RestoreWindow(GtkWindow* window) {
  if (IsWindowMaximized(window)) {
    gtk_window_unmaximize(window);
  } else {
    gtk_window_present(window);
  }
}

}  // namespace client
