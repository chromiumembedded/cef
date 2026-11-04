// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <cmath>

#include "base/time/time.h"
#include "cef/libcef/browser/browser_event_util.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace {

using Type = blink::WebInputEvent::Type;

constexpr struct {
  cef_mouse_input_event_type_t cef;
  Type web;
} kMouseTypes[] = {
    {CEF_MOUSE_EVENT_DOWN, Type::kMouseDown},
    {CEF_MOUSE_EVENT_UP, Type::kMouseUp},
    {CEF_MOUSE_EVENT_MOVE, Type::kMouseMove},
    {CEF_MOUSE_EVENT_ENTER, Type::kMouseEnter},
    {CEF_MOUSE_EVENT_LEAVE, Type::kMouseLeave},
    {CEF_MOUSE_EVENT_CONTEXT_MENU, Type::kContextMenu},
};

constexpr struct {
  cef_gesture_input_event_type_t cef;
  Type web;
} kGestureTypes[] = {
    {CEF_GESTURE_EVENT_SCROLL_BEGIN, Type::kGestureScrollBegin},
    {CEF_GESTURE_EVENT_SCROLL_END, Type::kGestureScrollEnd},
    {CEF_GESTURE_EVENT_SCROLL_UPDATE, Type::kGestureScrollUpdate},
    {CEF_GESTURE_EVENT_FLING_START, Type::kGestureFlingStart},
    {CEF_GESTURE_EVENT_FLING_CANCEL, Type::kGestureFlingCancel},
    {CEF_GESTURE_EVENT_PINCH_BEGIN, Type::kGesturePinchBegin},
    {CEF_GESTURE_EVENT_PINCH_END, Type::kGesturePinchEnd},
    {CEF_GESTURE_EVENT_PINCH_UPDATE, Type::kGesturePinchUpdate},
    {CEF_GESTURE_EVENT_BEGIN, Type::kGestureBegin},
    {CEF_GESTURE_EVENT_TAP_DOWN, Type::kGestureTapDown},
    {CEF_GESTURE_EVENT_SHOW_PRESS, Type::kGestureShowPress},
    {CEF_GESTURE_EVENT_TAP, Type::kGestureTap},
    {CEF_GESTURE_EVENT_TAP_CANCEL, Type::kGestureTapCancel},
    {CEF_GESTURE_EVENT_SHORT_PRESS, Type::kGestureShortPress},
    {CEF_GESTURE_EVENT_LONG_PRESS, Type::kGestureLongPress},
    {CEF_GESTURE_EVENT_LONG_TAP, Type::kGestureLongTap},
    {CEF_GESTURE_EVENT_TWO_FINGER_TAP, Type::kGestureTwoFingerTap},
    {CEF_GESTURE_EVENT_TAP_UNCONFIRMED, Type::kGestureTapUnconfirmed},
    {CEF_GESTURE_EVENT_DOUBLE_TAP, Type::kGestureDoubleTap},
    {CEF_GESTURE_EVENT_END, Type::kGestureEnd},
};

// These public enums intentionally use the corresponding Chromium values.
static_assert(static_cast<int>(CEF_MOUSE_BUTTON_NONE) ==
              static_cast<int>(blink::WebMouseEvent::Button::kNoButton));
static_assert(static_cast<int>(CEF_MOUSE_BUTTON_LEFT) ==
              static_cast<int>(blink::WebMouseEvent::Button::kLeft));
static_assert(static_cast<int>(CEF_MOUSE_BUTTON_MIDDLE) ==
              static_cast<int>(blink::WebMouseEvent::Button::kMiddle));
static_assert(static_cast<int>(CEF_MOUSE_BUTTON_RIGHT) ==
              static_cast<int>(blink::WebMouseEvent::Button::kRight));
static_assert(static_cast<int>(CEF_MOUSE_BUTTON_BACK) ==
              static_cast<int>(blink::WebMouseEvent::Button::kBack));
static_assert(static_cast<int>(CEF_MOUSE_BUTTON_FORWARD) ==
              static_cast<int>(blink::WebMouseEvent::Button::kForward));
static_assert(static_cast<int>(CEF_MOUSE_BUTTON_ERASER) ==
              static_cast<int>(blink::WebMouseEvent::Button::kEraser));
static_assert(static_cast<int>(CEF_GESTURE_DEVICE_UNINITIALIZED) ==
              static_cast<int>(blink::mojom::GestureDevice::kUninitialized));
