// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/chrome/views/toolbar_view_impl.h"

// static
CefRefPtr<CefToolbarViewImpl> CefToolbarViewImpl::Create(
    CefRefPtr<CefViewDelegate> delegate,
    Browser* browser,
    BrowserView* browser_view,
    absl::optional<ToolbarView::DisplayMode> display_mode) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  CefRefPtr<CefToolbarViewImpl> view =
      new CefToolbarViewImpl(delegate, browser, browser_view, display_mode);
  view->Initialize();
  return view;
}

// static
const char* const CefToolbarViewImpl::kTypeString = "ToolbarView";

CefToolbarViewImpl::CefToolbarViewImpl(
    CefRefPtr<CefViewDelegate> delegate,
    Browser* browser,
    BrowserView* browser_view,
    absl::optional<ToolbarView::DisplayMode> display_mode)
    : ParentClass(delegate),
      browser_(browser),
      browser_view_(browser_view),
      display_mode_(display_mode) {}

CefToolbarViewView* CefToolbarViewImpl::CreateRootView() {
  return new CefToolbarViewView(delegate(), browser_, browser_view_,
                                display_mode_);
}

void CefToolbarViewImpl::InitializeRootView() {
  static_cast<CefToolbarViewView*>(root_view())->Initialize();
}
