// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/browser_window_osr_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms-compat.h>
#include <gdk/gdkx.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <algorithm>

#include "include/base/cef_logging.h"
#include "include/base/cef_macros.h"
#include "include/views/cef_display.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/cefclient/browser/util_gtk.h"
#include "tests/shared/browser/geometry_util.h"
#include "tests/shared/browser/main_message_loop.h"
#include "tests/shared/browser/osr_gl_linux.h"

namespace client {

namespace {

// Static BrowserWindowOsrGtk::EventFilter needs to forward touch events
// to correct browser, so we maintain a vector of all windows.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif
std::vector<BrowserWindowOsrGtk*> g_browser_windows;
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

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

// From ui/events/keycodes/keyboard_codes_posix.h.
enum KeyboardCode {
  VKEY_BACK = 0x08,
  VKEY_TAB = 0x09,
  VKEY_BACKTAB = 0x0A,
  VKEY_CLEAR = 0x0C,
  VKEY_RETURN = 0x0D,
  VKEY_SHIFT = 0x10,
  VKEY_CONTROL = 0x11,
  VKEY_MENU = 0x12,
  VKEY_PAUSE = 0x13,
  VKEY_CAPITAL = 0x14,
  VKEY_KANA = 0x15,
  VKEY_HANGUL = 0x15,
  VKEY_JUNJA = 0x17,
  VKEY_FINAL = 0x18,
  VKEY_HANJA = 0x19,
  VKEY_KANJI = 0x19,
  VKEY_ESCAPE = 0x1B,
  VKEY_CONVERT = 0x1C,
  VKEY_NONCONVERT = 0x1D,
  VKEY_ACCEPT = 0x1E,
  VKEY_MODECHANGE = 0x1F,
  VKEY_SPACE = 0x20,
  VKEY_PRIOR = 0x21,
  VKEY_NEXT = 0x22,
  VKEY_END = 0x23,
  VKEY_HOME = 0x24,
  VKEY_LEFT = 0x25,
  VKEY_UP = 0x26,
  VKEY_RIGHT = 0x27,
  VKEY_DOWN = 0x28,
  VKEY_SELECT = 0x29,
  VKEY_PRINT = 0x2A,
  VKEY_EXECUTE = 0x2B,
  VKEY_SNAPSHOT = 0x2C,
  VKEY_INSERT = 0x2D,
  VKEY_DELETE = 0x2E,
  VKEY_HELP = 0x2F,
  VKEY_0 = 0x30,
  VKEY_1 = 0x31,
  VKEY_2 = 0x32,
  VKEY_3 = 0x33,
  VKEY_4 = 0x34,
  VKEY_5 = 0x35,
  VKEY_6 = 0x36,
  VKEY_7 = 0x37,
  VKEY_8 = 0x38,
  VKEY_9 = 0x39,
  VKEY_A = 0x41,
  VKEY_B = 0x42,
  VKEY_C = 0x43,
  VKEY_D = 0x44,
  VKEY_E = 0x45,
  VKEY_F = 0x46,
  VKEY_G = 0x47,
  VKEY_H = 0x48,
  VKEY_I = 0x49,
  VKEY_J = 0x4A,
  VKEY_K = 0x4B,
  VKEY_L = 0x4C,
  VKEY_M = 0x4D,
  VKEY_N = 0x4E,
  VKEY_O = 0x4F,
  VKEY_P = 0x50,
  VKEY_Q = 0x51,
  VKEY_R = 0x52,
  VKEY_S = 0x53,
  VKEY_T = 0x54,
  VKEY_U = 0x55,
  VKEY_V = 0x56,
  VKEY_W = 0x57,
  VKEY_X = 0x58,
  VKEY_Y = 0x59,
  VKEY_Z = 0x5A,
  VKEY_LWIN = 0x5B,
  VKEY_COMMAND = VKEY_LWIN,  // Provide the Mac name for convenience.
  VKEY_RWIN = 0x5C,
  VKEY_APPS = 0x5D,
  VKEY_SLEEP = 0x5F,
  VKEY_NUMPAD0 = 0x60,
  VKEY_NUMPAD1 = 0x61,
  VKEY_NUMPAD2 = 0x62,
  VKEY_NUMPAD3 = 0x63,
  VKEY_NUMPAD4 = 0x64,
  VKEY_NUMPAD5 = 0x65,
  VKEY_NUMPAD6 = 0x66,
  VKEY_NUMPAD7 = 0x67,
  VKEY_NUMPAD8 = 0x68,
  VKEY_NUMPAD9 = 0x69,
  VKEY_MULTIPLY = 0x6A,
  VKEY_ADD = 0x6B,
  VKEY_SEPARATOR = 0x6C,
  VKEY_SUBTRACT = 0x6D,
  VKEY_DECIMAL = 0x6E,
  VKEY_DIVIDE = 0x6F,
  VKEY_F1 = 0x70,
  VKEY_F2 = 0x71,
  VKEY_F3 = 0x72,
  VKEY_F4 = 0x73,
  VKEY_F5 = 0x74,
  VKEY_F6 = 0x75,
  VKEY_F7 = 0x76,
  VKEY_F8 = 0x77,
  VKEY_F9 = 0x78,
  VKEY_F10 = 0x79,
  VKEY_F11 = 0x7A,
  VKEY_F12 = 0x7B,
  VKEY_F13 = 0x7C,
  VKEY_F14 = 0x7D,
  VKEY_F15 = 0x7E,
  VKEY_F16 = 0x7F,
  VKEY_F17 = 0x80,
  VKEY_F18 = 0x81,
  VKEY_F19 = 0x82,
  VKEY_F20 = 0x83,
  VKEY_F21 = 0x84,
  VKEY_F22 = 0x85,
  VKEY_F23 = 0x86,
  VKEY_F24 = 0x87,
  VKEY_NUMLOCK = 0x90,
  VKEY_SCROLL = 0x91,
  VKEY_LSHIFT = 0xA0,
  VKEY_RSHIFT = 0xA1,
  VKEY_LCONTROL = 0xA2,
  VKEY_RCONTROL = 0xA3,
  VKEY_LMENU = 0xA4,
  VKEY_RMENU = 0xA5,
  VKEY_BROWSER_BACK = 0xA6,
  VKEY_BROWSER_FORWARD = 0xA7,
  VKEY_BROWSER_REFRESH = 0xA8,
  VKEY_BROWSER_STOP = 0xA9,
  VKEY_BROWSER_SEARCH = 0xAA,
  VKEY_BROWSER_FAVORITES = 0xAB,
  VKEY_BROWSER_HOME = 0xAC,
  VKEY_VOLUME_MUTE = 0xAD,
  VKEY_VOLUME_DOWN = 0xAE,
  VKEY_VOLUME_UP = 0xAF,
  VKEY_MEDIA_NEXT_TRACK = 0xB0,
  VKEY_MEDIA_PREV_TRACK = 0xB1,
  VKEY_MEDIA_STOP = 0xB2,
  VKEY_MEDIA_PLAY_PAUSE = 0xB3,
  VKEY_MEDIA_LAUNCH_MAIL = 0xB4,
  VKEY_MEDIA_LAUNCH_MEDIA_SELECT = 0xB5,
  VKEY_MEDIA_LAUNCH_APP1 = 0xB6,
  VKEY_MEDIA_LAUNCH_APP2 = 0xB7,
  VKEY_OEM_1 = 0xBA,
  VKEY_OEM_PLUS = 0xBB,
  VKEY_OEM_COMMA = 0xBC,
  VKEY_OEM_MINUS = 0xBD,
  VKEY_OEM_PERIOD = 0xBE,
  VKEY_OEM_2 = 0xBF,
  VKEY_OEM_3 = 0xC0,
  VKEY_OEM_4 = 0xDB,
  VKEY_OEM_5 = 0xDC,
  VKEY_OEM_6 = 0xDD,
  VKEY_OEM_7 = 0xDE,
  VKEY_OEM_8 = 0xDF,
  VKEY_OEM_102 = 0xE2,
  VKEY_OEM_103 = 0xE3,  // GTV KEYCODE_MEDIA_REWIND
  VKEY_OEM_104 = 0xE4,  // GTV KEYCODE_MEDIA_FAST_FORWARD
  VKEY_PROCESSKEY = 0xE5,
  VKEY_PACKET = 0xE7,
  VKEY_DBE_SBCSCHAR = 0xF3,
  VKEY_DBE_DBCSCHAR = 0xF4,
  VKEY_ATTN = 0xF6,
  VKEY_CRSEL = 0xF7,
  VKEY_EXSEL = 0xF8,
  VKEY_EREOF = 0xF9,
  VKEY_PLAY = 0xFA,
  VKEY_ZOOM = 0xFB,
  VKEY_NONAME = 0xFC,
  VKEY_PA1 = 0xFD,
  VKEY_OEM_CLEAR = 0xFE,
  VKEY_UNKNOWN = 0,

  // POSIX specific VKEYs. Note that as of Windows SDK 7.1, 0x97-9F, 0xD8-DA,
  // and 0xE8 are unassigned.
  VKEY_WLAN = 0x97,
  VKEY_POWER = 0x98,
  VKEY_BRIGHTNESS_DOWN = 0xD8,
  VKEY_BRIGHTNESS_UP = 0xD9,
  VKEY_KBD_BRIGHTNESS_DOWN = 0xDA,
  VKEY_KBD_BRIGHTNESS_UP = 0xE8,

