// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_TESTS_SHARED_BROWSER_OSR_INPUT_UTIL_H_
#define CEF_TESTS_SHARED_BROWSER_OSR_INPUT_UTIL_H_
#pragma once

#include <initializer_list>
#include <limits>
#include <vector>

#include "include/internal/cef_types_wrappers.h"

namespace client {

#if CEF_API_ADDED(CEF_NEXT)
inline uint32_t OsrMouseButtonFlag(cef_mouse_button_type_t button) {
  switch (button) {
    case CEF_MOUSE_BUTTON_LEFT:
      return CEF_EVENTFLAG_LEFT_MOUSE_BUTTON;
    case CEF_MOUSE_BUTTON_MIDDLE:
      return CEF_EVENTFLAG_MIDDLE_MOUSE_BUTTON;
    case CEF_MOUSE_BUTTON_RIGHT:
      return CEF_EVENTFLAG_RIGHT_MOUSE_BUTTON;
    case CEF_MOUSE_BUTTON_BACK:
      return CEF_EVENTFLAG_BACK_MOUSE_BUTTON;
    case CEF_MOUSE_BUTTON_FORWARD:
      return CEF_EVENTFLAG_FORWARD_MOUSE_BUTTON;
    default:
      return 0;
  }
}

// Keep existing popup-offset/DIP conversion in the platform views. Native
// frameworks differ in whether button-event modifiers describe the state
// before or after the transition; portable snapshots require the latter.
inline CefMouseInputEvent MakeOsrMouseInputEvent(
    const CefMouseEvent& mouse,
    const CefPoint& screen,
    cef_mouse_input_event_type_t type,
    cef_mouse_button_type_t button = CEF_MOUSE_BUTTON_NONE,
    int click_count = 0) {
  CefMouseInputEvent result;
  result.type = type;
  result.x = mouse.x;
  result.y = mouse.y;
  result.screen_x = screen.x;
  result.screen_y = screen.y;
  result.modifiers = mouse.modifiers;
  result.pointer_type = CEF_POINTER_TYPE_MOUSE;
  result.button = button;
  result.click_count = click_count;
  // Zero timestamp uses dispatch time, avoiding conversion between native
  // event clocks and Chromium's monotonic clock.
  if (type == CEF_MOUSE_EVENT_DOWN) {
    result.modifiers |= OsrMouseButtonFlag(button);
  } else if (type == CEF_MOUSE_EVENT_UP) {
    result.modifiers &= ~OsrMouseButtonFlag(button);
  } else {
    for (auto held : {CEF_MOUSE_BUTTON_LEFT, CEF_MOUSE_BUTTON_MIDDLE,
                      CEF_MOUSE_BUTTON_RIGHT, CEF_MOUSE_BUTTON_BACK,
                      CEF_MOUSE_BUTTON_FORWARD}) {
      if (result.modifiers & OsrMouseButtonFlag(held)) {
        result.button = held;
        break;
      }
    }
  }
  return result;
}

inline CefWheelInputEvent MakeOsrWheelInputEvent(const CefMouseEvent& mouse,
                                                 float screen_x,
                                                 float screen_y) {
  CefWheelInputEvent result;
  result.x = mouse.x;
  result.y = mouse.y;
  result.screen_x = screen_x;
  result.screen_y = screen_y;
  result.modifiers =
      mouse.modifiers &
      ~(CEF_EVENTFLAG_PRECISION_SCROLLING_DELTA | CEF_EVENTFLAG_SCROLL_BY_PAGE);
  return result;
}

// Match Windows wheel settings without rounding partial detents or pixel
// deltas. UINT_MAX is WHEEL_PAGESCROLL; zero disables scrolling on this axis.
// The caller normalizes ticks so positive values mean up/left.
inline void SetOsrWindowsWheelDelta(CefWheelInputEvent& event,
                                    float ticks,
                                    bool horizontal,
                                    uint32_t scroll_amount) {
  const bool page = scroll_amount == std::numeric_limits<uint32_t>::max();
  event.delta_units = page ? CEF_GESTURE_SCROLL_PAGE : CEF_GESTURE_SCROLL_PIXEL;
  const float delta =
      page ? (ticks > 0   ? 1.f
              : ticks < 0 ? -1.f
                          : 0.f)
           : ticks * static_cast<float>(scroll_amount) * (100.f / 3.f);
  event.wheel_ticks_x = horizontal ? ticks : 0;
  event.wheel_ticks_y = horizontal ? 0 : ticks;
  event.delta_x = horizontal ? delta : 0;
  event.delta_y = horizontal ? 0 : delta;
}

// WM_GESTURE zoom reports absolute finger separation, including on the final
// update. Convert it to incremental pinch scales. Zero distances are ignored,
// but must not prevent an active sequence from ending. Touchscreen pinches
// are nested in a scroll sequence, as in Chromium's touch gesture recognizer.
inline std::vector<CefGestureInputEvent> OsrPinchInputEventsFromWindowsZoom(
    uint64_t distance,
    bool begin,
    bool end,
    uint64_t& previous_distance) {
  std::vector<CefGestureInputEvent> events;
  auto append = [&](cef_gesture_input_event_type_t type, double scale = 1) {
    CefGestureInputEvent event;
    event.type = type;
    event.source_device = CEF_GESTURE_DEVICE_TOUCHSCREEN;
    event.primary_pointer_type = CEF_POINTER_TYPE_TOUCH;
    event.pointer_count = 2;
    event.scale = scale;
    events.push_back(event);
  };
  if (begin) {
    if (previous_distance) {
      append(CEF_GESTURE_EVENT_PINCH_END);
      append(CEF_GESTURE_EVENT_SCROLL_END);
    }
    previous_distance = distance;
    if (distance) {
      append(CEF_GESTURE_EVENT_SCROLL_BEGIN);
      append(CEF_GESTURE_EVENT_PINCH_BEGIN);
    }
  } else if (previous_distance && distance) {
    if (distance != previous_distance) {
      append(CEF_GESTURE_EVENT_PINCH_UPDATE,
             static_cast<double>(distance) / previous_distance);
    }
    previous_distance = distance;
  }
  if (end && previous_distance) {
    append(CEF_GESTURE_EVENT_PINCH_END);
    append(CEF_GESTURE_EVENT_SCROLL_END);
    previous_distance = 0;
  }
  return events;
}
#endif  // CEF_API_ADDED(CEF_NEXT)

}  // namespace client

#endif  // CEF_TESTS_SHARED_BROWSER_OSR_INPUT_UTIL_H_