static_assert(static_cast<int>(CEF_GESTURE_DEVICE_TOUCHPAD) ==
              static_cast<int>(blink::mojom::GestureDevice::kTouchpad));
static_assert(static_cast<int>(CEF_GESTURE_DEVICE_TOUCHSCREEN) ==
              static_cast<int>(blink::mojom::GestureDevice::kTouchscreen));
static_assert(
    static_cast<int>(CEF_GESTURE_DEVICE_SYNTHETIC_AUTOSCROLL) ==
    static_cast<int>(blink::mojom::GestureDevice::kSyntheticAutoscroll));
static_assert(static_cast<int>(CEF_GESTURE_DEVICE_SCROLLBAR) ==
              static_cast<int>(blink::mojom::GestureDevice::kScrollbar));
static_assert(static_cast<int>(CEF_GESTURE_SCROLL_PRECISE_PIXEL) ==
              static_cast<int>(ui::ScrollGranularity::kScrollByPrecisePixel));
static_assert(static_cast<int>(CEF_GESTURE_SCROLL_PIXEL) ==
              static_cast<int>(ui::ScrollGranularity::kScrollByPixel));
static_assert(static_cast<int>(CEF_GESTURE_SCROLL_LINE) ==
              static_cast<int>(ui::ScrollGranularity::kScrollByLine));
static_assert(static_cast<int>(CEF_GESTURE_SCROLL_PAGE) ==
              static_cast<int>(ui::ScrollGranularity::kScrollByPage));
static_assert(static_cast<int>(CEF_GESTURE_SCROLL_DOCUMENT) ==
              static_cast<int>(ui::ScrollGranularity::kScrollByDocument));

blink::WebPointerProperties::PointerType GetWebPointerType(
    cef_pointer_type_t type) {
  using PointerType = blink::WebPointerProperties::PointerType;
  switch (type) {
    case CEF_POINTER_TYPE_TOUCH:
      return PointerType::kTouch;
    case CEF_POINTER_TYPE_MOUSE:
      return PointerType::kMouse;
    case CEF_POINTER_TYPE_PEN:
      return PointerType::kPen;
    case CEF_POINTER_TYPE_ERASER:
      return PointerType::kEraser;
    default:
      return PointerType::kUnknown;
  }
}

cef_pointer_type_t GetCefPointerType(
    blink::WebPointerProperties::PointerType type) {
  using PointerType = blink::WebPointerProperties::PointerType;
  switch (type) {
    case PointerType::kTouch:
      return CEF_POINTER_TYPE_TOUCH;
    case PointerType::kMouse:
      return CEF_POINTER_TYPE_MOUSE;
    case PointerType::kPen:
      return CEF_POINTER_TYPE_PEN;
    case PointerType::kEraser:
      return CEF_POINTER_TYPE_ERASER;
    default:
      return CEF_POINTER_TYPE_UNKNOWN;
  }
}

bool IsBoolean(int value) {
  return value == 0 || value == 1;
}

constexpr struct {
  int web;
  uint32_t cef;
} kAdditionalModifiers[] = {
    {blink::WebInputEvent::kBackButtonDown, CEF_EVENTFLAG_BACK_MOUSE_BUTTON},
    {blink::WebInputEvent::kForwardButtonDown,
     CEF_EVENTFLAG_FORWARD_MOUSE_BUTTON},
    {blink::WebInputEvent::kIsTouchAccessibility,
     CEF_EVENTFLAG_TOUCH_ACCESSIBILITY},
    {blink::WebInputEvent::kIsCompatibilityEventForTouch,
     CEF_EVENTFLAG_FROM_TOUCH},
    {blink::WebInputEvent::kRelativeMotionEvent, CEF_EVENTFLAG_RELATIVE_MOTION},
    {blink::WebInputEvent::kFnKey, CEF_EVENTFLAG_FN_DOWN},
    {blink::WebInputEvent::kSymbolKey, CEF_EVENTFLAG_SYMBOL_DOWN},
    {blink::WebInputEvent::kScrollLockOn, CEF_EVENTFLAG_SCROLL_LOCK_ON},
};

int GetWebMouseGestureModifiers(uint32_t modifiers) {
  int result = GetWebEventModifiers(modifiers);
  for (const auto& entry : kAdditionalModifiers) {
    if (modifiers & entry.cef) {
      result |= entry.web;
    }
  }
  return result;
}

uint32_t GetCefMouseGestureModifiers(int modifiers) {
  uint32_t result = GetCefEventModifiers(modifiers);
  for (const auto& entry : kAdditionalModifiers) {
    if (modifiers & entry.web) {
      result |= entry.cef;
    }
  }
  return result;
}