  // Windows does not have a specific key code for AltGr. We use the unused 0xE1
  // (VK_OEM_AX) code to represent AltGr, matching the behaviour of Firefox on
  // Linux.
  VKEY_ALTGR = 0xE1,
  // Windows does not have a specific key code for Compose. We use the unused
  // 0xE6 (VK_ICO_CLEAR) code to represent Compose.
  VKEY_COMPOSE = 0xE6,
};

// From ui/events/keycodes/keyboard_code_conversion_x.cc.
// Gdk key codes (e.g. GDK_KEY_BackSpace) and X keysyms (e.g. XK_BackSpace)
// share the same values.
KeyboardCode KeyboardCodeFromXKeysym(unsigned int keysym) {
  switch (keysym) {
    case GDK_KEY_BackSpace:
      return VKEY_BACK;
    case GDK_KEY_Delete:
    case GDK_KEY_KP_Delete:
      return VKEY_DELETE;
    case GDK_KEY_Tab:
    case GDK_KEY_KP_Tab:
    case GDK_KEY_ISO_Left_Tab:
    case GDK_KEY_3270_BackTab:
      return VKEY_TAB;
    case GDK_KEY_Linefeed:
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_ISO_Enter:
      return VKEY_RETURN;
    case GDK_KEY_Clear:
    case GDK_KEY_KP_Begin:  // NumPad 5 without Num Lock, for crosbug.com/29169.
      return VKEY_CLEAR;
    case GDK_KEY_KP_Space:
    case GDK_KEY_space:
      return VKEY_SPACE;
    case GDK_KEY_Home:
    case GDK_KEY_KP_Home:
      return VKEY_HOME;
    case GDK_KEY_End:
    case GDK_KEY_KP_End:
      return VKEY_END;
    case GDK_KEY_Page_Up:
    case GDK_KEY_KP_Page_Up:  // aka GDK_KEY_KP_Prior
      return VKEY_PRIOR;
    case GDK_KEY_Page_Down:
    case GDK_KEY_KP_Page_Down:  // aka GDK_KEY_KP_Next
      return VKEY_NEXT;
    case GDK_KEY_Left:
    case GDK_KEY_KP_Left:
      return VKEY_LEFT;
    case GDK_KEY_Right:
    case GDK_KEY_KP_Right:
      return VKEY_RIGHT;
    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
      return VKEY_DOWN;
    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
      return VKEY_UP;
    case GDK_KEY_Escape:
      return VKEY_ESCAPE;
    case GDK_KEY_Kana_Lock:
    case GDK_KEY_Kana_Shift:
      return VKEY_KANA;
    case GDK_KEY_Hangul:
      return VKEY_HANGUL;
    case GDK_KEY_Hangul_Hanja:
      return VKEY_HANJA;
    case GDK_KEY_Kanji:
      return VKEY_KANJI;
    case GDK_KEY_Henkan:
      return VKEY_CONVERT;
    case GDK_KEY_Muhenkan:
      return VKEY_NONCONVERT;
    case GDK_KEY_Zenkaku_Hankaku:
      return VKEY_DBE_DBCSCHAR;
    case GDK_KEY_A:
    case GDK_KEY_a:
      return VKEY_A;
    case GDK_KEY_B:
    case GDK_KEY_b:
      return VKEY_B;
    case GDK_KEY_C:
    case GDK_KEY_c:
      return VKEY_C;
    case GDK_KEY_D:
    case GDK_KEY_d:
      return VKEY_D;
    case GDK_KEY_E:
    case GDK_KEY_e:
      return VKEY_E;
    case GDK_KEY_F:
    case GDK_KEY_f:
      return VKEY_F;
    case GDK_KEY_G:
    case GDK_KEY_g:
      return VKEY_G;
    case GDK_KEY_H:
    case GDK_KEY_h:
      return VKEY_H;
    case GDK_KEY_I:
    case GDK_KEY_i:
      return VKEY_I;
    case GDK_KEY_J:
    case GDK_KEY_j:
      return VKEY_J;
    case GDK_KEY_K:
    case GDK_KEY_k:
      return VKEY_K;
    case GDK_KEY_L:
    case GDK_KEY_l:
      return VKEY_L;
    case GDK_KEY_M:
    case GDK_KEY_m:
      return VKEY_M;
    case GDK_KEY_N:
    case GDK_KEY_n:
      return VKEY_N;
    case GDK_KEY_O:
    case GDK_KEY_o:
      return VKEY_O;
    case GDK_KEY_P:
    case GDK_KEY_p:
      return VKEY_P;
    case GDK_KEY_Q:
    case GDK_KEY_q:
      return VKEY_Q;
    case GDK_KEY_R:
    case GDK_KEY_r:
      return VKEY_R;
    case GDK_KEY_S:
    case GDK_KEY_s:
      return VKEY_S;
    case GDK_KEY_T:
    case GDK_KEY_t:
      return VKEY_T;
    case GDK_KEY_U:
    case GDK_KEY_u:
      return VKEY_U;
    case GDK_KEY_V:
    case GDK_KEY_v:
      return VKEY_V;
    case GDK_KEY_W:
    case GDK_KEY_w:
      return VKEY_W;
    case GDK_KEY_X:
    case GDK_KEY_x:
      return VKEY_X;
    case GDK_KEY_Y:
    case GDK_KEY_y:
      return VKEY_Y;
    case GDK_KEY_Z:
    case GDK_KEY_z:
      return VKEY_Z;

    case GDK_KEY_0:
    case GDK_KEY_1:
    case GDK_KEY_2:
    case GDK_KEY_3:
    case GDK_KEY_4:
    case GDK_KEY_5:
    case GDK_KEY_6:
    case GDK_KEY_7:
    case GDK_KEY_8:
    case GDK_KEY_9:
      return static_cast<KeyboardCode>(VKEY_0 + (keysym - GDK_KEY_0));

    case GDK_KEY_parenright:
      return VKEY_0;
    case GDK_KEY_exclam:
      return VKEY_1;
    case GDK_KEY_at:
      return VKEY_2;
    case GDK_KEY_numbersign:
      return VKEY_3;
    case GDK_KEY_dollar:
      return VKEY_4;
    case GDK_KEY_percent:
      return VKEY_5;
    case GDK_KEY_asciicircum:
      return VKEY_6;
    case GDK_KEY_ampersand:
      return VKEY_7;
    case GDK_KEY_asterisk:
      return VKEY_8;
    case GDK_KEY_parenleft:
      return VKEY_9;

    case GDK_KEY_KP_0:
    case GDK_KEY_KP_1:
    case GDK_KEY_KP_2:
    case GDK_KEY_KP_3:
    case GDK_KEY_KP_4:
    case GDK_KEY_KP_5:
    case GDK_KEY_KP_6:
    case GDK_KEY_KP_7:
    case GDK_KEY_KP_8:
    case GDK_KEY_KP_9:
      return static_cast<KeyboardCode>(VKEY_NUMPAD0 + (keysym - GDK_KEY_KP_0));

    case GDK_KEY_multiply:
    case GDK_KEY_KP_Multiply:
      return VKEY_MULTIPLY;
    case GDK_KEY_KP_Add:
      return VKEY_ADD;
    case GDK_KEY_KP_Separator:
      return VKEY_SEPARATOR;
    case GDK_KEY_KP_Subtract:
      return VKEY_SUBTRACT;
    case GDK_KEY_KP_Decimal:
      return VKEY_DECIMAL;
    case GDK_KEY_KP_Divide:
      return VKEY_DIVIDE;
    case GDK_KEY_KP_Equal:
    case GDK_KEY_equal:
    case GDK_KEY_plus:
      return VKEY_OEM_PLUS;
    case GDK_KEY_comma:
    case GDK_KEY_less:
      return VKEY_OEM_COMMA;
    case GDK_KEY_minus:
    case GDK_KEY_underscore:
      return VKEY_OEM_MINUS;
    case GDK_KEY_greater:
    case GDK_KEY_period:
      return VKEY_OEM_PERIOD;
    case GDK_KEY_colon:
    case GDK_KEY_semicolon:
      return VKEY_OEM_1;
    case GDK_KEY_question:
    case GDK_KEY_slash:
      return VKEY_OEM_2;
    case GDK_KEY_asciitilde:
    case GDK_KEY_quoteleft:
      return VKEY_OEM_3;
    case GDK_KEY_bracketleft:
    case GDK_KEY_braceleft:
      return VKEY_OEM_4;
    case GDK_KEY_backslash:
    case GDK_KEY_bar:
      return VKEY_OEM_5;
    case GDK_KEY_bracketright:
    case GDK_KEY_braceright:
      return VKEY_OEM_6;
    case GDK_KEY_quoteright:
    case GDK_KEY_quotedbl:
      return VKEY_OEM_7;
    case GDK_KEY_ISO_Level5_Shift:
      return VKEY_OEM_8;
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R:
      return VKEY_SHIFT;
    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R:
      return VKEY_CONTROL;
    case GDK_KEY_Meta_L:
    case GDK_KEY_Meta_R:
    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R:
      return VKEY_MENU;
    case GDK_KEY_ISO_Level3_Shift:
      return VKEY_ALTGR;
    case GDK_KEY_Multi_key:
      return VKEY_COMPOSE;
    case GDK_KEY_Pause:
      return VKEY_PAUSE;
    case GDK_KEY_Caps_Lock:
      return VKEY_CAPITAL;
    case GDK_KEY_Num_Lock:
      return VKEY_NUMLOCK;
    case GDK_KEY_Scroll_Lock:
      return VKEY_SCROLL;
    case GDK_KEY_Select:
      return VKEY_SELECT;
    case GDK_KEY_Print:
      return VKEY_PRINT;
    case GDK_KEY_Execute:
      return VKEY_EXECUTE;
    case GDK_KEY_Insert:
    case GDK_KEY_KP_Insert:
      return VKEY_INSERT;
    case GDK_KEY_Help:
      return VKEY_HELP;
    case GDK_KEY_Super_L:
      return VKEY_LWIN;
    case GDK_KEY_Super_R:
      return VKEY_RWIN;
    case GDK_KEY_Menu:
      return VKEY_APPS;
    case GDK_KEY_F1:
    case GDK_KEY_F2:
    case GDK_KEY_F3:
    case GDK_KEY_F4:
    case GDK_KEY_F5:
    case GDK_KEY_F6:
    case GDK_KEY_F7:
    case GDK_KEY_F8:
    case GDK_KEY_F9:
    case GDK_KEY_F10:
    case GDK_KEY_F11:
    case GDK_KEY_F12:
    case GDK_KEY_F13:
    case GDK_KEY_F14:
    case GDK_KEY_F15:
    case GDK_KEY_F16:
    case GDK_KEY_F17:
    case GDK_KEY_F18:
    case GDK_KEY_F19:
    case GDK_KEY_F20:
    case GDK_KEY_F21:
    case GDK_KEY_F22:
    case GDK_KEY_F23:
    case GDK_KEY_F24:
      return static_cast<KeyboardCode>(VKEY_F1 + (keysym - GDK_KEY_F1));
    case GDK_KEY_KP_F1:
    case GDK_KEY_KP_F2:
    case GDK_KEY_KP_F3:
    case GDK_KEY_KP_F4:
      return static_cast<KeyboardCode>(VKEY_F1 + (keysym - GDK_KEY_KP_F1));

    case GDK_KEY_guillemotleft:
    case GDK_KEY_guillemotright:
    case GDK_KEY_degree:
    // In the case of canadian multilingual keyboard layout, VKEY_OEM_102 is
    // assigned to ugrave key.
    case GDK_KEY_ugrave:
    case GDK_KEY_Ugrave:
    case GDK_KEY_brokenbar:
      return VKEY_OEM_102;  // international backslash key in 102 keyboard.

    // When evdev is in use, /usr/share/X11/xkb/symbols/inet maps F13-18 keys
    // to the special XF86XK symbols to support Microsoft Ergonomic keyboards:
    // https://bugs.freedesktop.org/show_bug.cgi?id=5783
    // In Chrome, we map these X key symbols back to F13-18 since we don't have
    // VKEYs for these XF86XK symbols.
    case GDK_KEY_Tools:
      return VKEY_F13;
    case GDK_KEY_Launch5:
      return VKEY_F14;
    case GDK_KEY_Launch6:
      return VKEY_F15;
    case GDK_KEY_Launch7:
      return VKEY_F16;
    case GDK_KEY_Launch8:
      return VKEY_F17;
    case GDK_KEY_Launch9:
      return VKEY_F18;
    case GDK_KEY_Refresh:
    case GDK_KEY_History:
    case GDK_KEY_OpenURL:
    case GDK_KEY_AddFavorite:
    case GDK_KEY_Go:
    case GDK_KEY_ZoomIn:
    case GDK_KEY_ZoomOut:
      // ui::AcceleratorGtk tries to convert the GDK_KEY_ keysyms on Chrome
      // startup. It's safe to return VKEY_UNKNOWN here since ui::AcceleratorGtk
      // also checks a Gdk keysym. http://crbug.com/109843
      return VKEY_UNKNOWN;
    // For supporting multimedia buttons on a USB keyboard.
    case GDK_KEY_Back:
      return VKEY_BROWSER_BACK;
    case GDK_KEY_Forward:
      return VKEY_BROWSER_FORWARD;
    case GDK_KEY_Reload:
      return VKEY_BROWSER_REFRESH;
    case GDK_KEY_Stop:
      return VKEY_BROWSER_STOP;
    case GDK_KEY_Search:
      return VKEY_BROWSER_SEARCH;
    case GDK_KEY_Favorites:
      return VKEY_BROWSER_FAVORITES;
    case GDK_KEY_HomePage:
      return VKEY_BROWSER_HOME;
    case GDK_KEY_AudioMute:
      return VKEY_VOLUME_MUTE;
    case GDK_KEY_AudioLowerVolume:
      return VKEY_VOLUME_DOWN;
    case GDK_KEY_AudioRaiseVolume:
      return VKEY_VOLUME_UP;
    case GDK_KEY_AudioNext:
      return VKEY_MEDIA_NEXT_TRACK;
    case GDK_KEY_AudioPrev:
      return VKEY_MEDIA_PREV_TRACK;
    case GDK_KEY_AudioStop:
      return VKEY_MEDIA_STOP;
    case GDK_KEY_AudioPlay:
      return VKEY_MEDIA_PLAY_PAUSE;
    case GDK_KEY_Mail:
      return VKEY_MEDIA_LAUNCH_MAIL;
    case GDK_KEY_LaunchA:  // F3 on an Apple keyboard.
      return VKEY_MEDIA_LAUNCH_APP1;
    case GDK_KEY_LaunchB:  // F4 on an Apple keyboard.
    case GDK_KEY_Calculator:
      return VKEY_MEDIA_LAUNCH_APP2;
    case GDK_KEY_WLAN:
      return VKEY_WLAN;
    case GDK_KEY_PowerOff:
      return VKEY_POWER;
    case GDK_KEY_MonBrightnessDown:
      return VKEY_BRIGHTNESS_DOWN;
    case GDK_KEY_MonBrightnessUp:
      return VKEY_BRIGHTNESS_UP;
    case GDK_KEY_KbdBrightnessDown:
      return VKEY_KBD_BRIGHTNESS_DOWN;
    case GDK_KEY_KbdBrightnessUp:
      return VKEY_KBD_BRIGHTNESS_UP;

      // TODO(sad): some keycodes are still missing.
  }
  return VKEY_UNKNOWN;
}

// From content/browser/renderer_host/input/web_input_event_util_posix.cc.
KeyboardCode GdkEventToWindowsKeyCode(const GdkEventKey* event) {
  static const unsigned int kHardwareCodeToGDKKeyval[] = {
      0,                 // 0x00:
      0,                 // 0x01:
      0,                 // 0x02:
      0,                 // 0x03:
      0,                 // 0x04:
      0,                 // 0x05:
      0,                 // 0x06:
      0,                 // 0x07:
      0,                 // 0x08:
      0,                 // 0x09: GDK_Escape
      GDK_1,             // 0x0A: GDK_1
      GDK_2,             // 0x0B: GDK_2
      GDK_3,             // 0x0C: GDK_3
      GDK_4,             // 0x0D: GDK_4
      GDK_5,             // 0x0E: GDK_5
      GDK_6,             // 0x0F: GDK_6
      GDK_7,             // 0x10: GDK_7
      GDK_8,             // 0x11: GDK_8
      GDK_9,             // 0x12: GDK_9
      GDK_0,             // 0x13: GDK_0
      GDK_minus,         // 0x14: GDK_minus
      GDK_equal,         // 0x15: GDK_equal
      0,                 // 0x16: GDK_BackSpace
      0,                 // 0x17: GDK_Tab
      GDK_q,             // 0x18: GDK_q
      GDK_w,             // 0x19: GDK_w
      GDK_e,             // 0x1A: GDK_e
      GDK_r,             // 0x1B: GDK_r
      GDK_t,             // 0x1C: GDK_t
      GDK_y,             // 0x1D: GDK_y
      GDK_u,             // 0x1E: GDK_u
      GDK_i,             // 0x1F: GDK_i
      GDK_o,             // 0x20: GDK_o
      GDK_p,             // 0x21: GDK_p
      GDK_bracketleft,   // 0x22: GDK_bracketleft
      GDK_bracketright,  // 0x23: GDK_bracketright
      0,                 // 0x24: GDK_Return
      0,                 // 0x25: GDK_Control_L
      GDK_a,             // 0x26: GDK_a
      GDK_s,             // 0x27: GDK_s
      GDK_d,             // 0x28: GDK_d
      GDK_f,             // 0x29: GDK_f
      GDK_g,             // 0x2A: GDK_g
      GDK_h,             // 0x2B: GDK_h
      GDK_j,             // 0x2C: GDK_j
      GDK_k,             // 0x2D: GDK_k
      GDK_l,             // 0x2E: GDK_l
      GDK_semicolon,     // 0x2F: GDK_semicolon
      GDK_apostrophe,    // 0x30: GDK_apostrophe
      GDK_grave,         // 0x31: GDK_grave
      0,                 // 0x32: GDK_Shift_L
      GDK_backslash,     // 0x33: GDK_backslash
      GDK_z,             // 0x34: GDK_z
      GDK_x,             // 0x35: GDK_x
      GDK_c,             // 0x36: GDK_c
      GDK_v,             // 0x37: GDK_v
      GDK_b,             // 0x38: GDK_b
      GDK_n,             // 0x39: GDK_n
      GDK_m,             // 0x3A: GDK_m
      GDK_comma,         // 0x3B: GDK_comma
      GDK_period,        // 0x3C: GDK_period
      GDK_slash,         // 0x3D: GDK_slash
      0,                 // 0x3E: GDK_Shift_R
      0,                 // 0x3F:
      0,                 // 0x40:
      0,                 // 0x41:
      0,                 // 0x42:
      0,                 // 0x43:
      0,                 // 0x44:
      0,                 // 0x45:
      0,                 // 0x46:
      0,                 // 0x47:
      0,                 // 0x48:
      0,                 // 0x49:
      0,                 // 0x4A:
      0,                 // 0x4B:
      0,                 // 0x4C:
      0,                 // 0x4D:
      0,                 // 0x4E:
      0,                 // 0x4F:
      0,                 // 0x50:
      0,                 // 0x51:
      0,                 // 0x52:
      0,                 // 0x53:
      0,                 // 0x54:
      0,                 // 0x55:
      0,                 // 0x56:
      0,                 // 0x57:
      0,                 // 0x58:
      0,                 // 0x59:
      0,                 // 0x5A:
      0,                 // 0x5B:
      0,                 // 0x5C:
      0,                 // 0x5D:
      0,                 // 0x5E:
      0,                 // 0x5F:
      0,                 // 0x60:
      0,                 // 0x61:
      0,                 // 0x62:
      0,                 // 0x63:
      0,                 // 0x64:
      0,                 // 0x65:
      0,                 // 0x66:
      0,                 // 0x67:
      0,                 // 0x68:
      0,                 // 0x69:
      0,                 // 0x6A:
      0,                 // 0x6B:
      0,                 // 0x6C:
      0,                 // 0x6D:
      0,                 // 0x6E:
      0,                 // 0x6F:
      0,                 // 0x70:
      0,                 // 0x71:
      0,                 // 0x72:
      GDK_Super_L,       // 0x73: GDK_Super_L
      GDK_Super_R,       // 0x74: GDK_Super_R
  };

  // |windows_key_code| has to include a valid virtual-key code even when we
  // use non-US layouts, e.g. even when we type an 'A' key of a US keyboard
  // on the Hebrew layout, |windows_key_code| should be VK_A.
  // On the other hand, |event->keyval| value depends on the current
  // GdkKeymap object, i.e. when we type an 'A' key of a US keyboard on
  // the Hebrew layout, |event->keyval| becomes GDK_hebrew_shin and this
  // KeyboardCodeFromXKeysym() call returns 0.
  // To improve compatibilty with Windows, we use |event->hardware_keycode|
  // for retrieving its Windows key-code for the keys when the
  // WebCore::windows_key_codeForEvent() call returns 0.
  // We shouldn't use |event->hardware_keycode| for keys that GdkKeymap
  // objects cannot change because |event->hardware_keycode| doesn't change
  // even when we change the layout options, e.g. when we swap a control
  // key and a caps-lock key, GTK doesn't swap their
  // |event->hardware_keycode| values but swap their |event->keyval| values.
  KeyboardCode windows_key_code = KeyboardCodeFromXKeysym(event->keyval);
  if (windows_key_code) {
    return windows_key_code;
  }

  if (event->hardware_keycode < std::size(kHardwareCodeToGDKKeyval)) {
    int keyval = kHardwareCodeToGDKKeyval[event->hardware_keycode];
    if (keyval) {
      return KeyboardCodeFromXKeysym(keyval);
    }
  }

  // This key is one that keyboard-layout drivers cannot change.
  // Use |event->keyval| to retrieve its |windows_key_code| value.
  return KeyboardCodeFromXKeysym(event->keyval);
}

// From content/browser/renderer_host/input/web_input_event_util_posix.cc.
KeyboardCode GetWindowsKeyCodeWithoutLocation(KeyboardCode key_code) {
  switch (key_code) {
    case VKEY_LCONTROL:
    case VKEY_RCONTROL:
      return VKEY_CONTROL;
    case VKEY_LSHIFT:
    case VKEY_RSHIFT:
      return VKEY_SHIFT;
    case VKEY_LMENU:
    case VKEY_RMENU:
      return VKEY_MENU;
    default:
      return key_code;
  }
}

// From content/browser/renderer_host/input/web_input_event_builders_gtk.cc.
// Gets the corresponding control character of a specified key code. See:
// http://en.wikipedia.org/wiki/Control_characters
// We emulate Windows behavior here.
int GetControlCharacter(KeyboardCode windows_key_code, bool shift) {
  if (windows_key_code >= VKEY_A && windows_key_code <= VKEY_Z) {
    // ctrl-A ~ ctrl-Z map to \x01 ~ \x1A
    return windows_key_code - VKEY_A + 1;
  }
  if (shift) {
    // following graphics chars require shift key to input.
    switch (windows_key_code) {
      // ctrl-@ maps to \x00 (Null byte)
      case VKEY_2:
        return 0;
      // ctrl-^ maps to \x1E (Record separator, Information separator two)
      case VKEY_6:
        return 0x1E;
      // ctrl-_ maps to \x1F (Unit separator, Information separator one)
      case VKEY_OEM_MINUS:
        return 0x1F;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        return 0;
    }
  } else {
    switch (windows_key_code) {
      // ctrl-[ maps to \x1B (Escape)
      case VKEY_OEM_4:
        return 0x1B;
      // ctrl-\ maps to \x1C (File separator, Information separator four)
      case VKEY_OEM_5:
        return 0x1C;
      // ctrl-] maps to \x1D (Group separator, Information separator three)
      case VKEY_OEM_6:
        return 0x1D;
      // ctrl-Enter maps to \x0A (Line feed)
      case VKEY_RETURN:
        return 0x0A;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        return 0;
    }
  }
}

CefBrowserHost::DragOperationsMask GetDragOperationsMask(
    GdkDragContext* drag_context) {
  int allowed_ops = DRAG_OPERATION_NONE;
  GdkDragAction drag_action = gdk_drag_context_get_actions(drag_context);
  if (drag_action & GDK_ACTION_COPY) {
    allowed_ops |= DRAG_OPERATION_COPY;
  }
  if (drag_action & GDK_ACTION_MOVE) {
    allowed_ops |= DRAG_OPERATION_MOVE;
  }
  if (drag_action & GDK_ACTION_LINK) {
    allowed_ops |= DRAG_OPERATION_LINK;
  }
  if (drag_action & GDK_ACTION_PRIVATE) {
    allowed_ops |= DRAG_OPERATION_PRIVATE;
  }
  return static_cast<CefBrowserHost::DragOperationsMask>(allowed_ops);
}

// Returns the CSS cursor name for |type|, or nullptr for the default cursor.
const char* GetCursorName(cef_cursor_type_t type) {
  switch (type) {
    case CT_CROSS:
      return "crosshair";
    case CT_HAND:
      return "pointer";
    case CT_IBEAM:
      return "text";
    case CT_WAIT:
      return "wait";
    case CT_HELP:
      return "help";
    case CT_EASTRESIZE:
      return "e-resize";
    case CT_NORTHRESIZE:
      return "n-resize";
    case CT_NORTHEASTRESIZE:
      return "ne-resize";
    case CT_NORTHWESTRESIZE:
      return "nw-resize";
    case CT_SOUTHRESIZE:
      return "s-resize";
    case CT_SOUTHEASTRESIZE:
      return "se-resize";
    case CT_SOUTHWESTRESIZE:
      return "sw-resize";
    case CT_WESTRESIZE:
      return "w-resize";
    case CT_NORTHSOUTHRESIZE:
      return "ns-resize";
    case CT_EASTWESTRESIZE:
      return "ew-resize";
    case CT_NORTHEASTSOUTHWESTRESIZE:
      return "nesw-resize";
    case CT_NORTHWESTSOUTHEASTRESIZE:
      return "nwse-resize";
    case CT_COLUMNRESIZE:
      return "col-resize";
    case CT_ROWRESIZE:
      return "row-resize";
    case CT_MIDDLEPANNING:
    case CT_EASTPANNING:
    case CT_NORTHPANNING:
    case CT_NORTHEASTPANNING:
    case CT_NORTHWESTPANNING:
    case CT_SOUTHPANNING:
    case CT_SOUTHEASTPANNING:
    case CT_SOUTHWESTPANNING:
    case CT_WESTPANNING:
    case CT_MIDDLE_PANNING_VERTICAL:
    case CT_MIDDLE_PANNING_HORIZONTAL:
      return "all-scroll";
    case CT_MOVE:
    case CT_DND_MOVE:
      return "move";
    case CT_VERTICALTEXT:
      return "vertical-text";
    case CT_CELL:
      return "cell";
    case CT_CONTEXTMENU:
      return "context-menu";
    case CT_ALIAS:
    case CT_DND_LINK:
      return "alias";
    case CT_PROGRESS:
      return "progress";
    case CT_NODROP:
    case CT_DND_NONE:
      return "no-drop";
    case CT_COPY:
    case CT_DND_COPY:
      return "copy";
    case CT_NONE:
      return "none";
    case CT_NOTALLOWED:
      return "not-allowed";
    case CT_ZOOMIN:
      return "zoom-in";
    case CT_ZOOMOUT:
      return "zoom-out";
    case CT_GRAB:
      return "grab";
    case CT_GRABBING:
      return "grabbing";
    default:
      return nullptr;
  }
}

// Returns a new cursor reference, or nullptr for the default cursor.
GdkCursor* CreateGdkCursor(GdkDisplay* display,
                           cef_cursor_type_t type,
                           const CefCursorInfo& custom_cursor_info) {
  if (type == CT_CUSTOM) {
    const int width = custom_cursor_info.size.width;
    const int height = custom_cursor_info.size.height;
    if (!custom_cursor_info.buffer || width <= 0 || height <= 0) {
      return nullptr;
    }

    // Convert from premultiplied BGRA to non-premultiplied RGBA.
    GdkPixbuf* pixbuf =
        gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    const int stride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar* dst = gdk_pixbuf_get_pixels(pixbuf);
    const auto* src = static_cast<const uint8_t*>(custom_cursor_info.buffer);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const uint8_t* s = src + (y * width + x) * 4;
        guchar* d = dst + y * stride + x * 4;
        const uint8_t a = s[3];
        d[0] = a ? s[2] * 255 / a : 0;
        d[1] = a ? s[1] * 255 / a : 0;
        d[2] = a ? s[0] * 255 / a : 0;
        d[3] = a;
      }
    }

