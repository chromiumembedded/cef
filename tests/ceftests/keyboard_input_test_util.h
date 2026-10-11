// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_KEYBOARD_INPUT_TEST_UTIL_H_
#define CEF_TESTS_CEFTESTS_KEYBOARD_INPUT_TEST_UTIL_H_
#pragma once

#include <array>
#include <vector>

#include "include/cef_browser.h"

#if CEF_API_ADDED(CEF_NEXT)
#if defined(OS_MAC) || defined(CEF_TESTS_USE_GTK)
// Convert native source input on the UI/source thread, before injection.
bool BuildNativeKeyboardInputEvents(
    std::array<CefKeyboardInputEvent, 3>& events);
#endif
#if defined(OS_MAC)
enum class MacTextClientInput {
  Character,
  OptionCharacter,
  Return,
  ShiftReturn,
  ControlCharacter,
  CommandCharacter,
  NonInsertionCommand,
  Clear,
};
bool BuildMacTextClientKeyboardInputEvents(
    MacTextClientInput input,
    std::vector<CefKeyboardInputEvent>& events);
void SendMacTextClientKeyboardInput(
    CefRefPtr<CefBrowser> browser,
    MacTextClientInput input,
    const std::vector<CefKeyboardInputEvent>& events);
#endif
#endif  // CEF_API_ADDED(CEF_NEXT)

#endif  // CEF_TESTS_CEFTESTS_KEYBOARD_INPUT_TEST_UTIL_H_
