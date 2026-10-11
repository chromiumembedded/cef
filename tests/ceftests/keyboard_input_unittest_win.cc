// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>
#include <vector>

#include "include/cef_input_event_utils.h"
#include "tests/gtest/include/gtest/gtest.h"

#if CEF_API_ADDED(CEF_NEXT)
namespace {

class ScopedKeyboardLayout {
 public:
  explicit ScopedKeyboardLayout(const wchar_t* name)
      : previous_(GetKeyboardLayout(0)) {
    const int count = GetKeyboardLayoutList(0, nullptr);
    EXPECT_GT(count, 0);
    if (count <= 0) {
      return;
    }
    std::vector<HKL> loaded(count);
    EXPECT_EQ(count, GetKeyboardLayoutList(count, loaded.data()));
    layout_ = LoadKeyboardLayoutW(name, KLF_NOTELLSHELL);
    unload_ = std::find(loaded.begin(), loaded.end(), layout_) == loaded.end();
  }

  ~ScopedKeyboardLayout() {
    if (GetKeyboardLayout(0) != previous_) {
      ActivateKeyboardLayout(previous_, 0);
    }
    if (layout_ && unload_) {
      UnloadKeyboardLayout(layout_);
    }
  }

  HKL get() const { return layout_; }

 private:
  const HKL previous_;
  HKL layout_ = nullptr;
  bool unload_ = false;
};

TEST(KeyboardInputTest, WindowsAltGraphModifierUsesExplicitLayout) {
  ScopedKeyboardLayout swedish(L"0000041D"), us(L"00000409");
  ASSERT_NE(nullptr, swedish.get());
  ASSERT_NE(nullptr, us.get());
  ASSERT_NE(nullptr, ActivateKeyboardLayout(us.get(), 0));
  ASSERT_EQ(us.get(), GetKeyboardLayout(0));
  std::vector<int> state(256);
  state[VK_CONTROL] = state[VK_LCONTROL] = state[VK_MENU] = state[VK_RMENU] =
      0x80;
  for (const auto& key : {std::pair{VK_MENU, 0x138}, std::pair{VK_LEFT, 0x14b},
                          std::pair{int{'A'}, 0x01e}}) {
    SCOPED_TRACE(key.first);
    const int64_t bits = 1 | ((key.second & 0xff) << 16) |
                         ((key.second & 0x100) ? (1LL << 24) : 0);
    for (UINT message : {WM_SYSKEYDOWN, WM_SYSKEYUP}) {
      CefKeyboardInputEvent event;
      ASSERT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
          message, key.first, bits, state,
          reinterpret_cast<uintptr_t>(swedish.get()), 123000000, event));
      EXPECT_TRUE(event.modifiers & CEF_EVENTFLAG_ALTGR_DOWN);
      EXPECT_FALSE(event.modifiers &
                   (CEF_EVENTFLAG_CONTROL_DOWN | CEF_EVENTFLAG_ALT_DOWN));
      if (key.first == VK_MENU) {
        EXPECT_EQ(CEF_DOM_KEY_ALT_GRAPH, event.dom_key);
        EXPECT_EQ(CEF_DOM_CODE_ALT_RIGHT, event.dom_code);
        EXPECT_TRUE(event.modifiers & CEF_EVENTFLAG_IS_RIGHT);
      }
      ASSERT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
          message, key.first, bits, state,
          reinterpret_cast<uintptr_t>(us.get()), 123000000, event));
      EXPECT_FALSE(event.modifiers & CEF_EVENTFLAG_ALTGR_DOWN);
      EXPECT_TRUE(event.modifiers & CEF_EVENTFLAG_CONTROL_DOWN);
      EXPECT_TRUE(event.modifiers & CEF_EVENTFLAG_ALT_DOWN);
    }
  }
  // Ctrl+Left Alt does not change the modifier identity of a non-printing key.
  state[VK_RMENU] = 0;
  state[VK_LMENU] = 0x80;
  CefKeyboardInputEvent event;
  ASSERT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
      WM_KEYDOWN, VK_LEFT, 1 | (0x4b << 16) | (1LL << 24), state,
      reinterpret_cast<uintptr_t>(swedish.get()), 123000000, event));
  EXPECT_FALSE(event.modifiers & CEF_EVENTFLAG_ALTGR_DOWN);
  EXPECT_TRUE(event.modifiers & CEF_EVENTFLAG_CONTROL_DOWN);
  EXPECT_TRUE(event.modifiers & CEF_EVENTFLAG_ALT_DOWN);
  // A released Right Alt still has AltGraph key identity, but is no longer
  // held.
  std::fill(state.begin(), state.end(), 0);
  ASSERT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
      WM_SYSKEYUP, VK_MENU,
      1 | (0x38 << 16) | (1LL << 24) | (1LL << 30) | (1LL << 31), state,
      reinterpret_cast<uintptr_t>(swedish.get()), 123000000, event));
  EXPECT_EQ(CEF_DOM_KEY_ALT_GRAPH, event.dom_key);
  EXPECT_FALSE(event.modifiers & CEF_EVENTFLAG_ALTGR_DOWN);
}

