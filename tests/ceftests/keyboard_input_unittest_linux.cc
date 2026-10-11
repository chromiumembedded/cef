// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "tests/ceftests/keyboard_input_test_util.h"

#if CEF_API_ADDED(CEF_NEXT) && defined(CEF_TESTS_USE_GTK)
#include <gdk/gdkkeysyms-compat.h>
#include <gtk/gtk.h>

#include <string>

#if defined(GDK_WINDOWING_WAYLAND)
#include <gdk/gdkwayland.h>
#endif  // defined(GDK_WINDOWING_WAYLAND)

#include "include/cef_input_event_utils.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/osr_keyboard_util_gtk.h"
#include "tests/shared/browser/util_linux.h"

namespace {

bool InitGtk() {
  if (gdk_display_get_default()) {
    return true;
  }

  // Match the Ozone platform, including switches added during CEF startup.
  const char* backend =
      client::IsRunningOnWayland(CefCommandLine::GetGlobalCommandLine())
          ? "wayland"
          : "x11";
  gdk_set_allowed_backends(backend);

  // GDK_BACKEND takes precedence over the allowed-backend setting. Override it
  // while opening the display, then restore it for subsequently launched apps.
  const char* previous_backend = g_getenv("GDK_BACKEND");
  const bool had_backend = previous_backend != nullptr;
  const std::string saved_backend = had_backend ? previous_backend : "";
  g_setenv("GDK_BACKEND", backend, true);
  const bool initialized = gtk_init_check(nullptr, nullptr);
  if (had_backend) {
    g_setenv("GDK_BACKEND", saved_backend.c_str(), true);
  } else {
    g_unsetenv("GDK_BACKEND");
  }
  return initialized;
}

// With --ozone-platform=x11, the GTK keyboard adapter must use the Xkb*
// functions for accurate modifier state; the GDK keymap functions are not a
// reliable substitute (see KeyboardInputEventsFromGdkEvent). Without
// CEF_TESTS_USE_X11 the adapter therefore does not support X11 displays, and
// these tests skip.
#define SKIP_IF_KEYBOARD_INPUT_UNSUPPORTED()                          \
  if (!IsNativeKeyboardInputSupported()) {                            \
    GTEST_SKIP() << "GTK keyboard adapter does not support this GDK " \
                    "backend";                                        \
  }

GdkEventKey NativeKey(GdkEventType type,
                      guint keyval,
                      guint16 code,
                      guint state = 0) {
  GdkEventKey event = {};
  event.type = type;
  event.window = gdk_get_default_root_window();
  event.keyval = keyval;
  event.hardware_keycode = code;
  event.state = state;
  return event;
}

void LinuxNativeKeyIdentityRepeatAndCharacterLocationImpl() {
  ASSERT_TRUE(InitGtk());
  SKIP_IF_KEYBOARD_INPUT_UNSUPPORTED();
  auto native = NativeKey(GDK_KEY_PRESS, GDK_a, 38);
  std::vector<CefKeyboardInputEvent> events;
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&native, true, events));
  ASSERT_EQ(2U, events.size());
  EXPECT_EQ(CEF_KEYEVENT_RAWKEYDOWN, events[0].type);
  EXPECT_EQ(CEF_DOM_CODE_US_A, events[0].dom_code);
  EXPECT_EQ(CEF_DOM_KEY_UNICODE_FLAG | uint32_t{'a'}, events[0].dom_key);
  EXPECT_EQ('A', events[0].windows_key_code);
  EXPECT_TRUE(events[0].modifiers & CEF_EVENTFLAG_IS_REPEAT);
  EXPECT_EQ(CEF_KEYEVENT_CHAR, events[1].type);
  EXPECT_EQ('a', events[1].windows_key_code);
  EXPECT_FALSE(events[1].modifiers &
               (CEF_EVENTFLAG_IS_REPEAT | CEF_EVENTFLAG_IS_KEY_PAD));
  native.type = GDK_KEY_RELEASE;
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&native, false, events));
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(CEF_KEYEVENT_KEYUP, events[0].type);
  EXPECT_EQ(CEF_DOM_CODE_US_A, events[0].dom_code);
  EXPECT_FALSE(events[0].modifiers & CEF_EVENTFLAG_IS_REPEAT);
}

