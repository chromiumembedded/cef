// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/key_event_util.h"

#include <cstring>

#include "cef/libcef/browser/browser_event_util.h"
#include "components/input/native_web_keyboard_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace {

void ExpectExposedFields(const blink::WebKeyboardEvent& expected,
                         const blink::WebKeyboardEvent& actual) {
  EXPECT_EQ(expected.GetType(), actual.GetType());
  EXPECT_EQ(expected.GetModifiers(), actual.GetModifiers());
  EXPECT_EQ(expected.TimeStamp(), actual.TimeStamp());
  EXPECT_EQ(expected.dom_code, actual.dom_code);
  EXPECT_EQ(expected.dom_key, actual.dom_key);
  EXPECT_EQ(expected.windows_key_code, actual.windows_key_code);
  EXPECT_EQ(expected.is_system_key, actual.is_system_key);
  EXPECT_EQ(expected.text, actual.text);
  EXPECT_EQ(expected.unmodified_text, actual.unmodified_text);
}

input::NativeWebKeyboardEvent ConvertLegacy(const CefKeyEvent& event) {
#if BUILDFLAG(IS_MAC)
  return GetLegacyWebKeyEvent(event, 3.25);
#else
  return GetLegacyWebKeyEvent(event,
                              base::TimeTicks::FromInternalValue(3250000));
#endif
}

}  // namespace

TEST(KeyEventUtil, LegacyCallbackTypesAndFields) {
  const struct {
    blink::WebInputEvent::Type web;
    cef_key_event_type_t cef;
  } types[] = {{blink::WebInputEvent::Type::kRawKeyDown, KEYEVENT_RAWKEYDOWN},
               {blink::WebInputEvent::Type::kKeyDown, KEYEVENT_KEYDOWN},
               {blink::WebInputEvent::Type::kKeyUp, KEYEVENT_KEYUP},
               {blink::WebInputEvent::Type::kChar, KEYEVENT_CHAR}};
  for (const auto& type : types) {
    blink::WebKeyboardEvent web(type.web, 0,
                                base::TimeTicks::FromInternalValue(1234));
    web.windows_key_code = 0x0108;
    web.native_key_code = 42;
    web.is_system_key = true;
    web.text = {0xd83d, 0xde00, 0, u'Z'};
    web.unmodified_text = {u'a', u'b', 0, u'X'};
    CefKeyEvent legacy;
    legacy.focus_on_editable_field = true;
    ASSERT_TRUE(GetCefKeyEvent(web, legacy));
    EXPECT_EQ(type.cef, legacy.type);
    EXPECT_EQ(web.windows_key_code, legacy.windows_key_code);
    EXPECT_EQ(web.native_key_code, legacy.native_key_code);
    EXPECT_TRUE(legacy.is_system_key);
    EXPECT_EQ(0xd83d, legacy.character);
    EXPECT_EQ(u'a', legacy.unmodified_character);
    // Focus context is supplied by the caller, not by this converter.
    EXPECT_TRUE(legacy.focus_on_editable_field);
  }
}

TEST(KeyEventUtil, LegacyCallbackModifierSubset) {
  const struct {
    int web;
    uint32_t cef;
  } flags[] = {
      {blink::WebInputEvent::kShiftKey, EVENTFLAG_SHIFT_DOWN},
      {blink::WebInputEvent::kControlKey, EVENTFLAG_CONTROL_DOWN},
      {blink::WebInputEvent::kAltKey, EVENTFLAG_ALT_DOWN},
      {blink::WebInputEvent::kMetaKey, EVENTFLAG_COMMAND_DOWN},
      {blink::WebInputEvent::kIsKeyPad, EVENTFLAG_IS_KEY_PAD},
      {blink::WebInputEvent::kIsLeft, EVENTFLAG_IS_LEFT},
      {blink::WebInputEvent::kIsRight, EVENTFLAG_IS_RIGHT},
      {blink::WebInputEvent::kAltGrKey, EVENTFLAG_ALTGR_DOWN},
      {blink::WebInputEvent::kIsAutoRepeat, EVENTFLAG_IS_REPEAT},
      {blink::WebInputEvent::kCapsLockOn, 0},
      {blink::WebInputEvent::kNumLockOn, 0},
      {blink::WebInputEvent::kLeftButtonDown, 0},
      {blink::WebInputEvent::kMiddleButtonDown, 0},
      {blink::WebInputEvent::kRightButtonDown, 0},
      {blink::WebInputEvent::kScrollLockOn, 0},
      {blink::WebInputEvent::kBackButtonDown, 0},
      {blink::WebInputEvent::kForwardButtonDown, 0},
  };
  for (const auto& flag : flags) {
    blink::WebKeyboardEvent web(blink::WebInputEvent::Type::kRawKeyDown,
                                flag.web, base::TimeTicks());
    CefKeyEvent legacy;
    ASSERT_TRUE(GetCefKeyEvent(web, legacy));
    EXPECT_EQ(flag.cef, static_cast<uint32_t>(legacy.modifiers));
  }
}