TEST(KeyboardInputTest, WindowsInternationalDeadKeyAndComposedCharacter) {
  ScopedKeyboardLayout international(L"00020409");
  ASSERT_NE(nullptr, international.get());
  const auto layout = reinterpret_cast<uintptr_t>(international.get());
  const std::vector<int> state(256);
  CefKeyboardInputEvent event;
  // Resolve the dead key without priming the desktop's native composition
  // buffer. Cache construction follows Chromium's normal translation behavior.
  for (UINT message : {WM_KEYDOWN, WM_KEYUP}) {
    ASSERT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
        message, VK_OEM_7, 1 | (0x28 << 16), state, layout, 123000000, event));
    EXPECT_EQ(CEF_DOM_CODE_QUOTE, event.dom_code);
    EXPECT_EQ(CEF_DOM_KEY_DEAD_FLAG | uint32_t{'\''}, event.dom_key);
    EXPECT_EQ(0, event.text[0]);
  }
  ASSERT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
      WM_KEYDOWN, 'A', 1 | (0x1e << 16), state, layout, 123000000, event));
  EXPECT_EQ(CEF_DOM_KEY_UNICODE_FLAG | uint32_t{'a'}, event.dom_key);
  // WM_CHAR carries the native composition result independently of the map.
  ASSERT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
      WM_CHAR, 0x00e1, 1 | (0x1e << 16), state, layout, 123000000, event));
  EXPECT_EQ(CEF_DOM_KEY_UNICODE_FLAG | 0x00e1u, event.dom_key);
  EXPECT_EQ(0x00e1, event.text[0]);
}

TEST(KeyboardInputTest, WindowsSwedishAltGraphAndControlAlt) {
  ScopedKeyboardLayout layout(L"0000041D");
  const HKL swedish = layout.get();
  ASSERT_NE(nullptr, swedish);
  const HKL active = GetKeyboardLayout(0);
  BYTE before[256], after[256];
  ASSERT_TRUE(GetKeyboardState(before));
  for (bool right_alt : {false, true}) {
    SCOPED_TRACE(right_alt ? "AltGr" : "Ctrl+Left Alt");
    std::vector<int> state(256);
    state[VK_CONTROL] = state[VK_LCONTROL] = state[VK_MENU] = 0x80;
    state[right_alt ? VK_RMENU : VK_LMENU] = 0x80;
    for (const auto type :
         {CEF_KEYEVENT_RAWKEYDOWN, CEF_KEYEVENT_CHAR, CEF_KEYEVENT_KEYUP}) {
      const UINT message = type == CEF_KEYEVENT_RAWKEYDOWN ? WM_KEYDOWN
                           : type == CEF_KEYEVENT_CHAR     ? WM_CHAR
                                                           : WM_KEYUP;
      const int64_t bits =
          1 | (0x03 << 16) |
          (type == CEF_KEYEVENT_KEYUP ? (1LL << 30) | (1LL << 31) : 0);
      CefKeyboardInputEvent event;
      ASSERT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
          message, type == CEF_KEYEVENT_CHAR ? '@' : '2', bits, state,
          reinterpret_cast<uintptr_t>(swedish), 123000000, event));
      EXPECT_EQ(type, event.type);
      EXPECT_EQ(CEF_DOM_CODE_DIGIT2, event.dom_code);
      EXPECT_EQ(CEF_DOM_KEY_UNICODE_FLAG | uint32_t{'@'}, event.dom_key);
      EXPECT_EQ(CEF_EVENTFLAG_ALTGR_DOWN, event.modifiers);
      EXPECT_FALSE(event.is_system_key);
      if (type == CEF_KEYEVENT_CHAR) {
        EXPECT_EQ('@', event.text[0]);
      }
    }
  }
  ASSERT_TRUE(GetKeyboardState(after));
  EXPECT_EQ(0, std::memcmp(before, after, sizeof(before)));
  EXPECT_EQ(active, GetKeyboardLayout(0));
}

