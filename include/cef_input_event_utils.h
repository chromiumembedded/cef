// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_INCLUDE_CEF_INPUT_EVENT_UTILS_H_
#define CEF_INCLUDE_CEF_INPUT_EVENT_UTILS_H_
#pragma once

#include <vector>

#include "include/cef_base.h"

#if CEF_API_ADDED(CEF_NEXT)

///
/// Returns a Windows-equivalent key code from CEF DOM code/key values, or zero
/// for invalid or unmappable input. Named keys preserve their logical meaning,
/// including remapped modifiers. Printable US characters use their logical
/// key identity; other characters and dead keys fall back to physical position.
/// Printable keypad keys retain their keypad key codes. Shift, Control and Alt
/// use non-located codes; their side remains represented by |dom_code|.
/// This stateless helper neither detects a layout nor generates insertion text.
///
/*--cef(added=next)--*/
int CefWindowsKeyCodeFromDomCodeAndKey(uint32_t dom_code, uint32_t dom_key);

#if defined(OS_WIN)

///
/// Synchronously converts a Windows keyboard message on its receiving thread.
/// |wparam| and |lparam| contain the SDK WPARAM/LPARAM values. Returns false
/// for unsupported messages or invalid input, leaving |event| unchanged. On
/// success fills one event. Detects state, layout and timestamp on this thread;
/// does not synthesize CHAR messages.
///
/*--cef(added=next,platform=win)--*/
bool CefKeyboardInputEventFromWindowsMessage(uint32_t message,
                                             uint64_t wparam,
                                             int64_t lparam,
                                             CefKeyboardInputEvent& event);

///
/// Explicit-state variant for deterministic tests and replay. |keyboard_state|
/// contains 256 Windows GetKeyboardState byte values (each integer is 0..255).
/// |keyboard_layout| contains the SDK HKL handle value in this process; zero
/// detects the receiving thread's layout. |timestamp| is monotonic microseconds
/// in this process; zero detects time. The provided state is authoritative and
/// does not change the application's key/lock state or active layout. Building
/// a layout cache uses Chromium's native translation behavior and may consume
/// pending dead-key state. An array of 256 zeros supplies no held keys or
/// active locks; the array must have exactly 256 entries. Returns false on
/// failure, leaving |event| unchanged.
///
/*--cef(added=next,platform=win)--*/
bool CefKeyboardInputEventFromWindowsMessageEx(
    uint32_t message,
    uint64_t wparam,
    int64_t lparam,
    const std::vector<int>& keyboard_state,
    uint64_t keyboard_layout,
    int64_t timestamp,
    CefKeyboardInputEvent& event);

#elif defined(OS_MAC)

///
/// Synchronously converts the borrowed NSEvent represented by |native_event| on
/// the AppKit main thread. Supports keydown, keyup, and flags-changed events.
/// For keydown/up, characters and charactersIgnoringModifiers must each contain
/// at most three UTF-16 units, leaving room for termination in the four-unit
/// buffers. Returns false for unsupported input or calls from another thread,
/// leaving |event| unchanged.
///
/// Conversion produces key events, not CHAR insertion events. Existing
/// NSTextInputContext/IME handling still determines insertion. AppKit insertion
/// commands such as Return may require a separate CHAR event even when no
/// insertText: call occurs; see cefclient's shared macOS text-input client for
/// an implementation of this handling.
///
/*--cef(added=next,platform=mac)--*/
bool CefKeyboardInputEventFromMacEvent(CefEventHandle native_event,
                                       CefKeyboardInputEvent& event);

///
/// Explicit-source variant for deterministic tests and replay. Must be called
/// on the AppKit main thread, with the same supported event types, text limits,
/// and separate insertion handling as CefKeyboardInputEventFromMacEvent.
/// Returns false on failure, leaving |event| unchanged. |input_source_id| is
/// the macOS TIS input-source ID of an installed keyboard layout, or empty to
/// detect the current source. |keyboard_type| is the Carbon keyboard type used
/// by UCKeyTranslate, or -1 to detect it. Does not activate the source. Event
/// attributes, including text, modifiers and time, come from NSEvent.
///
/*--cef(added=next,platform=mac,optional_param=input_source_id)--*/
bool CefKeyboardInputEventFromMacEventEx(CefEventHandle native_event,
                                         const CefString& input_source_id,
                                         int keyboard_type,
                                         CefKeyboardInputEvent& event);

#elif defined(OS_LINUX)

///
/// Returns the named or dead CEF DOM key for an X11/XKB keysym, or
/// CEF_DOM_KEY_NONE for a printable or unrecognized keysym. The numeric keysym
/// must use the X11/XKB namespace (including GTK GDK keyvals). This stateless
/// helper never accesses a display or keyboard layout. The source framework
/// must supply printable Unicode text, including non-BMP characters.
///
/*--cef(added=next,platform=linux)--*/
uint32_t CefNonPrintableDomKeyFromXKeysym(uint32_t keysym);

#endif  // defined(OS_LINUX)

#endif  // CEF_API_ADDED(CEF_NEXT)

#endif  // CEF_INCLUDE_CEF_INPUT_EVENT_UTILS_H_
