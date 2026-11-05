// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <cmath>
#include <limits>

#include "base/time/time.h"
#include "cef/libcef/browser/browser_event_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"

namespace {
using Type = blink::WebInputEvent::Type;

TEST(WheelInputEventUtil, FieldsAndDefaults) {
  CefWheelInputEvent event;
  EXPECT_EQ(sizeof(cef_wheel_input_event_t), event.size);
  blink::WebMouseWheelEvent web;
  const auto before = base::TimeTicks::Now();
  ASSERT_TRUE(GetWebWheelInputEvent(event, web));
  EXPECT_EQ(Type::kMouseWheel, web.GetType());
  EXPECT_GE(web.TimeStamp(), before);
  EXPECT_LE(web.TimeStamp(), base::TimeTicks::Now());
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseNone, web.phase);
  EXPECT_EQ(ui::ScrollGranularity::kScrollByPrecisePixel, web.delta_units);
  EXPECT_FLOAT_EQ(1, web.acceleration_ratio_x);
  EXPECT_FLOAT_EQ(1, web.acceleration_ratio_y);
  EXPECT_EQ(blink::WebInputEvent::DispatchType::kBlocking, web.dispatch_type);
  EXPECT_FALSE(web.has_synthetic_phase);

  event.timestamp = 123456789;
  event.modifiers = CEF_EVENTFLAG_SHIFT_DOWN | CEF_EVENTFLAG_BACK_MOUSE_BUTTON;
  event.x = -12.25f;
  event.y = 34.5f;
  event.screen_x = -112.75f;
  event.screen_y = 234.25f;
  event.delta_x = -0.25f;
  event.delta_y = 17.5f;
  event.wheel_ticks_x = -0.125f;
  event.wheel_ticks_y = 0.75f;
  event.delta_units = CEF_GESTURE_SCROLL_PIXEL;
  event.phase = CEF_WHEEL_PHASE_ENDED;
  event.momentum_phase = CEF_WHEEL_PHASE_BEGAN;
  event.rails_mode = 1;
  event.acceleration_ratio_x = 2;
  event.acceleration_ratio_y = 0.5f;
  ASSERT_TRUE(GetWebWheelInputEvent(event, web));
  EXPECT_EQ(123456789, web.TimeStamp().ToInternalValue());
  EXPECT_EQ(
      blink::WebInputEvent::kShiftKey | blink::WebInputEvent::kBackButtonDown,
      web.GetModifiers());
  EXPECT_EQ(blink::WebMouseEvent::Button::kBack, web.button);
  EXPECT_EQ(blink::WebPointerProperties::PointerType::kMouse, web.pointer_type);
  EXPECT_FLOAT_EQ(event.x, web.PositionInWidget().x());
  EXPECT_FLOAT_EQ(event.y, web.PositionInWidget().y());
  EXPECT_FLOAT_EQ(event.screen_x, web.PositionInScreen().x());
  EXPECT_FLOAT_EQ(event.screen_y, web.PositionInScreen().y());
  EXPECT_FLOAT_EQ(event.delta_x, web.delta_x);
  EXPECT_FLOAT_EQ(event.delta_y, web.delta_y);
  EXPECT_FLOAT_EQ(event.wheel_ticks_x, web.wheel_ticks_x);
  EXPECT_FLOAT_EQ(event.wheel_ticks_y, web.wheel_ticks_y);
  EXPECT_FLOAT_EQ(event.acceleration_ratio_x, web.acceleration_ratio_x);
  EXPECT_FLOAT_EQ(event.acceleration_ratio_y, web.acceleration_ratio_y);
  EXPECT_EQ(ui::ScrollGranularity::kScrollByPixel, web.delta_units);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseEnded, web.phase);
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseBegan, web.momentum_phase);
  EXPECT_EQ(blink::WebInputEvent::RailsMode::kRailsModeHorizontal,
            web.rails_mode);
  EXPECT_EQ(blink::WebMouseWheelEvent::EventAction::kScrollHorizontal,
            web.event_action);
}

TEST(WheelInputEventUtil, PhasesUnitsAndButtons) {
  constexpr struct {
    cef_wheel_event_phase_t cef;
    blink::WebMouseWheelEvent::Phase web;
  } phases[] = {
      {CEF_WHEEL_PHASE_NONE, blink::WebMouseWheelEvent::kPhaseNone},
      {CEF_WHEEL_PHASE_BEGAN, blink::WebMouseWheelEvent::kPhaseBegan},
      {CEF_WHEEL_PHASE_STATIONARY, blink::WebMouseWheelEvent::kPhaseStationary},
      {CEF_WHEEL_PHASE_CHANGED, blink::WebMouseWheelEvent::kPhaseChanged},
      {CEF_WHEEL_PHASE_ENDED, blink::WebMouseWheelEvent::kPhaseEnded},
      {CEF_WHEEL_PHASE_CANCELLED, blink::WebMouseWheelEvent::kPhaseCancelled},
      {CEF_WHEEL_PHASE_MAY_BEGIN, blink::WebMouseWheelEvent::kPhaseMayBegin},
      {CEF_WHEEL_PHASE_BLOCKED, blink::WebMouseWheelEvent::kPhaseBlocked},
  };
  CefWheelInputEvent event;
  blink::WebMouseWheelEvent web;
  for (const auto& phase : phases) {
    event.momentum_phase = phase.cef;
    ASSERT_TRUE(GetWebWheelInputEvent(event, web));
    EXPECT_EQ(phase.web, web.momentum_phase);
    event.phase = phase.cef;
    EXPECT_EQ(phase.cef != CEF_WHEEL_PHASE_BLOCKED,
              GetWebWheelInputEvent(event, web));
    if (phase.cef != CEF_WHEEL_PHASE_BLOCKED) {
      EXPECT_EQ(phase.web, web.phase);
    }
    event.phase = CEF_WHEEL_PHASE_NONE;
  }
  event.phase = static_cast<cef_wheel_event_phase_t>(CEF_WHEEL_PHASE_CHANGED |
                                                     CEF_WHEEL_PHASE_ENDED);
  ASSERT_TRUE(GetWebWheelInputEvent(event, web));
  EXPECT_EQ(blink::WebMouseWheelEvent::kPhaseChanged |
                blink::WebMouseWheelEvent::kPhaseEnded,
            web.phase);
  for (auto units : {CEF_GESTURE_SCROLL_PRECISE_PIXEL, CEF_GESTURE_SCROLL_PIXEL,
                     CEF_GESTURE_SCROLL_PAGE}) {
    for (int rails : {0, 1, 2}) {
      event.delta_units = units;
      event.rails_mode = rails;
      ASSERT_TRUE(GetWebWheelInputEvent(event, web));
      EXPECT_EQ(static_cast<int>(units), static_cast<int>(web.delta_units));
      EXPECT_EQ(rails, static_cast<int>(web.rails_mode));
    }
  }
  constexpr struct {
    uint32_t flag;
    blink::WebMouseEvent::Button button;
  } buttons[] = {
      {0, blink::WebMouseEvent::Button::kNoButton},
      {CEF_EVENTFLAG_LEFT_MOUSE_BUTTON, blink::WebMouseEvent::Button::kLeft},
      {CEF_EVENTFLAG_MIDDLE_MOUSE_BUTTON,
       blink::WebMouseEvent::Button::kMiddle},
      {CEF_EVENTFLAG_RIGHT_MOUSE_BUTTON, blink::WebMouseEvent::Button::kRight},
      {CEF_EVENTFLAG_BACK_MOUSE_BUTTON, blink::WebMouseEvent::Button::kBack},
      {CEF_EVENTFLAG_FORWARD_MOUSE_BUTTON,
       blink::WebMouseEvent::Button::kForward},
  };
  for (const auto& button : buttons) {
    event.modifiers = button.flag;
    ASSERT_TRUE(GetWebWheelInputEvent(event, web));
    EXPECT_EQ(button.button, web.button);
  }
}

