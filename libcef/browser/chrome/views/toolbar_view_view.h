// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_VIEWS_TOOLBAR_VIEW_VIEW_H_
#define CEF_LIBCEF_BROWSER_CHROME_VIEWS_TOOLBAR_VIEW_VIEW_H_
#pragma once

#include "libcef/browser/views/view_view.h"

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

class CefToolbarViewView : public CefViewView<ToolbarView, CefViewDelegate> {
 public:
  using ParentClass = CefViewView<ToolbarView, CefViewDelegate>;

  // |cef_delegate| may be nullptr.
  explicit CefToolbarViewView(CefViewDelegate* cef_delegate,
                              Browser* browser,
                              BrowserView* browser_view,
                              absl::optional<DisplayMode> display_mode);

  CefToolbarViewView(const CefToolbarViewView&) = delete;
  CefToolbarViewView& operator=(const CefToolbarViewView&) = delete;
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_VIEWS_TOOLBAR_VIEW_VIEW_H_
