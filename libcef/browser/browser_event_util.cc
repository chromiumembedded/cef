// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/browser_event_util.h"

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "components/input/native_web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

using BWIET = blink::WebInputEvent::Type;

namespace {

uint32_t TranslateWebInputModifiers(int web_modifiers) {
  static constexpr auto kFlagMap = base::MakeFixedFlatMap<
      blink::WebInputEvent::Modifiers, cef_event_flags_t>(
      {{blink::WebInputEvent::kShiftKey, EVENTFLAG_SHIFT_DOWN},
       {blink::WebInputEvent::kControlKey, EVENTFLAG_CONTROL_DOWN},
       {blink::WebInputEvent::kAltKey, EVENTFLAG_ALT_DOWN},
       {blink::WebInputEvent::kMetaKey, EVENTFLAG_COMMAND_DOWN},
       {blink::WebInputEvent::kIsKeyPad, EVENTFLAG_IS_KEY_PAD},
       {blink::WebInputEvent::kIsLeft, EVENTFLAG_IS_LEFT},
       {blink::WebInputEvent::kIsRight, EVENTFLAG_IS_RIGHT},
       {blink::WebInputEvent::kAltGrKey, EVENTFLAG_ALTGR_DOWN},
       {blink::WebInputEvent::kIsAutoRepeat, EVENTFLAG_IS_REPEAT},
       {blink::WebInputEvent::kLeftButtonDown, EVENTFLAG_LEFT_MOUSE_BUTTON},
       {blink::WebInputEvent::kMiddleButtonDown, EVENTFLAG_MIDDLE_MOUSE_BUTTON},
       {blink::WebInputEvent::kRightButtonDown, EVENTFLAG_RIGHT_MOUSE_BUTTON},
       {blink::WebInputEvent::kBackButtonDown, EVENTFLAG_X1_MOUSE_BUTTON},
       {blink::WebInputEvent::kForwardButtonDown, EVENTFLAG_X2_MOUSE_BUTTON}});
  uint32_t cef_modifiers = 0;
  for (const auto& entry : kFlagMap) {
    if (web_modifiers & entry.first) {
      cef_modifiers |= entry.second;
    }
  }
  return cef_modifiers;
}

}  // namespace

bool GetCefTouchEvent(const blink::WebGestureEvent& event,
                      CefTouchEvent& cef_event) {
  const auto type = event.GetType();
  if (type < BWIET::kGestureTypeFirst || type > BWIET::kGestureTypeLast) {
    return false;
  }

  cef_event.id = event.unique_touch_event_id;

  const gfx::PointF position = event.PositionInWidget();
  cef_event.x = position.x();
  cef_event.y = position.y();

  switch (event.GetType()) {
    case BWIET::kGestureTap:
    case BWIET::kGestureTapUnconfirmed:
    case BWIET::kGestureDoubleTap:
      cef_event.radius_x = event.data.tap.width;
      cef_event.radius_y = event.data.tap.height;
      break;
    case BWIET::kGestureTapDown:
      cef_event.radius_x = event.data.tap_down.width;
      cef_event.radius_y = event.data.tap_down.height;
      break;
    case BWIET::kGestureTwoFingerTap:
      cef_event.radius_x = event.data.two_finger_tap.first_finger_width;
      cef_event.radius_y = event.data.two_finger_tap.first_finger_height;
      break;
    case BWIET::kGestureShortPress:
    case BWIET::kGestureLongPress:
    case BWIET::kGestureLongTap:
      cef_event.radius_x = event.data.long_press.width;
      cef_event.radius_y = event.data.long_press.height;
      break;
    case BWIET::kGestureShowPress:
      cef_event.radius_x = event.data.show_press.width;
      cef_event.radius_y = event.data.show_press.height;
      break;
    default:
      cef_event.radius_x = 0;
      cef_event.radius_y = 0;
      break;
  }
  if (event.FrameScale()) {
    cef_event.radius_x /= 2.0f * event.FrameScale();
    cef_event.radius_y /= 2.0f * event.FrameScale();
  }

  cef_event.rotation_angle = 0;
  cef_event.pressure = 0;
  cef_event.type = CEF_TET_INVALID;
  cef_event.modifiers = TranslateWebInputModifiers(event.GetModifiers());
  cef_event.pointer_type = CEF_POINTER_TYPE_TOUCH;

  return true;
}

