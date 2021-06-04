// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/chrome/views/toolbar_view_view.h"

CefToolbarViewView::CefToolbarViewView(CefViewDelegate* cef_delegate,
                                       Browser* browser,
                                       BrowserView* browser_view,
                                       absl::optional<DisplayMode> display_mode)
    : ParentClass(cef_delegate, browser, browser_view, display_mode) {}
