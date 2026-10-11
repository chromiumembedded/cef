// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/browser_event_util.h"

#include "components/input/native_web_keyboard_event.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if !BUILDFLAG(IS_MAC)
input::NativeWebKeyboardEvent CreateNativeKeyboardInputEvent(
    const blink::WebKeyboardEvent& event,
    gfx::NativeView native_view) {
  if (!event.dom_code ||
      event.native_key_code == ui::KeycodeConverter::InvalidNativeKeycode()) {
    input::NativeWebKeyboardEvent result(event.GetType(), event.GetModifiers(),
                                         event.TimeStamp());
    static_cast<blink::WebKeyboardEvent&>(result) = event;
    return result;
  }
  return input::NativeWebKeyboardEvent(event, native_view);
}
#endif
