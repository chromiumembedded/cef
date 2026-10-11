// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/key_event_util.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/owned_objc.h"
#include "components/input/native_web_keyboard_event.h"
#include "ui/events/base_event_utils.h"

namespace {

NSUInteger NativeModifiers(int cef_modifiers) {
  NSUInteger native_modifiers = 0;
  if (cef_modifiers & EVENTFLAG_SHIFT_DOWN) {
    native_modifiers |= NSEventModifierFlagShift;
  }
  if (cef_modifiers & EVENTFLAG_CONTROL_DOWN) {
    native_modifiers |= NSEventModifierFlagControl;
  }
  if (cef_modifiers & EVENTFLAG_ALT_DOWN) {
    native_modifiers |= NSEventModifierFlagOption;
  }
  if (cef_modifiers & EVENTFLAG_COMMAND_DOWN) {
    native_modifiers |= NSEventModifierFlagCommand;
  }
  if (cef_modifiers & EVENTFLAG_CAPS_LOCK_ON) {
    native_modifiers |= NSEventModifierFlagCapsLock;
  }
  if (cef_modifiers & EVENTFLAG_NUM_LOCK_ON) {
    native_modifiers |= NSEventModifierFlagNumericPad;
  }

  return native_modifiers;
}

}  // namespace

input::NativeWebKeyboardEvent GetLegacyWebKeyEvent(const CefKeyEvent& key_event,
                                                   double timestamp) {
  input::NativeWebKeyboardEvent result(
      blink::WebInputEvent::Type::kUndefined,
      blink::WebInputEvent::Modifiers::kNoModifiers, ui::EventTimeForNow());

  // Use a synthetic NSEvent in order to obtain the windowsKeyCode member from
  // the NativeWebKeyboardEvent constructor. This is the only member which can
  // not be easily translated (without hardcoding keyCodes)
  // Determining whether a modifier key is left or right seems to be done
  // through the key code as well.
  NSEventType event_type;
  if (key_event.character == 0 && key_event.unmodified_character == 0) {
    // Check if both character and unmodified_characther are empty to determine
    // if this was a NSEventTypeFlagsChanged event.
    // A dead key will have an empty character, but a non-empty unmodified
    // character
    event_type = NSEventTypeFlagsChanged;
  } else {
    switch (key_event.type) {
      case KEYEVENT_RAWKEYDOWN:
      case KEYEVENT_KEYDOWN:
      case KEYEVENT_CHAR:
        event_type = NSEventTypeKeyDown;
        break;
      case KEYEVENT_KEYUP:
        event_type = NSEventTypeKeyUp;
        break;
    }
  }

  NSString* charactersIgnoringModifiers =
      [[NSString alloc] initWithCharacters:reinterpret_cast<const unichar*>(
                                               &key_event.unmodified_character)
                                    length:1];
  NSString* characters = [[NSString alloc]
      initWithCharacters:reinterpret_cast<const unichar*>(&key_event.character)
                  length:1];

  NSEvent* synthetic_event =
      [NSEvent keyEventWithType:event_type
                             location:NSMakePoint(0, 0)
                        modifierFlags:NativeModifiers(key_event.modifiers)
                            timestamp:timestamp
                         windowNumber:0
                              context:nil
                           characters:characters
          charactersIgnoringModifiers:charactersIgnoringModifiers
                            isARepeat:NO
                              keyCode:key_event.native_key_code];

  result =
      input::NativeWebKeyboardEvent(base::apple::OwnedNSEvent(synthetic_event));
  if (key_event.type == KEYEVENT_CHAR) {
    result.SetType(blink::WebInputEvent::Type::kChar);
  }

  result.is_system_key = key_event.is_system_key;

  return result;
}
