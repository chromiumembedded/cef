// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_BROWSER_FRAME_VIEW_H_
#define CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_BROWSER_FRAME_VIEW_H_
#pragma once

#include "chrome/browser/ui/views/frame/browser_frame_view.h"

// Stub implementation of BrowserFrameView to satisfy minimal usage expectations
// via BrowserWidget::GetFrameView and similar. See chrome_browser_widget.h for
// related documentation.
class ChromeBrowserFrameView : public BrowserFrameView {
 public:
  ChromeBrowserFrameView(BrowserWidget* browser_widget,
                         BrowserView* browser_view);
  ~ChromeBrowserFrameView() override;

  ChromeBrowserFrameView(const ChromeBrowserFrameView&) = delete;
  ChromeBrowserFrameView& operator=(const ChromeBrowserFrameView&) = delete;

  // BrowserFrameView methods:
  int GetTopInset(bool restored) const override;
  void UpdateThrobber(bool running) override {}

  // views::View methods:
  const views::Widget* GetWidget() const override;
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_BROWSER_FRAME_VIEW_H_
