// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_handler.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/capi/cef_app_capi.h"
#include "include/capi/cef_base_capi.h"
#include "include/capi/cef_parser_capi.h"
#include "include/capi/cef_task_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"

// Global instance pointer.
static simple_handler_t* g_instance = NULL;

//
// Browser list management helpers.
//
// REF-COUNTING RULES FOR BROWSER LIST:
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

// Add a browser to the list.
// Adds a reference - the list takes ownership of one reference.
static void browser_list_add(simple_handler_t* handler,
                             cef_browser_t* browser) {
  if (!handler || !browser) {
    return;
  }

  // Ensure capacity.
  if (handler->browser_count >= handler->browser_capacity) {
    size_t new_capacity =
        handler->browser_capacity == 0 ? 4 : handler->browser_capacity * 2;
    cef_browser_t** new_list = (cef_browser_t**)realloc(
        handler->browser_list, new_capacity * sizeof(cef_browser_t*));
    if (!new_list) {
      return;
    }
    handler->browser_list = new_list;
    handler->browser_capacity = new_capacity;
  }

  // Add a reference for the list.
  // The list now owns one reference to this browser.
  browser->base.add_ref(&browser->base);

  // Store the browser pointer.
  handler->browser_list[handler->browser_count++] = browser;
}

// Remove a browser from the list.
// Releases the list's reference - does not affect the caller's reference.
static void browser_list_remove(simple_handler_t* handler,
                                cef_browser_t* browser) {
  if (!handler || !browser) {
    return;
  }

  // Find and remove the browser.
  for (size_t i = 0; i < handler->browser_count; ++i) {
    // Add a reference before calling is_same, since CEF functions take
    // ownership of parameters passed to them.
    browser->base.add_ref(&browser->base);

    if (handler->browser_list[i]->is_same(handler->browser_list[i], browser)) {
      // Release the list's reference to this browser.
      handler->browser_list[i]->base.release(&handler->browser_list[i]->base);

      // Shift remaining elements.
      for (size_t j = i; j < handler->browser_count - 1; ++j) {
        handler->browser_list[j] = handler->browser_list[j + 1];
      }
      handler->browser_count--;
      break;
    }
  }
}

//
// Client handler implementation.
//

IMPLEMENT_ADDREF(simple_handler_t, simple_handler, ref_count)
IMPLEMENT_HAS_ONE_REF(simple_handler_t, simple_handler, ref_count)
IMPLEMENT_HAS_AT_LEAST_ONE_REF(simple_handler_t, simple_handler, ref_count)

int CEF_CALLBACK simple_handler_release(cef_base_ref_counted_t* self) {
  simple_handler_t* handler = (simple_handler_t*)self;
  int count = atomic_fetch_sub(&handler->ref_count, 1) - 1;
  if (count == 0) {
    // Release all handlers.
    if (handler->display_handler) {
      handler->display_handler->handler.base.release(
          &handler->display_handler->handler.base);
    }
    if (handler->life_span_handler) {
      handler->life_span_handler->handler.base.release(
          &handler->life_span_handler->handler.base);
    }
    if (handler->load_handler) {
      handler->load_handler->handler.base.release(
          &handler->load_handler->handler.base);
    }

    // Release all browsers in the list.
    for (size_t i = 0; i < handler->browser_count; ++i) {
      handler->browser_list[i]->base.release(&handler->browser_list[i]->base);
    }
    free(handler->browser_list);

    // Clear global instance if this is it.
    if (g_instance == handler) {
      g_instance = NULL;
    }

    free(handler);
    return 1;
  }
  return 0;
}

// Forward declarations for handler getters.
cef_display_handler_t* CEF_CALLBACK
simple_handler_get_display_handler(cef_client_t* self);
cef_life_span_handler_t* CEF_CALLBACK
simple_handler_get_life_span_handler(cef_client_t* self);
cef_load_handler_t* CEF_CALLBACK
simple_handler_get_load_handler(cef_client_t* self);

//
// Display handler implementation.
//

IMPLEMENT_ADDREF(simple_display_handler_t, display_handler, ref_count)
IMPLEMENT_HAS_ONE_REF(simple_display_handler_t, display_handler, ref_count)
IMPLEMENT_HAS_AT_LEAST_ONE_REF(simple_display_handler_t,
                               display_handler,
                               ref_count)