template <class Event>
bool ValidCommon(const Event& event) {
  constexpr uint32_t allowed_modifiers =
      CEF_EVENTFLAG_CAPS_LOCK_ON | CEF_EVENTFLAG_SHIFT_DOWN |
      CEF_EVENTFLAG_CONTROL_DOWN | CEF_EVENTFLAG_ALT_DOWN |
      CEF_EVENTFLAG_LEFT_MOUSE_BUTTON | CEF_EVENTFLAG_MIDDLE_MOUSE_BUTTON |
      CEF_EVENTFLAG_RIGHT_MOUSE_BUTTON | CEF_EVENTFLAG_COMMAND_DOWN |
      CEF_EVENTFLAG_NUM_LOCK_ON | CEF_EVENTFLAG_IS_KEY_PAD |
      CEF_EVENTFLAG_IS_LEFT | CEF_EVENTFLAG_IS_RIGHT |
      CEF_EVENTFLAG_ALTGR_DOWN | CEF_EVENTFLAG_IS_REPEAT |
      CEF_EVENTFLAG_BACK_MOUSE_BUTTON | CEF_EVENTFLAG_FORWARD_MOUSE_BUTTON |
      CEF_EVENTFLAG_TOUCH_ACCESSIBILITY | CEF_EVENTFLAG_FROM_TOUCH |
      CEF_EVENTFLAG_RELATIVE_MOTION | CEF_EVENTFLAG_FN_DOWN |
      CEF_EVENTFLAG_SYMBOL_DOWN | CEF_EVENTFLAG_SCROLL_LOCK_ON;
  return !(event.modifiers & ~allowed_modifiers) && event.timestamp >= 0 &&
         std::isfinite(event.x) && std::isfinite(event.y) &&
         std::isfinite(event.screen_x) && std::isfinite(event.screen_y);
}

template <class CefEvent, class WebEvent>
void CopyCommonToWeb(const CefEvent& event, WebEvent& result) {
  result.SetPositionInWidget(gfx::PointF(event.x, event.y));
  result.SetPositionInScreen(gfx::PointF(event.screen_x, event.screen_y));
}

template <class WebEvent, class CefEvent>
void CopyCommonToCef(const WebEvent& event, CefEvent& result) {
  result.modifiers = GetCefMouseGestureModifiers(event.GetModifiers());
  result.timestamp = event.TimeStamp().ToInternalValue();
  result.x = event.PositionInWidget().x();
  result.y = event.PositionInWidget().y();
  result.screen_x = event.PositionInScreen().x();
  result.screen_y = event.PositionInScreen().y();
}

base::TimeTicks Timestamp(int64_t value) {
  return value ? base::TimeTicks::FromInternalValue(value)
               : base::TimeTicks::Now();
}

}  // namespace

bool GetWebMouseInputEvent(const CefMouseInputEvent& event,
                           blink::WebMouseEvent& web_event) {
  Type type = Type::kUndefined;
  for (const auto& entry : kMouseTypes) {
    if (entry.cef == event.type) {
      type = entry.web;
      break;
    }
  }
  if (type == Type::kUndefined || !ValidCommon(event) ||
      event.button < CEF_MOUSE_BUTTON_NONE ||
      event.button > CEF_MOUSE_BUTTON_ERASER ||
      event.pointer_type < CEF_POINTER_TYPE_TOUCH ||
      event.pointer_type > CEF_POINTER_TYPE_UNKNOWN || event.click_count < 0 ||
      (!std::isnan(event.pressure) &&
       (!std::isfinite(event.pressure) || event.pressure < 0 ||
        event.pressure > 1)) ||
      !std::isfinite(event.tilt_x) || std::abs(event.tilt_x) > 90 ||
      !std::isfinite(event.tilt_y) || std::abs(event.tilt_y) > 90 ||
      !std::isfinite(event.tangential_pressure) ||
      std::abs(event.tangential_pressure) > 1 || event.twist < 0 ||
      event.twist > 359 || !IsBoolean(event.is_raw_movement_event) ||
      event.menu_source_type < 0 || event.menu_source_type > 10) {
    return false;
  }
  blink::WebMouseEvent result(type,
                              GetWebMouseGestureModifiers(event.modifiers),
                              Timestamp(event.timestamp), event.pointer_id);
  CopyCommonToWeb(event, result);
  result.button = static_cast<blink::WebMouseEvent::Button>(event.button);
  result.click_count = event.click_count;
  result.pointer_type = GetWebPointerType(event.pointer_type);
  result.id = event.pointer_id;
  result.force = event.pressure;
  result.tilt_x = event.tilt_x;
  result.tilt_y = event.tilt_y;
  result.tangential_pressure = event.tangential_pressure;
  result.twist = event.twist;
  result.movement_x = event.movement_x;
  result.movement_y = event.movement_y;
  result.is_raw_movement_event = event.is_raw_movement_event;
  result.device_id = event.device_id;
  result.menu_source_type =
      static_cast<ui::mojom::MenuSourceType>(event.menu_source_type);
  web_event = result;
  return true;
}

