// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/window_test_runner.h"

#include "tests/cefclient/browser/root_window.h"

namespace client::window_test {

void WindowTestRunner::SetPos(CefRefPtr<CefBrowser> browser,
                              int x,
                              int y,
                              int width,
                              int height) {
  REQUIRE_MAIN_THREAD();

  auto root_window = RootWindow::GetForBrowser(browser->GetIdentifier());
  root_window->SetBounds(
      x, y, width, height,
      /*content_bounds=*/root_window->DefaultToContentBounds());
}

void WindowTestRunner::Fullscreen(CefRefPtr<CefBrowser> browser) {
  NOTIMPLEMENTED();
}

void WindowTestRunner::SetTitleBarHeight(CefRefPtr<CefBrowser> browser,
                                         const std::optional<float>& height) {
  NOTIMPLEMENTED();
}

}  // namespace client::window_test
