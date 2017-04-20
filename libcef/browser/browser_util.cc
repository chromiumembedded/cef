// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/browser_util.h"

#include "content/public/browser/native_web_keyboard_event.h"

namespace browser_util {

bool GetCefKeyEvent(const content::NativeWebKeyboardEvent& event,
                    CefKeyEvent& cef_event) {
  switch (event.GetType()) {
    case blink::WebKeyboardEvent::kRawKeyDown:
      cef_event.type = KEYEVENT_RAWKEYDOWN;
      break;
    case blink::WebKeyboardEvent::kKeyDown:
      cef_event.type = KEYEVENT_KEYDOWN;
      break;
    case blink::WebKeyboardEvent::kKeyUp:
      cef_event.type = KEYEVENT_KEYUP;
      break;
    case blink::WebKeyboardEvent::kChar:
      cef_event.type = KEYEVENT_CHAR;
      break;
    default:
      return false;
  }

  cef_event.modifiers = 0;
  if (event.GetModifiers() & blink::WebKeyboardEvent::kShiftKey)
    cef_event.modifiers |= EVENTFLAG_SHIFT_DOWN;
  if (event.GetModifiers() & blink::WebKeyboardEvent::kControlKey)
    cef_event.modifiers |= EVENTFLAG_CONTROL_DOWN;
  if (event.GetModifiers() & blink::WebKeyboardEvent::kAltKey)
    cef_event.modifiers |= EVENTFLAG_ALT_DOWN;
  if (event.GetModifiers() & blink::WebKeyboardEvent::kMetaKey)
    cef_event.modifiers |= EVENTFLAG_COMMAND_DOWN;
  if (event.GetModifiers() & blink::WebKeyboardEvent::kIsKeyPad)
    cef_event.modifiers |= EVENTFLAG_IS_KEY_PAD;

  cef_event.windows_key_code = event.windows_key_code;
  cef_event.native_key_code = event.native_key_code;
  cef_event.is_system_key = event.is_system_key;
  cef_event.character = event.text[0];
  cef_event.unmodified_character = event.unmodified_text[0];

  return true;
}

bool GetCefKeyEvent(const ui::KeyEvent& event,
                    CefKeyEvent& cef_event) {
  content::NativeWebKeyboardEvent native_event(event);
  return GetCefKeyEvent(native_event, cef_event);
}

}  // namespace browser_util
