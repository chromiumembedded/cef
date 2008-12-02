// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "page_range.h"

#include "chrome/common/stl_util-inl.h"

namespace printing {

std::vector<int> PageRange::GetPages(const PageRanges& ranges) {
  std::set<int> pages;
  for (unsigned i = 0; i < ranges.size(); ++i) {
    const PageRange& range = ranges[i];
    // Ranges are inclusive.
    for (int i = range.from; i <= range.to; ++i) {
      pages.insert(i);
    }
  }
  return SetToVector(pages);
}

}  // namespace printing