bool GetCefMouseInputEvent(const blink::WebMouseEvent& event,
                           CefMouseInputEvent& cef_event) {
  CefMouseInputEvent result;
  bool found = false;
  for (const auto& entry : kMouseTypes) {
    if (entry.web == event.GetType()) {
      result.type = entry.cef;
      found = true;
      break;
    }
  }
  if (!found) {
    return false;
  }
  CopyCommonToCef(event, result);
  result.button = static_cast<cef_mouse_button_type_t>(event.button);
  result.click_count = event.click_count;
  result.pointer_type = GetCefPointerType(event.pointer_type);
  result.pointer_id = event.id;
  result.pressure = event.force;
  result.tilt_x = event.tilt_x;
  result.tilt_y = event.tilt_y;
  result.tangential_pressure = event.tangential_pressure;
  result.twist = event.twist;
  result.movement_x = event.movement_x;
  result.movement_y = event.movement_y;
  result.is_raw_movement_event = event.is_raw_movement_event;
  result.device_id = event.device_id;
  result.menu_source_type = static_cast<int>(event.menu_source_type);
  cef_event = result;
  return true;
}

bool GetWebGestureInputEvent(const CefGestureInputEvent& event,
                             blink::WebGestureEvent& web_event) {
  Type type = Type::kUndefined;
  for (const auto& entry : kGestureTypes) {
    if (entry.cef == event.type) {
      type = entry.web;
      break;
    }
  }
  if (type == Type::kUndefined || !ValidCommon(event) ||
      event.source_device <= CEF_GESTURE_DEVICE_UNINITIALIZED ||
      event.source_device > CEF_GESTURE_DEVICE_SCROLLBAR ||
      event.primary_pointer_type < CEF_POINTER_TYPE_TOUCH ||
      event.primary_pointer_type > CEF_POINTER_TYPE_UNKNOWN ||
      event.delta_units < CEF_GESTURE_SCROLL_PRECISE_PIXEL ||
      event.delta_units > CEF_GESTURE_SCROLL_DOCUMENT ||
      event.inertial_phase < 0 || event.inertial_phase > 2 ||
      event.tap_count < 0 || event.pointer_count < 0 ||
      !IsBoolean(event.is_source_touch_event_set_blocking) ||
      !IsBoolean(event.target_viewport) || !IsBoolean(event.prevent_boosting) ||
      !IsBoolean(event.needs_wheel_event) || !IsBoolean(event.zoom_disabled) ||
      !IsBoolean(event.synthetic) || !IsBoolean(event.cursor_control) ||
      !std::isfinite(event.width) || !std::isfinite(event.height) ||
      !std::isfinite(event.delta_x) || !std::isfinite(event.delta_y) ||
      !std::isfinite(event.delta_x_unconstrained) ||
      !std::isfinite(event.delta_y_unconstrained) ||
      !std::isfinite(event.scale) || !std::isfinite(event.velocity_x) ||
      !std::isfinite(event.velocity_y) || event.width < 0 || event.height < 0 ||
      (event.type == CEF_GESTURE_EVENT_PINCH_UPDATE && event.scale <= 0)) {
    return false;
  }
  // Only touchpad pinch and double-tap support wheel-first dispatch.
  if (event.needs_wheel_event &&
      (event.source_device != CEF_GESTURE_DEVICE_TOUCHPAD ||
       !(blink::WebInputEvent::IsPinchGestureEventType(type) ||
         type == Type::kGestureDoubleTap))) {
    return false;
  }
  blink::WebGestureEvent result(
      type, GetWebMouseGestureModifiers(event.modifiers),
      Timestamp(event.timestamp),
      static_cast<blink::mojom::GestureDevice>(event.source_device));
  CopyCommonToWeb(event, result);
  result.primary_pointer_type = GetWebPointerType(event.primary_pointer_type);
  result.primary_unique_touch_event_id = event.primary_unique_touch_event_id;
  result.unique_touch_event_id = event.unique_touch_event_id;
  result.is_source_touch_event_set_blocking =
      event.is_source_touch_event_set_blocking;
  switch (type) {
    case Type::kGestureTap:
    case Type::kGestureTapUnconfirmed:
    case Type::kGestureDoubleTap:
      result.data.tap.tap_count = event.tap_count;
      result.data.tap.width = event.width;
      result.data.tap.height = event.height;
      result.data.tap.needs_wheel_event = event.needs_wheel_event;
      break;
    case Type::kGestureTapDown:
      result.data.tap_down.tap_down_count = event.tap_count;
      result.data.tap_down.width = event.width;
      result.data.tap_down.height = event.height;
      break;
    case Type::kGestureShowPress:
      result.data.show_press.width = event.width;
      result.data.show_press.height = event.height;
      break;
    case Type::kGestureShortPress:
    case Type::kGestureLongPress:
    case Type::kGestureLongTap:
      result.data.long_press.width = event.width;
      result.data.long_press.height = event.height;
      break;
    case Type::kGestureTwoFingerTap:
      result.data.two_finger_tap.first_finger_width = event.width;
      result.data.two_finger_tap.first_finger_height = event.height;
      break;
    case Type::kGestureScrollBegin:
      result.data.scroll_begin.delta_x_hint = event.delta_x;
      result.data.scroll_begin.delta_y_hint = event.delta_y;
      result.data.scroll_begin.pointer_count = event.pointer_count;
      result.data.scroll_begin.delta_hint_units =
          static_cast<ui::ScrollGranularity>(event.delta_units);
      result.data.scroll_begin.inertial_phase =
          static_cast<blink::WebGestureEvent::InertialPhaseState>(
              event.inertial_phase);
      result.data.scroll_begin.target_viewport = event.target_viewport;
      result.data.scroll_begin.synthetic = event.synthetic;
      result.data.scroll_begin.cursor_control = event.cursor_control;
      break;
    case Type::kGestureScrollUpdate:
      result.data.scroll_update.delta_x = event.delta_x;
      result.data.scroll_update.delta_y = event.delta_y;
      result.data.scroll_update.delta_x_unconstrained =
          event.delta_x_unconstrained;
      result.data.scroll_update.delta_y_unconstrained =
          event.delta_y_unconstrained;
      result.data.scroll_update.delta_units =
          static_cast<ui::ScrollGranularity>(event.delta_units);
      result.data.scroll_update.inertial_phase =
          static_cast<blink::WebGestureEvent::InertialPhaseState>(
              event.inertial_phase);
      break;
    case Type::kGestureScrollEnd:
      result.data.scroll_end.delta_x_compensated = event.delta_x;
      result.data.scroll_end.delta_y_compensated = event.delta_y;
      result.data.scroll_end.delta_units =
          static_cast<ui::ScrollGranularity>(event.delta_units);
      result.data.scroll_end.inertial_phase =
          static_cast<blink::WebGestureEvent::InertialPhaseState>(
              event.inertial_phase);
      result.data.scroll_end.synthetic = event.synthetic;
      break;
    case Type::kGestureFlingStart:
      result.data.fling_start.velocity_x = event.velocity_x;
      result.data.fling_start.velocity_y = event.velocity_y;
      result.data.fling_start.target_viewport = event.target_viewport;
      break;
    case Type::kGestureFlingCancel:
      result.data.fling_cancel.target_viewport = event.target_viewport;
      result.data.fling_cancel.prevent_boosting = event.prevent_boosting;
      break;
    case Type::kGesturePinchBegin:
      result.data.pinch_begin.needs_wheel_event = event.needs_wheel_event;
      break;
    case Type::kGesturePinchEnd:
      result.data.pinch_end.needs_wheel_event = event.needs_wheel_event;
      break;
    case Type::kGesturePinchUpdate:
      result.data.pinch_update.scale = event.scale;
      result.data.pinch_update.needs_wheel_event = event.needs_wheel_event;
      result.data.pinch_update.zoom_disabled = event.zoom_disabled;
      break;
    default:
      break;
  }
  web_event = result;
  return true;
}

