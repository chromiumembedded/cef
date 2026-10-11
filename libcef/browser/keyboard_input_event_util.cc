// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <algorithm>

#include "base/time/time.h"
#include "cef/include/cef_input_event_utils.h"
#include "cef/libcef/browser/browser_event_util.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

int CefWindowsKeyCodeFromDomCodeAndKey(uint32_t dom_code, uint32_t dom_key) {
  const auto code = static_cast<ui::DomCode>(dom_code);
  const ui::DomKey key(dom_key);
  if ((dom_code && ui::KeycodeConverter::UsbKeycodeToDomCode(dom_code) ==
                       ui::DomCode::NONE) ||
      (dom_key && !key.IsValid())) {
    return ui::VKEY_UNKNOWN;
  }
  auto key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
  if (key_code != ui::VKEY_UNKNOWN) {
    key_code = ui::NonLocatedToLocatedKeyboardCode(key_code, code);
  } else {
    const auto physical = ui::DomCodeToUsLayoutKeyboardCode(code);
    const bool keypad = ui::KeycodeConverter::DomCodeToLocation(code) ==
                        ui::DomKeyLocation::NUMPAD;
    if (key.IsCharacter() && key.ToCharacter() <= 0xffff &&
        !(keypad && physical != ui::VKEY_UNKNOWN)) {
      key_code =
          ui::DomCodeToUsLayoutKeyboardCode(ui::UsLayoutDomKeyToDomCode(key));
    }
    if (key_code == ui::VKEY_UNKNOWN) {
      key_code = physical;
    }
  }
  // Chromium's general non-location conversion also collapses keypad digits
  // and Meta sides. Keep those identities, normalizing only these modifiers.
  switch (key_code) {
    case ui::VKEY_LSHIFT:
    case ui::VKEY_RSHIFT:
    case ui::VKEY_LCONTROL:
    case ui::VKEY_RCONTROL:
    case ui::VKEY_LMENU:
    case ui::VKEY_RMENU:
      return ui::LocatedToNonLocatedKeyboardCode(key_code);
    default:
      return key_code;
  }
}

namespace {

bool GetCefKeyEventType(blink::WebInputEvent::Type type,
                        cef_key_event_type_t& cef_type) {
  switch (type) {
    case blink::WebInputEvent::Type::kRawKeyDown:
      cef_type = CEF_KEYEVENT_RAWKEYDOWN;
      break;
    case blink::WebInputEvent::Type::kKeyDown:
      cef_type = CEF_KEYEVENT_KEYDOWN;
      break;
    case blink::WebInputEvent::Type::kKeyUp:
      cef_type = CEF_KEYEVENT_KEYUP;
      break;
    case blink::WebInputEvent::Type::kChar:
      cef_type = CEF_KEYEVENT_CHAR;
      break;
    default:
      return false;
  }
  return true;
}

constexpr struct {
  int web;
  uint32_t cef;
} kModifierMapping[] = {
    {blink::WebInputEvent::kShiftKey, CEF_EVENTFLAG_SHIFT_DOWN},
    {blink::WebInputEvent::kControlKey, CEF_EVENTFLAG_CONTROL_DOWN},
    {blink::WebInputEvent::kAltKey, CEF_EVENTFLAG_ALT_DOWN},
    {blink::WebInputEvent::kMetaKey, CEF_EVENTFLAG_COMMAND_DOWN},
    {blink::WebInputEvent::kCapsLockOn, CEF_EVENTFLAG_CAPS_LOCK_ON},
    {blink::WebInputEvent::kNumLockOn, CEF_EVENTFLAG_NUM_LOCK_ON},
    {blink::WebInputEvent::kLeftButtonDown, CEF_EVENTFLAG_LEFT_MOUSE_BUTTON},
    {blink::WebInputEvent::kMiddleButtonDown,
     CEF_EVENTFLAG_MIDDLE_MOUSE_BUTTON},
    {blink::WebInputEvent::kRightButtonDown, CEF_EVENTFLAG_RIGHT_MOUSE_BUTTON},
    {blink::WebInputEvent::kIsKeyPad, CEF_EVENTFLAG_IS_KEY_PAD},
    {blink::WebInputEvent::kIsLeft, CEF_EVENTFLAG_IS_LEFT},
    {blink::WebInputEvent::kIsRight, CEF_EVENTFLAG_IS_RIGHT},
    {blink::WebInputEvent::kAltGrKey, CEF_EVENTFLAG_ALTGR_DOWN},
    {blink::WebInputEvent::kIsAutoRepeat, CEF_EVENTFLAG_IS_REPEAT}};

}  // namespace

uint32_t GetCefEventModifiers(int modifiers) {
  uint32_t result = 0;
  for (const auto& entry : kModifierMapping) {
    if (modifiers & entry.web) {
      result |= entry.cef;
    }
  }
  return result;
}

int GetWebEventModifiers(uint32_t cef_modifiers) {
  int result = 0;
  for (const auto& entry : kModifierMapping) {
    if (cef_modifiers & entry.cef) {
      result |= entry.web;
    }
  }
  return result;
}

