// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <stdatomic.h>
#include <stdlib.h>

#include "include/capi/cef_app_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_browser_list.h"
#include "tests/cefsimple_capi/simple_handler.h"
#include "tests/cefsimple_capi/simple_utils.h"

//
// Life span handler implementation.
//

IMPLEMENT_REFCOUNTING_SIMPLE(simple_life_span_handler_t,
                             life_span_handler,
                             ref_count)

void CEF_CALLBACK
life_span_handler_on_after_created(cef_life_span_handler_t* self,
                                   cef_browser_t* browser) {
  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)self;

  // Add to the list of existing browsers.
  // browser_list_add adds its own reference, so we can release the parameter.
  browser_list_add(&handler->parent->browser_list, browser);

  // Release the browser callback parameter.
  // The list has its own reference now.
  browser->base.release(&browser->base);
}

int CEF_CALLBACK life_span_handler_do_close(cef_life_span_handler_t* self,
                                            cef_browser_t* browser) {
  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)self;

  // Closing the main window requires special handling.
  if (browser_list_count(&handler->parent->browser_list) == 1) {
    // Set a flag to indicate that the window close should be allowed.
    handler->parent->is_closing = 1;
  }

  // Release the browser callback parameter before returning.
  browser->base.release(&browser->base);

  // Allow the close. Return false to proceed with closing.
  return 0;
}

void CEF_CALLBACK
life_span_handler_on_before_close(cef_life_span_handler_t* self,
                                  cef_browser_t* browser) {
  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)self;

  // Remove from the list of existing browsers.
  // This releases the list's reference to the browser.
  browser_list_remove(&handler->parent->browser_list, browser);

  if (browser_list_count(&handler->parent->browser_list) == 0) {
    // All browser windows have closed. Quit the application message loop.
    cef_quit_message_loop();
  }

  // Release the browser callback parameter before returning.
  browser->base.release(&browser->base);
}

simple_life_span_handler_t* life_span_handler_create(simple_handler_t* parent) {
  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)calloc(
      1, sizeof(simple_life_span_handler_t));
  CHECK(handler);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base, cef_life_span_handler_t,
                           life_span_handler);

  // Set callbacks.
  handler->handler.on_after_created = life_span_handler_on_after_created;
  handler->handler.do_close = life_span_handler_do_close;
  handler->handler.on_before_close = life_span_handler_on_before_close;

  // Store parent reference (no ref count - parent owns us).
  handler->parent = parent;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  return handler;
}