    GdkCursor* cursor = gdk_cursor_new_from_pixbuf(
        display, pixbuf, custom_cursor_info.hotspot.x,
        custom_cursor_info.hotspot.y);
    g_object_unref(pixbuf);
    return cursor;
  }

  const char* name = GetCursorName(type);
  if (!name) {
    return nullptr;
  }
  return gdk_cursor_new_from_name(display, name);
}

}  // namespace

// Calls CefBrowserHost::SendExternalBeginFrame at a fixed rate on the UI
// thread. Reference counted so that pending tasks remain valid after the
// browser window is destroyed.
class BrowserWindowOsrGtk::BeginFrameTimer
    : public base::RefCountedThreadSafe<BeginFrameTimer> {
 public:
  BeginFrameTimer(CefRefPtr<CefBrowser> browser, int frame_rate)
      : browser_(browser),
        interval_ms_(std::max(1, 1000 / std::max(1, frame_rate))) {}

  void Start() {
    CEF_REQUIRE_UI_THREAD();
    Tick();
  }

  void Stop() {
    CEF_REQUIRE_UI_THREAD();
    browser_ = nullptr;
  }

 private:
  friend class base::RefCountedThreadSafe<BeginFrameTimer>;
  ~BeginFrameTimer() = default;

  void Tick() {
    CEF_REQUIRE_UI_THREAD();
    if (!browser_) {
      return;
    }
    browser_->GetHost()->SendExternalBeginFrame();
    CefPostDelayedTask(TID_UI,
                       base::BindOnce(&BeginFrameTimer::Tick,
                                      scoped_refptr<BeginFrameTimer>(this)),
                       interval_ms_);
  }

  CefRefPtr<CefBrowser> browser_;
  const int64_t interval_ms_;
};

