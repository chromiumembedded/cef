// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_EVENT_UTIL_H_
#define CEF_LIBCEF_BROWSER_BROWSER_EVENT_UTIL_H_
#pragma once

#include "cef/include/internal/cef_types_wrappers.h"
#include "ui/gfx/native_ui_types.h"

namespace input {
struct NativeWebKeyboardEvent;
}

namespace blink {
class WebKeyboardEvent;
}

// Convert between CEF event flags and Blink modifiers.
int GetWebEventModifiers(uint32_t cef_modifiers);
uint32_t GetCefEventModifiers(int web_modifiers);

// Complete portable keyboard conversions. No keyboard-layout interpretation.
// Invalid portable values are rejected without changing |web_event|.
bool GetWebKeyboardInputEvent(const CefKeyboardInputEvent& event,
                              blink::WebKeyboardEvent& web_event);
bool GetCefKeyboardInputEvent(const blink::WebKeyboardEvent& event,
                              CefKeyboardInputEvent& cef_event);
input::NativeWebKeyboardEvent CreateNativeKeyboardInputEvent(
    const blink::WebKeyboardEvent& event,
    gfx::NativeView native_view);

// DEPRECATED: Use GetCefKeyboardInputEvent for unified keyboard callbacks.
// Convert a blink::WebKeyboardEvent to a CefKeyEvent for legacy callbacks.
bool GetCefKeyEvent(const blink::WebKeyboardEvent& event,
                    CefKeyEvent& cef_event);

#endif  // CEF_LIBCEF_BROWSER_BROWSER_EVENT_UTIL_H_