TEST(KeyboardInputTest, WindowsExplicitStateAndCharacterUnits) {
  BYTE before[256], after[256];
  ASSERT_TRUE(GetKeyboardState(before));
  const HKL original_layout = GetKeyboardLayout(0);
  std::vector<int> state(256);
  state[VK_SHIFT] = 0x80;
  state[VK_CAPITAL] = state[VK_NUMLOCK] = 1;
  CefKeyboardInputEvent event;
  const auto layout = reinterpret_cast<uintptr_t>(GetKeyboardLayout(0));
  EXPECT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
      WM_SYSKEYDOWN, VK_RSHIFT, (0x36 << 16) | (1LL << 30), state, layout,
      123000000, event));
  EXPECT_EQ(CEF_DOM_CODE_SHIFT_RIGHT, event.dom_code);
  EXPECT_EQ(CEF_DOM_KEY_SHIFT, event.dom_key);
  EXPECT_EQ(static_cast<uint32_t>(
                CEF_EVENTFLAG_IS_RIGHT | CEF_EVENTFLAG_IS_REPEAT |
                CEF_EVENTFLAG_SHIFT_DOWN | CEF_EVENTFLAG_CAPS_LOCK_ON |
                CEF_EVENTFLAG_NUM_LOCK_ON),
            event.modifiers);
  EXPECT_TRUE(event.is_system_key);
  EXPECT_EQ(123000000, event.timestamp);
  for (uint32_t unit : {0xd83dU, 0xde00U}) {
    EXPECT_TRUE(CefKeyboardInputEventFromWindowsMessageEx(
        WM_CHAR, unit, 1 | (0x1e << 16), std::vector<int>(256), layout,
        123000000, event));
    EXPECT_EQ(CEF_KEYEVENT_CHAR, event.type);
    EXPECT_EQ(unit, event.text[0]);
    EXPECT_EQ(0, event.text[1]);
    EXPECT_EQ(CEF_DOM_CODE_US_A, event.dom_code);
  }
  std::array<unsigned char, sizeof(cef_keyboard_input_event_t)> unchanged;
  std::memcpy(unchanged.data(), &event, unchanged.size());
  EXPECT_FALSE(CefKeyboardInputEventFromWindowsMessageEx(
      WM_MOUSEMOVE, 0, 0, state, layout, 0, event));
  EXPECT_EQ(0, std::memcmp(unchanged.data(), &event, unchanged.size()));
  state[1] = 256;
  EXPECT_FALSE(CefKeyboardInputEventFromWindowsMessageEx(
      WM_KEYDOWN, 'A', 0, state, layout, 0, event));
  EXPECT_EQ(0, std::memcmp(unchanged.data(), &event, unchanged.size()));
  ASSERT_TRUE(GetKeyboardState(after));
  EXPECT_EQ(0, std::memcmp(before, after, sizeof(before)));
  EXPECT_EQ(original_layout, GetKeyboardLayout(0));
}

}  // namespace
#endif  // CEF_API_ADDED(CEF_NEXT)
