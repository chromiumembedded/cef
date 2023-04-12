// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_GEOMETRY_UTIL_H_
#define CEF_LIBCEF_BROWSER_GEOMETRY_UTIL_H_
#pragma once

namespace gfx {
class Rect;
}

// Create a new rectangle from the input |rect| rectangle that is fully visible
// on provided |screen_rect| screen. The width and height of the resulting
// rectangle are clamped to the screen width and height respectively if they
// would overflow.
gfx::Rect MakeVisibleOnScreenRect(const gfx::Rect& rect,
                                  const gfx::Rect& screen);

// Possibly subtract |overlay| from |bounds|. We only want to subtract overlays
// that are inside |bounds| and close to the edges, so |max_distance| is the
// maximum allowed distance between |overlay| and |bounds| extents in order to
// trigger the subtraction. Subtraction will occur from the closest edge. If
// distances are otherwise equal then top will be preferred followed by left.
gfx::Rect SubtractOverlayFromBoundingBox(const gfx::Rect& bounds,
                                         const gfx::Rect& overlay,
                                         int max_distance);

#endif  // CEF_LIBCEF_BROWSER_GEOMETRY_UTIL_H_