BrowserWindowOsrGtk::BrowserWindowOsrGtk(BrowserWindow::Delegate* delegate,
                                         bool with_controls,
                                         const std::string& startup_url,
                                         const OsrRendererSettings& settings)
    : BrowserWindow(delegate),
      settings_(settings),
      renderer_(settings.background_color, settings.show_update_rect),
      gl_enabled_(false),
      hidden_(false),
      widget_(nullptr),
      drag_trigger_event_(nullptr),
      drag_data_(nullptr),
      drag_operation_(DRAG_OPERATION_NONE),
      drag_context_(nullptr),
      drag_targets_(gtk_target_list_new(nullptr, 0)),
      drag_leave_(false),
      drag_drop_(false),
      device_scale_factor_(1.0f) {
  client_handler_ =
      new ClientHandlerOsr(this, this, with_controls, startup_url);
  g_browser_windows.push_back(this);
}

BrowserWindowOsrGtk::~BrowserWindowOsrGtk() {
  g_browser_windows.erase(
      std::find(g_browser_windows.begin(), g_browser_windows.end(), this));
  ScopedGdkThreadsEnter scoped_gdk_threads;

  if (drag_trigger_event_) {
    gdk_event_free(drag_trigger_event_);
  }
  if (drag_context_) {
    g_object_unref(drag_context_);
  }
  gtk_target_list_unref(drag_targets_);
}