TEST(WheelInputEventUtil, InvalidValuesPreserveOutput) {
  const CefWheelInputEvent valid;
  auto reject = [](const CefWheelInputEvent& event) {
    blink::WebMouseWheelEvent web(Type::kMouseWheel, 0,
                                  base::TimeTicks::FromInternalValue(47));
    web.delta_y = 23;
    EXPECT_FALSE(GetWebWheelInputEvent(event, web));
    EXPECT_EQ(47, web.TimeStamp().ToInternalValue());
    EXPECT_FLOAT_EQ(23, web.delta_y);
  };
  for (auto member :
       {&cef_wheel_input_event_t::x, &cef_wheel_input_event_t::y,
        &cef_wheel_input_event_t::screen_x, &cef_wheel_input_event_t::screen_y,
        &cef_wheel_input_event_t::delta_x, &cef_wheel_input_event_t::delta_y,
        &cef_wheel_input_event_t::wheel_ticks_x,
        &cef_wheel_input_event_t::wheel_ticks_y,
        &cef_wheel_input_event_t::acceleration_ratio_x,
        &cef_wheel_input_event_t::acceleration_ratio_y}) {
    for (float value : {std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()}) {
      auto event = valid;
      event.*member = value;
      reject(event);
    }
  }
  for (auto member : {&cef_wheel_input_event_t::acceleration_ratio_x,
                      &cef_wheel_input_event_t::acceleration_ratio_y}) {
    auto event = valid;
    event.*member = -0.01f;
    reject(event);
  }
  for (int units : {-1, 2, 4, 5}) {
    auto event = valid;
    event.delta_units = static_cast<cef_gesture_scroll_units_t>(units);
    reject(event);
  }
  for (auto member : {&cef_wheel_input_event_t::phase,
                      &cef_wheel_input_event_t::momentum_phase}) {
    for (int phase : {-1, 128}) {
      auto event = valid;
      event.*member = static_cast<cef_wheel_event_phase_t>(phase);
      reject(event);
    }
  }
  for (int rails : {-1, 3}) {
    auto event = valid;
    event.rails_mode = rails;
    reject(event);
  }
  auto event = valid;
  event.timestamp = -1;
  reject(event);
  event = valid;
  event.phase = CEF_WHEEL_PHASE_BLOCKED;
  reject(event);
}

TEST(WheelInputEventUtil, VersionedCopies) {
  struct Older {
    size_t size;
    uint32_t modifiers;
    int64_t timestamp;
  } older = {sizeof(Older), CEF_EVENTFLAG_SHIFT_DOWN, 123456789};
  CefWheelInputEvent event;
  event.delta_y = 47;
  event.Set(*reinterpret_cast<const cef_wheel_input_event_t*>(&older), true);
  EXPECT_EQ(sizeof(cef_wheel_input_event_t), event.size);
  EXPECT_EQ(CEF_EVENTFLAG_SHIFT_DOWN, event.modifiers);
  EXPECT_EQ(123456789, event.timestamp);
  EXPECT_FLOAT_EQ(0, event.delta_y);
  struct Newer {
    cef_wheel_input_event_t event;
    uint64_t future;
  } newer = {};
  newer.event.size = sizeof(newer);
  newer.event.delta_y = 23.5f;
  newer.future = 0xdeadbeef;
  event = CefWheelInputEvent(newer.event);
  EXPECT_EQ(sizeof(cef_wheel_input_event_t), event.size);
  EXPECT_FLOAT_EQ(23.5f, event.delta_y);
  event.AttachTo(newer.event);
  event.delta_y = 17.25f;
  event.DetachTo(newer.event);
  EXPECT_FLOAT_EQ(17.25f, newer.event.delta_y);
  EXPECT_EQ(0xdeadbeefU, newer.future);
}

template <class Event>
void ExpectCommon(const Event& original, const Event& restored) {
  EXPECT_EQ(original.GetType(), restored.GetType());
  EXPECT_EQ(original.GetModifiers(), restored.GetModifiers());
  EXPECT_EQ(original.TimeStamp(), restored.TimeStamp());
  EXPECT_FLOAT_EQ(original.PositionInWidget().x(),
                  restored.PositionInWidget().x());
  EXPECT_FLOAT_EQ(original.PositionInWidget().y(),
                  restored.PositionInWidget().y());
  EXPECT_FLOAT_EQ(original.PositionInScreen().x(),
                  restored.PositionInScreen().x());
  EXPECT_FLOAT_EQ(original.PositionInScreen().y(),
                  restored.PositionInScreen().y());
}
}  // namespace