void LinuxNativeModifiersKeypadDeadAndUnicodeImpl() {
  ASSERT_TRUE(InitGtk());
  SKIP_IF_KEYBOARD_INPUT_UNSUPPORTED();
  std::vector<CefKeyboardInputEvent> events;
  auto left = NativeKey(GDK_KEY_RELEASE, GDK_Shift_L, 50);
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&left, false, events));
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(CEF_DOM_CODE_SHIFT_LEFT, events[0].dom_code);
  EXPECT_TRUE(events[0].modifiers & CEF_EVENTFLAG_IS_LEFT);
  auto right = NativeKey(GDK_KEY_PRESS, GDK_Shift_R, 62, GDK_SHIFT_MASK);
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&right, false, events));
  EXPECT_EQ(CEF_DOM_CODE_SHIFT_RIGHT, events[0].dom_code);
  EXPECT_TRUE(events[0].modifiers & CEF_EVENTFLAG_IS_RIGHT);
  auto keypad = NativeKey(GDK_KEY_PRESS, GDK_KP_1, 87);
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&keypad, false, events));
  ASSERT_EQ(2U, events.size());
  EXPECT_EQ(CEF_DOM_CODE_NUMPAD1, events[0].dom_code);
  for (const auto& event : events) {
    EXPECT_TRUE(event.modifiers & CEF_EVENTFLAG_IS_KEY_PAD);
  }
  auto dead = NativeKey(GDK_KEY_PRESS, GDK_dead_acute, 48);
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&dead, false, events));
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(CEF_DOM_KEY_DEAD_FLAG | 0x0301u, events[0].dom_key);
  EXPECT_EQ(0, events[0].text[0]);
  auto unicode = NativeKey(GDK_KEY_PRESS, gdk_unicode_to_keyval(0x1f600), 38);
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&unicode, false, events));
  ASSERT_EQ(2U, events.size());
  EXPECT_EQ(CEF_DOM_KEY_UNICODE_FLAG | 0x1f600u, events[0].dom_key);
  EXPECT_EQ(0xd83d, events[1].text[0]);
  EXPECT_EQ(0xde00, events[1].text[1]);
  EXPECT_EQ(0, events[1].text[2]);
  // Modifier assignments belong to this native connection, not a fixed
  // assumption that Mod2/Mod5 mean NumLock/AltGraph.
  auto alt_graph = NativeKey(GDK_KEY_PRESS, GDK_ISO_Level3_Shift, 108);
  ASSERT_TRUE(
      client::KeyboardInputEventsFromGdkEvent(&alt_graph, false, events));
  EXPECT_EQ(CEF_DOM_KEY_ALT_GRAPH, events[0].dom_key);
  EXPECT_TRUE(events[0].modifiers & CEF_EVENTFLAG_ALTGR_DOWN);
}

void LinuxInvalidInputLeavesOutputUnchangedImpl() {
  std::vector<CefKeyboardInputEvent> events(1);
  events[0].dom_key = CEF_DOM_KEY_ENTER;
  EXPECT_FALSE(client::KeyboardInputEventsFromGdkEvent(nullptr, false, events));
  GdkEventKey native = {};
  native.type = GDK_NOTHING;
  EXPECT_FALSE(client::KeyboardInputEventsFromGdkEvent(&native, false, events));
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(CEF_DOM_KEY_ENTER, events[0].dom_key);
}

