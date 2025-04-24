// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/util_mac.h"

namespace client {

std::optional<CefRect> GetWindowBoundsInScreen(NSWindow* window) {
  if ([window isMiniaturized] or [window isZoomed]) {
    return std::nullopt;
  }

  auto screen = [window screen];
  if (screen == nil) {
    screen = [NSScreen mainScreen];
  }

  const auto bounds = [window frame];
  const auto screen_bounds = [screen frame];

  if (NSEqualRects(bounds, screen_bounds)) {
    // Don't include windows that are transitioning to fullscreen.
    return std::nullopt;
  }

  CefRect dip_bounds{static_cast<int>(bounds.origin.x),
                     static_cast<int>(bounds.origin.y),
                     static_cast<int>(bounds.size.width),
                     static_cast<int>(bounds.size.height)};

  // Convert from macOS coordinates (bottom-left origin) to DIP coordinates
  // (top-left origin).
  dip_bounds.y = static_cast<int>(screen_bounds.size.height) -
                 dip_bounds.height - dip_bounds.y;

  return dip_bounds;
}

}  // namespace client