// Extract the gesture type from a blink::WebGestureEvent.
cef_gesture_type_t GetCefGestureType(const blink::WebGestureEvent& event) {
  static constexpr auto kTypeMap =
      base::MakeFixedFlatMap<BWIET, cef_gesture_type_t>(
          {{BWIET::kGestureScrollBegin, CEF_GESTURE_TYPE_SCROLL_BEGIN},
           {BWIET::kGestureScrollEnd, CEF_GESTURE_TYPE_SCROLL_END},
           {BWIET::kGestureScrollUpdate, CEF_GESTURE_TYPE_SCROLL_UPDATE},
           {BWIET::kGestureFlingStart, CEF_GESTURE_TYPE_FLING_START},
           {BWIET::kGestureFlingCancel, CEF_GESTURE_TYPE_FLING_CANCEL},
           {BWIET::kGesturePinchBegin, CEF_GESTURE_TYPE_PINCH_BEGIN},
           {BWIET::kGesturePinchEnd, CEF_GESTURE_TYPE_PINCH_END},
           {BWIET::kGesturePinchUpdate, CEF_GESTURE_TYPE_PINCH_UPDATE},
           {BWIET::kGestureBegin, CEF_GESTURE_TYPE_BEGIN},
           {BWIET::kGestureTapDown, CEF_GESTURE_TYPE_TAP_DOWN},
           {BWIET::kGestureShowPress, CEF_GESTURE_TYPE_SHOW_PRESS},
           {BWIET::kGestureTap, CEF_GESTURE_TYPE_TAP},
           {BWIET::kGestureTapCancel, CEF_GESTURE_TYPE_TAP_CANCEL},
           {BWIET::kGestureShortPress, CEF_GESTURE_TYPE_SHORT_PRESS},
           {BWIET::kGestureLongPress, CEF_GESTURE_TYPE_LONG_PRESS},
           {BWIET::kGestureLongTap, CEF_GESTURE_TYPE_LONG_TAP},
           {BWIET::kGestureTwoFingerTap, CEF_GESTURE_TYPE_TWO_FINGER_TAP},
           {BWIET::kGestureTapUnconfirmed, CEF_GESTURE_TYPE_TAP_UNCONFIRMED},
           {BWIET::kGestureDoubleTap, CEF_GESTURE_TYPE_DOUBLE_TAP},
           {BWIET::kGestureEnd, CEF_GESTURE_TYPE_END}});
  const auto it = kTypeMap.find(event.GetType());
  return it == kTypeMap.end() ? CEF_GESTURE_TYPE_INVALID : it->second;
}

int GetTapCount(const blink::WebGestureEvent& event) {
  static constexpr auto kTypesWithTapCount = base::MakeFixedFlatSet<BWIET>(
      {BWIET::kGestureTap, BWIET::kGestureTapUnconfirmed,
       BWIET::kGestureDoubleTap});
  return kTypesWithTapCount.contains(event.GetType()) ? event.data.tap.tap_count
                                                      : 0;
}

bool GetCefKeyEvent(const input::NativeWebKeyboardEvent& event,
                    CefKeyEvent& cef_event) {
  static constexpr auto kTypeMap =
      base::MakeFixedFlatMap<BWIET, cef_key_event_type_t>(
          {{BWIET::kRawKeyDown, KEYEVENT_RAWKEYDOWN},
           {BWIET::kKeyDown, KEYEVENT_KEYDOWN},
           {BWIET::kKeyUp, KEYEVENT_KEYUP},
           {BWIET::kChar, KEYEVENT_CHAR}});
  const auto it = kTypeMap.find(event.GetType());
  if (it == kTypeMap.end()) {
    return false;
  }
  cef_event.type = it->second;

  cef_event.modifiers = TranslateWebInputModifiers(event.GetModifiers());
  cef_event.windows_key_code = event.windows_key_code;
  cef_event.native_key_code = event.native_key_code;
  cef_event.is_system_key = event.is_system_key;
  cef_event.character = event.text[0];
  cef_event.unmodified_character = event.unmodified_text[0];

  return true;
}

bool GetCefKeyEvent(const ui::KeyEvent& event, CefKeyEvent& cef_event) {
  return GetCefKeyEvent(input::NativeWebKeyboardEvent(event), cef_event);
}

bool GetCefMouseEvent(const blink::WebMouseEvent& event,
                      CefMouseEvent& cef_event) {
  const auto type = event.GetType();
  if (type < BWIET::kMouseTypeFirst || type > BWIET::kMouseTypeLast) {
    return false;
  }

  const gfx::PointF position = event.PositionInWidget();
  cef_event.x = position.x();
  cef_event.y = position.y();

  cef_event.modifiers = TranslateWebInputModifiers(event.GetModifiers());

  return true;
}

cef_mouse_event_type_t GetCefMouseEventType(const blink::WebMouseEvent& event) {
  static constexpr auto kTypeMap =
      base::MakeFixedFlatMap<BWIET, cef_mouse_event_type_t>(
          {{BWIET::kMouseDown, MET_DOWN},
           {BWIET::kMouseUp, MET_UP},
           {BWIET::kMouseMove, MET_MOVE},
           {BWIET::kMouseEnter, MET_ENTER},
           {BWIET::kMouseLeave, MET_LEAVE},
           {BWIET::kContextMenu, MET_CONTEXT_MENU}});
  const auto it = kTypeMap.find(event.GetType());
  return it == kTypeMap.end() ? MET_INVALID : it->second;
}

cef_mouse_button_type_t GetCefMouseButtonType(
    const blink::WebMouseEvent& event) {
  using BWPPB = blink::WebPointerProperties::Button;
  static constexpr auto kButtonMap =
      base::MakeFixedFlatMap<BWPPB, cef_mouse_button_type_t>(
          {{BWPPB::kLeft, MBT_LEFT},
           {BWPPB::kMiddle, MBT_MIDDLE},
           {BWPPB::kRight, MBT_RIGHT},
           {BWPPB::kBack, MBT_X1},
           {BWPPB::kForward, MBT_X2}});
  const auto it = kButtonMap.find(event.button);
  return it == kButtonMap.end() ? MBT_INVALID : it->second;
}
