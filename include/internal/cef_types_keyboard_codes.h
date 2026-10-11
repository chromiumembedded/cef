// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_INCLUDE_INTERNAL_CEF_TYPES_KEYBOARD_CODES_H_
#define CEF_INCLUDE_INTERNAL_CEF_TYPES_KEYBOARD_CODES_H_
#pragma once

#include "include/cef_api_hash.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CEF_API_ADDED(CEF_NEXT)

/// Physical key identities in Chromium's USB-based DomCode encoding.
#if !defined(GENERATING_CEF_API_HASH)
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) CEF_DOM_CODE_##id = usb
#define DOM_CODE_DECLARATION enum cef_dom_code_t
#include "include/base/internal/cef_dom_code_data.inc"
#undef DOM_CODE_DECLARATION
#undef DOM_CODE
#else
enum cef_dom_code_t { CEF_DOM_CODE_NONE = 0 };
#endif
typedef enum cef_dom_code_t cef_dom_code_t;

/// Encoded logical keys. Use bitwise OR to combine a Unicode scalar with
/// CEF_DOM_KEY_UNICODE_FLAG, or a dead-key combining character with
/// CEF_DOM_KEY_DEAD_FLAG. Named values come from Chromium's table.
typedef enum {
  CEF_DOM_KEY_NONE = 0,
  CEF_DOM_KEY_UNICODE_FLAG = 1 << 22,
  CEF_DOM_KEY_DEAD_FLAG = (1 << 22) | (1 << 21),
#if !defined(GENERATING_CEF_API_HASH)
#define DOM_KEY_MAP_DECLARATION_START
#define DOM_KEY_UNI(key, id, value) CEF_DOM_KEY_##id = (1 << 22) | (value),
#define DOM_KEY_MAP(key, id, value) CEF_DOM_KEY_##id = (1 << 21) | (value),
#define DOM_KEY_MAP_DECLARATION_END
#include "include/base/internal/cef_dom_key_data.inc"
#undef DOM_KEY_MAP_DECLARATION_END
#undef DOM_KEY_MAP
#undef DOM_KEY_UNI
#undef DOM_KEY_MAP_DECLARATION_START
#endif
} cef_dom_key_t;

#endif  // CEF_API_ADDED(CEF_NEXT)

#ifdef __cplusplus
}
#endif

#endif  // CEF_INCLUDE_INTERNAL_CEF_TYPES_KEYBOARD_CODES_H_
