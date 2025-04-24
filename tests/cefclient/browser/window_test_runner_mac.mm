// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/window_test_runner_mac.h"

#import <Cocoa/Cocoa.h>

#include "include/wrapper/cef_helpers.h"
#include "tests/shared/browser/main_message_loop.h"

namespace client {
namespace window_test {

namespace {

NSWindow* GetWindow(CefRefPtr<CefBrowser> browser) {
  NSView* view =
      CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(browser->GetHost()->GetWindowHandle());
  return [view window];
}

}  // namespace

WindowTestRunnerMac::WindowTestRunnerMac() = default;

void WindowTestRunnerMac::Minimize(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  [GetWindow(browser) performMiniaturize:nil];
}

void WindowTestRunnerMac::Maximize(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  [GetWindow(browser) performZoom:nil];
}

void WindowTestRunnerMac::Restore(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  NSWindow* window = GetWindow(browser);
  if ([window isMiniaturized]) {
    [window deminiaturize:nil];
  } else if ([window isZoomed]) {
    [window performZoom:nil];
  }
}

}  // namespace window_test
}  // namespace client
