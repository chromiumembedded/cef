// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/window_test_runner_views.h"

#include "include/views/cef_browser_view.h"
#include "include/views/cef_display.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

#include "tests/cefclient/browser/root_window_views.h"
#include "tests/cefclient/browser/views_window.h"

namespace client {
namespace window_test {

namespace {

CefRefPtr<CefWindow> GetWindow(const CefRefPtr<CefBrowser>& browser) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(browser->GetHost()->HasView());

  CefRefPtr<CefBrowserView> browser_view =
      CefBrowserView::GetForBrowser(browser);
  DCHECK(browser_view.get());

  CefRefPtr<CefWindow> window = browser_view->GetWindow();
  DCHECK(window.get());
  return window;
}

void SetTitlebarHeight(const CefRefPtr<CefBrowser>& browser,
                       const std::optional<float>& height) {
  CEF_REQUIRE_UI_THREAD();
  auto root_window = RootWindow::GetForBrowser(browser->GetIdentifier());
  DCHECK(root_window.get());

  auto root_window_views = static_cast<RootWindowViews*>(root_window.get());
  root_window_views->SetTitlebarHeight(height);
}

}  // namespace

WindowTestRunnerViews::WindowTestRunnerViews() {}

void WindowTestRunnerViews::SetPos(CefRefPtr<CefBrowser> browser,
                                   int x,
                                   int y,
                                   int width,
                                   int height) {
  CefRefPtr<CefWindow> window = GetWindow(browser);

  CefRect window_bounds(x, y, width, height);
  ModifyBounds(window->GetDisplay()->GetWorkArea(), window_bounds);

  window->SetBounds(window_bounds);
}

void WindowTestRunnerViews::Minimize(CefRefPtr<CefBrowser> browser) {
  GetWindow(browser)->Minimize();
}

void WindowTestRunnerViews::Maximize(CefRefPtr<CefBrowser> browser) {
  GetWindow(browser)->Maximize();
}

void WindowTestRunnerViews::Restore(CefRefPtr<CefBrowser> browser) {
  GetWindow(browser)->Restore();
}

void WindowTestRunnerViews::Fullscreen(CefRefPtr<CefBrowser> browser) {
  auto window = GetWindow(browser);

  // Results in a call to ViewsWindow::OnWindowFullscreenTransition().
  if (window->IsFullscreen()) {
    window->SetFullscreen(false);
  } else {
    window->SetFullscreen(true);
  }
}

void WindowTestRunnerViews::SetTitleBarHeight(
    CefRefPtr<CefBrowser> browser,
    const std::optional<float>& height) {
  SetTitlebarHeight(browser, height);
}

}  // namespace window_test
}  // namespace client
