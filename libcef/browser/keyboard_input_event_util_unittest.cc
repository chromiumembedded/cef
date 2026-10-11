// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <algorithm>

#include "base/time/time.h"
#include "cef/libcef/browser/browser_event_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace {

void ExpectEqual(const blink::WebKeyboardEvent& expected,
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

}  // namespace

TEST(KeyboardInputEventUtil, ViewsKeyEventPreservesResolvedLayout) {
  for (const auto type :
       {ui::EventType::kKeyPressed, ui::EventType::kKeyReleased}) {
    ui::KeyEvent native(type, ui::VKEY_Z, ui::DomCode::US_Z,
                        ui::EF_SHIFT_DOWN | ui::EF_IS_REPEAT,
                        ui::DomKey::FromCharacter('y'),
                        base::TimeTicks::FromInternalValue(123456789));
    CefKeyboardInputEvent snapshot;
    ASSERT_TRUE(
        GetCefKeyboardInputEvent(ui::MakeWebKeyboardEvent(native), snapshot));
    EXPECT_EQ(type == ui::EventType::kKeyPressed ? CEF_KEYEVENT_RAWKEYDOWN
                                                 : CEF_KEYEVENT_KEYUP,
              snapshot.type);
    EXPECT_EQ(CEF_DOM_CODE_US_Z, snapshot.dom_code);
    EXPECT_EQ(CEF_DOM_KEY_UNICODE_FLAG | uint32_t{'y'}, snapshot.dom_key);
    EXPECT_EQ(ui::VKEY_Z, snapshot.windows_key_code);
    EXPECT_EQ(uint32_t{CEF_EVENTFLAG_SHIFT_DOWN | CEF_EVENTFLAG_IS_REPEAT},
              snapshot.modifiers);
    EXPECT_EQ(123456789, snapshot.timestamp);
    EXPECT_EQ(u'y', snapshot.text[0]);
    EXPECT_EQ(u'y', snapshot.unmodified_text[0]);
  }
}

TEST(KeyboardInputEventUtil, CompleteSnapshotRoundTrip) {
  for (const auto type : {blink::WebInputEvent::Type::kRawKeyDown,
                          blink::WebInputEvent::Type::kKeyDown,
                          blink::WebInputEvent::Type::kKeyUp,
                          blink::WebInputEvent::Type::kChar}) {
    blink::WebKeyboardEvent original(
        type,
        blink::WebInputEvent::kShiftKey | blink::WebInputEvent::kControlKey |
            blink::WebInputEvent::kAltKey | blink::WebInputEvent::kMetaKey |
            blink::WebInputEvent::kCapsLockOn |
            blink::WebInputEvent::kNumLockOn |
            blink::WebInputEvent::kLeftButtonDown |
            blink::WebInputEvent::kMiddleButtonDown |
            blink::WebInputEvent::kRightButtonDown |
            blink::WebInputEvent::kIsKeyPad | blink::WebInputEvent::kIsLeft |
            blink::WebInputEvent::kIsRight | blink::WebInputEvent::kAltGrKey |
            blink::WebInputEvent::kIsAutoRepeat,
        base::TimeTicks::FromInternalValue(123456789));
    original.dom_code = static_cast<uint32_t>(ui::DomCode::US_A);
    // Deliberately unrelated to US_A: dispatch must retain the source layout.
    original.dom_key = ui::DomKey::FromCharacter(0x1f600);
    original.windows_key_code = 0x41;
    original.is_system_key = true;
    original.text = {0xd83d, 0xde00, 0, u'Z'};
    original.unmodified_text = {u'a', 0, u'X', u'Y'};

    CefKeyboardInputEvent snapshot;
    ASSERT_TRUE(GetCefKeyboardInputEvent(original, snapshot));
    EXPECT_EQ(sizeof(cef_keyboard_input_event_t), snapshot.size);
    CefKeyboardInputEvent owned(snapshot);
    std::fill(std::begin(snapshot.text), std::end(snapshot.text), u'!');
    blink::WebKeyboardEvent restored;
    ASSERT_TRUE(GetWebKeyboardInputEvent(owned, restored));
    ExpectEqual(original, restored);
  }
}

