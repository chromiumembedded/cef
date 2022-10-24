// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/screen_util.h"

#include <algorithm>

#include "ui/gfx/geometry/rect.h"

namespace {

constexpr int kMinWidth = 0;
constexpr int kMinHeight = 0;

// Makes sure that line segment lies entirely between min and max.
int clamp_segment_start(int start, int len, int min, int max) {
  start = std::clamp(start, min, max);
  const int end = start + len;
  const int excess = end - max;

  if (excess > 0)
    start = start - excess;

  return start;
}

}  // namespace

gfx::Rect MakeVisibleOnScreenRect(const gfx::Rect& rect,
                                  const gfx::Rect& screen) {
  const int width = std::clamp(rect.width(), kMinWidth, screen.width());
  const int height = std::clamp(rect.height(), kMinHeight, screen.height());

  const int right_border = screen.x() + screen.width();
  const int x = clamp_segment_start(rect.x(), width, screen.x(), right_border);

  const int bottom_border = screen.y() + screen.height();
  const int y =
      clamp_segment_start(rect.y(), height, screen.y(), bottom_border);

  return gfx::Rect(x, y, width, height);
}
