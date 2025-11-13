// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_BROWSER_LIST_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_BROWSER_LIST_H_

#include <stddef.h>

#include "include/capi/cef_browser_capi.h"

// Dynamic list of browser instances with automatic reference counting.
//
// THREAD SAFETY:
//   - NOT thread-safe. All methods must be called on the CEF UI thread.
//
// REF-COUNTING RULES:
//
// browser_list_add():
//   - Takes a browser pointer and ADDS a reference (list takes ownership)
//   - Ref count change: +1 (one new reference owned by the list)
//   - Caller must still release their own reference if they have one
//
// browser_list_remove():
//   - Finds the browser in the list and RELEASES the list's reference
//   - Ref count change: -1 (list gives up ownership)
//   - Does NOT modify the browser parameter's reference (caller still owns it)
//
// browser_list_destroy():
//   - Releases ALL browser references owned by the list
//   - Frees the internal array
//
typedef struct {
  cef_browser_t** browsers;
  size_t count;
  size_t capacity;
} browser_list_t;

// Initialize an empty browser list.
void browser_list_init(browser_list_t* list);

// Add a browser to the list.
// Adds a reference - the list takes ownership of one reference.
int browser_list_add(browser_list_t* list, cef_browser_t* browser);

// Remove a browser from the list.
// Releases the list's reference - does not affect the caller's reference.
void browser_list_remove(browser_list_t* list, cef_browser_t* browser);

// Destroy the browser list.
// Releases all browser references and frees internal memory.
void browser_list_destroy(browser_list_t* list);

// Get the number of browsers in the list.
size_t browser_list_count(const browser_list_t* list);

// Get a browser by index.
// Does NOT add a reference.
// Returns NULL if index is out of bounds.
cef_browser_t* browser_list_get(const browser_list_t* list, size_t index);

#endif  // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_BROWSER_LIST_H_