TEST(KeyboardInputEventUtil, UnknownPhysicalKeyAndDetectedTime) {
  CefKeyboardInputEvent snapshot;
  snapshot.type = CEF_KEYEVENT_CHAR;
  snapshot.dom_key = CEF_DOM_KEY_UNICODE_FLAG | 0x1f600;
  snapshot.text[0] = 0xd83d;
  snapshot.text[1] = 0xde00;
  blink::WebKeyboardEvent web;
  const auto before = base::TimeTicks::Now();
  ASSERT_TRUE(GetWebKeyboardInputEvent(snapshot, web));
  EXPECT_EQ(0, web.dom_code);
  EXPECT_EQ(snapshot.dom_key, static_cast<uint32_t>(web.dom_key));
  EXPECT_GE(web.TimeStamp(), before);
  EXPECT_LE(web.TimeStamp(), base::TimeTicks::Now());
}

TEST(KeyboardInputEventUtil, InvalidSnapshotsLeaveOutputUnchanged) {
  blink::WebKeyboardEvent output(blink::WebInputEvent::Type::kKeyUp, 0,
                                 base::TimeTicks::FromInternalValue(42));
  const auto original = output;
  CefKeyboardInputEvent valid;
  valid.type = CEF_KEYEVENT_RAWKEYDOWN;
  valid.dom_code = CEF_DOM_CODE_US_A;
  valid.dom_key = CEF_DOM_KEY_UNICODE_FLAG | 'a';
  for (int invalid = 0; invalid < 6; ++invalid) {
    CefKeyboardInputEvent snapshot(valid);
    switch (invalid) {
      case 0:
        snapshot.type = static_cast<cef_key_event_type_t>(100);
        break;
      case 1:
        snapshot.modifiers = 0x80000000u;
        break;
      case 2:
        snapshot.dom_code = 0xffffffffu;
        break;
      case 3:
        snapshot.timestamp = -1;
        break;
      case 4:
        snapshot.is_system_key = 2;
        break;
      case 5:
        snapshot.dom_key = 'a';  // Missing Chromium's DomKey type encoding.
        break;
    }
    EXPECT_FALSE(GetWebKeyboardInputEvent(snapshot, output));
    ExpectEqual(original, output);
  }
  CefKeyboardInputEvent snapshot(valid);
  output.SetType(blink::WebInputEvent::Type::kUndefined);
  EXPECT_FALSE(GetCefKeyboardInputEvent(output, snapshot));
  EXPECT_EQ(valid.dom_key, snapshot.dom_key);
  EXPECT_EQ(valid.type, snapshot.type);
}

TEST(KeyboardInputEventUtil, TextBuffersRequireTerminators) {
  for (const auto type : {CEF_KEYEVENT_RAWKEYDOWN, CEF_KEYEVENT_KEYDOWN,
                          CEF_KEYEVENT_KEYUP, CEF_KEYEVENT_CHAR}) {
    for (bool unmodified : {false, true}) {
      CefKeyboardInputEvent snapshot;
      snapshot.type = type;
      auto& buffer = unmodified ? snapshot.unmodified_text : snapshot.text;
      std::fill(std::begin(buffer), std::end(buffer), u'x');

      blink::WebKeyboardEvent output(blink::WebInputEvent::Type::kKeyUp, 0,
                                     base::TimeTicks::FromInternalValue(42));
      const auto original = output;
      EXPECT_FALSE(GetWebKeyboardInputEvent(snapshot, output));
      ExpectEqual(original, output);

      // A terminator anywhere in the buffer is sufficient. Preserve trailing
      // units, and accept the maximum three units of text before a terminator.
      for (size_t terminator = 0; terminator < std::size(buffer);
           ++terminator) {
        std::fill(std::begin(buffer), std::end(buffer), u'x');
        buffer[terminator] = 0;
        ASSERT_TRUE(GetWebKeyboardInputEvent(snapshot, output));
        const auto& actual = unmodified ? output.unmodified_text : output.text;
        EXPECT_TRUE(
            std::equal(std::begin(buffer), std::end(buffer), actual.begin()));
      }
    }
  }
}

