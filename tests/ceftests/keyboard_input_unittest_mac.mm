// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <IOKit/hidsystem/IOLLEvent.h>

#include <cstring>
#include <thread>

#include "include/cef_input_event_utils.h"
#include "tests/ceftests/keyboard_input_test_util.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/text_input_client_osr_mac.h"

#if CEF_API_ADDED(CEF_NEXT)
namespace {

NSEvent* KeyEvent(NSEventType type,
                  unsigned short code,
                  NSString* text,
                  NSString* unmodified,
                  NSEventModifierFlags flags = 0,
                  bool repeat = false) {
  return [NSEvent keyEventWithType:type
                          location:NSZeroPoint
                     modifierFlags:flags
                         timestamp:NSProcessInfo.processInfo.systemUptime
                      windowNumber:0
                           context:nil
                        characters:text
       charactersIgnoringModifiers:unmodified
                         isARepeat:repeat
                           keyCode:code];
}

CefString CurrentInputSource() {
  TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();
  CFStringRef identifier = static_cast<CFStringRef>(
      TISGetInputSourceProperty(source, kTISPropertyInputSourceID));
  char buffer[512] = {};
  EXPECT_TRUE(CFStringGetCString(identifier, buffer, sizeof(buffer),
                                 kCFStringEncodingUTF8));
  CFRelease(source);
  return buffer;
}

bool Convert(NSEvent* native,
             CefKeyboardInputEvent& event,
             const CefString& source = "com.apple.keylayout.US") {
  return CefKeyboardInputEventFromMacEventEx(
      CAST_NSEVENT_TO_CEF_EVENT_HANDLE(native), source, 40, event);
}

void MacNativeUnicodeAndDefaultStateImpl() {
  @autoreleasepool {
    NSEvent* native = KeyEvent(NSEventTypeKeyDown, kVK_ANSI_A, @"\U0001F600",
                               @"a", NSEventModifierFlagShift, true);
    CefKeyboardInputEvent detected, explicit_state;
    ASSERT_TRUE(CefKeyboardInputEventFromMacEvent(
        CAST_NSEVENT_TO_CEF_EVENT_HANDLE(native), detected));
    ASSERT_TRUE(CefKeyboardInputEventFromMacEventEx(
        CAST_NSEVENT_TO_CEF_EVENT_HANDLE(native), CurrentInputSource(),
        LMGetKbdLast(), explicit_state));
    EXPECT_EQ(0, std::memcmp(&detected, &explicit_state,
                             sizeof(cef_keyboard_input_event_t)));
    EXPECT_EQ(CEF_KEYEVENT_RAWKEYDOWN, detected.type);
    EXPECT_EQ(CEF_DOM_CODE_US_A, detected.dom_code);
    EXPECT_EQ(CEF_DOM_KEY_UNICODE_FLAG | 0x1f600u, detected.dom_key);
    EXPECT_EQ(static_cast<uint32_t>(CEF_EVENTFLAG_SHIFT_DOWN |
                                    CEF_EVENTFLAG_IS_REPEAT),
              detected.modifiers);
    EXPECT_EQ(0xd83d, detected.text[0]);
    EXPECT_EQ(0xde00, detected.text[1]);
    EXPECT_EQ(0, detected.text[2]);
    EXPECT_EQ('a', detected.unmodified_text[0]);
    EXPECT_GT(detected.timestamp, 0);
  }
}

void MacExplicitLayoutDoesNotChangeNativeStateImpl() {
  @autoreleasepool {
    const auto before = CurrentInputSource();
    // A control character forces Chromium to consult the supplied layout for
    // the logical glyph, instead of accepting NSEvent.characters directly.
    NSEvent* native = KeyEvent(NSEventTypeKeyDown, kVK_ANSI_Z, @"\x1a", @"z",
                               NSEventModifierFlagControl);
    CefKeyboardInputEvent us, german;
    ASSERT_TRUE(Convert(native, us));
    ASSERT_TRUE(Convert(native, german, "com.apple.keylayout.German"));
    EXPECT_EQ(CEF_DOM_CODE_US_Z, us.dom_code);
    EXPECT_EQ(us.dom_code, german.dom_code);
    EXPECT_EQ(CEF_DOM_KEY_UNICODE_FLAG | uint32_t{'z'}, us.dom_key);
    EXPECT_EQ(CEF_DOM_KEY_UNICODE_FLAG | uint32_t{'y'}, german.dom_key);
    EXPECT_EQ(us.text[0], german.text[0]);
    EXPECT_EQ(before, CurrentInputSource());
  }
}

void MacModifierTransitionsAndKeypadImpl() {
  @autoreleasepool {
    struct Transition {
      unsigned short code;
      NSEventModifierFlags flags;
      uint32_t dom_code;
      cef_key_event_type_t type;
      uint32_t modifiers;
    };
    const Transition transitions[] = {
        {kVK_Shift, NSEventModifierFlagShift | NX_DEVICELSHIFTKEYMASK,
         CEF_DOM_CODE_SHIFT_LEFT, CEF_KEYEVENT_RAWKEYDOWN,
         CEF_EVENTFLAG_SHIFT_DOWN | CEF_EVENTFLAG_IS_LEFT},
        {kVK_RightShift,
         NSEventModifierFlagShift | NX_DEVICELSHIFTKEYMASK |
             NX_DEVICERSHIFTKEYMASK,
         CEF_DOM_CODE_SHIFT_RIGHT, CEF_KEYEVENT_RAWKEYDOWN,
         CEF_EVENTFLAG_SHIFT_DOWN | CEF_EVENTFLAG_IS_RIGHT},
        // Right Shift goes up while Left Shift remains held.
        {kVK_RightShift, NSEventModifierFlagShift | NX_DEVICELSHIFTKEYMASK,
         CEF_DOM_CODE_SHIFT_RIGHT, CEF_KEYEVENT_KEYUP,
         CEF_EVENTFLAG_SHIFT_DOWN | CEF_EVENTFLAG_IS_RIGHT},
        {kVK_Shift, 0, CEF_DOM_CODE_SHIFT_LEFT, CEF_KEYEVENT_KEYUP,
         CEF_EVENTFLAG_IS_LEFT},
    };
    for (const auto& transition : transitions) {
      CefKeyboardInputEvent event;
      ASSERT_TRUE(Convert(KeyEvent(NSEventTypeFlagsChanged, transition.code,
                                   @"", @"", transition.flags),
                          event));
      EXPECT_EQ(transition.type, event.type);
      EXPECT_EQ(transition.dom_code, event.dom_code);
      EXPECT_EQ(CEF_DOM_KEY_SHIFT, event.dom_key);
      EXPECT_EQ(transition.modifiers, event.modifiers);
      EXPECT_EQ(0, event.text[0]);
    }
    CefKeyboardInputEvent keypad, clear;
    ASSERT_TRUE(Convert(
        KeyEvent(NSEventTypeKeyDown, kVK_ANSI_Keypad1, @"1", @"1"), keypad));
    EXPECT_EQ(CEF_DOM_CODE_NUMPAD1, keypad.dom_code);
    EXPECT_TRUE(keypad.modifiers & CEF_EVENTFLAG_IS_KEY_PAD);
    ASSERT_TRUE(Convert(KeyEvent(NSEventTypeKeyDown, kVK_ANSI_KeypadClear,
                                 @"\uf739", @"\uf739"),
                        clear));
    EXPECT_EQ(CEF_DOM_KEY_CLEAR, clear.dom_key);
  }
}

void MacInvalidInputLeavesOutputUnchangedImpl() {
  @autoreleasepool {
    CefKeyboardInputEvent event;
    ASSERT_TRUE(
        Convert(KeyEvent(NSEventTypeKeyUp, kVK_ANSI_A, @"a", @"a"), event));
    const CefKeyboardInputEvent before(event);
    EXPECT_FALSE(CefKeyboardInputEventFromMacEvent(nullptr, event));
    EXPECT_FALSE(
        Convert([NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                   location:NSZeroPoint
                              modifierFlags:0
                                  timestamp:0
                               windowNumber:0
                                    context:nil
                                    subtype:0
                                      data1:0
                                      data2:0],
                event));
    EXPECT_FALSE(Convert(
        KeyEvent(NSEventTypeKeyDown, kVK_ANSI_A, @"abcd", @"a"), event));
    EXPECT_FALSE(Convert(
        KeyEvent(NSEventTypeKeyDown, kVK_ANSI_A, @"a", @"abcd"), event));
    EXPECT_FALSE(Convert(KeyEvent(NSEventTypeKeyDown, kVK_ANSI_A, @"a", @"a"),
                         event, "cef.nonexistent.keyboard.layout"));
    EXPECT_FALSE(CefKeyboardInputEventFromMacEventEx(
        CAST_NSEVENT_TO_CEF_EVENT_HANDLE(
            KeyEvent(NSEventTypeKeyDown, kVK_ANSI_A, @"a", @"a")),
        "com.apple.keylayout.US", 256, event));
    EXPECT_EQ(0,
              std::memcmp(&before, &event, sizeof(cef_keyboard_input_event_t)));
  }
}

void MacOffMainThreadLeavesOutputUnchangedImpl() {
  @autoreleasepool {
    ASSERT_TRUE([NSThread isMainThread]);
    NSEvent* native = KeyEvent(NSEventTypeKeyDown, kVK_ANSI_A, @"a", @"a");
    CefKeyboardInputEvent event;
    ASSERT_TRUE(Convert(native, event));
    const CefKeyboardInputEvent before(event);
    std::thread worker([native, &event, &before]() {
      @autoreleasepool {
        EXPECT_FALSE([NSThread isMainThread]);
        EXPECT_FALSE(CefKeyboardInputEventFromMacEvent(
            CAST_NSEVENT_TO_CEF_EVENT_HANDLE(native), event));
        EXPECT_EQ(0, std::memcmp(&before, &event,
                                 sizeof(cef_keyboard_input_event_t)));
        EXPECT_FALSE(CefKeyboardInputEventFromMacEventEx(
            CAST_NSEVENT_TO_CEF_EVENT_HANDLE(native), "com.apple.keylayout.US",
            40, event));
        EXPECT_EQ(0, std::memcmp(&before, &event,
                                 sizeof(cef_keyboard_input_event_t)));
      }
    });
    worker.join();
  }
}

}  // namespace

