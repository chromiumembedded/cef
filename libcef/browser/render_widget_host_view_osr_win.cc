// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/render_widget_host_view_osr.h"
#include "libcef/browser/browser_host_impl.h"

namespace {

class CefCompositorHostWin : public gfx::WindowImpl {
 public:
  CefCompositorHostWin() {
    // Create a hidden 1x1 borderless window.
    set_window_style(WS_POPUP | WS_SYSMENU);
    Init(NULL, gfx::Rect(0, 0, 1, 1));
  }

  ~CefCompositorHostWin() override {
    DestroyWindow(hwnd());
  }

 private:
  CR_BEGIN_MSG_MAP_EX(CompositorHostWin)
    CR_MSG_WM_PAINT(OnPaint)
  CR_END_MSG_MAP()

  void OnPaint(HDC dc) {
    ValidateRect(hwnd(), NULL);
  }

  DISALLOW_COPY_AND_ASSIGN(CefCompositorHostWin);
};

}  // namespace

void CefRenderWidgetHostViewOSR::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
}

gfx::NativeViewId
    CefRenderWidgetHostViewOSR::GetParentForWindowlessPlugin() const {
  if (browser_impl_.get()) {
    return reinterpret_cast<gfx::NativeViewId>(
        browser_impl_->GetWindowHandle());
  }
  return NULL;
}

void CefRenderWidgetHostViewOSR::PlatformCreateCompositorWidget() {
  DCHECK(!window_);
  window_.reset(new CefCompositorHostWin());
  compositor_widget_ = window_->hwnd();
}

void CefRenderWidgetHostViewOSR::PlatformDestroyCompositorWidget() {
  window_.reset(NULL);
  compositor_widget_ = gfx::kNullAcceleratedWidget;
}