int CEF_CALLBACK display_handler_release(cef_base_ref_counted_t* self) {
  simple_display_handler_t* handler = (simple_display_handler_t*)self;
  int count = atomic_fetch_sub(&handler->ref_count, 1) - 1;
  if (count == 0) {
    free(handler);
    return 1;
  }
  return 0;
}

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
  if (!handler) {
    return NULL;
  }

  // Initialize base structure.
  handler->handler.base.size = sizeof(cef_display_handler_t);
  handler->handler.base.add_ref = display_handler_add_ref;
  handler->handler.base.release = display_handler_release;
  handler->handler.base.has_one_ref = display_handler_has_one_ref;
  handler->handler.base.has_at_least_one_ref =
      display_handler_has_at_least_one_ref;

  // Set callbacks.
  handler->handler.on_title_change = display_handler_on_title_change;

  // Store parent reference (no ref count - parent owns us).
  handler->parent = parent;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  return handler;
}

//
// Life span handler implementation.
//

IMPLEMENT_ADDREF(simple_life_span_handler_t, life_span_handler, ref_count)
IMPLEMENT_HAS_ONE_REF(simple_life_span_handler_t, life_span_handler, ref_count)
IMPLEMENT_HAS_AT_LEAST_ONE_REF(simple_life_span_handler_t,
                               life_span_handler,
                               ref_count)

int CEF_CALLBACK life_span_handler_release(cef_base_ref_counted_t* self) {
  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)self;
  int count = atomic_fetch_sub(&handler->ref_count, 1) - 1;
  if (count == 0) {
    free(handler);
    return 1;
  }
  return 0;
}

void CEF_CALLBACK
life_span_handler_on_after_created(cef_life_span_handler_t* self,
                                   cef_browser_t* browser) {
  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)self;

  // Add to the list of existing browsers.
  // browser_list_add adds its own reference, so we can release the parameter.
  browser_list_add(handler->parent, browser);

  // Release the browser callback parameter.
  // The list has its own reference now.
  browser->base.release(&browser->base);
}

int CEF_CALLBACK life_span_handler_do_close(cef_life_span_handler_t* self,
                                            cef_browser_t* browser) {
  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)self;

  // Closing the main window requires special handling.
  if (handler->parent->browser_count == 1) {
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
  browser_list_remove(handler->parent, browser);

  if (handler->parent->browser_count == 0) {
    // All browser windows have closed. Quit the application message loop.
    cef_quit_message_loop();
  }

  // Release the browser callback parameter before returning.
  browser->base.release(&browser->base);
}

