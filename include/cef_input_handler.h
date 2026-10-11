// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_INCLUDE_CEF_INPUT_HANDLER_H_
#define CEF_INCLUDE_CEF_INPUT_HANDLER_H_
#pragma once

#include "include/cef_base.h"
#include "include/cef_browser.h"

#if CEF_API_ADDED(CEF_NEXT)
///
/// Handles portable input events on the UI thread. Keyboard callbacks run
/// before the corresponding CefKeyboardHandler callbacks.
///
/*--cef(source=client,added=next)--*/
class CefInputHandler : public virtual CefBaseRefCounted {
 public:
  ///
  /// Called before keyboard input is offered to the renderer. Return true to
  /// consume the event. To classify it as an application shortcut for
  /// OnKeyboardEvent, set |is_keyboard_shortcut| to true and return false.
  /// Otherwise, return false to try CefKeyboardHandler::OnPreKeyEvent (if
  /// provided) before continuing normal processing. |is_keyboard_shortcut| is
  /// initially false.
  ///
  /// Returning true for RAWKEYDOWN causes Chromium to suppress subsequent CHAR
  /// and KEYUP events until the next keydown. Suppressed events do not reach
  /// these callbacks, except that Escape KEYUP is still offered to
  /// OnPreKeyboardEvent.
  ///
  /*--cef()--*/
  virtual bool OnPreKeyboardEvent(CefRefPtr<CefBrowser> browser,
                                  const CefKeyboardInputEvent& event,
                                  const CefInputEventContext& context,
                                  bool* is_keyboard_shortcut) {
    return false;
  }

  ///
  /// Called for keyboard events left unhandled by the renderer. Return true to
  /// consume the event or false to try CefKeyboardHandler::OnKeyEvent (if
  /// provided) before allowing normal platform fallback.
  ///
  /// This is not an observer for every submitted event. Renderer-consumed
  /// editing commands and events marked internally to skip unhandled processing
  /// do not reach this callback. Do not assume a complete down/character/up
  /// callback sequence.
  ///
  /*--cef()--*/
  virtual bool OnKeyboardEvent(CefRefPtr<CefBrowser> browser,
                               const CefKeyboardInputEvent& event,
                               const CefInputEventContext& context) {
    return false;
  }
};
#endif  // CEF_API_ADDED(CEF_NEXT)

#endif  // CEF_INCLUDE_CEF_INPUT_HANDLER_H_
