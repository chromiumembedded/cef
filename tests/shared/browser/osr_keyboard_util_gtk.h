// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_TESTS_SHARED_BROWSER_OSR_KEYBOARD_UTIL_GTK_H_
#define CEF_TESTS_SHARED_BROWSER_OSR_KEYBOARD_UTIL_GTK_H_
#pragma once

#include <gdk/gdk.h>

#include <vector>

#include "include/internal/cef_types_wrappers.h"

namespace client {
int GetCefStateModifiers(guint state);

#if CEF_API_ADDED(CEF_NEXT)
// Sample-local adapter for GTK3's X11 and Wayland backends. Toolkit/display
// objects never cross the CEF API.
// Failure leaves |events| unchanged. Repeat is tracked by the owning widget.
bool KeyboardInputEventsFromGdkEvent(
    const GdkEventKey* event,
    bool repeat,
    std::vector<CefKeyboardInputEvent>& events);
#endif  // CEF_API_ADDED(CEF_NEXT)
}  // namespace client

#endif  // CEF_TESTS_SHARED_BROWSER_OSR_KEYBOARD_UTIL_GTK_H_
