// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/geometry_util.h"

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

  if (excess > 0) {
    start = start - excess;
  }

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

gfx::Rect SubtractOverlayFromBoundingBox(const gfx::Rect& bounds,
                                         const gfx::Rect& overlay,
                                         int max_distance) {
  if (overlay.Contains(bounds)) {
    // Early exit; |bounds| is completely inside |overlay|.
    return bounds;
  }

  // Portion of |overlay| that is inside |bounds|.
  auto overlap = overlay;
  overlap.Intersect(bounds);
  if (overlap.IsEmpty()) {
    // Early exit; |bounds| and |overlay| don't intersect.
    return bounds;
  }

  gfx::Insets insets;

  if (overlap.width() >= overlap.height()) {
    // Wide overlay; maybe inset |bounds| in the Y direction.
    const int delta_top = overlap.y() - bounds.y();
    const int delta_bottom =
        bounds.y() + bounds.height() - overlap.y() - overlap.height();

    // Inset from the closest side that meets |max_distance| requirements.
    if (delta_top <= delta_bottom && delta_top <= max_distance) {
      // Inset from the top.
      insets.set_top(delta_top + overlap.height());
    } else if (delta_bottom <= max_distance) {
      // Inset from the bottom.
      insets.set_bottom(delta_bottom + overlap.height());
    }
  } else {
    // Tall overlay; maybe inset |bounds| in the X direction.
    const int delta_left = overlap.x() - bounds.x();
    const int delta_right =
        bounds.x() + bounds.width() - overlap.x() - overlap.width();

    // Inset from the closest side that meets |max_distance| requirements.
    if (delta_left <= delta_right && delta_left <= max_distance) {
      // Inset from the left.
      insets.set_left(delta_left + overlap.width());
    } else if (delta_right <= max_distance) {
      // Inset from the right.
      insets.set_right(delta_right + overlap.width());
    }
  }

  if (insets.IsEmpty()) {
    // |overlay| is too far inside |bounds| to trigger insets.
    return bounds;
  }

  auto result = bounds;
  result.Inset(insets);
  return result;
}