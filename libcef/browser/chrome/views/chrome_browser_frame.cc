// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/chrome/views/chrome_browser_frame.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

void ChromeBrowserFrame::Init(BrowserView* browser_view,
                              std::unique_ptr<Browser> browser) {
  DCHECK(browser_view);

  DCHECK(!browser_);
  browser_ = browser.get();
  DCHECK(browser_);

  // Initialize BrowserFrame state.
  InitBrowserView(browser_view);

  // Initialize BrowserView state.
  browser_view->InitBrowser(std::move(browser));
}

views::internal::RootView* ChromeBrowserFrame::CreateRootView() {
  // Bypass the BrowserFrame implementation.
  return views::Widget::CreateRootView();
}

std::unique_ptr<views::NonClientFrameView>
ChromeBrowserFrame::CreateNonClientFrameView() {
  // Bypass the BrowserFrame implementation.
  return views::Widget::CreateNonClientFrameView();
}