bool BuildNativeKeyboardInputEvents(
    std::array<CefKeyboardInputEvent, 3>& events) {
  @autoreleasepool {
    if (!Convert(KeyEvent(NSEventTypeKeyDown, kVK_ANSI_A, @"a", @"a"),
                 events[0]) ||
        !Convert(KeyEvent(NSEventTypeKeyUp, kVK_ANSI_A, @"a", @"a"),
                 events[2])) {
      return false;
    }
    // AppKit insertText: supplies the CHAR insertion separately from keydown.
    events[1] = events[0];
    events[1].type = CEF_KEYEVENT_CHAR;
    events[1].windows_key_code = 'a';
    return true;
  }
}

bool BuildMacTextClientKeyboardInputEvents(
    MacTextClientInput input,
    std::vector<CefKeyboardInputEvent>& events) {
  @autoreleasepool {
    unsigned short code = kVK_ANSI_A;
    NSString* text = @"a";
    NSString* unmodified = @"a";
    NSEventModifierFlags flags = 0;
    switch (input) {
      case MacTextClientInput::Character:
      case MacTextClientInput::NonInsertionCommand:
        break;
      case MacTextClientInput::OptionCharacter:
        text = @"\u00e5";
        flags = NSEventModifierFlagOption;
        break;
      case MacTextClientInput::Return:
      case MacTextClientInput::ShiftReturn:
        code = kVK_Return;
        text = unmodified = @"\r";
        if (input == MacTextClientInput::ShiftReturn) {
          flags = NSEventModifierFlagShift;
        }
        break;
      case MacTextClientInput::ControlCharacter:
        text = @"\x01";
        flags = NSEventModifierFlagControl;
        break;
      case MacTextClientInput::CommandCharacter:
        flags = NSEventModifierFlagCommand;
        break;
      case MacTextClientInput::Clear:
        code = kVK_ANSI_KeypadClear;
        text = unmodified = @"\uf739";
        break;
    }
    CefKeyboardInputEvent down, up;
    if (!Convert(KeyEvent(NSEventTypeKeyDown, code, text, unmodified, flags),
                 down) ||
        !Convert(KeyEvent(NSEventTypeKeyUp, code, text, unmodified, flags),
                 up)) {
      return false;
    }
    events = {down};
    if (input != MacTextClientInput::NonInsertionCommand &&
        input != MacTextClientInput::Clear) {
      CefKeyboardInputEvent character(down);
      character.type = CEF_KEYEVENT_CHAR;
      if (input == MacTextClientInput::Character ||
          input == MacTextClientInput::OptionCharacter) {
        character.windows_key_code = character.text[0];
      }
      events.push_back(character);
    }
    events.push_back(up);
    return true;
  }
}

