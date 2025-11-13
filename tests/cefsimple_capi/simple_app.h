// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_APP_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_APP_H_

#include <stdatomic.h>

#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_browser_process_handler_capi.h"

// Forward declaration.
typedef struct _simple_browser_process_handler_t
    simple_browser_process_handler_t;
typedef struct _simple_handler_t simple_handler_t;

// Application handler structure.
// Implements cef_app_t interface.
typedef struct _simple_app_t {
  // MUST be first member - CEF base structure.
  cef_app_t app;

  // Reference count for this object.
  atomic_int ref_count;

  // Browser process handler (owned by this structure).
  simple_browser_process_handler_t* browser_process_handler;
} simple_app_t;

// Browser process handler structure.
// Implements cef_browser_process_handler_t interface.
typedef struct _simple_browser_process_handler_t {
  // MUST be first member - CEF base structure.
  cef_browser_process_handler_t handler;

  // Reference count for this object.
  atomic_int ref_count;

  // Note: We don't store a client reference here.
  // GetDefaultClient() uses simple_handler_get_instance() to get the global
  // instance.
} simple_browser_process_handler_t;

// Create a new application handler instance.
// Returns a pointer with ref count of 1.
// Caller is responsible for releasing the reference when done.
simple_app_t* simple_app_create(void);

#endif  // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_APP_H_