void LinuxNativeShortcutAndReturnTextImpl() {
  ASSERT_TRUE(InitGtk());
  SKIP_IF_KEYBOARD_INPUT_UNSUPPORTED();
  std::vector<CefKeyboardInputEvent> events;
  auto native = NativeKey(GDK_KEY_PRESS, GDK_a, 38, GDK_CONTROL_MASK);
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&native, false, events));
  ASSERT_EQ(2U, events.size());
  EXPECT_EQ(CEF_DOM_KEY_UNICODE_FLAG | uint32_t{'a'}, events[0].dom_key);
  EXPECT_EQ(1, events[0].text[0]);
  EXPECT_EQ(1, events[1].text[0]);
  EXPECT_EQ(u'a', events[0].unmodified_text[0]);
  EXPECT_EQ(u'a', events[1].unmodified_text[0]);
  EXPECT_FALSE(events[0].is_system_key);

  // Shortcut modifiers must not change the source's logical key or the
  // unmodified text, while Alt still marks the system-key path.
  native.state = GDK_MOD1_MASK;
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&native, false, events));
  ASSERT_EQ(2U, events.size());
  EXPECT_EQ(CEF_DOM_KEY_UNICODE_FLAG | uint32_t{'a'}, events[0].dom_key);
  EXPECT_EQ(u'a', events[0].text[0]);
  EXPECT_EQ(u'a', events[0].unmodified_text[0]);
  EXPECT_TRUE(events[0].is_system_key);
  EXPECT_TRUE(events[1].is_system_key);
  // GTK excludes Mod1 from virtual-modifier expansion on both backends.
  EXPECT_FALSE(events[0].modifiers & CEF_EVENTFLAG_COMMAND_DOWN);
  EXPECT_FALSE(events[1].modifiers & CEF_EVENTFLAG_COMMAND_DOWN);

  // Return uses carriage return for insertion; Ctrl+Return uses line feed.
  native = NativeKey(GDK_KEY_PRESS, GDK_Return, 36);
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&native, false, events));
  ASSERT_EQ(2U, events.size());
  EXPECT_EQ(CEF_DOM_KEY_ENTER, events[0].dom_key);
  EXPECT_EQ(CEF_DOM_CODE_ENTER, events[0].dom_code);
  EXPECT_EQ(u'\r', events[0].text[0]);
  EXPECT_EQ(u'\r', events[1].text[0]);
  EXPECT_EQ(u'\r', events[0].unmodified_text[0]);
  native.state = GDK_CONTROL_MASK;
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&native, false, events));
  ASSERT_EQ(2U, events.size());
  EXPECT_EQ(u'\n', events[0].text[0]);
  EXPECT_EQ(u'\n', events[1].text[0]);
  EXPECT_EQ(u'\r', events[0].unmodified_text[0]);
}

#if defined(GDK_WINDOWING_WAYLAND)
void LinuxWaylandModifierStateImpl() {
  ASSERT_TRUE(InitGtk());
  auto* display = gdk_display_get_default();
  if (!GDK_IS_WAYLAND_DISPLAY(display)) {
    GTEST_SKIP() << "Run with --ozone-platform=wayland";
  }

  auto* keymap = gdk_keymap_get_for_display(display);
  const auto group_shift =
      gdk_keymap_get_modifier_mask(keymap, GDK_MODIFIER_INTENT_SHIFT_GROUP);
  std::vector<CefKeyboardInputEvent> events;

  // Ctrl+Alt must remain shortcut modifiers, not be mistaken for AltGraph.
  auto native =
      NativeKey(GDK_KEY_PRESS, GDK_a, 38, GDK_CONTROL_MASK | GDK_MOD1_MASK);
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&native, false, events));
  ASSERT_EQ(2U, events.size());
  EXPECT_FALSE(events[0].modifiers & CEF_EVENTFLAG_ALTGR_DOWN);
  EXPECT_TRUE(events[0].modifiers & CEF_EVENTFLAG_CONTROL_DOWN);
  EXPECT_TRUE(events[0].modifiers & CEF_EVENTFLAG_ALT_DOWN);
  EXPECT_EQ(1, events[1].text[0]);
  EXPECT_EQ(!!gdk_keymap_get_num_lock_state(keymap),
            !!(events[0].modifiers & CEF_EVENTFLAG_NUM_LOCK_ON));

  // GTK's group-shift modifier allows text even with Ctrl/Alt also present.
  if (!group_shift) {
    GTEST_SKIP() << "Keymap has no group-shift modifier";
  }
  native.state |= group_shift;
  native.keyval = GDK_at;
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&native, false, events));
  ASSERT_EQ(2U, events.size());
  EXPECT_TRUE(events[0].modifiers & CEF_EVENTFLAG_ALTGR_DOWN);
  EXPECT_EQ(u'@', events[0].text[0]);
  EXPECT_EQ(u'@', events[1].text[0]);
  EXPECT_EQ(CEF_DOM_KEY_UNICODE_FLAG | uint32_t{'@'}, events[0].dom_key);

  native.type = GDK_KEY_RELEASE;
  ASSERT_TRUE(client::KeyboardInputEventsFromGdkEvent(&native, false, events));
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ(CEF_KEYEVENT_KEYUP, events[0].type);
  EXPECT_TRUE(events[0].modifiers & CEF_EVENTFLAG_ALTGR_DOWN);
}
#endif  // defined(GDK_WINDOWING_WAYLAND)

}  // namespace

