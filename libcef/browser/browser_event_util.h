// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_EVENT_UTIL_H_
#define CEF_LIBCEF_BROWSER_BROWSER_EVENT_UTIL_H_
#pragma once

#include "cef/include/internal/cef_types_wrappers.h"

namespace blink {
class WebGestureEvent;
class WebMouseEvent;
}  // namespace blink

namespace input {
struct NativeWebKeyboardEvent;
}

namespace ui {
class KeyEvent;
}

// Convert a blink::WebGestureEvent to a CefTouchEvent.
bool GetCefTouchEvent(const blink::WebGestureEvent& event,
                      CefTouchEvent& cef_event);

// Extract the gesture type from a blink::WebGestureEvent.
cef_gesture_type_t GetCefGestureType(const blink::WebGestureEvent& event);

// Extract the tap count from a gesture event.
int GetTapCount(const blink::WebGestureEvent& event);

// Convert a input::NativeWebKeyboardEvent to a CefKeyEvent.
bool GetCefKeyEvent(const input::NativeWebKeyboardEvent& event,
                    CefKeyEvent& cef_event);

// Convert a ui::KeyEvent to a CefKeyEvent.
bool GetCefKeyEvent(const ui::KeyEvent& event, CefKeyEvent& cef_event);

// Convert a blink::WebMouseEvent to a CefMouseEvent.
bool GetCefMouseEvent(const blink::WebMouseEvent& event,
                      CefMouseEvent& cef_event);

// Extract the mouse event type from a blink::WebMouseEvent.
cef_mouse_event_type_t GetCefMouseEventType(const blink::WebMouseEvent& event);

// Extract the mouse button type from a blink::WebMouseEvent.
cef_mouse_button_type_t GetCefMouseButtonType(
    const blink::WebMouseEvent& event);

#endif  // CEF_LIBCEF_BROWSER_BROWSER_EVENT_UTIL_H_
