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
// the browser process UI thread unless otherwise indicated.
class WindowTestRunner {
 public:
  virtual ~WindowTestRunner() = default;

  virtual void SetPos(CefRefPtr<CefBrowser> browser,
                      int x,
                      int y,
                      int width,
                      int height) = 0;
  virtual void Minimize(CefRefPtr<CefBrowser> browser) = 0;
  virtual void Maximize(CefRefPtr<CefBrowser> browser) = 0;
  virtual void Restore(CefRefPtr<CefBrowser> browser) = 0;
  virtual void Fullscreen(CefRefPtr<CefBrowser> browser);

  // Fit |window| inside |display|. Coordinates are relative to the upper-left
  // corner of the display.
  static void ModifyBounds(const CefRect& display, CefRect& window);

  virtual void SetTitleBarHeight(CefRefPtr<CefBrowser> browser,
                                 const std::optional<float>& height);
};

}  // namespace client::window_test

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_WINDOW_TEST_RUNNER_H_