TEST(MouseInputEventUtil, CompleteSnapshotRoundTrip) {
  for (auto type : {Type::kMouseDown, Type::kMouseUp, Type::kMouseMove,
                    Type::kMouseEnter, Type::kMouseLeave, Type::kContextMenu}) {
    for (auto button : {blink::WebMouseEvent::Button::kNoButton,
                        blink::WebMouseEvent::Button::kLeft,
                        blink::WebMouseEvent::Button::kMiddle,
                        blink::WebMouseEvent::Button::kRight,
                        blink::WebMouseEvent::Button::kBack,
                        blink::WebMouseEvent::Button::kForward,
                        blink::WebMouseEvent::Button::kEraser}) {
      blink::WebMouseEvent original(
          type,
          blink::WebInputEvent::kShiftKey |
              blink::WebInputEvent::kLeftButtonDown |
              blink::WebInputEvent::kMiddleButtonDown |
              blink::WebInputEvent::kRightButtonDown |
              blink::WebInputEvent::kBackButtonDown |
              blink::WebInputEvent::kForwardButtonDown |
              blink::WebInputEvent::kIsCompatibilityEventForTouch |
              blink::WebInputEvent::kRelativeMotionEvent |
              blink::WebInputEvent::kIsTouchAccessibility |
              blink::WebInputEvent::kFnKey | blink::WebInputEvent::kSymbolKey |
              blink::WebInputEvent::kScrollLockOn,
          base::TimeTicks::FromInternalValue(123456789));
      original.SetPositionInWidget(12.5f, 34.25f);
      original.SetPositionInScreen(112.5f, 234.25f);
      original.button = button;
      original.click_count = 2;
      original.pointer_type = blink::WebPointerProperties::PointerType::kPen;
      original.id = 17;
      original.force = 0.75f;
      original.tilt_x = -23.5;
      original.tilt_y = 44.5;
      original.tangential_pressure = -0.5f;
      original.twist = 145;
      original.movement_x = -13;
      original.movement_y = 19;
      original.is_raw_movement_event = true;
      original.device_id = 37;
      original.menu_source_type = ui::mojom::MenuSourceType::kStylus;
      CefMouseInputEvent snapshot;
      ASSERT_TRUE(GetCefMouseInputEvent(original, snapshot));
      EXPECT_EQ(sizeof(cef_mouse_input_event_t), snapshot.size);
      EXPECT_EQ(CEF_POINTER_TYPE_PEN, snapshot.pointer_type);
      EXPECT_EQ(static_cast<int>(original.button), snapshot.button);
      EXPECT_EQ(original.click_count, snapshot.click_count);
      EXPECT_EQ(original.id, snapshot.pointer_id);
      EXPECT_EQ(original.force, snapshot.pressure);
      EXPECT_EQ(original.tilt_x, snapshot.tilt_x);
      EXPECT_EQ(original.tilt_y, snapshot.tilt_y);
      EXPECT_EQ(original.tangential_pressure, snapshot.tangential_pressure);
      EXPECT_EQ(original.twist, snapshot.twist);
      EXPECT_EQ(original.movement_x, snapshot.movement_x);
      EXPECT_EQ(original.movement_y, snapshot.movement_y);
      EXPECT_EQ(original.is_raw_movement_event, snapshot.is_raw_movement_event);
      EXPECT_EQ(original.device_id, snapshot.device_id);
      EXPECT_EQ(static_cast<int>(original.menu_source_type),
                snapshot.menu_source_type);
      EXPECT_FLOAT_EQ(original.PositionInWidget().x(), snapshot.x);
      EXPECT_FLOAT_EQ(original.PositionInWidget().y(), snapshot.y);
      EXPECT_FLOAT_EQ(original.PositionInScreen().x(), snapshot.screen_x);
      EXPECT_FLOAT_EQ(original.PositionInScreen().y(), snapshot.screen_y);
      EXPECT_EQ(123456789, snapshot.timestamp);
      CefMouseInputEvent owned(snapshot);
      blink::WebMouseEvent restored;
      ASSERT_TRUE(GetWebMouseInputEvent(owned, restored));
      ExpectCommon(original, restored);
      EXPECT_EQ(original.button, restored.button);
      EXPECT_EQ(original.click_count, restored.click_count);
      EXPECT_EQ(original.pointer_type, restored.pointer_type);
      EXPECT_EQ(original.id, restored.id);
      EXPECT_EQ(original.force, restored.force);
      EXPECT_EQ(original.tilt_x, restored.tilt_x);
      EXPECT_EQ(original.tilt_y, restored.tilt_y);
      EXPECT_EQ(original.tangential_pressure, restored.tangential_pressure);
      EXPECT_EQ(original.twist, restored.twist);
      EXPECT_EQ(original.movement_x, restored.movement_x);
      EXPECT_EQ(original.movement_y, restored.movement_y);
      EXPECT_EQ(original.is_raw_movement_event, restored.is_raw_movement_event);
      EXPECT_EQ(original.device_id, restored.device_id);
      EXPECT_EQ(original.menu_source_type, restored.menu_source_type);
    }
  }
}

TEST(MouseInputEventUtil, InvalidSnapshotsAndUnsupportedPressure) {
  CefMouseInputEvent valid;
  valid.type = CEF_MOUSE_EVENT_MOVE;
  valid.button = CEF_MOUSE_BUTTON_NONE;
  for (int i = 0; i < 12; ++i) {
    CefMouseInputEvent event(valid);
    switch (i) {
      case 0:
        event.type = static_cast<cef_mouse_input_event_type_t>(999);
        break;
      case 1:
        event.button = static_cast<cef_mouse_button_type_t>(999);
        break;
      case 2:
        event.pointer_type = static_cast<cef_pointer_type_t>(999);
        break;
      case 3:
        event.modifiers = CEF_EVENTFLAG_SCROLL_BY_PAGE;
        break;
      case 4:
        event.timestamp = -1;
        break;
      case 5:
        event.x = std::numeric_limits<float>::infinity();
        break;
      case 6:
        event.pressure = 1.1f;
        break;
      case 7:
        event.tilt_x = 91;
        break;
      case 8:
        event.twist = 360;
        break;
      case 9:
        event.is_raw_movement_event = 2;
        break;
      case 10:
        event.menu_source_type = 11;
        break;
      case 11:
        event.click_count = -1;
        break;
    }
    blink::WebMouseEvent output;
    output.click_count = 47;
    EXPECT_FALSE(GetWebMouseInputEvent(event, output)) << i;
    EXPECT_EQ(Type::kUndefined, output.GetType());
    EXPECT_EQ(47, output.click_count);
  }
  valid.pressure = std::numeric_limits<float>::quiet_NaN();
  blink::WebMouseEvent web;
  ASSERT_TRUE(GetWebMouseInputEvent(valid, web));
  EXPECT_TRUE(std::isnan(web.force));
  EXPECT_GT(web.TimeStamp().ToInternalValue(), 0);
  CefMouseInputEvent snapshot;
  ASSERT_TRUE(GetCefMouseInputEvent(web, snapshot));
  EXPECT_TRUE(std::isnan(snapshot.pressure));
  web = blink::WebMouseEvent();
  snapshot.click_count = 47;
  EXPECT_FALSE(GetCefMouseInputEvent(web, snapshot));
  EXPECT_EQ(47, snapshot.click_count);
  blink::WebMouseWheelEvent wheel(blink::WebInputEvent::Type::kMouseWheel,
                                  blink::WebInputEvent::kNoModifiers,
                                  base::TimeTicks::Now());
  EXPECT_FALSE(GetCefMouseInputEvent(wheel, snapshot));
  EXPECT_EQ(47, snapshot.click_count);
}

TEST(MouseGestureInputEventUtil, LegacyPointerTypesRetainTheirMeaning) {
  using PointerType = blink::WebPointerProperties::PointerType;
  constexpr struct {
    cef_pointer_type_t cef;
    PointerType web;
  } types[] = {
      {CEF_POINTER_TYPE_TOUCH, PointerType::kTouch},
      {CEF_POINTER_TYPE_MOUSE, PointerType::kMouse},
      {CEF_POINTER_TYPE_PEN, PointerType::kPen},
      {CEF_POINTER_TYPE_ERASER, PointerType::kEraser},
      {CEF_POINTER_TYPE_UNKNOWN, PointerType::kUnknown},
  };
  for (const auto& type : types) {
    CefMouseInputEvent mouse;
    mouse.type = CEF_MOUSE_EVENT_MOVE;
    mouse.pointer_type = type.cef;
    blink::WebMouseEvent web_mouse;
    ASSERT_TRUE(GetWebMouseInputEvent(mouse, web_mouse));
    EXPECT_EQ(type.web, web_mouse.pointer_type);
    ASSERT_TRUE(GetCefMouseInputEvent(web_mouse, mouse));
    EXPECT_EQ(type.cef, mouse.pointer_type);

    CefGestureInputEvent gesture;
    gesture.type = CEF_GESTURE_EVENT_TAP;
    gesture.source_device = CEF_GESTURE_DEVICE_TOUCHSCREEN;
    gesture.primary_pointer_type = type.cef;
    blink::WebGestureEvent web_gesture;
    ASSERT_TRUE(GetWebGestureInputEvent(gesture, web_gesture));
    EXPECT_EQ(type.web, web_gesture.primary_pointer_type);
    ASSERT_TRUE(GetCefGestureInputEvent(web_gesture, gesture));
    EXPECT_EQ(type.cef, gesture.primary_pointer_type);
  }
}

