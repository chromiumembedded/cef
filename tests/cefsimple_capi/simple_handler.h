// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_HANDLER_H_

#include <stdatomic.h>

#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_client_capi.h"
#include "tests/cefsimple_capi/simple_browser_list.h"

// Forward declarations.
typedef struct _simple_display_handler_t simple_display_handler_t;
typedef struct _simple_life_span_handler_t simple_life_span_handler_t;
typedef struct _simple_load_handler_t simple_load_handler_t;

// Client handler structure.
// Implements cef_client_t interface.
typedef struct _simple_handler_t {
  // MUST be first member - CEF base structure.
  cef_client_t client;

  // Reference count for this object.
  atomic_int ref_count;

  // Handler implementations (owned by this structure).
  simple_display_handler_t* display_handler;
  simple_life_span_handler_t* life_span_handler;
  simple_load_handler_t* load_handler;

  // True if this client is Alloy style, otherwise Chrome style.
  int is_alloy_style;

  // List of existing browser windows.
  browser_list_t browser_list;

  // Set to true when browsers are closing.
  int is_closing;
} simple_handler_t;

// Display handler structure.
// Implements cef_display_handler_t interface.
typedef struct _simple_display_handler_t {
  // MUST be first member - CEF base structure.
  cef_display_handler_t handler;

  // Reference count for this object.
  atomic_int ref_count;

  // Back reference to parent handler.
  simple_handler_t* parent;
} simple_display_handler_t;

// Life span handler structure.
// Implements cef_life_span_handler_t interface.
typedef struct _simple_life_span_handler_t {
  // MUST be first member - CEF base structure.
  cef_life_span_handler_t handler;

  // Reference count for this object.
  atomic_int ref_count;

  // Back reference to parent handler.
  simple_handler_t* parent;
} simple_life_span_handler_t;

// Load handler structure.
// Implements cef_load_handler_t interface.
typedef struct _simple_load_handler_t {
  // MUST be first member - CEF base structure.
  cef_load_handler_t handler;

  // Reference count for this object.
  atomic_int ref_count;

  // Back reference to parent handler.
  simple_handler_t* parent;
} simple_load_handler_t;

// Create a new client handler instance.
// Parameters:
//   is_alloy_style - 1 for Alloy style, 0 for Chrome style
// Returns a pointer with ref count of 1.
// Caller is responsible for releasing the reference when done.
simple_handler_t* simple_handler_create(int is_alloy_style);

// Get the global singleton instance (if created).
// Does NOT add a reference.
// Returns NULL if no instance has been created yet.
simple_handler_t* simple_handler_get_instance(void);

// Request that all existing browser windows close.
void simple_handler_close_all_browsers(simple_handler_t* handler,
                                       int force_close);

// Show the main window (for macOS dock icon activation).
void simple_handler_show_main_window(simple_handler_t* handler);

// Platform-specific title change implementation.
// Declared here but implemented in platform-specific .c files.
void simple_handler_platform_title_change(simple_handler_t* handler,
                                          cef_browser_t* browser,
                                          const cef_string_t* title);

// Platform-specific show window implementation (macOS only).
// Declared here but implemented in platform-specific .c files.
void simple_handler_platform_show_window(simple_handler_t* handler,
                                         cef_browser_t* browser);

#endif  // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_HANDLER_H_
