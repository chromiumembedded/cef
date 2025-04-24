// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/window_test_runner_gtk.h"

#include <gtk/gtk.h>

#include "include/wrapper/cef_helpers.h"
#include "tests/cefclient/browser/root_window.h"
#include "tests/cefclient/browser/util_gtk.h"
#include "tests/shared/browser/main_message_loop.h"

namespace client {
namespace window_test {

namespace {

GtkWindow* GetWindow(CefRefPtr<CefBrowser> browser) {
  scoped_refptr<RootWindow> root_window =
      RootWindow::GetForBrowser(browser->GetIdentifier());
  if (root_window) {
    GtkWindow* window = GTK_WINDOW(root_window->GetWindowHandle());
    if (!window) {
      LOG(ERROR) << "No GtkWindow for browser";
    }
    return window;
  }
  return nullptr;
}

}  // namespace

WindowTestRunnerGtk::WindowTestRunnerGtk() = default;

void WindowTestRunnerGtk::Minimize(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();
  ScopedGdkThreadsEnter scoped_gdk_threads;
  if (auto* window = GetWindow(browser)) {
    MinimizeWindow(window);
  }
}

void WindowTestRunnerGtk::Maximize(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();
  ScopedGdkThreadsEnter scoped_gdk_threads;
  if (auto* window = GetWindow(browser)) {
    MaximizeWindow(window);
  }
}

void WindowTestRunnerGtk::Restore(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();
  ScopedGdkThreadsEnter scoped_gdk_threads;
  if (auto* window = GetWindow(browser)) {
    RestoreWindow(window);
  }
}

}  // namespace window_test
}  // namespace client