void BrowserWindowOsrGtk::CreateBrowser(
    ClientWindowHandle parent_handle,
    const CefRect& rect,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  REQUIRE_MAIN_THREAD();

  // Windowless rendering requires Alloy style.
  DCHECK(delegate_->UseAlloyStyle());

  // Create the native window.
  Create(parent_handle);

  ScopedGdkThreadsEnter scoped_gdk_threads;

  // Retrieve the X11 Window ID for the GTK parent window. The parent window is
  // optional and Wayland has no equivalent.
  GtkWidget* window =
      gtk_widget_get_ancestor(GTK_WIDGET(parent_handle), GTK_TYPE_WINDOW);
  GdkWindow* gdk_window = gtk_widget_get_window(window);
  CefWindowHandle handle = kNullWindowHandle;
  if (GDK_IS_X11_WINDOW(gdk_window)) {
    handle = GDK_WINDOW_XID(gdk_window);
    DCHECK(handle);
  }

  CefWindowInfo window_info;
  window_info.SetAsWindowless(handle);

  window_info.shared_texture_enabled = settings_.shared_texture_enabled;
  window_info.external_begin_frame_enabled =
      settings_.external_begin_frame_enabled;

  // Windowless rendering requires Alloy style.
  DCHECK_EQ(CEF_RUNTIME_STYLE_ALLOY, window_info.runtime_style);

  // Create the browser asynchronously.
  CefBrowserHost::CreateBrowser(window_info, client_handler_,
                                client_handler_->startup_url(), settings,
                                extra_info, request_context);
}

void BrowserWindowOsrGtk::GetPopupConfig(CefWindowHandle temp_handle,
                                         CefWindowInfo& windowInfo,
                                         CefRefPtr<CefClient>& client,
                                         CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();

  windowInfo.SetAsWindowless(temp_handle);

  // Windowless rendering requires Alloy style.
  DCHECK_EQ(CEF_RUNTIME_STYLE_ALLOY, windowInfo.runtime_style);

  windowInfo.shared_texture_enabled = settings_.shared_texture_enabled;
  windowInfo.external_begin_frame_enabled =
      settings_.external_begin_frame_enabled;

  client = client_handler_;
}

void BrowserWindowOsrGtk::ShowPopup(ClientWindowHandle parent_handle,
                                    int x,
                                    int y,
                                    size_t width,
                                    size_t height) {
  REQUIRE_MAIN_THREAD();
  DCHECK(browser_.get());

  // Create the native window.
  Create(parent_handle);

  // Send resize notification so the compositor is assigned the correct
  // viewport size and begins rendering.
  browser_->GetHost()->WasResized();

  Show();
}

void BrowserWindowOsrGtk::Show() {
  REQUIRE_MAIN_THREAD();

  if (hidden_) {
    // Set the browser as visible.
    browser_->GetHost()->WasHidden(false);
    hidden_ = false;
  }

  // Give focus to the browser.
  browser_->GetHost()->SetFocus(true);
}

void BrowserWindowOsrGtk::Hide() {
  REQUIRE_MAIN_THREAD();

  if (!browser_) {
    return;
  }

  // Remove focus from the browser.
  browser_->GetHost()->SetFocus(false);

  if (!hidden_) {
    // Set the browser as hidden.
    browser_->GetHost()->WasHidden(true);
    hidden_ = true;
  }
}

void BrowserWindowOsrGtk::SetBounds(int x, int y, size_t width, size_t height) {
  REQUIRE_MAIN_THREAD();
  // Nothing to do here. GTK will take care of positioning in the container.
}

void BrowserWindowOsrGtk::SetFocus(bool focus) {
  REQUIRE_MAIN_THREAD();
  if (widget_ && focus) {
    gtk_widget_grab_focus(widget_);
  }
}

void BrowserWindowOsrGtk::SetDeviceScaleFactor(float device_scale_factor) {
  REQUIRE_MAIN_THREAD();
  {
    base::AutoLock lock_scope(lock_);
    if (device_scale_factor == device_scale_factor_) {
      return;
    }

    // Apply some sanity checks.
    if (device_scale_factor < 0.5f || device_scale_factor > 4.0f) {
      return;
    }

    device_scale_factor_ = device_scale_factor;
  }

  if (browser_) {
    browser_->GetHost()->NotifyScreenInfoChanged();
    browser_->GetHost()->WasResized();
  }
}

float BrowserWindowOsrGtk::GetDeviceScaleFactor() const {
  REQUIRE_MAIN_THREAD();
  base::AutoLock lock_scope(lock_);
  return device_scale_factor_;
}

ClientWindowHandle BrowserWindowOsrGtk::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();
  return widget_;
}

void BrowserWindowOsrGtk::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  if (settings_.external_begin_frame_enabled) {
    // The client drives frame production, so frames are only produced while
    // the timer is running.
    begin_frame_timer_ = base::MakeRefCounted<BeginFrameTimer>(
        browser, settings_.begin_frame_rate);
    begin_frame_timer_->Start();
  }
}

void BrowserWindowOsrGtk::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  if (begin_frame_timer_) {
    begin_frame_timer_->Stop();
    begin_frame_timer_ = nullptr;
  }

  // Detach |this| from the ClientHandlerOsr.
  auto handler = ClientHandlerOsr::GetForClient(client_handler_);
  CHECK(handler);
  handler->DetachOsrDelegate();

  ScopedGdkThreadsEnter scoped_gdk_threads;

  UnregisterDragDrop();

  // Disconnect all signal handlers that reference |this|.
  g_signal_handlers_disconnect_matched(widget_, G_SIGNAL_MATCH_DATA, 0, 0,
                                       nullptr, nullptr, this);

  DisableGL();
}

bool BrowserWindowOsrGtk::GetRootScreenRect(CefRefPtr<CefBrowser> browser,
                                            CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  if (!settings_.real_screen_bounds) {
    return false;
  }

  if (!widget_) {
    return false;
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  GtkWidget* toplevel = gtk_widget_get_toplevel(widget_);

  // Convert to DIP coordinates.
  rect = DeviceToLogical(
      GetWindowBounds(GTK_WINDOW(toplevel), /*include_frame=*/true),
      device_scale_factor);
  return true;
}

void BrowserWindowOsrGtk::GetViewRect(CefRefPtr<CefBrowser> browser,
                                      CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  if (!widget_) {
    // Never return an empty rectangle.
    rect.width = rect.height = 1;
    return;
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  GtkAllocation allocation = {};
  gtk_widget_get_allocation(widget_, &allocation);

  // Convert to DIP coordinates.
  rect = DeviceToLogical(
      {allocation.x, allocation.y, allocation.width, allocation.height},
      device_scale_factor);
  if (rect.width == 0) {
    rect.width = 1;
  }
  if (rect.height == 0) {
    rect.height = 1;
  }
  if (!settings_.real_screen_bounds) {
    rect.x = rect.y = 0;
  }
}

bool BrowserWindowOsrGtk::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                                         int viewX,
                                         int viewY,
                                         int& screenX,
                                         int& screenY) {
  CEF_REQUIRE_UI_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  // Get the widget position in the window.
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget_, &allocation);

  // Convert from view DIP coordinates to window (pixel) coordinates.
  screenX = allocation.x + LogicalToDevice(viewX, device_scale_factor);
  screenY = allocation.y + LogicalToDevice(viewY, device_scale_factor);
  return true;
}

