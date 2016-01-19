// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_UTIL_H_
#define CEF_LIBCEF_BROWSER_BROWSER_UTIL_H_
#pragma once

#include "include/internal/cef_types_wrappers.h"

namespace content {
struct NativeWebKeyboardEvent;
}

namespace ui {
class KeyEvent;
}

namespace browser_util {

// Convert a content::NativeWebKeyboardEvent to a CefKeyEvent.
bool GetCefKeyEvent(const content::NativeWebKeyboardEvent& event,
                    CefKeyEvent& cef_event);

// Convert a ui::KeyEvent to a CefKeyEvent.
bool GetCefKeyEvent(const ui::KeyEvent& event,
                    CefKeyEvent& cef_event);

}  // namespace browser_util

#endif  // CEF_LIBCEF_BROWSER_BROWSER_UTIL_H_
