// Copyright 2017 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_ACCESSIBILITY_UTIL_H_
#define CEF_LIBCEF_BROWSER_OSR_ACCESSIBILITY_UTIL_H_
#pragma once

#include <vector>
#include "include/cef_values.h"

namespace content {
struct AXEventNotificationDetails;
struct AXLocationChangeNotificationDetails;
}  // namespace content

namespace osr_accessibility_util {

// Convert Accessibility Event and location updates to CefValue, which may be
// consumed or serialized with CefJSONWrite.
CefRefPtr<CefValue> ParseAccessibilityEventData(
    const content::AXEventNotificationDetails& data);

CefRefPtr<CefValue> ParseAccessibilityLocationData(
    const std::vector<content::AXLocationChangeNotificationDetails>& data);

}  // namespace osr_accessibility_util

#endif  // CEF_LIBCEF_BROWSER_ACCESSIBILITY_UTIL_H_