TEST(GestureInputEventUtil, SubtypeSnapshotsPreserveBlinkFields) {
  for (auto type : {Type::kGestureScrollBegin,  Type::kGestureScrollEnd,
                    Type::kGestureScrollUpdate, Type::kGestureFlingStart,
                    Type::kGestureFlingCancel,  Type::kGesturePinchBegin,
                    Type::kGesturePinchEnd,     Type::kGesturePinchUpdate,
                    Type::kGestureBegin,        Type::kGestureTapDown,
                    Type::kGestureShowPress,    Type::kGestureTap,
                    Type::kGestureTapCancel,    Type::kGestureShortPress,
                    Type::kGestureLongPress,    Type::kGestureLongTap,
                    Type::kGestureTwoFingerTap, Type::kGestureTapUnconfirmed,
                    Type::kGestureDoubleTap,    Type::kGestureEnd}) {
    blink::WebGestureEvent original(
        type, blink::WebInputEvent::kControlKey,
        base::TimeTicks::FromInternalValue(123456789),
        blink::mojom::GestureDevice::kTouchpad);
    original.SetPositionInWidget(gfx::PointF(12.5f, 34.25f));
    original.SetPositionInScreen(gfx::PointF(112.5f, 234.25f));
    original.primary_pointer_type =
        blink::WebPointerProperties::PointerType::kTouch;
    original.primary_unique_touch_event_id = 17;
    original.unique_touch_event_id = 23;
    original.is_source_touch_event_set_blocking = true;
    switch (type) {
      case Type::kGestureTap:
      case Type::kGestureTapUnconfirmed:
      case Type::kGestureDoubleTap:
        original.data.tap.tap_count = 2;
        original.data.tap.width = 24.5f;
        original.data.tap.height = 31.25f;
        original.data.tap.needs_wheel_event = type == Type::kGestureDoubleTap;
        break;
      case Type::kGestureTapDown:
        original.data.tap_down.tap_down_count = 2;
        original.data.tap_down.width = 24.5f;
        original.data.tap_down.height = 31.25f;
        break;
      case Type::kGestureShowPress:
        original.data.show_press.width = 24.5f;
        original.data.show_press.height = 31.25f;
        break;
      case Type::kGestureShortPress:
      case Type::kGestureLongPress:
      case Type::kGestureLongTap:
        original.data.long_press.width = 24.5f;
        original.data.long_press.height = 31.25f;
        break;
      case Type::kGestureTwoFingerTap:
        original.data.two_finger_tap.first_finger_width = 24.5f;
        original.data.two_finger_tap.first_finger_height = 31.25f;
        break;
      case Type::kGestureScrollBegin:
        original.data.scroll_begin.delta_x_hint = -12.5f;
        original.data.scroll_begin.delta_y_hint = 17.25f;
        original.data.scroll_begin.pointer_count = 2;
        original.data.scroll_begin.delta_hint_units =
            static_cast<ui::ScrollGranularity>(1);
        original.data.scroll_begin.inertial_phase =
            static_cast<blink::WebGestureEvent::InertialPhaseState>(1);
        original.data.scroll_begin.target_viewport = true;
        original.data.scroll_begin.synthetic = true;
        original.data.scroll_begin.cursor_control = true;
        break;
      case Type::kGestureScrollUpdate:
        original.data.scroll_update.delta_x = -12.5f;
        original.data.scroll_update.delta_y = 17.25f;
        original.data.scroll_update.delta_x_unconstrained = -19.75f;
        original.data.scroll_update.delta_y_unconstrained = 23.5f;
        original.data.scroll_update.delta_units =
            static_cast<ui::ScrollGranularity>(1);
        original.data.scroll_update.inertial_phase =
            static_cast<blink::WebGestureEvent::InertialPhaseState>(1);
        break;
      case Type::kGestureScrollEnd:
        original.data.scroll_end.delta_x_compensated = -12.5f;
        original.data.scroll_end.delta_y_compensated = 17.25f;
        original.data.scroll_end.delta_units =
            static_cast<ui::ScrollGranularity>(1);
        original.data.scroll_end.inertial_phase =
            static_cast<blink::WebGestureEvent::InertialPhaseState>(1);
        original.data.scroll_end.synthetic = true;
        break;
      case Type::kGestureFlingStart:
        original.data.fling_start.velocity_x = 123.5f;
        original.data.fling_start.velocity_y = -234.25f;
        original.data.fling_start.target_viewport = true;
        break;
      case Type::kGestureFlingCancel:
        original.data.fling_cancel.target_viewport = true;
        original.data.fling_cancel.prevent_boosting = true;
        break;
      case Type::kGesturePinchBegin:
        original.data.pinch_begin.needs_wheel_event = true;
        break;
      case Type::kGesturePinchEnd:
        original.data.pinch_end.needs_wheel_event = true;
        break;
      case Type::kGesturePinchUpdate:
        original.data.pinch_update.scale = 1.25f;
        original.data.pinch_update.needs_wheel_event = true;
        original.data.pinch_update.zoom_disabled = true;
        break;
      default:
        break;
    }
    CefGestureInputEvent snapshot;
    ASSERT_TRUE(GetCefGestureInputEvent(original, snapshot));
    EXPECT_EQ(sizeof(cef_gesture_input_event_t), snapshot.size);
    EXPECT_FLOAT_EQ(original.PositionInWidget().x(), snapshot.x);
    EXPECT_FLOAT_EQ(original.PositionInWidget().y(), snapshot.y);
    EXPECT_FLOAT_EQ(original.PositionInScreen().x(), snapshot.screen_x);
    EXPECT_FLOAT_EQ(original.PositionInScreen().y(), snapshot.screen_y);
    EXPECT_EQ(123456789, snapshot.timestamp);
    blink::WebGestureEvent restored;
    ASSERT_TRUE(
        GetWebGestureInputEvent(CefGestureInputEvent(snapshot), restored));
    ExpectCommon(original, restored);
    EXPECT_EQ(original.SourceDevice(), restored.SourceDevice());
    EXPECT_EQ(original.primary_pointer_type, restored.primary_pointer_type);
    EXPECT_EQ(original.primary_unique_touch_event_id,
              restored.primary_unique_touch_event_id);
    EXPECT_EQ(original.unique_touch_event_id, restored.unique_touch_event_id);
    EXPECT_TRUE(restored.is_source_touch_event_set_blocking);
    switch (type) {
      case Type::kGestureTap:
      case Type::kGestureTapUnconfirmed:
      case Type::kGestureDoubleTap:
        EXPECT_EQ(original.data.tap.tap_count, restored.data.tap.tap_count);
        EXPECT_EQ(original.data.tap.tap_count, snapshot.tap_count);
        EXPECT_EQ(original.data.tap.width, restored.data.tap.width);
        EXPECT_EQ(original.data.tap.width, snapshot.width);
        EXPECT_EQ(original.data.tap.height, restored.data.tap.height);
        EXPECT_EQ(original.data.tap.height, snapshot.height);
        EXPECT_EQ(original.data.tap.needs_wheel_event,
                  restored.data.tap.needs_wheel_event);
        EXPECT_EQ(original.data.tap.needs_wheel_event,
                  snapshot.needs_wheel_event);
        break;
      case Type::kGestureTapDown:
        EXPECT_EQ(original.data.tap_down.tap_down_count,
                  restored.data.tap_down.tap_down_count);
        EXPECT_EQ(original.data.tap_down.tap_down_count, snapshot.tap_count);
        EXPECT_EQ(original.data.tap_down.width, restored.data.tap_down.width);
        EXPECT_EQ(original.data.tap_down.width, snapshot.width);
        EXPECT_EQ(original.data.tap_down.height, restored.data.tap_down.height);
        EXPECT_EQ(original.data.tap_down.height, snapshot.height);
        break;
      case Type::kGestureShowPress:
        EXPECT_EQ(original.data.show_press.width,
                  restored.data.show_press.width);
        EXPECT_EQ(original.data.show_press.width, snapshot.width);
        EXPECT_EQ(original.data.show_press.height,
                  restored.data.show_press.height);
        EXPECT_EQ(original.data.show_press.height, snapshot.height);
        break;
      case Type::kGestureShortPress:
      case Type::kGestureLongPress:
      case Type::kGestureLongTap:
        EXPECT_EQ(original.data.long_press.width,
                  restored.data.long_press.width);
        EXPECT_EQ(original.data.long_press.width, snapshot.width);
        EXPECT_EQ(original.data.long_press.height,
                  restored.data.long_press.height);
        EXPECT_EQ(original.data.long_press.height, snapshot.height);
        break;
      case Type::kGestureTwoFingerTap:
        EXPECT_EQ(original.data.two_finger_tap.first_finger_width,
                  restored.data.two_finger_tap.first_finger_width);
        EXPECT_EQ(original.data.two_finger_tap.first_finger_width,
                  snapshot.width);
        EXPECT_EQ(original.data.two_finger_tap.first_finger_height,
                  restored.data.two_finger_tap.first_finger_height);
        EXPECT_EQ(original.data.two_finger_tap.first_finger_height,
                  snapshot.height);
        break;
      case Type::kGestureScrollBegin:
        EXPECT_EQ(original.data.scroll_begin.delta_x_hint,
                  restored.data.scroll_begin.delta_x_hint);
        EXPECT_EQ(original.data.scroll_begin.delta_x_hint, snapshot.delta_x);
        EXPECT_EQ(original.data.scroll_begin.delta_y_hint,
                  restored.data.scroll_begin.delta_y_hint);
        EXPECT_EQ(original.data.scroll_begin.delta_y_hint, snapshot.delta_y);
        EXPECT_EQ(original.data.scroll_begin.pointer_count,
                  restored.data.scroll_begin.pointer_count);
        EXPECT_EQ(original.data.scroll_begin.pointer_count,
                  snapshot.pointer_count);
        EXPECT_EQ(original.data.scroll_begin.delta_hint_units,
                  restored.data.scroll_begin.delta_hint_units);
        EXPECT_EQ(static_cast<int>(original.data.scroll_begin.delta_hint_units),
                  snapshot.delta_units);
        EXPECT_EQ(original.data.scroll_begin.inertial_phase,
                  restored.data.scroll_begin.inertial_phase);
        EXPECT_EQ(static_cast<int>(original.data.scroll_begin.inertial_phase),
                  snapshot.inertial_phase);
        EXPECT_EQ(original.data.scroll_begin.target_viewport,
                  restored.data.scroll_begin.target_viewport);
        EXPECT_EQ(original.data.scroll_begin.target_viewport,
                  snapshot.target_viewport);
        EXPECT_EQ(original.data.scroll_begin.synthetic,
                  restored.data.scroll_begin.synthetic);
        EXPECT_EQ(original.data.scroll_begin.synthetic, snapshot.synthetic);
        EXPECT_EQ(original.data.scroll_begin.cursor_control,
                  restored.data.scroll_begin.cursor_control);
        EXPECT_EQ(original.data.scroll_begin.cursor_control,
                  snapshot.cursor_control);
        break;
      case Type::kGestureScrollUpdate:
        EXPECT_EQ(original.data.scroll_update.delta_x,
                  restored.data.scroll_update.delta_x);
        EXPECT_EQ(original.data.scroll_update.delta_x, snapshot.delta_x);
        EXPECT_EQ(original.data.scroll_update.delta_y,
                  restored.data.scroll_update.delta_y);
        EXPECT_EQ(original.data.scroll_update.delta_y, snapshot.delta_y);
        EXPECT_EQ(original.data.scroll_update.delta_x_unconstrained,
                  restored.data.scroll_update.delta_x_unconstrained);
        EXPECT_EQ(original.data.scroll_update.delta_x_unconstrained,
                  snapshot.delta_x_unconstrained);
        EXPECT_EQ(original.data.scroll_update.delta_y_unconstrained,
                  restored.data.scroll_update.delta_y_unconstrained);
        EXPECT_EQ(original.data.scroll_update.delta_y_unconstrained,
                  snapshot.delta_y_unconstrained);
        EXPECT_EQ(original.data.scroll_update.delta_units,
                  restored.data.scroll_update.delta_units);
        EXPECT_EQ(static_cast<int>(original.data.scroll_update.delta_units),
                  snapshot.delta_units);
        EXPECT_EQ(original.data.scroll_update.inertial_phase,
                  restored.data.scroll_update.inertial_phase);
        EXPECT_EQ(static_cast<int>(original.data.scroll_update.inertial_phase),
                  snapshot.inertial_phase);
        break;
      case Type::kGestureScrollEnd:
        EXPECT_EQ(original.data.scroll_end.delta_x_compensated,
                  restored.data.scroll_end.delta_x_compensated);
        EXPECT_EQ(original.data.scroll_end.delta_x_compensated,
                  snapshot.delta_x);
        EXPECT_EQ(original.data.scroll_end.delta_y_compensated,
                  restored.data.scroll_end.delta_y_compensated);
        EXPECT_EQ(original.data.scroll_end.delta_y_compensated,
                  snapshot.delta_y);
        EXPECT_EQ(original.data.scroll_end.delta_units,
                  restored.data.scroll_end.delta_units);
        EXPECT_EQ(static_cast<int>(original.data.scroll_end.delta_units),
                  snapshot.delta_units);
        EXPECT_EQ(original.data.scroll_end.inertial_phase,
                  restored.data.scroll_end.inertial_phase);
        EXPECT_EQ(static_cast<int>(original.data.scroll_end.inertial_phase),
                  snapshot.inertial_phase);
        EXPECT_EQ(original.data.scroll_end.synthetic,
                  restored.data.scroll_end.synthetic);
        EXPECT_EQ(original.data.scroll_end.synthetic, snapshot.synthetic);
        break;
      case Type::kGestureFlingStart:
        EXPECT_EQ(original.data.fling_start.velocity_x,
                  restored.data.fling_start.velocity_x);
        EXPECT_EQ(original.data.fling_start.velocity_x, snapshot.velocity_x);
        EXPECT_EQ(original.data.fling_start.velocity_y,
                  restored.data.fling_start.velocity_y);
        EXPECT_EQ(original.data.fling_start.velocity_y, snapshot.velocity_y);
        EXPECT_EQ(original.data.fling_start.target_viewport,
                  restored.data.fling_start.target_viewport);
        EXPECT_EQ(original.data.fling_start.target_viewport,
                  snapshot.target_viewport);
        break;
      case Type::kGestureFlingCancel:
        EXPECT_EQ(original.data.fling_cancel.target_viewport,
                  restored.data.fling_cancel.target_viewport);
        EXPECT_EQ(original.data.fling_cancel.target_viewport,
                  snapshot.target_viewport);
        EXPECT_EQ(original.data.fling_cancel.prevent_boosting,
                  restored.data.fling_cancel.prevent_boosting);
        EXPECT_EQ(original.data.fling_cancel.prevent_boosting,
                  snapshot.prevent_boosting);
        break;
      case Type::kGesturePinchBegin:
        EXPECT_EQ(original.data.pinch_begin.needs_wheel_event,
                  restored.data.pinch_begin.needs_wheel_event);
        EXPECT_EQ(original.data.pinch_begin.needs_wheel_event,
                  snapshot.needs_wheel_event);
        break;
      case Type::kGesturePinchEnd:
        EXPECT_EQ(original.data.pinch_end.needs_wheel_event,
                  restored.data.pinch_end.needs_wheel_event);
        EXPECT_EQ(original.data.pinch_end.needs_wheel_event,
                  snapshot.needs_wheel_event);
        break;
      case Type::kGesturePinchUpdate:
        EXPECT_EQ(original.data.pinch_update.scale,
                  restored.data.pinch_update.scale);
        EXPECT_EQ(original.data.pinch_update.scale, snapshot.scale);
        EXPECT_EQ(original.data.pinch_update.needs_wheel_event,
                  restored.data.pinch_update.needs_wheel_event);
        EXPECT_EQ(original.data.pinch_update.needs_wheel_event,
                  snapshot.needs_wheel_event);
        EXPECT_EQ(original.data.pinch_update.zoom_disabled,
                  restored.data.pinch_update.zoom_disabled);
        EXPECT_EQ(original.data.pinch_update.zoom_disabled,
                  snapshot.zoom_disabled);
        break;
      default:
        break;
    }
    if (type != Type::kGestureScrollBegin) {
      EXPECT_EQ(0, snapshot.pointer_count);
    }
    if (type != Type::kGesturePinchUpdate) {
      EXPECT_EQ(0, snapshot.scale);
    }
    if (type != Type::kGestureFlingStart) {
      EXPECT_EQ(0, snapshot.velocity_x);
    }
  }
}

