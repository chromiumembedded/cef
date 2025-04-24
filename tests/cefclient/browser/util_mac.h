// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_UTIL_MAC_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_UTIL_MAC_H_
#pragma once

#include <optional>

#include <Cocoa/Cocoa.h>

#include "include/internal/cef_types_wrappers.h"

namespace client {

// Returns the current DIP screen bounds for a visible window in the restored
// position, or nullopt if the window is currently minimized or fullscreen.
std::optional<CefRect> GetWindowBoundsInScreen(NSWindow* window);

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_UTIL_MAC_H_
