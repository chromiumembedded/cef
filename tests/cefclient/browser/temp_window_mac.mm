// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/temp_window_mac.h"

#include <Cocoa/Cocoa.h>

#include "include/base/cef_logging.h"
#include "include/cef_app.h"

namespace client {

namespace {

TempWindowMac* g_temp_window = nullptr;

}  // namespace

class TempWindowMacImpl {
 public:
  TempWindowMacImpl() {
    // Create a borderless non-visible 1x1 window.
    window_ = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1, 1)
                                          styleMask:NSWindowStyleMaskBorderless
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    CHECK(window_);
  }
  ~TempWindowMacImpl() {
    DCHECK(window_);
    window_ = nil;
  }

 private:
  NSWindow* window_;

  friend class TempWindowMac;
};

TempWindowMac::TempWindowMac() {
  DCHECK(!g_temp_window);
  impl_.reset(new TempWindowMacImpl);
  g_temp_window = this;
}

TempWindowMac::~TempWindowMac() {
  impl_.reset();
  g_temp_window = nullptr;
}

// static
CefWindowHandle TempWindowMac::GetWindowHandle() {
  DCHECK(g_temp_window);
  return CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(
      g_temp_window->impl_->window_.contentView);
}

}  // namespace client
