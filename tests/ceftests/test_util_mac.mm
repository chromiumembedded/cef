// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "tests/ceftests/test_util.h"

#import <Cocoa/Cocoa.h>

#include "include/wrapper/cef_helpers.h"

bool SetNativeTestBrowserVisible(CefRefPtr<CefBrowser> browser, bool visible) {
  CEF_REQUIRE_UI_THREAD();
  NSView* view =
      CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(browser->GetHost()->GetWindowHandle());
  NSWindow* window = [view window];
  if (!window) {
    return false;
  }
  if (visible) {
    [window orderFront:nil];
  } else {
    [window orderOut:nil];
  }
  return true;
}
