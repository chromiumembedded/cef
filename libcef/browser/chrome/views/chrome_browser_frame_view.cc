// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/chrome/views/chrome_browser_frame_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"

ChromeBrowserFrameView::ChromeBrowserFrameView(BrowserWidget* browser_widget,
                                               BrowserView* browser_view)
    : BrowserFrameView(browser_widget, browser_view) {}

ChromeBrowserFrameView::~ChromeBrowserFrameView() = default;

gfx::Rect ChromeBrowserFrameView::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  return gfx::Rect();
}

gfx::Rect ChromeBrowserFrameView::GetBoundsForWebAppFrameToolbar(
    const gfx::Size& toolbar_preferred_size) const {
  return gfx::Rect();
}

int ChromeBrowserFrameView::GetTopInset(bool restored) const {
  return browser_view()->y();
}

const views::Widget* ChromeBrowserFrameView::GetWidget() const {
  return browser_widget();
}