bool BrowserWindowOsrGtk::GetScreenInfo(CefRefPtr<CefBrowser> browser,
                                        CefScreenInfo& screen_info) {
  CEF_REQUIRE_UI_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  screen_info.device_scale_factor = device_scale_factor;

  if (settings_.real_screen_bounds) {
    CefRect root_rect;
    GetRootScreenRect(browser, root_rect);

    auto display = CefDisplay::GetDisplayMatchingBounds(
        root_rect, /*input_pixel_coords=*/false);
    screen_info.rect = display->GetBounds();
    screen_info.available_rect = display->GetWorkArea();
  } else {
    CefRect view_rect;
    GetViewRect(browser, view_rect);

    // Keep HTML select popups inside the view rectangle.
    screen_info.rect = view_rect;
    screen_info.available_rect = view_rect;
  }

  return true;
}

void BrowserWindowOsrGtk::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                      bool show) {
  CEF_REQUIRE_UI_THREAD();

  {
    // The renderer is also accessed on the main thread when drawing.
    ScopedGdkThreadsEnter scoped_gdk_threads;
    renderer_.OnPopupShow(show);
  }
  if (!show) {
    browser->GetHost()->Invalidate(PET_VIEW);
  }
}

void BrowserWindowOsrGtk::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                      const CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  // The renderer is also accessed on the main thread when drawing.
  ScopedGdkThreadsEnter scoped_gdk_threads;
  renderer_.OnPopupSize(LogicalToDevice(rect, device_scale_factor));
}