TEST(KeyEventUtil, UnsupportedLegacyCallbackLeavesOutputUnchanged) {
  CefKeyEvent event;
  event.windows_key_code = 123;
  event.character = u'x';
  const CefKeyEvent before(event);
  blink::WebKeyboardEvent web;
  EXPECT_FALSE(GetCefKeyEvent(web, event));
  EXPECT_EQ(0, std::memcmp(&before, &event, sizeof(cef_key_event_t)));
}

TEST(KeyEventUtil, LegacyAndPortableConversionsShareCallbackValues) {
  for (const auto type :
       {KEYEVENT_RAWKEYDOWN, KEYEVENT_KEYDOWN, KEYEVENT_KEYUP, KEYEVENT_CHAR}) {
    CefKeyEvent legacy;
    legacy.type = type;
    legacy.windows_key_code = type == KEYEVENT_CHAR ? 'a' : ui::VKEY_A;
    legacy.native_key_code =
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::US_A);
    legacy.character = legacy.unmodified_character = u'a';
    const auto web = ConvertLegacy(legacy);
    EXPECT_EQ(type == KEYEVENT_CHAR ? blink::WebInputEvent::Type::kChar
              : type == KEYEVENT_KEYUP
                  ? blink::WebInputEvent::Type::kKeyUp
                  : blink::WebInputEvent::Type::kRawKeyDown,
              web.GetType());
    EXPECT_EQ(static_cast<int>(ui::DomCode::US_A), web.dom_code);
#if !BUILDFLAG(IS_MAC)
    // Aura legacy keydown text is derived from the platform key/layout path.
    if (type == KEYEVENT_CHAR) {
      EXPECT_EQ(u'a', web.text[0]);
    }
#else
    EXPECT_EQ(u'a', web.text[0]);
#endif
    CefKeyboardInputEvent portable;
    ASSERT_TRUE(GetCefKeyboardInputEvent(web, portable));
    blink::WebKeyboardEvent rebuilt;
    ASSERT_TRUE(GetWebKeyboardInputEvent(portable, rebuilt));
    ExpectExposedFields(web, rebuilt);
    CefKeyEvent original_callback, portable_callback;
    ASSERT_TRUE(GetCefKeyEvent(web, original_callback));
    ASSERT_TRUE(GetCefKeyEvent(rebuilt, portable_callback));
    EXPECT_EQ(0, std::memcmp(&original_callback, &portable_callback,
                             sizeof(cef_key_event_t)));
  }
}

TEST(KeyEventUtil, LegacyCharacterUnitsAreNotNarrowed) {
  for (char16_t unit :
       {u'\u0108', u'\u011b', u'\u20ac', char16_t{0xd83d}, char16_t{0xde00}}) {
    CefKeyEvent event;
    event.type = KEYEVENT_CHAR;
    event.native_key_code =
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::US_A);
    event.windows_key_code = unit;
    event.character = event.unmodified_character = unit;
    const auto web = ConvertLegacy(event);
    EXPECT_EQ(blink::WebInputEvent::Type::kChar, web.GetType());
    EXPECT_EQ(unit, web.text[0]);
#if BUILDFLAG(IS_WIN)
    EXPECT_EQ(unit, web.windows_key_code);
#endif
    CefKeyEvent callback;
    ASSERT_TRUE(GetCefKeyEvent(web, callback));
    EXPECT_EQ(unit, callback.character);
  }
}

