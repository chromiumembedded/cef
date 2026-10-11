// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/include/cef_input_event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_xkb.h"

uint32_t CefNonPrintableDomKeyFromXKeysym(uint32_t keysym) {
  return ui::NonPrintableXKeySymToDomKey(keysym);
}
