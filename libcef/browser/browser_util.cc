// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/browser_util.h"

#include "content/public/browser/native_web_keyboard_event.h"

namespace browser_util {

bool GetCefKeyEvent(const content::NativeWebKeyboardEvent& event,
                    CefKeyEvent& cef_event) {
  switch (event.type) {
    case blink::WebKeyboardEvent::RawKeyDown:
      cef_event.type = KEYEVENT_RAWKEYDOWN;
      break;
    case blink::WebKeyboardEvent::KeyDown:
      cef_event.type = KEYEVENT_KEYDOWN;
      break;
    case blink::WebKeyboardEvent::KeyUp:
      cef_event.type = KEYEVENT_KEYUP;
      break;
    case blink::WebKeyboardEvent::Char:
      cef_event.type = KEYEVENT_CHAR;
      break;
    default:
      return false;
  }

  cef_event.modifiers = 0;
  if (event.modifiers & blink::WebKeyboardEvent::ShiftKey)
    cef_event.modifiers |= EVENTFLAG_SHIFT_DOWN;
  if (event.modifiers & blink::WebKeyboardEvent::ControlKey)
    cef_event.modifiers |= EVENTFLAG_CONTROL_DOWN;
  if (event.modifiers & blink::WebKeyboardEvent::AltKey)
    cef_event.modifiers |= EVENTFLAG_ALT_DOWN;
  if (event.modifiers & blink::WebKeyboardEvent::MetaKey)
    cef_event.modifiers |= EVENTFLAG_COMMAND_DOWN;
  if (event.modifiers & blink::WebKeyboardEvent::IsKeyPad)
    cef_event.modifiers |= EVENTFLAG_IS_KEY_PAD;

  cef_event.windows_key_code = event.windowsKeyCode;
  cef_event.native_key_code = event.nativeKeyCode;
  cef_event.is_system_key = event.isSystemKey;
  cef_event.character = event.text[0];
  cef_event.unmodified_character = event.unmodifiedText[0];

  return true;
}

bool GetCefKeyEvent(const ui::KeyEvent& event,
                    CefKeyEvent& cef_event) {
  content::NativeWebKeyboardEvent native_event(event);
  return GetCefKeyEvent(native_event, cef_event);
}

}  // namespace browser_util
