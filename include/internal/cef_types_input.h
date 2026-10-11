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
#if CEF_API_ADDED(CEF_NEXT)

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

#endif  // CEF_API_ADDED(CEF_NEXT)

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
#if CEF_API_ADDED(CEF_NEXT)

  ///
  /// Notification that a key transitioned from "up" to "down".
  ///
  CEF_KEYEVENT_RAWKEYDOWN = 0,

  ///
  /// Notification that a key was pressed. This does not necessarily correspond
  /// to a character depending on the key and language. Use CEF_KEYEVENT_CHAR
  /// for character input.
  ///
  CEF_KEYEVENT_KEYDOWN = 1,

  ///
  /// Notification that a key was released.
  ///
  CEF_KEYEVENT_KEYUP = 2,

  ///
  /// Notification that a character was typed. Use this for text input. Key
  /// down events may generate 0, 1, or more than one character event depending
  /// on the key, locale, and operating system.
  ///
  CEF_KEYEVENT_CHAR = 3,

#endif  // CEF_API_ADDED(CEF_NEXT)

  ///
  /// Legacy names. Values match the corresponding CEF_KEYEVENT_* constants.
  /// Do not add new values here; add them to the CEF_KEYEVENT_* set above.
  ///
  KEYEVENT_RAWKEYDOWN = 0,
  KEYEVENT_KEYDOWN,
  KEYEVENT_KEYUP,
  KEYEVENT_CHAR
} cef_key_event_type_t;

#if CEF_API_ADDED(CEF_NEXT)

///
/// Portable keyboard snapshot. Identity and text are resolved by the sender;
/// sending does not look up the receiving keyboard layout. DOM code/key use
/// Chromium's USB-based DomCode and encoded DomKey values, respectively.
///
typedef struct _cef_keyboard_input_event_t {
  size_t size;
  cef_key_event_type_t type;
  /// cef_event_flags_t CEF_EVENTFLAG_* values, including location and repeat.
  /// CEF_EVENTFLAG_PRECISION_SCROLLING_DELTA and CEF_EVENTFLAG_SCROLL_BY_PAGE
  /// are not valid keyboard modifiers.
  uint32_t modifiers;
  /// Monotonic microseconds in this process. Zero means use the send time.
  int64_t timestamp;
  uint32_t dom_code;
  uint32_t dom_key;
  int windows_key_code;
  int is_system_key;
  /// Blink-compatible UTF-16 buffers. Each buffer must contain a zero
  /// terminator within its four units, allowing at most three units of text.
  /// Preserve all four units, including any after the terminator. Windows CHAR
  /// messages may each contain one surrogate unit; conversion does not buffer
  /// across messages.
  char16_t text[4];
  /// Text with modifiers other than Shift removed, following Blink's native
  /// conversion semantics. Preserve this source buffer when replacing |text|
  /// with insertion text; do not copy modified insertion text into this field.
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

#endif  // CEF_API_ADDED(CEF_NEXT)

#ifdef __cplusplus
}
#endif

#endif  // CEF_INCLUDE_INTERNAL_CEF_TYPES_INPUT_H_
