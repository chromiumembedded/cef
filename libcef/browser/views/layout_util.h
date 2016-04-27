// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_LAYOUT_UTIL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_LAYOUT_UTIL_H_
#pragma once

#include "include/views/cef_layout.h"

#include "ui/views/layout/layout_manager.h"

namespace views {
class View;
}

// The below functions manage the relationship between CefLayout and
// views::LayoutManager instances. See comments in view_impl.h for a usage
// overview.

namespace layout_util {

// Returns the CefLayout object associated with |owner_view|.
CefRefPtr<CefLayout> GetFor(const views::View* owner_view);

// Assign ownership of |layout| to |owner_view|. If a CefLayout is already
// associated with |owner_view| it will be invalidated.
void Assign(CefRefPtr<CefLayout> layout, views::View* owner_view);

}  // namespace layout_util

#endif  // CEF_LIBCEF_BROWSER_VIEWS_LAYOUT_UTIL_H_
