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
#include "tests/shared/browser/geometry_util.h"

namespace client::window_test {

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

void MinimizeImpl(CefRefPtr<CefBrowser> browser) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&MinimizeImpl, browser));
    return;
  }

  GetWindow(browser)->Minimize();
}

void MaximizeImpl(CefRefPtr<CefBrowser> browser) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&MaximizeImpl, browser));
    return;
  }

  GetWindow(browser)->Maximize();
}

void RestoreImpl(CefRefPtr<CefBrowser> browser) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&RestoreImpl, browser));
    return;
  }

  GetWindow(browser)->Restore();
}

void FullscreenImpl(CefRefPtr<CefBrowser> browser) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&FullscreenImpl, browser));
    return;
  }

  auto window = GetWindow(browser);

  // Results in a call to ViewsWindow::OnWindowFullscreenTransition().
  if (window->IsFullscreen()) {
    window->SetFullscreen(false);
  } else {
    window->SetFullscreen(true);
  }
}

}  // namespace

WindowTestRunnerViews::WindowTestRunnerViews() = default;

void WindowTestRunnerViews::Minimize(CefRefPtr<CefBrowser> browser) {
  MinimizeImpl(browser);
}

void WindowTestRunnerViews::Maximize(CefRefPtr<CefBrowser> browser) {
  MaximizeImpl(browser);
}

void WindowTestRunnerViews::Restore(CefRefPtr<CefBrowser> browser) {
  RestoreImpl(browser);
}

void WindowTestRunnerViews::Fullscreen(CefRefPtr<CefBrowser> browser) {
  FullscreenImpl(browser);
}

void WindowTestRunnerViews::SetTitleBarHeight(
    CefRefPtr<CefBrowser> browser,
    const std::optional<float>& height) {
  REQUIRE_MAIN_THREAD();

  auto root_window = RootWindow::GetForBrowser(browser->GetIdentifier());
  auto root_window_views = static_cast<RootWindowViews*>(root_window.get());
  root_window_views->SetTitlebarHeight(height);
}

}  // namespace client::window_test
