// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/chrome/views/toolbar_view_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"

CefToolbarViewView::CefToolbarViewView(CefViewDelegate* cef_delegate,
                                       Browser* browser,
                                       BrowserView* browser_view,
                                       std::optional<DisplayMode> display_mode)
    : ParentClass(cef_delegate, browser, browser_view, display_mode) {}

BEGIN_METADATA(CefToolbarViewView)
END_METADATA