simple_life_span_handler_t* life_span_handler_create(simple_handler_t* parent) {
  simple_life_span_handler_t* handler = (simple_life_span_handler_t*)calloc(
      1, sizeof(simple_life_span_handler_t));
  if (!handler) {
    return NULL;
  }

  // Initialize base structure.
  handler->handler.base.size = sizeof(cef_life_span_handler_t);
  handler->handler.base.add_ref = life_span_handler_add_ref;
  handler->handler.base.release = life_span_handler_release;
  handler->handler.base.has_one_ref = life_span_handler_has_one_ref;
  handler->handler.base.has_at_least_one_ref =
      life_span_handler_has_at_least_one_ref;

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

//
// Load handler implementation.
//

IMPLEMENT_ADDREF(simple_load_handler_t, load_handler, ref_count)
IMPLEMENT_HAS_ONE_REF(simple_load_handler_t, load_handler, ref_count)
IMPLEMENT_HAS_AT_LEAST_ONE_REF(simple_load_handler_t, load_handler, ref_count)

int CEF_CALLBACK load_handler_release(cef_base_ref_counted_t* self) {
  simple_load_handler_t* handler = (simple_load_handler_t*)self;
  int count = atomic_fetch_sub(&handler->ref_count, 1) - 1;
  if (count == 0) {
    free(handler);
    return 1;
  }
  return 0;
}

void CEF_CALLBACK load_handler_on_load_error(cef_load_handler_t* self,
                                             cef_browser_t* browser,
                                             cef_frame_t* frame,
                                             cef_errorcode_t errorCode,
                                             const cef_string_t* errorText,
                                             const cef_string_t* failedUrl) {
  simple_load_handler_t* handler = (simple_load_handler_t*)self;

  // Only display error page in alloy style and not for aborted downloads.
  // Allow Chrome to show the error page in other cases.
  if (handler->parent->is_alloy_style && errorCode != ERR_ABORTED) {
    // Display a load error message using a data: URI.
    char error_html[1024];
    snprintf(error_html, sizeof(error_html),
             "<html><body bgcolor=\"white\">"
             "<h2>Failed to load URL with error %d.</h2></body></html>",
             errorCode);

    // Convert to cef_string_t.
    cef_string_t error_str = {};
    cef_string_from_ascii(error_html, strlen(error_html), &error_str);

    // Create data URI.
    cef_string_t mime_type = {};
    cef_string_from_ascii("text/html", 9, &mime_type);

    // Base64 encode the error HTML.
    cef_string_userfree_t encoded =
        cef_base64_encode(error_str.str, error_str.length);

    if (encoded) {
      // Create the data URI.
      char data_uri[2048];
      cef_string_utf8_t encoded_utf8 = {};
      cef_string_to_utf8(encoded->str, encoded->length, &encoded_utf8);
      snprintf(data_uri, sizeof(data_uri), "data:text/html;base64,%s",
               encoded_utf8.str);
      cef_string_utf8_clear(&encoded_utf8);

      // Load the data URI.
      cef_string_t data_uri_str = {};
      cef_string_from_ascii(data_uri, strlen(data_uri), &data_uri_str);
      frame->load_url(frame, &data_uri_str);
      cef_string_clear(&data_uri_str);

      cef_string_userfree_free(encoded);
    }

    cef_string_clear(&error_str);
    cef_string_clear(&mime_type);
  }

  // Release all object parameters.
  browser->base.release(&browser->base);
  frame->base.release(&frame->base);
}

simple_load_handler_t* load_handler_create(simple_handler_t* parent) {
  simple_load_handler_t* handler =
      (simple_load_handler_t*)calloc(1, sizeof(simple_load_handler_t));
  if (!handler) {
    return NULL;
  }

  // Initialize base structure.
  handler->handler.base.size = sizeof(cef_load_handler_t);
  handler->handler.base.add_ref = load_handler_add_ref;
  handler->handler.base.release = load_handler_release;
  handler->handler.base.has_one_ref = load_handler_has_one_ref;
  handler->handler.base.has_at_least_one_ref =
      load_handler_has_at_least_one_ref;

  // Set callbacks.
  handler->handler.on_load_error = load_handler_on_load_error;

  // Store parent reference (no ref count - parent owns us).
  handler->parent = parent;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  return handler;
}

//
// Client handler getter implementations.
//

cef_display_handler_t* CEF_CALLBACK
simple_handler_get_display_handler(cef_client_t* self) {
  simple_handler_t* handler = (simple_handler_t*)self;
  if (handler->display_handler) {
    // Add reference before returning.
    handler->display_handler->handler.base.add_ref(
        &handler->display_handler->handler.base);
    return &handler->display_handler->handler;
  }
  return NULL;
}

cef_life_span_handler_t* CEF_CALLBACK
simple_handler_get_life_span_handler(cef_client_t* self) {
  simple_handler_t* handler = (simple_handler_t*)self;
  if (handler->life_span_handler) {
    // Add reference before returning.
    handler->life_span_handler->handler.base.add_ref(
        &handler->life_span_handler->handler.base);
    return &handler->life_span_handler->handler;
  }
  return NULL;
}

cef_load_handler_t* CEF_CALLBACK
simple_handler_get_load_handler(cef_client_t* self) {
  simple_handler_t* handler = (simple_handler_t*)self;
  if (handler->load_handler) {
    // Add reference before returning.
    handler->load_handler->handler.base.add_ref(
        &handler->load_handler->handler.base);
    return &handler->load_handler->handler;
  }
  return NULL;
}

//
// Public API implementation.
//

simple_handler_t* simple_handler_create(int is_alloy_style) {
  simple_handler_t* handler =
      (simple_handler_t*)calloc(1, sizeof(simple_handler_t));
  if (!handler) {
    return NULL;
  }

  // Initialize base structure.
  handler->client.base.size = sizeof(cef_client_t);
  handler->client.base.add_ref = simple_handler_add_ref;
  handler->client.base.release = simple_handler_release;
  handler->client.base.has_one_ref = simple_handler_has_one_ref;
  handler->client.base.has_at_least_one_ref =
      simple_handler_has_at_least_one_ref;

  // Set callbacks.
  handler->client.get_display_handler = simple_handler_get_display_handler;
  handler->client.get_life_span_handler = simple_handler_get_life_span_handler;
  handler->client.get_load_handler = simple_handler_get_load_handler;

  // Create sub-handlers.
  handler->display_handler = display_handler_create(handler);
  handler->life_span_handler = life_span_handler_create(handler);
  handler->load_handler = load_handler_create(handler);

  // Initialize other fields.
  handler->is_alloy_style = is_alloy_style;
  handler->browser_list = NULL;
  handler->browser_count = 0;
  handler->browser_capacity = 0;
  handler->is_closing = 0;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  // Set global instance.
  if (!g_instance) {
    g_instance = handler;
  }

  return handler;
}

simple_handler_t* simple_handler_get_instance(void) {
  return g_instance;
}

// Task for closing browsers on the UI thread.
typedef struct _close_browsers_task_t {
  cef_task_t task;
  atomic_int ref_count;
  simple_handler_t* handler;
  int force_close;
} close_browsers_task_t;

IMPLEMENT_ADDREF(close_browsers_task_t, close_browsers_task, ref_count)
IMPLEMENT_HAS_ONE_REF(close_browsers_task_t, close_browsers_task, ref_count)
IMPLEMENT_HAS_AT_LEAST_ONE_REF(close_browsers_task_t,
                               close_browsers_task,
                               ref_count)

int CEF_CALLBACK close_browsers_task_release(cef_base_ref_counted_t* self) {
  close_browsers_task_t* task = (close_browsers_task_t*)self;
  int count = atomic_fetch_sub(&task->ref_count, 1) - 1;
  if (count == 0) {
    // Don't release handler reference - we don't own it.
    free(task);
    return 1;
  }
  return 0;
}

void CEF_CALLBACK close_browsers_task_execute(cef_task_t* self) {
  close_browsers_task_t* task = (close_browsers_task_t*)self;

  if (task->handler->browser_count == 0) {
    return;
  }

  // Close all browsers.
  for (size_t i = 0; i < task->handler->browser_count; ++i) {
    cef_browser_t* browser = task->handler->browser_list[i];
    cef_browser_host_t* host = browser->get_host(browser);
    if (host) {
      host->close_browser(host, task->force_close);
      host->base.release(&host->base);
    }
  }
}

void simple_handler_close_all_browsers(simple_handler_t* handler,
                                       int force_close) {
  if (!handler) {
    return;
  }

  if (!cef_currently_on(TID_UI)) {
    // Execute on the UI thread.
    close_browsers_task_t* task =
        (close_browsers_task_t*)calloc(1, sizeof(close_browsers_task_t));
    if (!task) {
      return;
    }

    task->task.base.size = sizeof(cef_task_t);
    task->task.base.add_ref = close_browsers_task_add_ref;
    task->task.base.release = close_browsers_task_release;
    task->task.base.has_one_ref = close_browsers_task_has_one_ref;
    task->task.base.has_at_least_one_ref =
        close_browsers_task_has_at_least_one_ref;
    task->task.execute = close_browsers_task_execute;
    task->handler = handler;
    task->force_close = force_close;
    atomic_store(&task->ref_count, 1);

    cef_post_task(TID_UI, &task->task);
    return;
  }

  // Already on UI thread, execute directly.
  if (handler->browser_count == 0) {
    return;
  }

  for (size_t i = 0; i < handler->browser_count; ++i) {
    cef_browser_t* browser = handler->browser_list[i];
    cef_browser_host_t* host = browser->get_host(browser);
    if (host) {
      host->close_browser(host, force_close);
      host->base.release(&host->base);
    }
  }
}

void simple_handler_show_main_window(simple_handler_t* handler) {
  if (!handler || handler->browser_count == 0) {
    return;
  }

  cef_browser_t* main_browser = handler->browser_list[0];
  simple_handler_platform_show_window(handler, main_browser);
}

// Default platform implementations (can be overridden in platform-specific
// files).
#if !defined(OS_MAC)
void simple_handler_platform_show_window(simple_handler_t* handler,
                                         cef_browser_t* browser) {
  // Not implemented on this platform.
}
#endif
