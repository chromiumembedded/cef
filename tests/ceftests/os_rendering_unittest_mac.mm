// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#include "tests/ceftests/os_rendering_unittest_mac.h"

#import <Cocoa/Cocoa.h>

namespace osr_unittests {

CefWindowHandle GetFakeView() {
  NSScreen* mainScreen = [NSScreen mainScreen];
  NSRect screenRect = [mainScreen visibleFrame];
  NSView* fakeView = [[NSView alloc] initWithFrame:screenRect];
  return CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(fakeView);
}

}  // namespace osr_unittests