TEST(GestureInputEventUtil, InvalidSnapshotsLeaveOutputUnchanged) {
  CefGestureInputEvent valid;
  valid.type = CEF_GESTURE_EVENT_TAP;
  valid.source_device = CEF_GESTURE_DEVICE_TOUCHSCREEN;
  for (int i = 0; i < 12; ++i) {
    CefGestureInputEvent event(valid);
    switch (i) {
      case 0:
        event.type = static_cast<cef_gesture_input_event_type_t>(999);
        break;
      case 1:
        event.source_device = CEF_GESTURE_DEVICE_UNINITIALIZED;
        break;
      case 2:
        event.primary_pointer_type = static_cast<cef_pointer_type_t>(999);
        break;
      case 3:
        event.modifiers = 1U << 31;
        break;
      case 4:
        event.timestamp = -1;
        break;
      case 5:
        event.y = std::numeric_limits<float>::quiet_NaN();
        break;
      case 6:
        event.delta_units = static_cast<cef_gesture_scroll_units_t>(999);
        break;
      case 7:
        event.inertial_phase = 3;
        break;
      case 8:
        event.target_viewport = 2;
        break;
      case 9:
        event.type = CEF_GESTURE_EVENT_PINCH_UPDATE;
        event.scale = 0;
        break;
      case 10:
        event.width = -1;
        break;
      case 11:
        event.needs_wheel_event = 1;
        break;
    }
    blink::WebGestureEvent output;
    output.unique_touch_event_id = 47;
    EXPECT_FALSE(GetWebGestureInputEvent(event, output)) << i;
    EXPECT_EQ(Type::kUndefined, output.GetType());
    EXPECT_EQ(47U, output.unique_touch_event_id);
  }
  blink::WebGestureEvent web;
  ASSERT_TRUE(GetWebGestureInputEvent(valid, web));
  EXPECT_GT(web.TimeStamp().ToInternalValue(), 0);
  web = blink::WebGestureEvent();
  CefGestureInputEvent snapshot;
  snapshot.tap_count = 47;
  EXPECT_FALSE(GetCefGestureInputEvent(web, snapshot));
  EXPECT_EQ(47, snapshot.tap_count);
}