TEST(KeyboardInputEventUtil, ChromiumKeyTablesMatchPublicValues) {
  const struct {
    uint32_t cef;
    uint32_t chromium;
  } codes[] =
#define DOM_CODE_DECLARATION
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  {                                                   \
    static_cast<uint32_t>(CEF_DOM_CODE_##id),         \
        static_cast<uint32_t>(ui::DomCode::id)        \
  }
#include "ui/events/keycodes/dom/dom_code_data.inc"
#undef DOM_CODE
#undef DOM_CODE_DECLARATION
      for (const auto& code : codes) EXPECT_EQ(code.chromium, code.cef);
#define DOM_KEY_MAP_DECLARATION_START
#define DOM_KEY_UNI(key, id, value)                \
  EXPECT_EQ(static_cast<uint32_t>(ui::DomKey::id), \
            static_cast<uint32_t>(CEF_DOM_KEY_##id));
#define DOM_KEY_MAP(key, id, value) DOM_KEY_UNI(key, id, value)
#define DOM_KEY_MAP_DECLARATION_END
#include "ui/events/keycodes/dom/dom_key_data.inc"
#undef DOM_KEY_MAP_DECLARATION_END
#undef DOM_KEY_MAP
#undef DOM_KEY_UNI
#undef DOM_KEY_MAP_DECLARATION_START
}

TEST(KeyboardInputEventUtil, StructSizesAndBufferOwnership) {
  struct Older {
    size_t size;
    cef_key_event_type_t type;
    uint32_t modifiers;
    int64_t timestamp;
  } older = {sizeof(Older), CEF_KEYEVENT_KEYUP, CEF_EVENTFLAG_IS_RIGHT,
             123000000};
  CefKeyboardInputEvent event;
  event.dom_key = CEF_DOM_KEY_ENTER;
  event.text[0] = 'x';
  event.Set(*reinterpret_cast<const cef_keyboard_input_event_t*>(&older), true);
  EXPECT_EQ(CEF_KEYEVENT_KEYUP, event.type);
  EXPECT_EQ(CEF_EVENTFLAG_IS_RIGHT, event.modifiers);
  EXPECT_EQ(0U, event.dom_key);
  EXPECT_EQ(0, event.text[0]);
  struct Newer {
    cef_keyboard_input_event_t event;
    uint64_t future;
  } newer = {};
  newer.event.size = sizeof(newer);
  newer.event.text[0] = 'x';
  newer.future = 0xdeadbeef;
  event.AttachTo(newer.event);
  event.text[0] = 'y';
  event.DetachTo(newer.event);
  EXPECT_EQ(sizeof(cef_keyboard_input_event_t), newer.event.size);
  EXPECT_EQ('y', newer.event.text[0]);
  EXPECT_EQ(0xdeadbeefU, newer.future);
}

TEST(KeyboardInputEventUtil, ContextSizeCopiesAndSelfAssignment) {
  struct Older {
    size_t size;
    int focus_on_editable_field;
  } older = {sizeof(Older), 1};
  CefInputEventContext context;
  context.os_event = reinterpret_cast<cef_event_handle_t>(uintptr_t{0x1234});
  context.Set(*reinterpret_cast<const cef_input_event_context_t*>(&older),
              false);
  EXPECT_EQ(sizeof(cef_input_event_context_t), context.size);
  EXPECT_EQ(1, context.focus_on_editable_field);
  EXPECT_EQ(nullptr, context.os_event);

  const cef_input_event_context_t& self = context;
  context = self;
  EXPECT_EQ(sizeof(cef_input_event_context_t), context.size);
  EXPECT_EQ(1, context.focus_on_editable_field);

  struct Newer {
    cef_input_event_context_t context;
    uint64_t future;
  } newer = {};
  newer.context.size = sizeof(newer);
  newer.context.focus_on_editable_field = 1;
  newer.future = 0xdeadbeef;
  const CefInputEventContext copy(newer.context);
  EXPECT_EQ(sizeof(cef_input_event_context_t), copy.size);
  EXPECT_EQ(1, copy.focus_on_editable_field);
  EXPECT_EQ(0xdeadbeefU, newer.future);
}