void BrowserWindowOsrGtk::OnPaint(CefRefPtr<CefBrowser> browser,
                                  CefRenderHandler::PaintElementType type,
                                  const CefRenderHandler::RectList& dirtyRects,
                                  const void* buffer,
                                  int width,
                                  int height) {
  CEF_REQUIRE_UI_THREAD();

  if (width <= 2 && height <= 2) {
    // Ignore really small buffer sizes while the widget is starting up.
    return;
  }

  if (!EnableGL()) {
    return;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;
  if (!surface_->MakeCurrent()) {
    return;
  }
  const bool updated =
      renderer_.OnPaint(type, dirtyRects, buffer, width, height);
  surface_->ReleaseCurrent();
  if (updated) {
    surface_->Invalidate();
  }
}

void BrowserWindowOsrGtk::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirtyRects,
    const CefAcceleratedPaintInfo& info) {
  CEF_REQUIRE_UI_THREAD();

  if (!EnableGL()) {
    return;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;
  if (!surface_->MakeCurrent()) {
    return;
  }
  // Copies the frame before returning, as CEF reuses the buffer.
  const bool updated = renderer_.OnAcceleratedPaint(type, dirtyRects, info);
  surface_->ReleaseCurrent();
  if (updated) {
    surface_->Invalidate();
  }
}

void BrowserWindowOsrGtk::RenderSurface(unsigned int framebuffer,
                                        int width,
                                        int height) {
  REQUIRE_MAIN_THREAD();

  // Draw the most recent frame scaled to the surface, including while waiting
  // for a frame at a new size. Called with the GDK lock held.
  if (!gl_enabled_ || !renderer_.Render(framebuffer, width, height)) {
    const gl::Api* api = gl::GetApi();
    if (api) {
      api->glBindFramebuffer(gl::kGlFramebuffer, framebuffer);
      api->glClearColor(0, 0, 0, 0);
      api->glClear(gl::kGlColorBufferBit);
    }
  }
}

void BrowserWindowOsrGtk::OnCursorChange(
    CefRefPtr<CefBrowser> browser,
    CefCursorHandle cursor,
    cef_cursor_type_t type,
    const CefCursorInfo& custom_cursor_info) {
  CEF_REQUIRE_UI_THREAD();

  ScopedGdkThreadsEnter scoped_gdk_threads;

  GdkWindow* gdk_window = gtk_widget_get_window(widget_);
  if (!gdk_window) {
    return;
  }

  GdkCursor* gdk_cursor = CreateGdkCursor(gdk_window_get_display(gdk_window),
                                          type, custom_cursor_info);
  gdk_window_set_cursor(gdk_window, gdk_cursor);
  if (gdk_cursor) {
    g_object_unref(gdk_cursor);
  }
}

bool BrowserWindowOsrGtk::StartDragging(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDragData> drag_data,
    CefRenderHandler::DragOperationsMask allowed_ops,
    int x,
    int y) {
  CEF_REQUIRE_UI_THREAD();

  if (!drag_data->HasImage()) {
    LOG(ERROR) << "Drag image representation not available";
    return false;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  DragReset();
  drag_data_ = drag_data;

  // Begin drag.
  if (drag_trigger_event_) {
    LOG(ERROR) << "Dragging started, but last mouse event is missing";
    DragReset();
    return false;
  }
  drag_context_ = gtk_drag_begin(widget_, drag_targets_, GDK_ACTION_COPY,
                                 1,  // left mouse button
                                 drag_trigger_event_);
  if (!drag_context_) {
    LOG(ERROR) << "GTK drag begin failed";
    DragReset();
    return false;
  }
  g_object_ref(drag_context_);

  // Send drag enter event.
  CefMouseEvent ev;
  ev.x = x;
  ev.y = y;
  ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
  browser->GetHost()->DragTargetDragEnter(drag_data, ev, allowed_ops);

  return true;
}

void BrowserWindowOsrGtk::UpdateDragCursor(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::DragOperation operation) {
  CEF_REQUIRE_UI_THREAD();
  drag_operation_ = operation;
}

void BrowserWindowOsrGtk::OnImeCompositionRangeChanged(
    CefRefPtr<CefBrowser> browser,
    const CefRange& selection_range,
    const CefRenderHandler::RectList& character_bounds) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrGtk::UpdateAccessibilityTree(CefRefPtr<CefValue> value) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrGtk::UpdateAccessibilityLocation(
    CefRefPtr<CefValue> value) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrGtk::Create(ClientWindowHandle parent_handle) {
  REQUIRE_MAIN_THREAD();
  DCHECK(!widget_);

  ScopedGdkThreadsEnter scoped_gdk_threads;

  surface_ = OsrGlSurfaceGtk::Create(
      [this](unsigned int framebuffer, int width, int height) {
        RenderSurface(framebuffer, width, height);
      });
  widget_ = surface_->widget();
  DCHECK(widget_);

  gtk_widget_set_can_focus(widget_, TRUE);

  g_signal_connect(G_OBJECT(widget_), "size_allocate",
                   G_CALLBACK(&BrowserWindowOsrGtk::SizeAllocation), this);

  gtk_widget_set_events(
      widget_, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                   GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
                   GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                   GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
                   GDK_SCROLL_MASK | GDK_FOCUS_CHANGE_MASK);
  g_signal_connect(G_OBJECT(widget_), "button_press_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::ClickEvent), this);
  g_signal_connect(G_OBJECT(widget_), "button_release_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::ClickEvent), this);
  g_signal_connect(G_OBJECT(widget_), "key_press_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::KeyEvent), this);
  g_signal_connect(G_OBJECT(widget_), "key_release_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::KeyEvent), this);
  g_signal_connect(G_OBJECT(widget_), "enter_notify_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::MoveEvent), this);
  g_signal_connect(G_OBJECT(widget_), "leave_notify_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::MoveEvent), this);
  g_signal_connect(G_OBJECT(widget_), "motion_notify_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::MoveEvent), this);
  g_signal_connect(G_OBJECT(widget_), "scroll_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::ScrollEvent), this);
  g_signal_connect(G_OBJECT(widget_), "focus_in_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::FocusEvent), this);
  g_signal_connect(G_OBJECT(widget_), "focus_out_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::FocusEvent), this);
  g_signal_connect(G_OBJECT(widget_), "touch-event",
                   G_CALLBACK(&BrowserWindowOsrGtk::TouchEvent), this);

  RegisterDragDrop();

  gtk_widget_set_vexpand(widget_, TRUE);
  gtk_grid_attach(GTK_GRID(parent_handle), widget_, 0, 3, 1, 1);

  // Make the GlArea visible in the parent container.
  gtk_widget_show_all(parent_handle);
}

// static
gint BrowserWindowOsrGtk::SizeAllocation(GtkWidget* widget,
                                         GtkAllocation* allocation,
                                         BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();
  if (self->browser_.get()) {
    // Results in a call to GetViewRect().
    self->browser_->GetHost()->WasResized();
  }
  return TRUE;
}

// static
gint BrowserWindowOsrGtk::ClickEvent(GtkWidget* widget,
                                     GdkEventButton* event,
                                     BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (!self->browser_.get()) {
    return TRUE;
  }

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  CefBrowserHost::MouseButtonType button_type = MBT_LEFT;
  switch (event->button) {
    case 1:
      break;
    case 2:
      button_type = MBT_MIDDLE;
      break;
    case 3:
      button_type = MBT_RIGHT;
      break;
    default:
      // Other mouse buttons are not handled here.
      return FALSE;
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  CefMouseEvent mouse_event;
  mouse_event.x = event->x;
  mouse_event.y = event->y;
  self->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  DeviceToLogical(mouse_event, device_scale_factor);
  mouse_event.modifiers = GetCefStateModifiers(event->state);

  bool mouse_up = (event->type == GDK_BUTTON_RELEASE);
  if (!mouse_up) {
    gtk_widget_grab_focus(widget);
  }

  int click_count = 1;
  switch (event->type) {
    case GDK_2BUTTON_PRESS:
      click_count = 2;
      break;
    case GDK_3BUTTON_PRESS:
      click_count = 3;
      break;
    default:
      break;
  }

  host->SendMouseClickEvent(mouse_event, button_type, mouse_up, click_count);

  // Save mouse event that can be a possible trigger for drag.
  if (!self->drag_context_ && button_type == MBT_LEFT) {
    if (self->drag_trigger_event_) {
      gdk_event_free(self->drag_trigger_event_);
    }
    self->drag_trigger_event_ =
        gdk_event_copy(reinterpret_cast<GdkEvent*>(event));
  }

  return TRUE;
}

// static
gint BrowserWindowOsrGtk::KeyEvent(GtkWidget* widget,
                                   GdkEventKey* event,
                                   BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (!self->browser_.get()) {
    return TRUE;
  }

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  // Based on WebKeyboardEventBuilder::Build from
  // content/browser/renderer_host/input/web_input_event_builders_gtk.cc.
  CefKeyEvent key_event;
  KeyboardCode windows_key_code = GdkEventToWindowsKeyCode(event);
  key_event.windows_key_code =
      GetWindowsKeyCodeWithoutLocation(windows_key_code);
  key_event.native_key_code = event->hardware_keycode;

  key_event.modifiers = GetCefStateModifiers(event->state);
  if (event->keyval >= GDK_KP_Space && event->keyval <= GDK_KP_9) {
    key_event.modifiers |= EVENTFLAG_IS_KEY_PAD;
  }
  if (key_event.modifiers & EVENTFLAG_ALT_DOWN) {
    key_event.is_system_key = true;
  }

  if (windows_key_code == VKEY_RETURN) {
    // We need to treat the enter key as a key press of character \r.  This
    // is apparently just how webkit handles it and what it expects.
    key_event.unmodified_character = '\r';
  } else {
    // FIXME: fix for non BMP chars
    key_event.unmodified_character =
        static_cast<int>(gdk_keyval_to_unicode(event->keyval));
  }

  // If ctrl key is pressed down, then control character shall be input.
  if (key_event.modifiers & EVENTFLAG_CONTROL_DOWN) {
    key_event.character = GetControlCharacter(
        windows_key_code, key_event.modifiers & EVENTFLAG_SHIFT_DOWN);
  } else {
    key_event.character = key_event.unmodified_character;
  }

  if (event->type == GDK_KEY_PRESS) {
    key_event.type = KEYEVENT_RAWKEYDOWN;
    host->SendKeyEvent(key_event);
    key_event.type = KEYEVENT_CHAR;
    host->SendKeyEvent(key_event);
  } else {
    key_event.type = KEYEVENT_KEYUP;
    host->SendKeyEvent(key_event);
  }

  return TRUE;
}

// static
gint BrowserWindowOsrGtk::MoveEvent(GtkWidget* widget,
                                    GdkEventMotion* event,
                                    BrowserWindowOsrGtk* self) {
  if (!self->browser_.get()) {
    return TRUE;
  }

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  gint x, y;
  GdkModifierType state;

  if (event->is_hint) {
    gdk_window_get_pointer(event->window, &x, &y, &state);
  } else {
    x = (gint)event->x;
    y = (gint)event->y;
    state = (GdkModifierType)event->state;
    if (x == 0 && y == 0) {
      // Invalid coordinates of (0,0) appear from time to time in
      // enter-notify-event and leave-notify-event events. Sending them may
      // cause StartDragging to never get called, so just ignore these.
      return TRUE;
    }
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  CefMouseEvent mouse_event;
  mouse_event.x = x;
  mouse_event.y = y;
  self->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  DeviceToLogical(mouse_event, device_scale_factor);
  mouse_event.modifiers = GetCefStateModifiers(state);

  bool mouse_leave = (event->type == GDK_LEAVE_NOTIFY);
  host->SendMouseMoveEvent(mouse_event, mouse_leave);

  // Save mouse event that can be a possible trigger for drag.
  if (!self->drag_context_ &&
      (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)) {
    if (self->drag_trigger_event_) {
      gdk_event_free(self->drag_trigger_event_);
    }
    self->drag_trigger_event_ =
        gdk_event_copy(reinterpret_cast<GdkEvent*>(event));
  }

  return TRUE;
}

// static
gint BrowserWindowOsrGtk::ScrollEvent(GtkWidget* widget,
                                      GdkEventScroll* event,
                                      BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (!self->browser_.get()) {
    return TRUE;
  }

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  CefMouseEvent mouse_event;
  mouse_event.x = event->x;
  mouse_event.y = event->y;
  self->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  DeviceToLogical(mouse_event, device_scale_factor);
  mouse_event.modifiers = GetCefStateModifiers(event->state);
  mouse_event.modifiers |= EVENTFLAG_PRECISION_SCROLLING_DELTA;

  static const int scrollbarPixelsPerGtkTick = 40;
  int deltaX = 0;
  int deltaY = 0;
  switch (event->direction) {
    case GDK_SCROLL_UP:
      deltaY = scrollbarPixelsPerGtkTick;
      break;
    case GDK_SCROLL_DOWN:
      deltaY = -scrollbarPixelsPerGtkTick;
      break;
    case GDK_SCROLL_LEFT:
      deltaX = scrollbarPixelsPerGtkTick;
      break;
    case GDK_SCROLL_RIGHT:
      deltaX = -scrollbarPixelsPerGtkTick;
      break;
    case GDK_SCROLL_SMOOTH:
      NOTIMPLEMENTED();
      break;
  }

  host->SendMouseWheelEvent(mouse_event, deltaX, deltaY);
  return TRUE;
}

// static
gint BrowserWindowOsrGtk::FocusEvent(GtkWidget* widget,
                                     GdkEventFocus* event,
                                     BrowserWindowOsrGtk* self) {
  // May be called on the main thread and the UI thread.
  if (self->browser_.get()) {
    self->browser_->GetHost()->SetFocus(event->in == TRUE);
  }
  return TRUE;
}

// static
gboolean BrowserWindowOsrGtk::TouchEvent(GtkWidget* widget,
                                         GdkEventTouch* event,
                                         BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (!self->browser_.get()) {
    return TRUE;
  }

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  CefTouchEvent touch_event;
  switch (event->type) {
    case GDK_TOUCH_BEGIN:
      touch_event.type = CEF_TET_PRESSED;
      break;
    case GDK_TOUCH_UPDATE:
      touch_event.type = CEF_TET_MOVED;
      break;
    case GDK_TOUCH_END:
      touch_event.type = CEF_TET_RELEASED;
      break;
    default:
      return TRUE;
  }

  touch_event.x = event->x;
  touch_event.y = event->y;
  touch_event.radius_x = 0;
  touch_event.radius_y = 0;
  touch_event.rotation_angle = 0;
  touch_event.pressure = 0;
  DeviceToLogical(touch_event, device_scale_factor);
  touch_event.modifiers = GetCefStateModifiers(event->state);

  host->SendTouchEvent(touch_event);
  return TRUE;
}

bool BrowserWindowOsrGtk::IsOverPopupWidget(int x, int y) const {
  const CefRect& rc = renderer_.popup_rect();
  int popup_right = rc.x + rc.width;
  int popup_bottom = rc.y + rc.height;
  return (x >= rc.x) && (x < popup_right) && (y >= rc.y) && (y < popup_bottom);
}

int BrowserWindowOsrGtk::GetPopupXOffset() const {
  return renderer_.original_popup_rect().x - renderer_.popup_rect().x;
}

int BrowserWindowOsrGtk::GetPopupYOffset() const {
  return renderer_.original_popup_rect().y - renderer_.popup_rect().y;
}

void BrowserWindowOsrGtk::ApplyPopupOffset(int& x, int& y) const {
  if (IsOverPopupWidget(x, y)) {
    x += GetPopupXOffset();
    y += GetPopupYOffset();
  }
}

bool BrowserWindowOsrGtk::EnableGL() {
  CEF_REQUIRE_UI_THREAD();

  if (gl_enabled_) {
    return true;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;
  if (!surface_ || !surface_->MakeCurrent()) {
    return false;
  }

  gl_enabled_ = renderer_.Initialize();
  surface_->ReleaseCurrent();
  return gl_enabled_;
}

void BrowserWindowOsrGtk::DisableGL() {
  CEF_REQUIRE_UI_THREAD();

  if (!gl_enabled_) {
    return;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;
  if (surface_->MakeCurrent()) {
    renderer_.Cleanup();
    surface_->ReleaseCurrent();
  }
  surface_->Destroy();

  gl_enabled_ = false;
}

void BrowserWindowOsrGtk::RegisterDragDrop() {
  REQUIRE_MAIN_THREAD();

  ScopedGdkThreadsEnter scoped_gdk_threads;

  // Succession of CEF d&d calls:
  // 1. DragTargetDragEnter
  // 2. DragTargetDragOver
  // 3. DragTargetDragLeave - optional
  // 4. DragSourceSystemDragEnded - optional, to cancel dragging
  // 5. DragTargetDrop
  // 6. DragSourceEndedAt
  // 7. DragSourceSystemDragEnded

  // Succession of GTK d&d events:
  // 1. drag-begin-event, drag-data-get
  // 2. drag-motion
  // 3. drag-leave
  // 4. drag-failed
  // 5. drag-drop, drag-data-received
  // 6. 7. drag-end-event

  // Using gtk_drag_begin in StartDragging instead of calling
  // gtk_drag_source_set here. Doing so because when using gtk_drag_source_set
  // then StartDragging is being called very late, about ten DragMotion events
  // after DragBegin, and drag icon can be set only when beginning drag.
  // Default values for drag threshold are set to 8 pixels in both GTK and
  // Chromium, but doesn't work as expected.
  // --OFF--
  // gtk_drag_source_set(widget_, GDK_BUTTON1_MASK, nullptr, 0,
  // GDK_ACTION_COPY);

  // Source widget events.
  g_signal_connect(G_OBJECT(widget_), "drag_begin",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragBegin), this);
  g_signal_connect(G_OBJECT(widget_), "drag_data_get",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragDataGet), this);
  g_signal_connect(G_OBJECT(widget_), "drag_end",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragEnd), this);

  // Destination widget and its events.
  gtk_drag_dest_set(widget_, (GtkDestDefaults)0, (GtkTargetEntry*)nullptr, 0,
                    (GdkDragAction)GDK_ACTION_COPY);
  g_signal_connect(G_OBJECT(widget_), "drag_motion",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragMotion), this);
  g_signal_connect(G_OBJECT(widget_), "drag_leave",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragLeave), this);
  g_signal_connect(G_OBJECT(widget_), "drag_failed",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragFailed), this);
  g_signal_connect(G_OBJECT(widget_), "drag_drop",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragDrop), this);
  g_signal_connect(G_OBJECT(widget_), "drag_data_received",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragDataReceived), this);
}

void BrowserWindowOsrGtk::UnregisterDragDrop() {
  ScopedGdkThreadsEnter scoped_gdk_threads;
  gtk_drag_dest_unset(widget_);
  // Drag events are unregistered in OnBeforeClose by calling
  // g_signal_handlers_disconnect_matched.
}

void BrowserWindowOsrGtk::DragReset() {
  if (drag_trigger_event_) {
    gdk_event_free(drag_trigger_event_);
    drag_trigger_event_ = nullptr;
  }
  drag_data_ = nullptr;
  drag_operation_ = DRAG_OPERATION_NONE;
  if (drag_context_) {
    g_object_unref(drag_context_);
    drag_context_ = nullptr;
  }
  drag_leave_ = false;
  drag_drop_ = false;
}

// static
void BrowserWindowOsrGtk::DragBegin(GtkWidget* widget,
                                    GdkDragContext* drag_context,
                                    BrowserWindowOsrGtk* self) {
  // Load drag icon.
  if (!self->drag_data_->HasImage()) {
    LOG(ERROR) << "Failed to set drag icon, drag image not available";
    return;
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  int pixel_width = 0;
  int pixel_height = 0;
  CefRefPtr<CefBinaryValue> image_binary =
      self->drag_data_->GetImage()->GetAsPNG(device_scale_factor, true,
                                             pixel_width, pixel_height);
  if (!image_binary) {
    LOG(ERROR) << "Failed to set drag icon, drag image error";
    return;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  size_t image_size = image_binary->GetSize();
  guint8* image_buffer = (guint8*)malloc(image_size);  // must free
  image_binary->GetData((void*)image_buffer, image_size, 0);
  GdkPixbufLoader* loader = nullptr;  // must unref
  GError* error = nullptr;            // must free
  GdkPixbuf* pixbuf = nullptr;        // owned by loader
  gboolean success = FALSE;
  loader = gdk_pixbuf_loader_new_with_type("png", &error);
  if (error == nullptr && loader) {
    success =
        gdk_pixbuf_loader_write(loader, image_buffer, image_size, nullptr);
    if (success) {
      success = gdk_pixbuf_loader_close(loader, nullptr);
      if (success) {
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pixbuf) {
          CefPoint image_hotspot = self->drag_data_->GetImageHotspot();
          int hotspot_x = image_hotspot.x;
          int hotspot_y = image_hotspot.y;
          gtk_drag_set_icon_pixbuf(drag_context, pixbuf, hotspot_x, hotspot_y);
        } else {
          LOG(ERROR) << "Failed to set drag icon, pixbuf error";
        }
      } else {
        LOG(ERROR) << "Failed to set drag icon, loader close error";
      }
    } else {
      LOG(ERROR) << "Failed to set drag icon, loader write error";
    }
  } else {
    LOG(ERROR) << "Failed to set drag icon, loader creation error";
  }
  if (loader) {
    g_object_unref(loader);  // unref
  }
  if (error) {
    g_error_free(error);  // free
  }
  free(image_buffer);  // free
}

// static
void BrowserWindowOsrGtk::DragDataGet(GtkWidget* widget,
                                      GdkDragContext* drag_context,
                                      GtkSelectionData* data,
                                      guint info,
                                      guint time,
                                      BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();
  // No drag targets are set so this callback is never called.
}

// static
void BrowserWindowOsrGtk::DragEnd(GtkWidget* widget,
                                  GdkDragContext* drag_context,
                                  BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (self->browser_) {
    // Sometimes there is DragEnd event generated without prior DragDrop.
    // Maybe related to drag-leave bug described in comments in DragLeave.
    if (!self->drag_drop_) {
      // Real coordinates not available.
      self->browser_->GetHost()->DragSourceEndedAt(-1, -1,
                                                   self->drag_operation_);
    }
    self->browser_->GetHost()->DragSourceSystemDragEnded();
  }

  self->DragReset();
}

// static
gboolean BrowserWindowOsrGtk::DragMotion(GtkWidget* widget,
                                         GdkDragContext* drag_context,
                                         gint x,
                                         gint y,
                                         guint time,
                                         BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  // MoveEvent is never called during drag & drop, so must call
  // SendMouseMoveEvent here.
  CefMouseEvent mouse_event;
  mouse_event.x = x;
  mouse_event.y = y;
  mouse_event.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
  self->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  DeviceToLogical(mouse_event, device_scale_factor);
  if (self->browser_) {
    bool mouse_leave = self->drag_leave_;
    self->browser_->GetHost()->SendMouseMoveEvent(mouse_event, mouse_leave);
  }

  // Mouse event.
  CefMouseEvent ev;
  ev.x = x;
  ev.y = y;
  ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;

  CefBrowserHost::DragOperationsMask allowed_ops =
      GetDragOperationsMask(drag_context);

  // Send drag enter event if needed.
  if (self->drag_leave_ && self->browser_) {
    self->browser_->GetHost()->DragTargetDragEnter(self->drag_data_, ev,
                                                   allowed_ops);
  }

  // Send drag over event.
  if (self->browser_) {
    self->browser_->GetHost()->DragTargetDragOver(ev, allowed_ops);
  }

  // Update GTK drag status.
  if (widget == self->widget_) {
    gdk_drag_status(drag_context, GDK_ACTION_COPY, time);
    if (self->drag_leave_) {
      self->drag_leave_ = false;
    }
    return TRUE;
  } else {
    LOG(WARNING) << "Invalid drag destination widget";
    gdk_drag_status(drag_context, (GdkDragAction)0, time);
    return FALSE;
  }
}

// static
void BrowserWindowOsrGtk::DragLeave(GtkWidget* widget,
                                    GdkDragContext* drag_context,
                                    guint time,
                                    BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  // There is no drag-enter event in GTK. The first drag-motion event
  // after drag-leave will be a drag-enter event.

  // There seems to be a bug during GTK drop, drag-leave event is generated
  // just before drag-drop. A solution is to call DragTargetDragEnter
  // and DragTargetDragOver in DragDrop when drag_leave_ is true.

  // Send drag leave event.
  if (self->browser_) {
    self->browser_->GetHost()->DragTargetDragLeave();
  }

  self->drag_leave_ = true;
}

// static
gboolean BrowserWindowOsrGtk::DragFailed(GtkWidget* widget,
                                         GdkDragContext* drag_context,
                                         GtkDragResult result,
                                         BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  // Send drag end coordinates and system drag ended event.
  if (self->browser_) {
    // Real coordinates not available.
    self->browser_->GetHost()->DragSourceEndedAt(-1, -1, self->drag_operation_);
    self->browser_->GetHost()->DragSourceSystemDragEnded();
  }

  self->DragReset();
  return TRUE;
}

// static
gboolean BrowserWindowOsrGtk::DragDrop(GtkWidget* widget,
                                       GdkDragContext* drag_context,
                                       gint x,
                                       gint y,
                                       guint time,
                                       BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  // Finish GTK drag.
  gtk_drag_finish(drag_context, TRUE, FALSE, time);

  // Mouse event.
  CefMouseEvent ev;
  ev.x = x;
  ev.y = y;
  ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;

  CefBrowserHost::DragOperationsMask allowed_ops =
      GetDragOperationsMask(drag_context);

  // Send drag enter/over events if needed (read comment in DragLeave).
  if (self->drag_leave_ && self->browser_) {
    self->browser_->GetHost()->DragTargetDragEnter(self->drag_data_, ev,
                                                   allowed_ops);
    self->browser_->GetHost()->DragTargetDragOver(ev, allowed_ops);
  }

  // Send drag drop event.
  if (self->browser_) {
    self->browser_->GetHost()->DragTargetDrop(ev);
  }

  // Send drag end coordinates.
  if (self->browser_) {
    self->browser_->GetHost()->DragSourceEndedAt(x, y, self->drag_operation_);
  }

  self->drag_drop_ = true;
  return TRUE;
}

// static
void BrowserWindowOsrGtk::DragDataReceived(GtkWidget* widget,
                                           GdkDragContext* drag_context,
                                           gint x,
                                           gint y,
                                           GtkSelectionData* data,
                                           guint info,
                                           guint time,
                                           BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();
  // This callback is never called because DragDrop does not call
  // gtk_drag_get_data, as only dragging inside web view is supported.
}

}  // namespace client
