// Copyright 2017 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_ACCESSIBILITY_UTIL_H_
#define CEF_LIBCEF_BROWSER_OSR_ACCESSIBILITY_UTIL_H_
#pragma once

#include <vector>

#include "cef/include/cef_values.h"

namespace ui {
struct AXLocationAndScrollUpdates;
struct AXUpdatesAndEvents;
class AXTreeID;
}  // namespace ui

namespace osr_accessibility_util {

// Convert Accessibility Event and location updates to CefValue, which may be
// consumed or serialized with CefJSONWrite.
CefRefPtr<CefValue> ParseAccessibilityEventData(
    const ui::AXUpdatesAndEvents& details);

CefRefPtr<CefValue> ParseAccessibilityLocationData(
    const ui::AXTreeID& tree_id,
    const ui::AXLocationAndScrollUpdates& details);

}  // namespace osr_accessibility_util

#endif  // CEF_LIBCEF_BROWSER_ACCESSIBILITY_UTIL_H_
