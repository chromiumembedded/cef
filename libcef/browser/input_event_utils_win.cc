// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <limits>
#include <memory>

#include "base/no_destructor.h"
#include "base/threading/thread_local.h"
#include "cef/include/cef_input_event_utils.h"
#include "cef/libcef/browser/browser_event_util.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/platform_key_map_win.h"
#include "ui/events/win/events_win_utils.h"

namespace {

struct LayoutCache {
  explicit LayoutCache(HKL source) : layout(source), map(source) {}
  HKL layout;
  ui::PlatformKeyMap map;
};

ui::PlatformKeyMap& KeyMap(HKL layout) {
  // Building the map probes all 256 virtual keys across 16 modifier
  // combinations with ToUnicodeEx. Cache the last layout per thread to avoid
  // repeating this work for every event, rebuilding when the layout changes.
  // Per-event keyboard state is not cached.
  static base::NoDestructor<base::ThreadLocalOwnedPointer<LayoutCache>> cache;
  if (!cache->Get() || cache->Get()->layout != layout) {
    cache->Set(std::make_unique<LayoutCache>(layout));
  }
  return cache->Get()->map;
}

bool Convert(uint32_t message,
             uint64_t wparam,
             int64_t lparam,
             const BYTE* state,
             HKL layout,
             int64_t timestamp,
             CefKeyboardInputEvent& event) {
  const bool character = message == WM_CHAR || message == WM_SYSCHAR;
  const bool keyup = message == WM_KEYUP || message == WM_SYSKEYUP;
  if (!character && !keyup && message != WM_KEYDOWN &&
      message != WM_SYSKEYDOWN) {
    return false;
  }
  if (wparam > 0xffff || timestamp < 0 || !layout) {
    return false;
  }
  if (static_cast<int64_t>(static_cast<LPARAM>(lparam)) != lparam) {
    return false;
  }
  CHROME_MSG native = {};
  native.message = message;
  native.wParam = static_cast<WPARAM>(wparam);
  native.lParam = static_cast<LPARAM>(lparam);
  int flags = 0;
  if (state[VK_SHIFT] & 0x80) {
    flags |= ui::EF_SHIFT_DOWN;
  }
  if ((state[VK_RMENU] & 0x80) && KeyMap(layout).UsesAltGraphForLayout()) {
    flags |= ui::EF_ALTGR_DOWN;
  } else {
    if (state[VK_CONTROL] & 0x80) {
      flags |= ui::EF_CONTROL_DOWN;
    }
    if (state[VK_MENU] & 0x80) {
      flags |= ui::EF_ALT_DOWN;
    }
  }
  if ((state[VK_LWIN] | state[VK_RWIN]) & 0x80) {
    flags |= ui::EF_COMMAND_DOWN;
  }
  if (state[VK_CAPITAL] & 1) {
    flags |= ui::EF_CAPS_LOCK_ON;
  }
  if (state[VK_NUMLOCK] & 1) {
    flags |= ui::EF_NUM_LOCK_ON;
  }
  if (lparam & (int64_t{1} << 24)) {
    flags |= ui::EF_IS_EXTENDED_KEY;
  }
  if (!character && !keyup && (lparam & (int64_t{1} << 30))) {
    flags |= ui::EF_IS_REPEAT;
  }
  const auto code = ui::CodeFromMSG(native);
  const auto key_code = ui::KeyboardCodeFromMSG(native);
  const auto time = timestamp ? base::TimeTicks::FromInternalValue(timestamp)
                              : base::TimeTicks::Now();
  if (character) {
    flags = ui::PlatformKeyMap::ReplaceControlAndAltWithAltGraph(flags);
    auto key = ui::KeyEvent::FromCharacter(static_cast<char16_t>(wparam),
                                           key_code, code, flags, time);
    auto web = ui::MakeWebKeyboardEvent(key);
    web.is_system_key = message == WM_SYSCHAR;
    return GetCefKeyboardInputEvent(web, event);
  }
  auto dom_key =
      KeyMap(layout).DomKeyFromKeyboardCodeForLayout(key_code, &flags);
  ui::KeyEvent key(
      keyup ? ui::EventType::kKeyReleased : ui::EventType::kKeyPressed,
      key_code, code, flags, dom_key, time);
  auto web = ui::MakeWebKeyboardEvent(key);
  web.is_system_key = message == WM_SYSKEYDOWN || message == WM_SYSKEYUP;
  return GetCefKeyboardInputEvent(web, event);
}

}  // namespace

bool CefKeyboardInputEventFromWindowsMessage(uint32_t message,
                                             uint64_t wparam,
                                             int64_t lparam,
                                             CefKeyboardInputEvent& event) {
  BYTE state[256];
  if (!GetKeyboardState(state)) {
    return false;
  }
  return Convert(message, wparam, lparam, state, GetKeyboardLayout(0), 0,
                 event);
}

bool CefKeyboardInputEventFromWindowsMessageEx(
    uint32_t message,
    uint64_t wparam,
    int64_t lparam,
    const std::vector<int>& keyboard_state,
    uint64_t keyboard_layout,
    int64_t timestamp,
    CefKeyboardInputEvent& event) {
  if (keyboard_state.size() != 256 ||
      keyboard_layout > std::numeric_limits<uintptr_t>::max()) {
    return false;
  }
  BYTE state[256];
  for (size_t i = 0; i < 256; ++i) {
    if (keyboard_state[i] < 0 || keyboard_state[i] > 255) {
      return false;
    }
    state[i] = static_cast<BYTE>(keyboard_state[i]);
  }
  HKL layout =
      keyboard_layout
          ? reinterpret_cast<HKL>(static_cast<uintptr_t>(keyboard_layout))
          : GetKeyboardLayout(0);
  return Convert(message, wparam, lparam, state, layout, timestamp, event);
}
