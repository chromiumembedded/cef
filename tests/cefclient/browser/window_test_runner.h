// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_WINDOW_TEST_RUNNER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_WINDOW_TEST_RUNNER_H_
#pragma once

#include <optional>

#include "include/cef_browser.h"

namespace client::window_test {

// Implement this interface for different platforms. Methods will be called on
// the browser process main thread unless otherwise indicated.
class WindowTestRunner {
 public:
  virtual ~WindowTestRunner() = default;

  void SetPos(CefRefPtr<CefBrowser> browser,
              int x,
              int y,
              int width,
              int height);
  virtual void Minimize(CefRefPtr<CefBrowser> browser) = 0;
  virtual void Maximize(CefRefPtr<CefBrowser> browser) = 0;
  virtual void Restore(CefRefPtr<CefBrowser> browser) = 0;
  virtual void Fullscreen(CefRefPtr<CefBrowser> browser);

  virtual void SetTitleBarHeight(CefRefPtr<CefBrowser> browser,
                                 const std::optional<float>& height);
};

}  // namespace client::window_test

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_WINDOW_TEST_RUNNER_H_