bool GetCefGestureInputEvent(const blink::WebGestureEvent& event,
                             CefGestureInputEvent& cef_event) {
  CefGestureInputEvent result;
  bool found = false;
  for (const auto& entry : kGestureTypes) {
    if (entry.web == event.GetType()) {
      result.type = entry.cef;
      found = true;
      break;
    }
  }
  if (!found) {
    return false;
  }
  CopyCommonToCef(event, result);
  result.source_device =
      static_cast<cef_gesture_device_t>(event.SourceDevice());
  result.primary_pointer_type = GetCefPointerType(event.primary_pointer_type);
  result.primary_unique_touch_event_id = event.primary_unique_touch_event_id;
  result.unique_touch_event_id = event.unique_touch_event_id;
  result.is_source_touch_event_set_blocking =
      event.is_source_touch_event_set_blocking;
  switch (event.GetType()) {
    case Type::kGestureTap:
    case Type::kGestureTapUnconfirmed:
    case Type::kGestureDoubleTap:
      result.tap_count = event.data.tap.tap_count;
      result.width = event.data.tap.width;
      result.height = event.data.tap.height;
      result.needs_wheel_event = event.data.tap.needs_wheel_event;
      break;
    case Type::kGestureTapDown:
      result.tap_count = event.data.tap_down.tap_down_count;
      result.width = event.data.tap_down.width;
      result.height = event.data.tap_down.height;
      break;
    case Type::kGestureShowPress:
      result.width = event.data.show_press.width;
      result.height = event.data.show_press.height;
      break;
    case Type::kGestureShortPress:
    case Type::kGestureLongPress:
    case Type::kGestureLongTap:
      result.width = event.data.long_press.width;
      result.height = event.data.long_press.height;
      break;
    case Type::kGestureTwoFingerTap:
      result.width = event.data.two_finger_tap.first_finger_width;
      result.height = event.data.two_finger_tap.first_finger_height;
      break;
    case Type::kGestureScrollBegin:
      result.delta_x = event.data.scroll_begin.delta_x_hint;
      result.delta_y = event.data.scroll_begin.delta_y_hint;
      result.pointer_count = event.data.scroll_begin.pointer_count;
      result.delta_units = static_cast<cef_gesture_scroll_units_t>(
          event.data.scroll_begin.delta_hint_units);
      result.inertial_phase =
          static_cast<int>(event.data.scroll_begin.inertial_phase);
      result.target_viewport = event.data.scroll_begin.target_viewport;
      result.synthetic = event.data.scroll_begin.synthetic;
      result.cursor_control = event.data.scroll_begin.cursor_control;
      break;
    case Type::kGestureScrollUpdate:
      result.delta_x = event.data.scroll_update.delta_x;
      result.delta_y = event.data.scroll_update.delta_y;
      result.delta_x_unconstrained =
          event.data.scroll_update.delta_x_unconstrained;
      result.delta_y_unconstrained =
          event.data.scroll_update.delta_y_unconstrained;
      result.delta_units = static_cast<cef_gesture_scroll_units_t>(
          event.data.scroll_update.delta_units);
      result.inertial_phase =
          static_cast<int>(event.data.scroll_update.inertial_phase);
      break;
    case Type::kGestureScrollEnd:
      result.delta_x = event.data.scroll_end.delta_x_compensated;
      result.delta_y = event.data.scroll_end.delta_y_compensated;
      result.delta_units = static_cast<cef_gesture_scroll_units_t>(
          event.data.scroll_end.delta_units);
      result.inertial_phase =
          static_cast<int>(event.data.scroll_end.inertial_phase);
      result.synthetic = event.data.scroll_end.synthetic;
      break;
    case Type::kGestureFlingStart:
      result.velocity_x = event.data.fling_start.velocity_x;
      result.velocity_y = event.data.fling_start.velocity_y;
      result.target_viewport = event.data.fling_start.target_viewport;
      break;
    case Type::kGestureFlingCancel:
      result.target_viewport = event.data.fling_cancel.target_viewport;
      result.prevent_boosting = event.data.fling_cancel.prevent_boosting;
      break;
    case Type::kGesturePinchBegin:
      result.needs_wheel_event = event.data.pinch_begin.needs_wheel_event;
      break;
    case Type::kGesturePinchEnd:
      result.needs_wheel_event = event.data.pinch_end.needs_wheel_event;
      break;
    case Type::kGesturePinchUpdate:
      result.scale = event.data.pinch_update.scale;
      result.needs_wheel_event = event.data.pinch_update.needs_wheel_event;
      result.zoom_disabled = event.data.pinch_update.zoom_disabled;
      break;
    default:
      break;
  }
  cef_event = result;
  return true;
}