TEST(MouseGestureInputEventUtil, EventTypeMappings) {
  constexpr struct {
    cef_mouse_input_event_type_t cef;
    Type web;
  } mouse_types[] = {
      {CEF_MOUSE_EVENT_DOWN, Type::kMouseDown},
      {CEF_MOUSE_EVENT_UP, Type::kMouseUp},
      {CEF_MOUSE_EVENT_MOVE, Type::kMouseMove},
      {CEF_MOUSE_EVENT_ENTER, Type::kMouseEnter},
      {CEF_MOUSE_EVENT_LEAVE, Type::kMouseLeave},
      {CEF_MOUSE_EVENT_CONTEXT_MENU, Type::kContextMenu},
  };
  for (const auto& type : mouse_types) {
    CefMouseInputEvent event;
    event.type = type.cef;
    blink::WebMouseEvent web;
    ASSERT_TRUE(GetWebMouseInputEvent(event, web));
    EXPECT_EQ(type.web, web.GetType());
    web.SetType(type.web);
    ASSERT_TRUE(GetCefMouseInputEvent(web, event));
    EXPECT_EQ(type.cef, event.type);
  }
  constexpr struct {
    cef_gesture_input_event_type_t cef;
    Type web;
  } gesture_types[] = {
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
  for (const auto& type : gesture_types) {
    CefGestureInputEvent event;
    event.type = type.cef;
    event.source_device = CEF_GESTURE_DEVICE_TOUCHSCREEN;
    event.scale = 1;
    blink::WebGestureEvent web;
    ASSERT_TRUE(GetWebGestureInputEvent(event, web));
    EXPECT_EQ(type.web, web.GetType());
    web.SetType(type.web);
    ASSERT_TRUE(GetCefGestureInputEvent(web, event));
    EXPECT_EQ(type.cef, event.type);
  }
}

TEST(MouseGestureInputEventUtil, IndividualModifiers) {
  constexpr struct {
    uint32_t cef;
    int web;
  } modifiers[] = {
      {CEF_EVENTFLAG_CAPS_LOCK_ON, blink::WebInputEvent::kCapsLockOn},
      {CEF_EVENTFLAG_SHIFT_DOWN, blink::WebInputEvent::kShiftKey},
      {CEF_EVENTFLAG_CONTROL_DOWN, blink::WebInputEvent::kControlKey},
      {CEF_EVENTFLAG_ALT_DOWN, blink::WebInputEvent::kAltKey},
      {CEF_EVENTFLAG_LEFT_MOUSE_BUTTON, blink::WebInputEvent::kLeftButtonDown},
      {CEF_EVENTFLAG_MIDDLE_MOUSE_BUTTON,
       blink::WebInputEvent::kMiddleButtonDown},
      {CEF_EVENTFLAG_RIGHT_MOUSE_BUTTON,
       blink::WebInputEvent::kRightButtonDown},
      {CEF_EVENTFLAG_COMMAND_DOWN, blink::WebInputEvent::kMetaKey},
      {CEF_EVENTFLAG_NUM_LOCK_ON, blink::WebInputEvent::kNumLockOn},
      {CEF_EVENTFLAG_IS_KEY_PAD, blink::WebInputEvent::kIsKeyPad},
      {CEF_EVENTFLAG_IS_LEFT, blink::WebInputEvent::kIsLeft},
      {CEF_EVENTFLAG_IS_RIGHT, blink::WebInputEvent::kIsRight},
      {CEF_EVENTFLAG_ALTGR_DOWN, blink::WebInputEvent::kAltGrKey},
      {CEF_EVENTFLAG_IS_REPEAT, blink::WebInputEvent::kIsAutoRepeat},
      {CEF_EVENTFLAG_BACK_MOUSE_BUTTON, blink::WebInputEvent::kBackButtonDown},
      {CEF_EVENTFLAG_FORWARD_MOUSE_BUTTON,
       blink::WebInputEvent::kForwardButtonDown},
      {CEF_EVENTFLAG_TOUCH_ACCESSIBILITY,
       blink::WebInputEvent::kIsTouchAccessibility},
      {CEF_EVENTFLAG_FROM_TOUCH,
       blink::WebInputEvent::kIsCompatibilityEventForTouch},
      {CEF_EVENTFLAG_RELATIVE_MOTION,
       blink::WebInputEvent::kRelativeMotionEvent},
      {CEF_EVENTFLAG_FN_DOWN, blink::WebInputEvent::kFnKey},
      {CEF_EVENTFLAG_SYMBOL_DOWN, blink::WebInputEvent::kSymbolKey},
      {CEF_EVENTFLAG_SCROLL_LOCK_ON, blink::WebInputEvent::kScrollLockOn},
  };
  uint32_t allowed = 0;
  for (const auto& modifier : modifiers) {
    SCOPED_TRACE(modifier.cef);
    allowed |= modifier.cef;
    CefMouseInputEvent mouse;
    mouse.type = CEF_MOUSE_EVENT_MOVE;
    mouse.modifiers = modifier.cef;
    blink::WebMouseEvent web_mouse;
    ASSERT_TRUE(GetWebMouseInputEvent(mouse, web_mouse));
    EXPECT_EQ(modifier.web, web_mouse.GetModifiers());
    ASSERT_TRUE(GetCefMouseInputEvent(web_mouse, mouse));
    EXPECT_EQ(modifier.cef, mouse.modifiers);

    CefGestureInputEvent gesture;
    gesture.type = CEF_GESTURE_EVENT_TAP;
    gesture.source_device = CEF_GESTURE_DEVICE_TOUCHSCREEN;
    gesture.modifiers = modifier.cef;
    blink::WebGestureEvent web_gesture;
    ASSERT_TRUE(GetWebGestureInputEvent(gesture, web_gesture));
    EXPECT_EQ(modifier.web, web_gesture.GetModifiers());
    ASSERT_TRUE(GetCefGestureInputEvent(web_gesture, gesture));
    EXPECT_EQ(modifier.cef, gesture.modifiers);
    CefWheelInputEvent wheel;
    wheel.modifiers = modifier.cef;
    blink::WebMouseWheelEvent web_wheel;
    ASSERT_TRUE(GetWebWheelInputEvent(wheel, web_wheel));
    EXPECT_EQ(modifier.web, web_wheel.GetModifiers());
  }
  // Reject every unexposed bit, including wheel-only CEF flags.
  for (unsigned bit = 0; bit < 32; ++bit) {
    const uint32_t modifier = 1U << bit;
    if (allowed & modifier) {
      continue;
    }
    CefMouseInputEvent mouse;
    mouse.type = CEF_MOUSE_EVENT_MOVE;
    mouse.modifiers = modifier;
    blink::WebMouseEvent web_mouse;
    EXPECT_FALSE(GetWebMouseInputEvent(mouse, web_mouse));
    CefGestureInputEvent gesture;
    gesture.type = CEF_GESTURE_EVENT_TAP;
    gesture.source_device = CEF_GESTURE_DEVICE_TOUCHSCREEN;
    gesture.modifiers = modifier;
    blink::WebGestureEvent web_gesture;
    EXPECT_FALSE(GetWebGestureInputEvent(gesture, web_gesture));
    CefWheelInputEvent wheel;
    wheel.modifiers = modifier;
    blink::WebMouseWheelEvent web_wheel;
    EXPECT_FALSE(GetWebWheelInputEvent(wheel, web_wheel));
  }
}

TEST(MouseInputEventUtil, ValidationBoundaries) {
  CefMouseInputEvent valid;
  valid.type = CEF_MOUSE_EVENT_MOVE;
  valid.button = CEF_MOUSE_BUTTON_NONE;
  auto reject = [](const CefMouseInputEvent& event) {
    blink::WebMouseEvent output(Type::kMouseUp, 0,
                                base::TimeTicks::FromInternalValue(47));
    output.click_count = 23;
    EXPECT_FALSE(GetWebMouseInputEvent(event, output));
    EXPECT_EQ(Type::kMouseUp, output.GetType());
    EXPECT_EQ(47, output.TimeStamp().ToInternalValue());
    EXPECT_EQ(23, output.click_count);
  };
  for (auto member :
       {&cef_mouse_input_event_t::x, &cef_mouse_input_event_t::y,
        &cef_mouse_input_event_t::screen_x, &cef_mouse_input_event_t::screen_y,
        &cef_mouse_input_event_t::tangential_pressure}) {
    for (float value : {std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()}) {
      CefMouseInputEvent event(valid);
      event.*member = value;
      reject(event);
    }
  }
  for (auto member :
       {&cef_mouse_input_event_t::tilt_x, &cef_mouse_input_event_t::tilt_y}) {
    for (double value :
         {-90.01, 90.01, std::numeric_limits<double>::quiet_NaN(),
          std::numeric_limits<double>::infinity()}) {
      CefMouseInputEvent event(valid);
      event.*member = value;
      reject(event);
    }
  }
  for (float value : {-0.01f, 1.01f, std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity()}) {
    CefMouseInputEvent event(valid);
    event.pressure = value;
    reject(event);
  }
  for (float value : {-1.01f, 1.01f}) {
    CefMouseInputEvent event(valid);
    event.tangential_pressure = value;
    reject(event);
  }
  for (int value : {-1, 2}) {
    CefMouseInputEvent event(valid);
    event.is_raw_movement_event = value;
    reject(event);
  }
  for (int value : {-1, 11}) {
    CefMouseInputEvent event(valid);
    event.menu_source_type = value;
    reject(event);
  }
  for (int value : {-1, 360}) {
    CefMouseInputEvent event(valid);
    event.twist = value;
    reject(event);
  }
  for (int value : {-2, 6}) {
    CefMouseInputEvent event(valid);
    event.button = static_cast<cef_mouse_button_type_t>(value);
    reject(event);
  }
  for (int value : {-1, 5}) {
    CefMouseInputEvent event(valid);
    event.pointer_type = static_cast<cef_pointer_type_t>(value);
    reject(event);
  }
  // Signed positions/movement and endpoint values are valid.
  for (bool upper : {false, true}) {
    CefMouseInputEvent event(valid);
    event.x = -12.5f;
    event.screen_y = -234.25f;
    event.pressure = upper ? 1 : 0;
    event.tangential_pressure = upper ? 1 : -1;
    event.tilt_x = upper ? 90 : -90;
    event.tilt_y = upper ? -90 : 90;
    event.twist = upper ? 359 : 0;
    event.menu_source_type = upper ? 10 : 0;
    event.is_raw_movement_event = upper;
    event.movement_x = std::numeric_limits<int32_t>::min();
    event.movement_y = std::numeric_limits<int32_t>::max();
    blink::WebMouseEvent output;
    EXPECT_TRUE(GetWebMouseInputEvent(event, output));
  }
}

TEST(GestureInputEventUtil, ValidationBoundaries) {
  CefGestureInputEvent valid;
  valid.type = CEF_GESTURE_EVENT_SCROLL_UPDATE;
  valid.source_device = CEF_GESTURE_DEVICE_TOUCHSCREEN;
  auto reject = [](const CefGestureInputEvent& event) {
    blink::WebGestureEvent output(Type::kGestureTap, 0,
                                  base::TimeTicks::FromInternalValue(47),
                                  blink::mojom::GestureDevice::kTouchpad);
    output.unique_touch_event_id = 23;
    EXPECT_FALSE(GetWebGestureInputEvent(event, output));
    EXPECT_EQ(Type::kGestureTap, output.GetType());
    EXPECT_EQ(47, output.TimeStamp().ToInternalValue());
    EXPECT_EQ(23U, output.unique_touch_event_id);
  };
  for (auto member :
       {&cef_gesture_input_event_t::x, &cef_gesture_input_event_t::y,
        &cef_gesture_input_event_t::screen_x,
        &cef_gesture_input_event_t::screen_y, &cef_gesture_input_event_t::width,
        &cef_gesture_input_event_t::height, &cef_gesture_input_event_t::delta_x,
        &cef_gesture_input_event_t::delta_y,
        &cef_gesture_input_event_t::delta_x_unconstrained,
        &cef_gesture_input_event_t::delta_y_unconstrained,
        &cef_gesture_input_event_t::scale,
        &cef_gesture_input_event_t::velocity_x,
        &cef_gesture_input_event_t::velocity_y}) {
    for (float value : {std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()}) {
      CefGestureInputEvent event(valid);
      event.*member = value;
      reject(event);
    }
  }
  for (auto member :
       {&cef_gesture_input_event_t::is_source_touch_event_set_blocking,
        &cef_gesture_input_event_t::target_viewport,
        &cef_gesture_input_event_t::prevent_boosting,
        &cef_gesture_input_event_t::needs_wheel_event,
        &cef_gesture_input_event_t::zoom_disabled,
        &cef_gesture_input_event_t::synthetic,
        &cef_gesture_input_event_t::cursor_control}) {
    for (int value : {-1, 2}) {
      CefGestureInputEvent event(valid);
      event.*member = value;
      reject(event);
    }
  }
  for (auto member : {&cef_gesture_input_event_t::tap_count,
                      &cef_gesture_input_event_t::pointer_count}) {
    CefGestureInputEvent event(valid);
    event.*member = -1;
    reject(event);
  }
  for (auto member : {&cef_gesture_input_event_t::width,
                      &cef_gesture_input_event_t::height}) {
    CefGestureInputEvent event(valid);
    event.*member = -0.01f;
    reject(event);
  }
  for (int value : {-1, 3}) {
    CefGestureInputEvent event(valid);
    event.inertial_phase = value;
    reject(event);
  }
  for (int value : {-1, 5}) {
    CefGestureInputEvent event(valid);
    event.delta_units = static_cast<cef_gesture_scroll_units_t>(value);
    reject(event);
    event = valid;
    event.source_device = static_cast<cef_gesture_device_t>(value);
    reject(event);
    event = valid;
    event.primary_pointer_type = static_cast<cef_pointer_type_t>(value);
    reject(event);
  }
  for (float value : {-1.0f, 0.0f}) {
    CefGestureInputEvent event(valid);
    event.type = CEF_GESTURE_EVENT_PINCH_UPDATE;
    event.scale = value;
    reject(event);
  }
  // Exercise every accepted source, granularity and momentum enum value.
  for (auto source :
       {CEF_GESTURE_DEVICE_TOUCHPAD, CEF_GESTURE_DEVICE_TOUCHSCREEN,
        CEF_GESTURE_DEVICE_SYNTHETIC_AUTOSCROLL,
        CEF_GESTURE_DEVICE_SCROLLBAR}) {
    for (auto units : {CEF_GESTURE_SCROLL_PRECISE_PIXEL,
                       CEF_GESTURE_SCROLL_PIXEL, CEF_GESTURE_SCROLL_LINE,
                       CEF_GESTURE_SCROLL_PAGE, CEF_GESTURE_SCROLL_DOCUMENT}) {
      for (int phase : {0, 1, 2}) {
        CefGestureInputEvent event(valid);
        event.source_device = source;
        event.delta_units = units;
        event.inertial_phase = phase;
        event.delta_x = -12.5f;
        event.delta_y = 17.25f;
        blink::WebGestureEvent output;
        ASSERT_TRUE(GetWebGestureInputEvent(event, output));
        EXPECT_EQ(static_cast<int>(source),
                  static_cast<int>(output.SourceDevice()));
        EXPECT_EQ(static_cast<int>(units),
                  static_cast<int>(output.data.scroll_update.delta_units));
        EXPECT_EQ(phase,
                  static_cast<int>(output.data.scroll_update.inertial_phase));
      }
    }
  }
  // Wheel-first dispatch is valid only for touchpad pinch/double-tap.
  for (auto type :
       {CEF_GESTURE_EVENT_PINCH_BEGIN, CEF_GESTURE_EVENT_PINCH_UPDATE,
        CEF_GESTURE_EVENT_PINCH_END, CEF_GESTURE_EVENT_DOUBLE_TAP}) {
    CefGestureInputEvent event(valid);
    event.type = type;
    event.source_device = CEF_GESTURE_DEVICE_TOUCHPAD;
    event.scale = 1.25f;
    event.needs_wheel_event = 1;
    blink::WebGestureEvent output;
    ASSERT_TRUE(GetWebGestureInputEvent(event, output));
    event.source_device = CEF_GESTURE_DEVICE_TOUCHSCREEN;
    reject(event);
  }
}

TEST(MouseGestureInputEventUtil, VersionedStructCopies) {
  struct Older {
    size_t size;
    cef_mouse_input_event_type_t type;
    uint32_t modifiers;
    int64_t timestamp;
  } older = {sizeof(Older), CEF_MOUSE_EVENT_UP, CEF_EVENTFLAG_BACK_MOUSE_BUTTON,
             123456789};
  CefMouseInputEvent mouse;
  mouse.click_count = 47;
  mouse.Set(*reinterpret_cast<const cef_mouse_input_event_t*>(&older), true);
  EXPECT_EQ(sizeof(cef_mouse_input_event_t), mouse.size);
  EXPECT_EQ(CEF_MOUSE_EVENT_UP, mouse.type);
  EXPECT_EQ(CEF_EVENTFLAG_BACK_MOUSE_BUTTON, mouse.modifiers);
  EXPECT_EQ(0, mouse.click_count);
  struct Newer {
    cef_gesture_input_event_t event;
    uint64_t future;
  } newer = {};
  newer.event.size = sizeof(newer);
  newer.event.type = CEF_GESTURE_EVENT_FLING_CANCEL;
  newer.event.prevent_boosting = 1;
  newer.future = 0xdeadbeef;
  CefGestureInputEvent gesture(newer.event);
  EXPECT_EQ(sizeof(cef_gesture_input_event_t), gesture.size);
  EXPECT_EQ(1, gesture.prevent_boosting);
  gesture.AttachTo(newer.event);
  gesture.prevent_boosting = 0;
  gesture.DetachTo(newer.event);
  EXPECT_EQ(0, newer.event.prevent_boosting);
  EXPECT_EQ(0xdeadbeefU, newer.future);
}