void SendMacTextClientKeyboardInput(
    CefRefPtr<CefBrowser> browser,
    MacTextClientInput input,
    const std::vector<CefKeyboardInputEvent>& events) {
  @autoreleasepool {
    CefTextInputClientOSRMac* client =
        [[CefTextInputClientOSRMac alloc] initWithBrowser:browser];
    [client
        HandleKeyEventBeforeTextInputClient:KeyEvent(NSEventTypeKeyDown,
                                                     kVK_ANSI_A, @"a", @"a")];
    switch (input) {
      case MacTextClientInput::Character:
      case MacTextClientInput::OptionCharacter: {
        const unichar character = events[0].text[0];
        NSString* text = [NSString stringWithCharacters:&character length:1];
        [client insertText:text replacementRange:NSMakeRange(NSNotFound, 0)];
        break;
      }
      case MacTextClientInput::Return:
      case MacTextClientInput::ShiftReturn:
        // Like Chromium, ignore AppKit's noop: when tracking edit commands.
        [client doCommandBySelector:@selector(noop:)];
        [client doCommandBySelector:@selector(insertNewline:)];
        break;
      case MacTextClientInput::ControlCharacter:
      case MacTextClientInput::CommandCharacter:
      case MacTextClientInput::NonInsertionCommand:
        [client doCommandBySelector:@selector(moveToBeginningOfLine:)];
        break;
      case MacTextClientInput::Clear:
        [client doCommandBySelector:@selector(insertText:)];
        break;
    }
    [client HandleKeyEventAfterTextInputClient:events[0] keyEventValid:YES];
    browser->GetHost()->SendKeyboardEvent(events.back());
    [client detach];
  }
}

UI_THREAD_TEST(KeyboardInputTest, MacNativeUnicodeAndDefaultState)
UI_THREAD_TEST(KeyboardInputTest, MacExplicitLayoutDoesNotChangeNativeState)
UI_THREAD_TEST(KeyboardInputTest, MacModifierTransitionsAndKeypad)
UI_THREAD_TEST(KeyboardInputTest, MacInvalidInputLeavesOutputUnchanged)
UI_THREAD_TEST(KeyboardInputTest, MacOffMainThreadLeavesOutputUnchanged)
#endif  // CEF_API_ADDED(CEF_NEXT)
