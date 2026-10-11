// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "base/check.h"
#include "cef/libcef/browser/key_event_util.h"
#include "components/input/native_web_keyboard_event.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if BUILDFLAG(IS_WIN)
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/events/keycodes/platform_key_map_win.h"
#else
#include "ui/base/ozone_buildflags.h"
#include "ui/events/keycodes/keysym_to_unicode.h"
#if BUILDFLAG(SUPPORTS_OZONE_X11)
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/keycodes/keyboard_code_conversion_xkb.h"
#endif
#endif

int GetLegacyUiEventModifiers(uint32_t cef_modifiers) {
  int result = 0;
  // Set modifiers based on key state.
  if (cef_modifiers & EVENTFLAG_CAPS_LOCK_ON) {
    result |= ui::EF_CAPS_LOCK_ON;
  }
  if (cef_modifiers & EVENTFLAG_SHIFT_DOWN) {
    result |= ui::EF_SHIFT_DOWN;
  }
  if (cef_modifiers & EVENTFLAG_CONTROL_DOWN) {
    result |= ui::EF_CONTROL_DOWN;
  }
  if (cef_modifiers & EVENTFLAG_ALT_DOWN) {
    result |= ui::EF_ALT_DOWN;
  }
  if (cef_modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON) {
    result |= ui::EF_LEFT_MOUSE_BUTTON;
  }
  if (cef_modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON) {
    result |= ui::EF_MIDDLE_MOUSE_BUTTON;
  }
  if (cef_modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON) {
    result |= ui::EF_RIGHT_MOUSE_BUTTON;
  }
  if (cef_modifiers & EVENTFLAG_COMMAND_DOWN) {
    result |= ui::EF_COMMAND_DOWN;
  }
  if (cef_modifiers & EVENTFLAG_NUM_LOCK_ON) {
    result |= ui::EF_NUM_LOCK_ON;
  }
  if (cef_modifiers & EVENTFLAG_IS_KEY_PAD) {
    result |= ui::EF_IS_EXTENDED_KEY;
  }
  if (cef_modifiers & EVENTFLAG_ALTGR_DOWN) {
    result |= ui::EF_ALTGR_DOWN;
  }
  if (cef_modifiers & EVENTFLAG_IS_REPEAT) {
    result |= ui::EF_IS_REPEAT;
  }
  if (cef_modifiers & EVENTFLAG_PRECISION_SCROLLING_DELTA) {
    result |= ui::EF_PRECISION_SCROLLING_DELTA;
  }
  if (cef_modifiers & EVENTFLAG_SCROLL_BY_PAGE) {
    result |= ui::EF_SCROLL_BY_PAGE;
  }

  return result;
}

ui::KeyEvent GetLegacyUiKeyEvent(const CefKeyEvent& key_event,
                                 base::TimeTicks time_stamp) {
  int flags = GetLegacyUiEventModifiers(key_event.modifiers);

#if BUILDFLAG(IS_WIN)

  ui::KeyboardCode key_code =
      ui::KeyboardCodeForWindowsKeyCode(key_event.windows_key_code);
  char16_t character = key_event.windows_key_code;

#else  // BUILDFLAG(IS_WIN)

  ui::KeyboardCode key_code =
      static_cast<ui::KeyboardCode>(key_event.windows_key_code);

#if BUILDFLAG(SUPPORTS_OZONE_X11)
  // Intentionally retained legacy behavior: derive the character from
  // |windows_key_code| and ignore |character|. Legacy Linux CHAR events must
  // carry the key-down's Windows key code. The portable API has no such limit.
  int keysym = ui::XKeysymForWindowsKeyCode(
      key_code, !!(key_event.modifiers & EVENTFLAG_SHIFT_DOWN));
  char16_t character = ui::GetUnicodeCharacterFromXKeySym(keysym);
#else
  char16_t character = key_event.character;
#endif

#endif  // BUILDFLAG(IS_WIN)

  ui::DomCode dom_code =
      ui::KeycodeConverter::NativeKeycodeToDomCode(key_event.native_key_code);

  if (key_event.type == KEYEVENT_CHAR) {
    return ui::KeyEvent::FromCharacter(character, key_code, dom_code, flags,
                                       time_stamp);
  }

  ui::EventType type = ui::EventType::kUnknown;
  switch (key_event.type) {
    case KEYEVENT_RAWKEYDOWN:
    case KEYEVENT_KEYDOWN:
      type = ui::EventType::kKeyPressed;
      break;
    case KEYEVENT_KEYUP:
      type = ui::EventType::kKeyReleased;
      break;
    default:
      DCHECK(false);
  }

#if BUILDFLAG(IS_WIN)
  ui::DomKey dom_key =
      ui::PlatformKeyMap::DomKeyFromKeyboardCode(key_code, &flags);
#elif BUILDFLAG(SUPPORTS_OZONE_X11)
  ui::DomKey dom_key = ui::XKeySymToDomKey(keysym, character);
#else
  ui::DomKey dom_key = ui::DomKey::NONE;
#endif

  return ui::KeyEvent(type, key_code, dom_code, flags, dom_key, time_stamp);
}

input::NativeWebKeyboardEvent GetLegacyWebKeyEvent(const CefKeyEvent& key_event,
                                                   base::TimeTicks timestamp) {
  ui::KeyEvent ui_event = GetLegacyUiKeyEvent(key_event, timestamp);
  if (key_event.type == KEYEVENT_CHAR) {
    // Preserve the complete UTF-16 unit instead of narrowing it through a
    // Windows KeyboardCode (see #4282). Linux legacy callers supply character.
#if BUILDFLAG(IS_WIN)
    return input::NativeWebKeyboardEvent(
        ui_event, static_cast<char16_t>(key_event.windows_key_code));
#else
    return input::NativeWebKeyboardEvent(ui_event, key_event.character);
#endif
  }
  return input::NativeWebKeyboardEvent(ui_event);
}
