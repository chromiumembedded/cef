// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_INCLUDE_INTERNAL_CEF_TYPES_INPUT_H_
#define CEF_INCLUDE_INTERNAL_CEF_TYPES_INPUT_H_
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "include/cef_api_hash.h"
#include "include/internal/cef_types_keyboard_codes.h"

// Bring in platform-specific event handle definitions.
#if defined(OS_WIN)
#include "include/internal/cef_types_win.h"
#elif defined(OS_MAC)
#include "include/internal/cef_types_mac.h"
#elif defined(OS_LINUX)
#include "include/internal/cef_types_linux.h"
#endif  // defined(OS_LINUX)

#ifdef __cplusplus
extern "C" {
#endif

///
/// Supported event bit flags.
///
typedef enum {
#if CEF_API_ADDED(15800)

  CEF_EVENTFLAG_NONE = 0,
  CEF_EVENTFLAG_CAPS_LOCK_ON = 1 << 0,
  CEF_EVENTFLAG_SHIFT_DOWN = 1 << 1,
  CEF_EVENTFLAG_CONTROL_DOWN = 1 << 2,
  CEF_EVENTFLAG_ALT_DOWN = 1 << 3,
  CEF_EVENTFLAG_LEFT_MOUSE_BUTTON = 1 << 4,
  CEF_EVENTFLAG_MIDDLE_MOUSE_BUTTON = 1 << 5,
  CEF_EVENTFLAG_RIGHT_MOUSE_BUTTON = 1 << 6,
  /// macOS Command key.
  CEF_EVENTFLAG_COMMAND_DOWN = 1 << 7,
  CEF_EVENTFLAG_NUM_LOCK_ON = 1 << 8,
  CEF_EVENTFLAG_IS_KEY_PAD = 1 << 9,
  CEF_EVENTFLAG_IS_LEFT = 1 << 10,
  CEF_EVENTFLAG_IS_RIGHT = 1 << 11,
  CEF_EVENTFLAG_ALTGR_DOWN = 1 << 12,
  CEF_EVENTFLAG_IS_REPEAT = 1 << 13,
  CEF_EVENTFLAG_PRECISION_SCROLLING_DELTA = 1 << 14,
  CEF_EVENTFLAG_SCROLL_BY_PAGE = 1 << 15,
  CEF_EVENTFLAG_BACK_MOUSE_BUTTON = 1 << 16,
  CEF_EVENTFLAG_FORWARD_MOUSE_BUTTON = 1 << 17,
  CEF_EVENTFLAG_TOUCH_ACCESSIBILITY = 1 << 18,
  CEF_EVENTFLAG_FROM_TOUCH = 1 << 19,
  CEF_EVENTFLAG_RELATIVE_MOTION = 1 << 20,
  CEF_EVENTFLAG_FN_DOWN = 1 << 21,
  CEF_EVENTFLAG_SYMBOL_DOWN = 1 << 22,
  CEF_EVENTFLAG_SCROLL_LOCK_ON = 1 << 23,

#endif  // CEF_API_ADDED(15800)

  ///
  /// Legacy names. Values match the corresponding CEF_EVENTFLAG_* constants.
  /// Do not add new values here; add them to the CEF_EVENTFLAG_* set above.
  ///
  EVENTFLAG_NONE = 0,
  EVENTFLAG_CAPS_LOCK_ON = 1 << 0,
  EVENTFLAG_SHIFT_DOWN = 1 << 1,
  EVENTFLAG_CONTROL_DOWN = 1 << 2,
  EVENTFLAG_ALT_DOWN = 1 << 3,
  EVENTFLAG_LEFT_MOUSE_BUTTON = 1 << 4,
  EVENTFLAG_MIDDLE_MOUSE_BUTTON = 1 << 5,
  EVENTFLAG_RIGHT_MOUSE_BUTTON = 1 << 6,
  EVENTFLAG_COMMAND_DOWN = 1 << 7,
  EVENTFLAG_NUM_LOCK_ON = 1 << 8,
  EVENTFLAG_IS_KEY_PAD = 1 << 9,
  EVENTFLAG_IS_LEFT = 1 << 10,
  EVENTFLAG_IS_RIGHT = 1 << 11,
  EVENTFLAG_ALTGR_DOWN = 1 << 12,
  EVENTFLAG_IS_REPEAT = 1 << 13,
  EVENTFLAG_PRECISION_SCROLLING_DELTA = 1 << 14,
  EVENTFLAG_SCROLL_BY_PAGE = 1 << 15,
} cef_event_flags_t;

///
/// Key event types.
///
typedef enum {
#if CEF_API_ADDED(15800)

  ///
  /// Notification that a key transitioned from "up" to "down".
  ///
  CEF_KEY_EVENT_RAWKEYDOWN = 0,

  ///
  /// Notification that a key was pressed. This does not necessarily correspond
  /// to a character depending on the key and language. Use CEF_KEY_EVENT_CHAR
  /// for character input.
  ///
  CEF_KEY_EVENT_KEYDOWN = 1,

  ///
  /// Notification that a key was released.
  ///
  CEF_KEY_EVENT_KEYUP = 2,

  ///
  /// Notification that a character was typed. Use this for text input. Key
  /// down events may generate 0, 1, or more than one character event depending
  /// on the key, locale, and operating system.
  ///
  CEF_KEY_EVENT_CHAR = 3,

#endif  // CEF_API_ADDED(15800)

  ///
  /// Legacy names. Values match the corresponding CEF_KEY_EVENT_* constants.
  /// Do not add new values here; add them to the CEF_KEY_EVENT_* set above.
  ///
  KEYEVENT_RAWKEYDOWN = 0,
  KEYEVENT_KEYDOWN,
  KEYEVENT_KEYUP,
  KEYEVENT_CHAR
} cef_key_event_type_t;

///
/// Changed button for down/up, or a held button for movement.
///
typedef enum {
#if CEF_API_ADDED(15800)
  CEF_MOUSE_BUTTON_NONE = -1,
  CEF_MOUSE_BUTTON_LEFT = 0,
  CEF_MOUSE_BUTTON_MIDDLE,
  CEF_MOUSE_BUTTON_RIGHT,
  CEF_MOUSE_BUTTON_BACK,
  CEF_MOUSE_BUTTON_FORWARD,
  CEF_MOUSE_BUTTON_ERASER,
#endif  // CEF_API_ADDED(15800)

  ///
  /// Legacy names. Values match the corresponding CEF_MOUSE_BUTTON_* constants.
  /// Do not add new values here; add them to the CEF_MOUSE_BUTTON_* set above.
  ///
  MBT_LEFT = 0,
  MBT_MIDDLE,
  MBT_RIGHT,
} cef_mouse_button_type_t;

///
/// The device type that caused the event.
///
typedef enum {
  CEF_POINTER_TYPE_TOUCH = 0,
  CEF_POINTER_TYPE_MOUSE,
  CEF_POINTER_TYPE_PEN,
  CEF_POINTER_TYPE_ERASER,
  CEF_POINTER_TYPE_UNKNOWN
} cef_pointer_type_t;

#if CEF_API_ADDED(15800)

///
/// Mouse event types. Wheel input is a separate event family.
///
typedef enum {
  CEF_MOUSE_EVENT_DOWN = 0,
  CEF_MOUSE_EVENT_UP,
  CEF_MOUSE_EVENT_MOVE,
  CEF_MOUSE_EVENT_ENTER,
  CEF_MOUSE_EVENT_LEAVE,
  CEF_MOUSE_EVENT_CONTEXT_MENU,
} cef_mouse_input_event_type_t;

///
/// Portable mouse snapshot. Positions are floating point DIPs relative to the
/// target widget and screen, respectively. Values retain Blink semantics.
/// Event names in member descriptions refer to CEF_MOUSE_EVENT_* values.
///
typedef struct _cef_mouse_input_event_t {
  ///
  /// Size of this structure. Initialize to sizeof(cef_mouse_input_event_t).
  ///
  size_t size;

  ///
  /// Mouse event type. See the CEF_MOUSE_EVENT_* constants.
  ///
  cef_mouse_input_event_type_t type;

  ///
  /// Combination of cef_event_flags_t CEF_EVENTFLAG_* values for keyboard
  /// modifiers, lock state, held mouse buttons, and event-source attributes.
  /// Held-button flags describe the state after a down/up transition; |button|
  /// identifies the button that changed. Precision-scrolling and
  /// scroll-by-page flags are not valid for mouse input.
  ///
  uint32_t modifiers;

  ///
  /// Monotonic microseconds in this process. Zero means use the send time.
  /// Negative values are invalid. This is a process-local monotonic timestamp,
  /// not a wall-clock time.
  ///
  int64_t timestamp;

  ///
  /// Horizontal position in DIPs. When sending, relative to the left edge of
  /// the browser view; in callbacks, relative to the receiving widget after
  /// input routing.
  ///
  float x;

  ///
  /// Vertical position in DIPs. When sending, relative to the top edge of the
  /// browser view; in callbacks, relative to the receiving widget after input
  /// routing.
  ///
  float y;

  ///
  /// Horizontal screen position in DIPs, relative to the desktop origin.
  ///
  float screen_x;

  ///
  /// Vertical screen position in DIPs, relative to the desktop origin.
  ///
  float screen_y;

  ///
  /// Changed button for DOWN/UP events, or a held button for movement events.
  /// Use CEF_MOUSE_BUTTON_NONE when no button applies. If several buttons are
  /// held, this field identifies one of them; |modifiers| contains the
  /// complete held-button state.
  ///
  cef_mouse_button_type_t button;

  ///
  /// Number of consecutive clicks associated with the event: 1 for a single
  /// click, 2 for a double click, and so on. Must be nonnegative. Use zero for
  /// events without a click count.
  ///
  int32_t click_count;

  ///
  /// Device classification of the pointer, such as CEF_POINTER_TYPE_MOUSE or
  /// CEF_POINTER_TYPE_PEN. Use CEF_POINTER_TYPE_UNKNOWN when unavailable.
  ///
  cef_pointer_type_t pointer_type;

  ///
  /// Identifier of the pointer that produced the event. Preserve it for
  /// related events from the same pointer. Blink uses INT32_MAX for the
  /// ordinary mouse pointer.
  ///
  int32_t pointer_id;

  ///
  /// Normalized contact pressure in [0,1], or NaN if unsupported by the
  /// device. Zero represents no pressure and one represents maximum pressure.
  ///
  float pressure;

  ///
  /// Stylus tilt from the surface normal in the X-Z plane, in degrees in
  /// [-90,90]. Positive values tilt to the right. Use zero when unsupported.
  ///
  double tilt_x;

  ///
  /// Stylus tilt from the surface normal in the Y-Z plane, in degrees in
  /// [-90,90]. Positive values tilt towards the user. Use zero when
  /// unsupported.
  ///
  double tilt_y;

  ///
  /// Normalized barrel pressure in [-1,1]. Zero is the neutral position and is
  /// also used when the device does not support this attribute.
  ///
  float tangential_pressure;

  ///
  /// Clockwise stylus rotation around its major axis, in degrees in [0,359].
  /// Use zero when unsupported.
  ///
  int32_t twist;

  ///
  /// Horizontal movement reported by the input source, preserved without
  /// conversion. Raw OS movement uses device units when
  /// |is_raw_movement_event| is 1. With that flag set to 0, Blink may derive
  /// DOM movement from position changes instead.
  ///
  int32_t movement_x;

  ///
  /// Vertical movement reported by the input source. Units and interpretation
  /// follow |movement_x| and |is_raw_movement_event|.
  ///
  int32_t movement_y;

  ///
  /// Set to 1 when |movement_x| and |movement_y| contain raw, unadjusted OS
  /// movement, or 0 otherwise. Raw movement is independent of the DIP
  /// positions in this structure.
  ///
  int32_t is_raw_movement_event;

  ///
  /// Identifier of the physical pen device on platforms that supply one. Use
  /// -1 when unavailable. This is separate from |pointer_id|, which identifies
  /// the pointer in the event stream.
  ///
  int32_t device_id;

  ///
  /// Source of a CONTEXT_MENU event, using Chromium MenuSourceType values: 0
  /// none, 1 mouse, 2 keyboard, 3 touch, 4 touch edit menu, 5 long press, 6
  /// long tap, 7 touch handle, 8 stylus, 9 adjust selection, 10 adjust
  /// selection reset. Use zero for other event types.
  ///
  int32_t menu_source_type;
} cef_mouse_input_event_t;

///
/// Gesture event types, including distinct show-press and short-press events.
///
typedef enum {
  CEF_GESTURE_EVENT_SCROLL_BEGIN = 0,
  CEF_GESTURE_EVENT_SCROLL_END,
  CEF_GESTURE_EVENT_SCROLL_UPDATE,
  CEF_GESTURE_EVENT_FLING_START,
  CEF_GESTURE_EVENT_FLING_CANCEL,
  CEF_GESTURE_EVENT_PINCH_BEGIN,
  CEF_GESTURE_EVENT_PINCH_END,
  CEF_GESTURE_EVENT_PINCH_UPDATE,
  CEF_GESTURE_EVENT_BEGIN,
  CEF_GESTURE_EVENT_TAP_DOWN,
  CEF_GESTURE_EVENT_SHOW_PRESS,
  CEF_GESTURE_EVENT_TAP,
  CEF_GESTURE_EVENT_TAP_CANCEL,
  CEF_GESTURE_EVENT_SHORT_PRESS,
  CEF_GESTURE_EVENT_LONG_PRESS,
  CEF_GESTURE_EVENT_LONG_TAP,
  CEF_GESTURE_EVENT_TWO_FINGER_TAP,
  CEF_GESTURE_EVENT_TAP_UNCONFIRMED,
  CEF_GESTURE_EVENT_DOUBLE_TAP,
  CEF_GESTURE_EVENT_END,
} cef_gesture_input_event_type_t;

///
/// Source device for a gesture.
///
typedef enum {
  CEF_GESTURE_DEVICE_UNINITIALIZED = 0,
  CEF_GESTURE_DEVICE_TOUCHPAD,
  CEF_GESTURE_DEVICE_TOUCHSCREEN,
  CEF_GESTURE_DEVICE_SYNTHETIC_AUTOSCROLL,
  CEF_GESTURE_DEVICE_SCROLLBAR,
} cef_gesture_device_t;

///
/// Gesture scrolling units, matching Chromium ScrollGranularity.
///
typedef enum {
  CEF_GESTURE_SCROLL_PRECISE_PIXEL = 0,
  CEF_GESTURE_SCROLL_PIXEL,
  CEF_GESTURE_SCROLL_LINE,
  CEF_GESTURE_SCROLL_PAGE,
  CEF_GESTURE_SCROLL_DOCUMENT,
} cef_gesture_scroll_units_t;

///
/// Portable gesture snapshot. Only the fields documented for |type| apply;
/// initialize other subtype fields to zero. Positions are widget/screen DIPs.
/// Exposed values retain their Blink representation and semantics.
/// Event names in member descriptions refer to CEF_GESTURE_EVENT_* values.
/// All floating-point members must be finite. Boolean members accept 0 or 1.
///
typedef struct _cef_gesture_input_event_t {
  ///
  /// Size of this structure. Initialize to sizeof(cef_gesture_input_event_t).
  ///
  size_t size;

  ///
  /// Gesture event type. See the CEF_GESTURE_EVENT_* constants. Determines
  /// which subtype-specific members apply.
  ///
  cef_gesture_input_event_type_t type;

  ///
  /// Combination of cef_event_flags_t CEF_EVENTFLAG_* values for keyboard
  /// modifiers, lock state, held mouse buttons, and event-source attributes.
  /// Precision-scrolling and scroll-by-page flags are not valid for gesture
  /// input; specify scrolling units with |delta_units|.
  ///
  uint32_t modifiers;

  ///
  /// Monotonic microseconds in this process. Zero means use the send time.
  /// Negative values are invalid. This is a process-local monotonic timestamp,
  /// not a wall-clock time.
  ///
  int64_t timestamp;

  ///
  /// Horizontal position in DIPs. When sending, relative to the left edge of
  /// the browser view; in callbacks, relative to the receiving widget after
  /// input routing.
  ///
  float x;

  ///
  /// Vertical position in DIPs. When sending, relative to the top edge of the
  /// browser view; in callbacks, relative to the receiving widget after input
  /// routing.
  ///
  float y;

  ///
  /// Horizontal screen position in DIPs, relative to the desktop origin.
  ///
  float screen_x;

  ///
  /// Vertical screen position in DIPs, relative to the desktop origin.
  ///
  float screen_y;

  ///
  /// Device or scrolling source that generated the gesture. Sending requires
  /// an initialized source, such as CEF_GESTURE_DEVICE_TOUCHPAD or
  /// CEF_GESTURE_DEVICE_TOUCHSCREEN.
  ///
  cef_gesture_device_t source_device;

  ///
  /// Device classification of the first touch point in the gesture sequence.
  /// Use CEF_POINTER_TYPE_UNKNOWN when the initiating pointer type is
  /// unavailable.
  ///
  cef_pointer_type_t primary_pointer_type;

  ///
  /// Correlation ID of the first touch event in the gesture sequence.
  /// Identifies the initiating touch event independently of
  /// |unique_touch_event_id|. Use zero when there is no associated touch
  /// event.
  ///
  uint32_t primary_unique_touch_event_id;

  ///
  /// For touchscreen gestures, correlation ID of the touch event that released
  /// this gesture from Chromium's touch-disposition filter. Zero is used when
  /// the gesture was not released through a touch event, including synthesized
  /// gestures, pinches, and gestures from other source devices.
  ///
  uint32_t unique_touch_event_id;

  ///
  /// Set to 1 if the source touch event was marked to block subsequent input,
  /// or 0 otherwise. Describes source touch dispatch policy independently of
  /// whether the gesture is consumed.
  ///
  int32_t is_source_touch_event_set_blocking;

  ///
  /// Tap count for TAP, TAP_UNCONFIRMED, and DOUBLE_TAP, or tap-down count for
  /// TAP_DOWN. Must be nonnegative. Set to zero for other event types.
  ///
  int32_t tap_count;

  ///
  /// Nonnegative horizontal extent of the contact area in DIPs. Applies to
  /// TAP, TAP_UNCONFIRMED, DOUBLE_TAP, TAP_DOWN, SHOW_PRESS, SHORT_PRESS,
  /// LONG_PRESS, and LONG_TAP. For TWO_FINGER_TAP, describes the first
  /// finger's contact area. Set to zero for other event types.
  ///
  float width;

  ///
  /// Nonnegative vertical extent of the contact area in DIPs. The event types
  /// and contact selection are the same as for |width|.
  ///
  float height;

  ///
  /// Horizontal scroll amount in |delta_units|. For SCROLL_BEGIN, the initial
  /// movement hint; for SCROLL_UPDATE, the scroll delta; for SCROLL_END, the
  /// latency-compensated delta not already sent as updates. Positive values
  /// scroll left. Set to zero for other event types.
  ///
  float delta_x;

  ///
  /// Vertical scroll amount in |delta_units|, with the same subtype meanings
  /// as |delta_x|. Positive values scroll up. Set to zero for other event
  /// types.
  ///
  float delta_y;

  ///
  /// Horizontal SCROLL_UPDATE delta before axis locking or snapping, in
  /// |delta_units| and with the same sign convention as |delta_x|. Set to zero
  /// for other event types.
  ///
  float delta_x_unconstrained;

  ///
  /// Vertical SCROLL_UPDATE delta before axis locking or snapping, in
  /// |delta_units| and with the same sign convention as |delta_y|. Set to zero
  /// for other event types.
  ///
  float delta_y_unconstrained;

  ///
  /// Granularity of scroll hints or deltas for SCROLL_BEGIN, SCROLL_UPDATE,
  /// and SCROLL_END, including unconstrained update deltas. Zero selects
  /// CEF_GESTURE_SCROLL_PRECISE_PIXEL. Set to zero for other event types.
  ///
  cef_gesture_scroll_units_t delta_units;

  ///
  /// Momentum phase for SCROLL_BEGIN, SCROLL_UPDATE, and SCROLL_END: 0
  /// unknown, 1 non-momentum, 2 momentum. Touchscreen scrolling uses zero and
  /// represents flinging with FLING_START. Set to zero for other event types.
  ///
  int32_t inertial_phase;

  ///
  /// Number of pointers down at SCROLL_BEGIN. Must be nonnegative. Set to zero
  /// for other event types.
  ///
  int32_t pointer_count;

  ///
  /// Relative scale factor for PINCH_UPDATE. Must be positive: 1 leaves the
  /// scale unchanged, values greater than 1 zoom in, and values less than 1
  /// zoom out. Set to zero for other event types.
  ///
  float scale;

  ///
  /// Initial horizontal fling velocity in pixels per second for FLING_START.
  /// Set to zero for other event types.
  ///
  float velocity_x;

  ///
  /// Initial vertical fling velocity in pixels per second for FLING_START. Set
  /// to zero for other event types.
  ///
  float velocity_y;

  ///
  /// Set to 1 to target the viewport, or 0 to use normal gesture targeting.
  /// Applies to SCROLL_BEGIN, FLING_START, and FLING_CANCEL. Scroll-begin and
  /// fling-start viewport targeting skip the normal hit test. Set to zero for
  /// other event types.
  ///
  int32_t target_viewport;

  ///
  /// For FLING_CANCEL, set to 1 to exclude this cancellation from fling
  /// boosting, or 0 for normal processing. Fling boosting combines successive
  /// flings into a continuation. Set to zero for other event types.
  ///
  int32_t prevent_boosting;

  ///
  /// For touchpad PINCH_BEGIN, PINCH_UPDATE, PINCH_END, and DOUBLE_TAP, set to
  /// 1 to offer the gesture as a wheel event first, or 0 to dispatch the
  /// gesture directly. Chromium may clear this flag and resubmit the gesture
  /// after the wheel acknowledgment. Set to zero for other devices and event
  /// types.
  ///
  int32_t needs_wheel_event;

  ///
  /// For touchpad PINCH_UPDATE, set to 1 to prevent page-scale changes while
  /// preserving wheel handling requested by |needs_wheel_event|, or 0 for
  /// normal zooming. Set to zero for other devices and event types.
  ///
  int32_t zoom_disabled;

  ///
  /// For SCROLL_BEGIN, set to 1 when generated from a mouse wheel or
  /// scrollbar. For SCROLL_END, set to 1 when generated from a wheel event
  /// with a synthetic phase. Use zero otherwise.
  ///
  int32_t synthetic;

  ///
  /// For SCROLL_BEGIN, set to 1 to use the entire scroll sequence for cursor
  /// control instead of scrolling, or 0 for normal scrolling. Set to zero for
  /// other event types.
  ///
  int32_t cursor_control;
} cef_gesture_input_event_t;

///
/// Portable keyboard snapshot. Identity and text are resolved by the sender;
/// sending does not look up the receiving keyboard layout. DOM code/key use
/// Chromium's USB-based DomCode and encoded DomKey values, respectively.
///
typedef struct _cef_keyboard_input_event_t {
  ///
  /// Size of this structure. Initialize to sizeof(cef_keyboard_input_event_t).
  ///
  size_t size;

  ///
  /// Keyboard event type. See the CEF_KEY_EVENT_* constants.
  ///
  cef_key_event_type_t type;

  ///
  /// Combination of cef_event_flags_t CEF_EVENTFLAG_* values for Shift,
  /// Control, Alt, Command, AltGr, Caps Lock, Num Lock, keypad/left/right
  /// location, repeat, and held left/middle/right mouse buttons.
  /// CEF_EVENTFLAG_PRECISION_SCROLLING_DELTA and CEF_EVENTFLAG_SCROLL_BY_PAGE
  /// are not valid keyboard modifiers. Other flags outside this keyboard
  /// subset are also invalid when sending.
  ///
  uint32_t modifiers;

  ///
  /// Monotonic microseconds in this process. Zero means use the send time.
  /// Negative values are invalid. This is a process-local monotonic timestamp,
  /// not a wall-clock time.
  ///
  int64_t timestamp;

  ///
  /// Physical key identity in Chromium's USB-based DomCode encoding. Use
  /// CEF_DOM_CODE_* values, or zero if unknown. Identifies the key's physical
  /// position independently of the active keyboard layout.
  ///
  uint32_t dom_code;

  ///
  /// Logical key meaning in Chromium's encoded DomKey representation. Use
  /// CEF_DOM_KEY_* named values, or encoded Unicode/dead-key values as defined
  /// by cef_dom_key_t. Zero indicates no known logical key value.
  ///
  uint32_t dom_key;

  ///
  /// Windows-equivalent virtual-key code used by Chromium on every platform,
  /// or zero if unknown. Modifier-key location is conveyed by |modifiers|.
  /// CefWindowsKeyCodeFromDomCodeAndKey can derive this value from portable
  /// key identity.
  ///
  int windows_key_code;

  ///
  /// Set to 1 for Chromium's system-key classification, or 0 otherwise. Native
  /// Windows conversion preserves the distinction between ordinary and system
  /// key messages; other platforms normally use zero.
  ///
  int is_system_key;

  ///
  /// Text generated by this event. Blink-compatible UTF-16 buffers. Each
  /// buffer must contain a zero terminator within its four units, allowing at
  /// most three units of text. Preserve all four units, including any after
  /// the terminator. Windows CHAR messages may each contain one surrogate
  /// unit; conversion does not buffer across messages. Text in RAWKEYDOWN is
  /// metadata; character insertion requires a separate CHAR event.
  ///
  char16_t text[4];

  ///
  /// Text with modifiers other than Shift removed, following Blink's native
  /// conversion semantics. Preserve this source buffer when replacing |text|
  /// with insertion text; do not copy modified insertion text into this field.
  /// Uses the same UTF-16 buffer format and termination requirements as
  /// |text|.
  ///
  char16_t unmodified_text[4];
} cef_keyboard_input_event_t;

///
/// Callback-only input information supplied by CEF. The native handle may be
/// null or refer to a synthesized event for injected input. It is borrowed for
/// the duration of the callback and must not be retained.
///
typedef struct _cef_input_event_context_t {
  size_t size;
  int focus_on_editable_field;
  cef_event_handle_t os_event;
} cef_input_event_context_t;

///
/// Scroll sequence phases. Values are flags and may be combined when the
/// native input source reports simultaneous transitions.
///
typedef enum {
  CEF_WHEEL_PHASE_NONE = 0,
  CEF_WHEEL_PHASE_BEGAN = 1 << 0,
  CEF_WHEEL_PHASE_STATIONARY = 1 << 1,
  CEF_WHEEL_PHASE_CHANGED = 1 << 2,
  CEF_WHEEL_PHASE_ENDED = 1 << 3,
  CEF_WHEEL_PHASE_CANCELLED = 1 << 4,
  CEF_WHEEL_PHASE_MAY_BEGIN = 1 << 5,
  CEF_WHEEL_PHASE_BLOCKED = 1 << 6,
} cef_wheel_event_phase_t;

///
/// Portable wheel snapshot for mouse wheels and trackpad scrolling. Positions
/// are in DIPs. Deltas and ticks are positive for scrolling up or left,
/// opposite to DOM WheelEvent deltas. Initialize unused fields to zero.
///
typedef struct _cef_wheel_input_event_t {
  ///
  /// Size of this structure. Initialize to sizeof(cef_wheel_input_event_t).
  ///
  size_t size;

  ///
  /// Combination of cef_event_flags_t CEF_EVENTFLAG_* keyboard modifiers, lock
  /// state, held mouse buttons and event-source attributes. Specify precision
  /// and page scrolling with |delta_units|, not modifier flags.
  ///
  uint32_t modifiers;

  ///
  /// Monotonic microseconds in this process. Zero uses the dispatch time.
  /// Negative values are invalid. This is not a wall-clock timestamp.
  ///
  int64_t timestamp;

  ///
  /// Horizontal position in DIPs relative to the left edge of the browser view.
  ///
  float x;

  ///
  /// Vertical position in DIPs relative to the top edge of the browser view.
  ///
  float y;

  ///
  /// Horizontal screen position in DIPs relative to the desktop origin.
  ///
  float screen_x;

  ///
  /// Vertical screen position in DIPs relative to the desktop origin.
  ///
  float screen_y;

  ///
  /// Horizontal scroll amount in |delta_units|. Preserve fractional values for
  /// high-resolution wheels and trackpads.
  ///
  float delta_x;

  ///
  /// Vertical scroll amount in |delta_units|.
  ///
  float delta_y;

  ///
  /// Horizontal wheel rotation in ticks, independent of |delta_x|. A typical
  /// wheel detent is one tick. Trackpads may report fractional ticks.
  ///
  float wheel_ticks_x;

  ///
  /// Vertical wheel rotation in ticks, independent of |delta_y|.
  ///
  float wheel_ticks_y;

  ///
  /// Scroll units, sharing the gesture granularity constants. Only
  /// CEF_GESTURE_SCROLL_PRECISE_PIXEL, CEF_GESTURE_SCROLL_PIXEL and
  /// CEF_GESTURE_SCROLL_PAGE are supported. Zero selects precise pixels.
  ///
  cef_gesture_scroll_units_t delta_units;

  ///
  /// Phase of direct scrolling. Use CEF_WHEEL_PHASE_NONE for a discrete mouse
  /// wheel or when phase information is unavailable. BLOCKED is not valid.
  ///
  cef_wheel_event_phase_t phase;

  ///
  /// Phase of inertial scrolling after contact ends. BLOCKED indicates that
  /// momentum will not follow. Use NONE when unavailable. Send coherent
  /// begin/update/end transitions when providing phase information.
  ///
  cef_wheel_event_phase_t momentum_phase;

  ///
  /// Axis constraint: 0 for free scrolling, 1 for horizontal, 2 for vertical.
  ///
  int32_t rails_mode;

  ///
  /// Horizontal acceleration ratio applied by the input source. Positive
  /// values are preserved; zero means unspecified and uses a ratio of one.
  ///
  float acceleration_ratio_x;

  ///
  /// Vertical acceleration ratio. Same semantics as |acceleration_ratio_x|.
  ///
  float acceleration_ratio_y;
} cef_wheel_input_event_t;

#endif  // CEF_API_ADDED(15800)

#ifdef __cplusplus
}
#endif

#endif  // CEF_INCLUDE_INTERNAL_CEF_TYPES_INPUT_H_