bool IsNativeKeyboardInputSupported() {
  return InitGtk() &&
         client::IsKeyboardInputSupportedForDisplay(gdk_display_get_default());
}

bool BuildNativeKeyboardInputEvents(
    std::array<CefKeyboardInputEvent, 3>& events) {
  if (!InitGtk()) {
    return false;
  }
  auto down = NativeKey(GDK_KEY_PRESS, GDK_a, 38);
  auto up = NativeKey(GDK_KEY_RELEASE, GDK_a, 38);
  std::vector<CefKeyboardInputEvent> converted;
  if (!client::KeyboardInputEventsFromGdkEvent(&down, false, converted) ||
      converted.size() != 2) {
    return false;
  }
  events[0] = converted[0];
  events[1] = converted[1];
  if (!client::KeyboardInputEventsFromGdkEvent(&up, false, converted) ||
      converted.size() != 1) {
    return false;
  }
  events[2] = converted[0];
  return true;
}

// This scalar mapping test needs GTK's keysym definitions, but no display or
// GTK initialization.
TEST(KeyboardInputTest, LinuxNamedAndDeadKeysymMappings) {
  EXPECT_EQ(CEF_DOM_KEY_PRINT_SCREEN,
            CefNonPrintableDomKeyFromXKeysym(GDK_Print));
  EXPECT_EQ(CEF_DOM_KEY_CONTROL,
            CefNonPrintableDomKeyFromXKeysym(GDK_Control_R));
  EXPECT_EQ(CEF_DOM_KEY_DEAD_FLAG | 0x0301u,
            CefNonPrintableDomKeyFromXKeysym(GDK_dead_acute));
  // Extended dead-key encodings and printable Unicode remain full scalars.
  EXPECT_EQ(CEF_DOM_KEY_DEAD_FLAG | 0x1f600u,
            CefNonPrintableDomKeyFromXKeysym(0x08000000u | 0x1f600u));
  EXPECT_EQ(CEF_DOM_KEY_NONE, CefNonPrintableDomKeyFromXKeysym(GDK_a));
  EXPECT_EQ(CEF_DOM_KEY_NONE,
            CefNonPrintableDomKeyFromXKeysym(gdk_unicode_to_keyval(0x1f600)));
  EXPECT_EQ(CEF_DOM_KEY_NONE, CefNonPrintableDomKeyFromXKeysym(0xffffffffu));
}

// Mouse and keyboard callers share this modifier conversion. Source-local
// modifier slots must not leak through as unrelated CEF flags.
TEST(KeyboardInputTest, LinuxSharedGtkStateModifiers) {
  EXPECT_EQ(CEF_EVENTFLAG_NONE, client::GetCefStateModifiers(0));
  EXPECT_EQ(CEF_EVENTFLAG_NONE,
            client::GetCefStateModifiers(GDK_MOD2_MASK | GDK_MOD5_MASK));
  EXPECT_EQ(
      CEF_EVENTFLAG_SHIFT_DOWN | CEF_EVENTFLAG_CAPS_LOCK_ON |
          CEF_EVENTFLAG_CONTROL_DOWN | CEF_EVENTFLAG_ALT_DOWN |
          CEF_EVENTFLAG_LEFT_MOUSE_BUTTON | CEF_EVENTFLAG_MIDDLE_MOUSE_BUTTON |
          CEF_EVENTFLAG_RIGHT_MOUSE_BUTTON,
      client::GetCefStateModifiers(
          GDK_SHIFT_MASK | GDK_LOCK_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK |
          GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK));
}

UI_THREAD_TEST(KeyboardInputTest,
               LinuxNativeKeyIdentityRepeatAndCharacterLocation)
UI_THREAD_TEST(KeyboardInputTest, LinuxNativeModifiersKeypadDeadAndUnicode)
UI_THREAD_TEST(KeyboardInputTest, LinuxInvalidInputLeavesOutputUnchanged)
UI_THREAD_TEST(KeyboardInputTest, LinuxNativeShortcutAndReturnText)
#if defined(GDK_WINDOWING_WAYLAND)
UI_THREAD_TEST(KeyboardInputTest, LinuxWaylandModifierState)
#endif  // defined(GDK_WINDOWING_WAYLAND)
#endif  // CEF_API_ADDED(CEF_NEXT) && defined(CEF_TESTS_USE_GTK)
