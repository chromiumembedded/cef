// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_KEY_EVENT_UTIL_H_
#define CEF_LIBCEF_BROWSER_KEY_EVENT_UTIL_H_
#pragma once

#include "base/time/time.h"
#include "build/build_config.h"
#include "cef/include/internal/cef_types_wrappers.h"

namespace input {
struct NativeWebKeyboardEvent;
}
namespace ui {
class KeyEvent;
}

// DEPRECATED: These conversions preserve the legacy SendKeyEvent semantics.
// Use GetWebKeyboardInputEvent for resolved portable input.
#if BUILDFLAG(IS_MAC)
input::NativeWebKeyboardEvent GetLegacyWebKeyEvent(const CefKeyEvent& event,
                                                   double timestamp);
#else
int GetLegacyUiEventModifiers(uint32_t modifiers);
ui::KeyEvent GetLegacyUiKeyEvent(const CefKeyEvent& event,
                                 base::TimeTicks timestamp);
input::NativeWebKeyboardEvent GetLegacyWebKeyEvent(const CefKeyEvent& event,
                                                   base::TimeTicks timestamp);
#endif  // BUILDFLAG(IS_MAC)

#endif  // CEF_LIBCEF_BROWSER_KEY_EVENT_UTIL_H_
