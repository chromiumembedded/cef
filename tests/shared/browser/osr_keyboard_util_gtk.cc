// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "tests/shared/browser/osr_keyboard_util_gtk.h"

#include <X11/XKBlib.h>
#include <gdk/gdkkeysyms-compat.h>
#include <gdk/gdkx.h>

#if defined(GDK_WINDOWING_WAYLAND)
#include <gdk/gdkwayland.h>
#endif  // defined(GDK_WINDOWING_WAYLAND)

#include <algorithm>
#include <iterator>
#include <utility>

#include "include/cef_input_event_utils.h"

namespace client {
int GetCefStateModifiers(guint state) {
  int modifiers = 0;
  if (state & GDK_SHIFT_MASK) {
    modifiers |= EVENTFLAG_SHIFT_DOWN;
  }
  if (state & GDK_LOCK_MASK) {
    modifiers |= EVENTFLAG_CAPS_LOCK_ON;
  }
  if (state & GDK_CONTROL_MASK) {
    modifiers |= EVENTFLAG_CONTROL_DOWN;
  }
  if (state & GDK_MOD1_MASK) {
    modifiers |= EVENTFLAG_ALT_DOWN;
  }
  if (state & GDK_BUTTON1_MASK) {
    modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
  }
  if (state & GDK_BUTTON2_MASK) {
    modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
  }
  if (state & GDK_BUTTON3_MASK) {
    modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
  }
  return modifiers;
}

#if CEF_API_ADDED(CEF_NEXT)
namespace {

uint32_t DomCodeFromXkbKeycode(uint32_t code) {
  static constexpr struct {
    uint32_t xkb;
    uint32_t dom;
  } mapping[] =
#define DOM_CODE_DECLARATION
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  {                                                   \
    xkb, CEF_DOM_CODE_##id                            \
  }
#include "include/base/internal/cef_dom_code_data.inc"
#undef DOM_CODE
#undef DOM_CODE_DECLARATION
      for (const auto& entry : mapping) if (entry.xkb == code) return entry.dom;
  return CEF_DOM_CODE_NONE;
}
void SetKeyboardText(uint32_t scalar, char16_t (&text)[4]) {
  std::fill(std::begin(text), std::end(text), 0);
  if (scalar <= 0xffff) {
    text[0] = static_cast<char16_t>(scalar);
  } else if (scalar <= 0x10ffff) {
    scalar -= 0x10000;
    text[0] = static_cast<char16_t>(0xd800 + (scalar >> 10));
    text[1] = static_cast<char16_t>(0xdc00 + (scalar & 0x3ff));
  }
}
int GetControlCharacter(int windows_key_code, bool shift) {
  if (windows_key_code >= 'A' && windows_key_code <= 'Z') {
    // ctrl-A ~ ctrl-Z map to \x01 ~ \x1A
    return windows_key_code - 'A' + 1;
  }
  if (shift) {
    // following graphics chars require shift key to input.
    switch (windows_key_code) {
      // ctrl-@ maps to \x00 (Null byte)
      case '2':
        return 0;
      // ctrl-^ maps to \x1E (Record separator, Information separator two)
      case '6':
        return 0x1E;
      // ctrl-_ maps to \x1F (Unit separator, Information separator one)
      case 0xbd /* VK_OEM_MINUS */:
        return 0x1F;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        return 0;
    }
  } else {
    switch (windows_key_code) {
      // ctrl-[ maps to \x1B (Escape)
      case 0xdb /* VK_OEM_4 */:
        return 0x1B;
      // ctrl-\ maps to \x1C (File separator, Information separator four)
      case 0xdc /* VK_OEM_5 */:
        return 0x1C;
      // ctrl-] maps to \x1D (Group separator, Information separator three)
      case 0xdd /* VK_OEM_6 */:
        return 0x1D;
      // ctrl-Enter maps to \x0A (Line feed)
      case 0x0d /* VK_RETURN */:
        return 0x0A;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        return 0;
    }
  }
}

}  // namespace

bool KeyboardInputEventsFromGdkEvent(
    const GdkEventKey* event,
    bool repeat,
    std::vector<CefKeyboardInputEvent>& result) {
  if (!event || !event->window ||
      (event->type != GDK_KEY_PRESS && event->type != GDK_KEY_RELEASE)) {
    return false;
  }
  auto* display = gdk_window_get_display(event->window);
  const bool is_x11 = GDK_IS_X11_DISPLAY(display);
  bool supported_backend = is_x11;
#if defined(GDK_WINDOWING_WAYLAND)
  supported_backend |= GDK_IS_WAYLAND_DISPLAY(display);
#endif  // defined(GDK_WINDOWING_WAYLAND)
  if (!supported_backend) {
    return false;
  }
  auto* keymap = gdk_keymap_get_for_display(display);
  std::vector<CefKeyboardInputEvent> events;
  CefKeyboardInputEvent key_event;
  key_event.type = event->type == GDK_KEY_PRESS ? CEF_KEYEVENT_RAWKEYDOWN
                                                : CEF_KEYEVENT_KEYUP;
  // GTK3's X11 and Wayland backends both use XKB hardware keycodes. Wayland
  // adds 8 to the protocol's evdev code before constructing GdkEventKey.
  key_event.dom_code = DomCodeFromXkbKeycode(event->hardware_keycode);
  const uint32_t unicode = gdk_keyval_to_unicode(event->keyval);
  key_event.dom_key = CefNonPrintableDomKeyFromXKeysym(event->keyval);
  if (key_event.dom_key == CEF_DOM_KEY_NONE) {
    key_event.dom_key =
        unicode ? CEF_DOM_KEY_UNICODE_FLAG | unicode : CEF_DOM_KEY_UNIDENTIFIED;
  }
  key_event.windows_key_code =
      CefWindowsKeyCodeFromDomCodeAndKey(key_event.dom_code, key_event.dom_key);
  key_event.modifiers = GetCefStateModifiers(event->state);
  if (repeat) {
    key_event.modifiers |= CEF_EVENTFLAG_IS_REPEAT;
  }
  GdkModifierType virtual_state = static_cast<GdkModifierType>(event->state);
  bool num_lock_on;
  guint alt_graph_mask;
  if (is_x11) {
    // Resolve the source X11 connection's actual modifier assignments. Mod2
    // and Mod5 are conventions, not guaranteed NumLock/AltGraph identities.
    auto* native_display = gdk_x11_display_get_xdisplay(display);
    num_lock_on =
        event->state & XkbKeysymToModifiers(native_display, GDK_Num_Lock);
    alt_graph_mask =
        XkbKeysymToModifiers(native_display, GDK_ISO_Level3_Shift) |
        XkbKeysymToModifiers(native_display, GDK_Mode_switch);
  } else {
    // Wayland has no Xlib connection. Use GTK's lock state and backend-specific
    // group-shift mask instead of guessing identities from Mod2/Mod5.
    num_lock_on = gdk_keymap_get_num_lock_state(keymap);
    alt_graph_mask =
        gdk_keymap_get_modifier_mask(keymap, GDK_MODIFIER_INTENT_SHIFT_GROUP);
  }
  gdk_keymap_add_virtual_modifiers(keymap, &virtual_state);
  if (virtual_state & (GDK_SUPER_MASK | GDK_META_MASK)) {
    key_event.modifiers |= CEF_EVENTFLAG_COMMAND_DOWN;
  }
  if (num_lock_on) {
    key_event.modifiers |= CEF_EVENTFLAG_NUM_LOCK_ON;
  }
  if (event->keyval >= GDK_KP_Space && event->keyval <= GDK_KP_9) {
    key_event.modifiers |= CEF_EVENTFLAG_IS_KEY_PAD;
  }
  switch (key_event.dom_code) {
    case CEF_DOM_CODE_SHIFT_LEFT:
    case CEF_DOM_CODE_CONTROL_LEFT:
    case CEF_DOM_CODE_ALT_LEFT:
    case CEF_DOM_CODE_META_LEFT:
      key_event.modifiers |= CEF_EVENTFLAG_IS_LEFT;
      break;
    case CEF_DOM_CODE_SHIFT_RIGHT:
    case CEF_DOM_CODE_CONTROL_RIGHT:
    case CEF_DOM_CODE_ALT_RIGHT:
    case CEF_DOM_CODE_META_RIGHT:
      key_event.modifiers |= CEF_EVENTFLAG_IS_RIGHT;
      break;
    default:
      break;
  }
  if ((event->state & alt_graph_mask) ||
      key_event.dom_key == CEF_DOM_KEY_ALT_GRAPH) {
    key_event.modifiers |= CEF_EVENTFLAG_ALTGR_DOWN;
  }
  key_event.is_system_key = !!(key_event.modifiers & CEF_EVENTFLAG_ALT_DOWN);
  // Chromium's unmodified text keeps Shift, but removes shortcut modifiers.
  guint unmodified = 0;
  gint group, level;
  GdkModifierType consumed;
  if (!gdk_keymap_translate_keyboard_state(
          keymap, event->hardware_keycode,
          static_cast<GdkModifierType>(event->state & GDK_SHIFT_MASK),
          event->group, &unmodified, &group, &level, &consumed)) {
    return false;
  }
  uint32_t unmodified_text = gdk_keyval_to_unicode(unmodified);
  if (key_event.windows_key_code == 0x0d /* VK_RETURN */) {
    unmodified_text = '\r';
  }
  SetKeyboardText(unmodified_text, key_event.unmodified_text);
  uint32_t insertion = unicode;
  if (key_event.windows_key_code == 0x0d /* VK_RETURN */) {
    insertion = '\r';
  }
  if ((key_event.modifiers & CEF_EVENTFLAG_CONTROL_DOWN) &&
      !(key_event.modifiers & CEF_EVENTFLAG_ALTGR_DOWN)) {
    insertion =
        GetControlCharacter(key_event.windows_key_code,
                            key_event.modifiers & CEF_EVENTFLAG_SHIFT_DOWN);
  }
  if ((key_event.dom_key & CEF_DOM_KEY_DEAD_FLAG) == CEF_DOM_KEY_DEAD_FLAG) {
    insertion = 0;
  }
  SetKeyboardText(insertion, key_event.text);
  events.push_back(key_event);
  if (event->type == GDK_KEY_PRESS && insertion) {
    key_event.type = CEF_KEYEVENT_CHAR;
    key_event.modifiers &= ~CEF_EVENTFLAG_IS_REPEAT;
    key_event.windows_key_code = key_event.text[0];
    events.push_back(key_event);
  }
  result = std::move(events);
  return true;
}
#endif  // CEF_API_ADDED(CEF_NEXT)

}  // namespace client