TEST(KeyEventUtil, LegacyMinimalTypingPayload) {
  // Preserve the platform-specific "done" payloads formerly exercised by
  // OSRTest.KeyEvents. The IME navigation tests cover insertion/submission;
  // this test covers conversion without redundant character/key-code fields.
  const struct {
    char character;
    int mac_native_code;
    ui::DomCode dom_code;
  } keys[] = {{'d', 2, ui::DomCode::US_D},
              {'o', 31, ui::DomCode::US_O},
              {'n', 45, ui::DomCode::US_N},
              {'e', 14, ui::DomCode::US_E}};
  for (const auto& key : keys) {
    SCOPED_TRACE(key.character);
    for (const auto type :
         {KEYEVENT_RAWKEYDOWN, KEYEVENT_CHAR, KEYEVENT_KEYUP}) {
      SCOPED_TRACE(type);
      CefKeyEvent minimal;
      minimal.type = type;
      const int virtual_key = key.character - 'a' + 'A';
#if BUILDFLAG(IS_WIN)
      // WM_CHAR carries the character in wParam; down/up carry the virtual
      // key. No character fields are supplied by the legacy OSR sender.
      const BYTE vk = LOBYTE(VkKeyScanA(key.character));
      ASSERT_EQ(virtual_key, vk);
      minimal.windows_key_code = type == KEYEVENT_CHAR ? key.character : vk;
      minimal.native_key_code = (MapVirtualKey(vk, MAPVK_VK_TO_VSC) << 16) | 1;
      if (type == KEYEVENT_KEYUP) {
        minimal.native_key_code |= 0xC0000000;
      }
#elif BUILDFLAG(IS_MAC)
      // macOS derives windows_key_code from the synthetic NSEvent.
      minimal.native_key_code = key.mac_native_code;
      minimal.character = minimal.unmodified_character = key.character;
#else
      minimal.native_key_code = key.character;  // Legacy X11/XKB keysym.
      minimal.windows_key_code = virtual_key;
      minimal.character = minimal.unmodified_character = key.character;
#endif
      const auto web = ConvertLegacy(minimal);
      EXPECT_EQ(type == KEYEVENT_CHAR ? blink::WebInputEvent::Type::kChar
                : type == KEYEVENT_KEYUP
                    ? blink::WebInputEvent::Type::kKeyUp
                    : blink::WebInputEvent::Type::kRawKeyDown,
                web.GetType());
      EXPECT_EQ(0, web.GetModifiers());
      EXPECT_FALSE(web.is_system_key);
#if BUILDFLAG(IS_MAC)
      EXPECT_EQ(static_cast<int>(key.dom_code), web.dom_code);
      EXPECT_EQ(virtual_key, web.windows_key_code);
      EXPECT_EQ(key.character, web.text[0]);
      EXPECT_EQ(key.character, web.unmodified_text[0]);
#else
      EXPECT_EQ(minimal.windows_key_code, web.windows_key_code);
#endif
      if (type == KEYEVENT_CHAR) {
        EXPECT_EQ(key.character, web.text[0]);
        CefKeyEvent callback;
        ASSERT_TRUE(GetCefKeyEvent(web, callback));
        EXPECT_EQ(KEYEVENT_CHAR, callback.type);
        EXPECT_EQ(key.character, callback.character);
      }

      // Adding the redundant fields must not alter the converted event.
      CefKeyEvent complete(minimal);
      complete.character = complete.unmodified_character = key.character;
#if BUILDFLAG(IS_MAC)
      complete.windows_key_code = virtual_key;
#endif
      ExpectExposedFields(web, ConvertLegacy(complete));
    }
  }
}

TEST(KeyEventUtil, LegacyModifierTransitionIdentity) {
  for (const auto code : {ui::DomCode::SHIFT_LEFT, ui::DomCode::SHIFT_RIGHT}) {
    for (bool down : {false, true}) {
      CefKeyEvent event;
      event.type = down ? KEYEVENT_RAWKEYDOWN : KEYEVENT_KEYUP;
      event.windows_key_code = ui::VKEY_SHIFT;
      event.native_key_code =
          ui::KeycodeConverter::DomCodeToNativeKeycode(code);
      event.modifiers = down ? EVENTFLAG_SHIFT_DOWN : 0;
      const auto web = ConvertLegacy(event);
      EXPECT_EQ(down ? blink::WebInputEvent::Type::kRawKeyDown
                     : blink::WebInputEvent::Type::kKeyUp,
                web.GetType());
      EXPECT_EQ(static_cast<int>(code), web.dom_code);
      EXPECT_EQ(ui::DomKey::SHIFT, ui::DomKey(web.dom_key));
      EXPECT_EQ(down, !!(web.GetModifiers() & blink::WebInputEvent::kShiftKey));
      EXPECT_TRUE(web.GetModifiers() & (code == ui::DomCode::SHIFT_LEFT
                                            ? blink::WebInputEvent::kIsLeft
                                            : blink::WebInputEvent::kIsRight));
    }
  }
}