bool GetWebKeyboardInputEvent(const CefKeyboardInputEvent& event,
                              blink::WebKeyboardEvent& web_event) {
  blink::WebInputEvent::Type type;
  switch (event.type) {
    case CEF_KEYEVENT_RAWKEYDOWN:
      type = blink::WebInputEvent::Type::kRawKeyDown;
      break;
    case CEF_KEYEVENT_KEYDOWN:
      type = blink::WebInputEvent::Type::kKeyDown;
      break;
    case CEF_KEYEVENT_KEYUP:
      type = blink::WebInputEvent::Type::kKeyUp;
      break;
    case CEF_KEYEVENT_CHAR:
      type = blink::WebInputEvent::Type::kChar;
      break;
    default:
      return false;
  }
  constexpr uint32_t allowed_modifiers =
      CEF_EVENTFLAG_CAPS_LOCK_ON | CEF_EVENTFLAG_SHIFT_DOWN |
      CEF_EVENTFLAG_CONTROL_DOWN | CEF_EVENTFLAG_ALT_DOWN |
      CEF_EVENTFLAG_LEFT_MOUSE_BUTTON | CEF_EVENTFLAG_MIDDLE_MOUSE_BUTTON |
      CEF_EVENTFLAG_RIGHT_MOUSE_BUTTON | CEF_EVENTFLAG_COMMAND_DOWN |
      CEF_EVENTFLAG_NUM_LOCK_ON | CEF_EVENTFLAG_IS_KEY_PAD |
      CEF_EVENTFLAG_IS_LEFT | CEF_EVENTFLAG_IS_RIGHT |
      CEF_EVENTFLAG_ALTGR_DOWN | CEF_EVENTFLAG_IS_REPEAT;
  if ((event.modifiers & ~allowed_modifiers) || event.timestamp < 0 ||
      (event.dom_key && !ui::DomKey(event.dom_key).IsValid()) ||
      (event.is_system_key != 0 && event.is_system_key != 1)) {
    return false;
  }
  // Accept unknown physical identity, but never reinterpret an unknown code as
  // a real local key. DomKey is transferred in Chromium's encoded form.
  if (event.dom_code && ui::KeycodeConverter::UsbKeycodeToDomCode(
                            event.dom_code) == ui::DomCode::NONE) {
    return false;
  }
  // Chromium's serialization and Blink string consumers require a terminator
  // within each buffer. Preserve any units following the first terminator.
  if (std::find(std::begin(event.text), std::end(event.text), 0) ==
          std::end(event.text) ||
      std::find(std::begin(event.unmodified_text),
                std::end(event.unmodified_text),
                0) == std::end(event.unmodified_text)) {
    return false;
  }
  blink::WebKeyboardEvent result(
      type, GetWebEventModifiers(event.modifiers),
      event.timestamp ? base::TimeTicks::FromInternalValue(event.timestamp)
                      : base::TimeTicks::Now());
  result.dom_code = event.dom_code;
  result.dom_key = event.dom_key;
  result.windows_key_code = event.windows_key_code;
  result.is_system_key = event.is_system_key;
  std::copy(std::begin(event.text), std::end(event.text), result.text.begin());
  std::copy(std::begin(event.unmodified_text), std::end(event.unmodified_text),
            result.unmodified_text.begin());
  if (event.dom_code) {
    result.native_key_code = ui::KeycodeConverter::DomCodeToNativeKeycode(
        static_cast<ui::DomCode>(event.dom_code));
  }
  web_event = result;
  return true;
}

bool GetCefKeyboardInputEvent(const blink::WebKeyboardEvent& event,
                              CefKeyboardInputEvent& cef_event) {
  static_assert(blink::WebKeyboardEvent::kTextLengthCap == 4);
  CefKeyboardInputEvent result;
  if (!GetCefKeyEventType(event.GetType(), result.type)) {
    return false;
  }
  result.modifiers = GetCefEventModifiers(event.GetModifiers());
  result.timestamp = event.TimeStamp().ToInternalValue();
  result.dom_code = event.dom_code;
  result.dom_key = event.dom_key;
  result.windows_key_code = event.windows_key_code;
  result.is_system_key = event.is_system_key;
  std::copy(event.text.begin(), event.text.end(), result.text);
  std::copy(event.unmodified_text.begin(), event.unmodified_text.end(),
            result.unmodified_text);
  cef_event = result;
  return true;
}

// DEPRECATED: Use GetCefKeyboardInputEvent for unified keyboard callbacks.
bool GetCefKeyEvent(const blink::WebKeyboardEvent& event,
                    CefKeyEvent& cef_event) {
  if (!GetCefKeyEventType(event.GetType(), cef_event.type)) {
    return false;
  }

  // Preserve the modifier subset exposed by legacy keyboard callbacks.
  constexpr uint32_t legacy_modifiers =
      EVENTFLAG_SHIFT_DOWN | EVENTFLAG_CONTROL_DOWN | EVENTFLAG_ALT_DOWN |
      EVENTFLAG_COMMAND_DOWN | EVENTFLAG_IS_KEY_PAD | EVENTFLAG_IS_LEFT |
      EVENTFLAG_IS_RIGHT | EVENTFLAG_ALTGR_DOWN | EVENTFLAG_IS_REPEAT;
  cef_event.modifiers =
      GetCefEventModifiers(event.GetModifiers()) & legacy_modifiers;

  cef_event.windows_key_code = event.windows_key_code;
  cef_event.native_key_code = event.native_key_code;
  cef_event.is_system_key = event.is_system_key;
  cef_event.character = event.text[0];
  cef_event.unmodified_character = event.unmodified_text[0];

  return true;
}
