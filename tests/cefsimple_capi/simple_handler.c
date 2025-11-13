// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_handler.h"

#include <stdatomic.h>
#include <stdlib.h>

#include "include/capi/cef_task_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_browser_list.h"
#include "tests/cefsimple_capi/simple_utils.h"

// Global instance pointer.
static simple_handler_t* g_instance = NULL;

// Forward declarations for handler create functions.
simple_display_handler_t* display_handler_create(simple_handler_t* parent);
simple_life_span_handler_t* life_span_handler_create(simple_handler_t* parent);
simple_load_handler_t* load_handler_create(simple_handler_t* parent);

//
// Client handler implementation.
//

IMPLEMENT_REFCOUNTING_MANUAL(simple_handler_t, simple_handler, ref_count)

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

    // Destroy the browser list.
    browser_list_destroy(&handler->browser_list);

    // Clear global instance if this is it.
    if (g_instance == handler) {
      g_instance = NULL;
    }

    free(handler);
    return 1;
  }
  return 0;
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
  CHECK(handler);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&handler->client.base, cef_client_t, simple_handler);

  // Set callbacks.
  handler->client.get_display_handler = simple_handler_get_display_handler;
  handler->client.get_life_span_handler = simple_handler_get_life_span_handler;
  handler->client.get_load_handler = simple_handler_get_load_handler;

  // Create sub-handlers.
  handler->display_handler = display_handler_create(handler);
  CHECK(handler->display_handler);
  handler->life_span_handler = life_span_handler_create(handler);
  CHECK(handler->life_span_handler);
  handler->load_handler = load_handler_create(handler);
  CHECK(handler->load_handler);

  // Initialize other fields.
  handler->is_alloy_style = is_alloy_style;
  browser_list_init(&handler->browser_list);
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

IMPLEMENT_REFCOUNTING_MANUAL(close_browsers_task_t,
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

  size_t count = browser_list_count(&task->handler->browser_list);
  if (count == 0) {
    return;
  }

  // Close all browsers.
  for (size_t i = 0; i < count; ++i) {
    cef_browser_t* browser = browser_list_get(&task->handler->browser_list, i);
    cef_browser_host_t* host = browser->get_host(browser);
    if (host) {
      host->close_browser(host, task->force_close);
      host->base.release(&host->base);
    }
  }
}

void simple_handler_close_all_browsers(simple_handler_t* handler,
                                       int force_close) {
  CHECK(handler);

  if (!cef_currently_on(TID_UI)) {
    // Execute on the UI thread.
    close_browsers_task_t* task =
        (close_browsers_task_t*)calloc(1, sizeof(close_browsers_task_t));
    CHECK(task);

    INIT_CEF_BASE_REFCOUNTED(&task->task.base, cef_task_t, close_browsers_task);
    task->task.execute = close_browsers_task_execute;
    task->handler = handler;
    task->force_close = force_close;
    atomic_store(&task->ref_count, 1);

    cef_post_task(TID_UI, &task->task);
    return;
  }

  // Already on UI thread, execute directly.
  size_t count = browser_list_count(&handler->browser_list);
  if (count == 0) {
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    cef_browser_t* browser = browser_list_get(&handler->browser_list, i);
    cef_browser_host_t* host = browser->get_host(browser);
    if (host) {
      host->close_browser(host, force_close);
      host->base.release(&host->base);
    }
  }
}

void simple_handler_show_main_window(simple_handler_t* handler) {
  CHECK(handler);
  if (browser_list_count(&handler->browser_list) == 0) {
    return;
  }

  cef_browser_t* main_browser = browser_list_get(&handler->browser_list, 0);
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
