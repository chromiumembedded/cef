// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/chrome/views/chrome_views_util.h"

#include "libcef/browser/views/view_util.h"

namespace cef {

bool IsCefView(views::View* view) {
  return view_util::GetFor(view, /*find_known_parent=*/false) != nullptr;
}

}  // namespace cef
