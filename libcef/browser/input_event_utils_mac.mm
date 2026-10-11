// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "cef/include/cef_input_event_utils.h"
#include "cef/libcef/browser/browser_event_util.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/input/web_input_event_builders_mac.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

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
  input::NativeWebKeyboardEvent result(event, native_view);
  NSEvent* native = result.os_event.Get();
  if ((event.GetModifiers() & blink::WebInputEvent::kIsAutoRepeat) &&
      native.type != NSEventTypeFlagsChanged) {
    result.os_event = base::apple::OwnedNSEvent([NSEvent
                   keyEventWithType:native.type
                           location:native.locationInWindow
                      modifierFlags:native.modifierFlags
                          timestamp:native.timestamp
                       windowNumber:native.windowNumber
                            context:nil
                         characters:native.characters
        charactersIgnoringModifiers:native.charactersIgnoringModifiers
                          isARepeat:YES
                            keyCode:native.keyCode]);
    [result.os_event.Get() eventRef];
  }
  return result;
}

bool CefKeyboardInputEventFromMacEvent(CefEventHandle native_event,
                                       CefKeyboardInputEvent& event) {
  return CefKeyboardInputEventFromMacEventEx(native_event, CefString(), -1,
                                             event);
}

bool CefKeyboardInputEventFromMacEventEx(CefEventHandle native_event,
                                         const CefString& input_source_id,
                                         int keyboard_type,
                                         CefKeyboardInputEvent& event) {
  if (![NSThread isMainThread]) {
    return false;
  }
  NSEvent* native = CAST_CEF_EVENT_HANDLE_TO_NSEVENT(native_event);
  if (!native ||
      (native.type != NSEventTypeKeyDown && native.type != NSEventTypeKeyUp &&
       native.type != NSEventTypeFlagsChanged)) {
    return false;
  }
  if (native.type != NSEventTypeFlagsChanged &&
      (native.characters.length >= blink::WebKeyboardEvent::kTextLengthCap ||
       native.charactersIgnoringModifiers.length >=
           blink::WebKeyboardEvent::kTextLengthCap)) {
    return false;
  }
  if (keyboard_type < -1 || keyboard_type > 255) {
    return false;
  }
  base::apple::ScopedCFTypeRef<CFArrayRef> sources;
  base::apple::ScopedCFTypeRef<TISInputSourceRef> detected_source;
  TISInputSourceRef source = nullptr;
  if (!input_source_id.empty()) {
    NSString* identifier =
        base::SysUTF16ToNSString(input_source_id.ToString16());
    if (!identifier) {
      return false;
    }
    NSDictionary* properties =
        @{(__bridge NSString*)kTISPropertyInputSourceID : identifier};
    sources.reset(
        TISCreateInputSourceList((__bridge CFDictionaryRef)properties, true));
    if (!sources || CFArrayGetCount(sources.get()) != 1) {
      return false;
    }
    source = static_cast<TISInputSourceRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(sources.get(), 0)));
    if (!TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData)) {
      return false;
    }
  } else {
    detected_source.reset(TISCopyCurrentKeyboardLayoutInputSource());
    source = detected_source.get();
  }
  if (!source) {
    return false;
  }
  if (keyboard_type == -1) {
    keyboard_type = LMGetKbdLast();
  }
  auto key = ui::DomKeyFromNSEvent(native, source, keyboard_type);
  if (key == ui::DomKey::NONE) {
    key = ui::DomKey::UNIDENTIFIED;
  }
  return GetCefKeyboardInputEvent(
      input::WebKeyboardEventBuilder::Build(native, static_cast<uint32_t>(key)),
      event);
}
