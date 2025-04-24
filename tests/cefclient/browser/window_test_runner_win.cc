// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/window_test_runner_win.h"

#include "tests/shared/browser/main_message_loop.h"

namespace client::window_test {

namespace {

HWND GetRootHwnd(CefRefPtr<CefBrowser> browser) {
  return ::GetAncestor(browser->GetHost()->GetWindowHandle(), GA_ROOT);
}

// Toggles the current display state.
void Toggle(HWND root_hwnd, UINT nCmdShow) {
  // Retrieve current window placement information.
  WINDOWPLACEMENT placement;
  ::GetWindowPlacement(root_hwnd, &placement);

  if (placement.showCmd == nCmdShow) {
    ::ShowWindow(root_hwnd, SW_RESTORE);
  } else {
    ::ShowWindow(root_hwnd, nCmdShow);
  }
}

}  // namespace

WindowTestRunnerWin::WindowTestRunnerWin() = default;

void WindowTestRunnerWin::Minimize(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();

  HWND root_hwnd = GetRootHwnd(browser);
  if (!root_hwnd) {
    return;
  }
  Toggle(root_hwnd, SW_MINIMIZE);
}

void WindowTestRunnerWin::Maximize(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();

  HWND root_hwnd = GetRootHwnd(browser);
  if (!root_hwnd) {
    return;
  }
  Toggle(root_hwnd, SW_MAXIMIZE);
}

void WindowTestRunnerWin::Restore(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();

  HWND root_hwnd = GetRootHwnd(browser);
  if (!root_hwnd) {
    return;
  }
  ::ShowWindow(root_hwnd, SW_RESTORE);
}

}  // namespace client::window_test
