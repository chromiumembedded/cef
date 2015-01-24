// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/render_widget_host_view_osr.h"

#include <X11/Xlib.h>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/window_x11.h"

#include "ui/gfx/x/x11_types.h"

void CefRenderWidgetHostViewOSR::PlatformCreateCompositorWidget() {
  // Create a hidden 1x1 window. It will delete itself on close.
  window_ = new CefWindowX11(NULL, None, gfx::Rect(0, 0, 1, 1));
  compositor_widget_ = window_->xwindow();
}

void CefRenderWidgetHostViewOSR::PlatformDestroyCompositorWidget() {
  DCHECK(window_);
  window_->Close();
  compositor_widget_ = gfx::kNullAcceleratedWidget;
}
