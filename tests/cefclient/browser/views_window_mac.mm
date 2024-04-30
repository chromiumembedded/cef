// Copyright (c) 2023 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/views_window.h"

#include <Cocoa/Cocoa.h>

namespace client {

void ViewsWindow::NudgeWindow() {
  if (window_) {
    auto view = CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(window_->GetWindowHandle());
    NSWindow* main_window = view.window;

    auto theme_frame = main_window.contentView.superview;
    // Nudge view frame a little to force an update.
    NSSize size = theme_frame.frame.size;
    [theme_frame setFrameSize:NSMakeSize(size.width - 1, size.height)];
    [theme_frame setFrameSize:size];
  }
}

}  // namespace client