#if BUILDFLAG(IS_MAC)

TEST(KeyEventUtil, LegacyMacModifiersAndSystemKey) {
  const struct {
    uint32_t cef;
    int web;
  } flags[] = {
      {EVENTFLAG_SHIFT_DOWN, blink::WebInputEvent::kShiftKey},
      {EVENTFLAG_CONTROL_DOWN, blink::WebInputEvent::kControlKey},
      {EVENTFLAG_ALT_DOWN, blink::WebInputEvent::kAltKey},
      {EVENTFLAG_COMMAND_DOWN, blink::WebInputEvent::kMetaKey},
      {EVENTFLAG_CAPS_LOCK_ON, blink::WebInputEvent::kCapsLockOn},
  };
  for (const auto& flag : flags) {
    CefKeyEvent event;
    event.type = KEYEVENT_RAWKEYDOWN;
    event.native_key_code =
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::US_A);
    event.character = u'a';
    event.unmodified_character = u'x';
    event.modifiers = flag.cef;
    event.is_system_key = true;
    const auto web = ConvertLegacy(event);
    EXPECT_TRUE(web.GetModifiers() & flag.web);
    EXPECT_TRUE(web.is_system_key);
    EXPECT_EQ(u'a', web.text[0]);
    EXPECT_EQ(u'x', web.unmodified_text[0]);
  }
  // The legacy synthetic NSEvent has isARepeat:NO regardless of this hint.
  CefKeyEvent event;
  event.type = KEYEVENT_RAWKEYDOWN;
  event.character = event.unmodified_character = u'a';
  event.native_key_code =
      ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::US_A);
  event.modifiers = EVENTFLAG_IS_REPEAT;
  EXPECT_FALSE(ConvertLegacy(event).GetModifiers() &
               blink::WebInputEvent::kIsAutoRepeat);
}
#else  // BUILDFLAG(IS_MAC)

TEST(KeyEventUtil, LegacyAuraModifierSemantics) {
  const struct {
    uint32_t cef;
    int ui;
  } flags[] = {
      {EVENTFLAG_CAPS_LOCK_ON, ui::EF_CAPS_LOCK_ON},
      {EVENTFLAG_SHIFT_DOWN, ui::EF_SHIFT_DOWN},
      {EVENTFLAG_CONTROL_DOWN, ui::EF_CONTROL_DOWN},
      {EVENTFLAG_ALT_DOWN, ui::EF_ALT_DOWN},
      {EVENTFLAG_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON},
      {EVENTFLAG_MIDDLE_MOUSE_BUTTON, ui::EF_MIDDLE_MOUSE_BUTTON},
      {EVENTFLAG_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON},
      {EVENTFLAG_COMMAND_DOWN, ui::EF_COMMAND_DOWN},
      {EVENTFLAG_NUM_LOCK_ON, ui::EF_NUM_LOCK_ON},
      {EVENTFLAG_IS_KEY_PAD, ui::EF_IS_EXTENDED_KEY},
      {EVENTFLAG_ALTGR_DOWN, ui::EF_ALTGR_DOWN},
      {EVENTFLAG_IS_REPEAT, ui::EF_IS_REPEAT},
      {EVENTFLAG_PRECISION_SCROLLING_DELTA, ui::EF_PRECISION_SCROLLING_DELTA},
      {EVENTFLAG_SCROLL_BY_PAGE, ui::EF_SCROLL_BY_PAGE},
      // Legacy Aura location comes from physical identity, not side flag hints.
      {EVENTFLAG_IS_LEFT, 0},
      {EVENTFLAG_IS_RIGHT, 0},
  };
  for (const auto& flag : flags) {
    EXPECT_EQ(flag.ui, GetLegacyUiEventModifiers(flag.cef));
  }
}

#endif  // BUILDFLAG(IS_MAC)
