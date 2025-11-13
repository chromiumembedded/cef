// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <stdatomic.h>
#include <stdlib.h>

#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_handler.h"
#include "tests/cefsimple_capi/simple_utils.h"

//
// Display handler implementation.
//

IMPLEMENT_REFCOUNTING_SIMPLE(simple_display_handler_t,
                             display_handler,
                             ref_count)

void CEF_CALLBACK display_handler_on_title_change(cef_display_handler_t* self,
                                                  cef_browser_t* browser,
                                                  const cef_string_t* title) {
  simple_display_handler_t* handler = (simple_display_handler_t*)self;

  // Call platform-specific implementation.
  if (handler->parent->is_alloy_style) {
    simple_handler_platform_title_change(handler->parent, browser, title);
  }

  // Release the browser reference (CEF gave us one for this callback).
  browser->base.release(&browser->base);
}

simple_display_handler_t* display_handler_create(simple_handler_t* parent) {
  simple_display_handler_t* handler =
      (simple_display_handler_t*)calloc(1, sizeof(simple_display_handler_t));
  CHECK(handler);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base, cef_display_handler_t,
                           display_handler);

  // Set callbacks.
  handler->handler.on_title_change = display_handler_on_title_change;

  // Store parent reference (no ref count - parent owns us).
  handler->parent = parent;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  return handler;
}
