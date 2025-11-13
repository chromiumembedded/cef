// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_app.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_command_line_capi.h"
#include "include/capi/views/cef_browser_view_capi.h"
#include "include/capi/views/cef_window_capi.h"
#include "tests/cefsimple_capi/ref_counted.h"
#include "tests/cefsimple_capi/simple_handler.h"
#include "tests/cefsimple_capi/simple_utils.h"
#include "tests/cefsimple_capi/simple_views.h"

// Implement reference counting functions for simple_app_t.
IMPLEMENT_REFCOUNTING_MANUAL(simple_app_t, simple_app, ref_count)

// Release function for simple_app_t with custom cleanup logic.
int CEF_CALLBACK simple_app_release(cef_base_ref_counted_t* self) {
  simple_app_t* app = (simple_app_t*)self;
  int count = atomic_fetch_sub(&app->ref_count, 1) - 1;
  if (count == 0) {
    // Release the browser process handler if we own one.
    if (app->browser_process_handler) {
      app->browser_process_handler->handler.base.release(
          &app->browser_process_handler->handler.base);
    }
    free(app);
    return 1;
  }
  return 0;
}

// Returns the browser process handler.
// Adds a reference before returning (CEF will release it when done).
cef_browser_process_handler_t* CEF_CALLBACK
simple_app_get_browser_process_handler(cef_app_t* self) {
  simple_app_t* app = (simple_app_t*)self;
  if (app->browser_process_handler) {
    // Add reference for CEF (it will release when done).
    app->browser_process_handler->handler.base.add_ref(
        &app->browser_process_handler->handler.base);
    return &app->browser_process_handler->handler;
  }
  return NULL;
}

// Forward declarations for browser process handler functions.
void CEF_CALLBACK browser_process_handler_on_context_initialized(
    cef_browser_process_handler_t* self);
cef_client_t* CEF_CALLBACK
browser_process_handler_get_default_client(cef_browser_process_handler_t* self);

// Implement reference counting functions for browser process handler.
IMPLEMENT_REFCOUNTING_SIMPLE(simple_browser_process_handler_t,
                             browser_process_handler,
                             ref_count)

// Called after CEF initialization to create the browser.
void CEF_CALLBACK browser_process_handler_on_context_initialized(
    cef_browser_process_handler_t* self) {
  // Get the global command line.
  cef_command_line_t* command_line = cef_command_line_get_global();
  CHECK(command_line);

  // Check if Alloy style will be used.
  cef_string_t alloy_switch = {};
  cef_string_from_ascii("use-alloy-style", 15, &alloy_switch);
  int use_alloy_style = command_line->has_switch(command_line, &alloy_switch);
  cef_string_clear(&alloy_switch);

  // Create the client handler.
  simple_handler_t* client_handler = simple_handler_create(use_alloy_style);
  CHECK(client_handler);

  // The client_handler is stored globally via simple_handler_get_instance().
  // GetDefaultClient() will retrieve it from there when needed.

  // Specify CEF browser settings.
  cef_browser_settings_t browser_settings = {};
  browser_settings.size = sizeof(cef_browser_settings_t);

  // Get the URL from command line or use default.
  cef_string_t url_switch = {};
  cef_string_from_ascii("url", 3, &url_switch);
  cef_string_userfree_t url_value =
      command_line->get_switch_value(command_line, &url_switch);
  cef_string_clear(&url_switch);

  cef_string_t url = {};
  if (url_value && url_value->length > 0) {
    cef_string_copy(url_value->str, url_value->length, &url);
  } else {
    cef_string_from_ascii("https://www.google.com", 22, &url);
  }
  if (url_value) {
    cef_string_userfree_free(url_value);
  }

  // Check if Views framework should be used.
  // Views is enabled by default (add `--use-native` to disable).
  cef_string_t native_switch = {};
  cef_string_from_ascii("use-native", 10, &native_switch);
  int use_native = command_line->has_switch(command_line, &native_switch);
  cef_string_clear(&native_switch);

  int use_views = !use_native;

  // Determine runtime style.
  cef_runtime_style_t runtime_style = CEF_RUNTIME_STYLE_DEFAULT;
  if (use_alloy_style) {
    runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  }

  if (use_views) {
    // Create the BrowserView using Views framework.
    simple_browser_view_delegate_t* browser_view_delegate =
        browser_view_delegate_create(runtime_style);
    CHECK(browser_view_delegate);

    // Create the browser view.
    // We transfer our client_handler and browser_view_delegate references to
    // CEF. CEF will release them when the browser view is destroyed.
    cef_browser_view_t* browser_view = cef_browser_view_create(
        &client_handler->client, &url, &browser_settings, NULL, NULL,
        &browser_view_delegate->delegate);

    // Note: We DON'T release browser_view_delegate here - we transferred
    // ownership to CEF.

    if (browser_view) {
      // Optionally configure the initial show state.
      cef_show_state_t initial_show_state = CEF_SHOW_STATE_NORMAL;
      cef_string_t show_state_switch = {};
      cef_string_from_ascii("initial-show-state", 18, &show_state_switch);
      cef_string_userfree_t show_state_value =
          command_line->get_switch_value(command_line, &show_state_switch);
      cef_string_clear(&show_state_switch);

      if (show_state_value && show_state_value->length > 0) {
        // Check for "minimized"
        cef_string_t minimized = {};
        cef_string_from_ascii("minimized", 9, &minimized);
        if (cef_string_utf16_cmp(show_state_value, &minimized) == 0) {
          initial_show_state = CEF_SHOW_STATE_MINIMIZED;
        }
        cef_string_clear(&minimized);

        // Check for "maximized"
        cef_string_t maximized = {};
        cef_string_from_ascii("maximized", 9, &maximized);
        if (cef_string_utf16_cmp(show_state_value, &maximized) == 0) {
          initial_show_state = CEF_SHOW_STATE_MAXIMIZED;
        }
        cef_string_clear(&maximized);

#if defined(OS_MAC)
        // Hidden show state is only supported on macOS.
        cef_string_t hidden = {};
        cef_string_from_ascii("hidden", 6, &hidden);
        if (cef_string_utf16_cmp(show_state_value, &hidden) == 0) {
          initial_show_state = CEF_SHOW_STATE_HIDDEN;
        }
        cef_string_clear(&hidden);
#endif
      }

      if (show_state_value) {
        cef_string_userfree_free(show_state_value);
      }

      // Create the Window. It will show itself after creation.
      // We transfer our browser_view reference to the window delegate.
      simple_window_delegate_t* window_delegate = window_delegate_create(
          browser_view, runtime_style, initial_show_state);
      CHECK(window_delegate);

      // Create the window.
      // We transfer our window_delegate reference to CEF.
      cef_window_create_top_level(&window_delegate->delegate);
    }
  } else {
    // Information used when creating the native window.
    cef_window_info_t window_info = {};
    window_info.size = sizeof(cef_window_info_t);

#if defined(OS_WIN)
    // On Windows we need to specify certain flags that will be passed to
    // CreateWindowEx().
    cef_string_t window_name = {};
    cef_string_from_ascii("cefsimple_capi", 14, &window_name);

    window_info.style =
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE;
    window_info.bounds.x = CW_USEDEFAULT;
    window_info.bounds.y = CW_USEDEFAULT;
    window_info.bounds.width = CW_USEDEFAULT;
    window_info.bounds.height = CW_USEDEFAULT;
    window_info.window_name = window_name;
#elif defined(OS_LINUX)
    window_info.bounds.width = 800;
    window_info.bounds.height = 600;
#endif

    // Use the runtime style determined earlier.
    window_info.runtime_style = runtime_style;

    // Create the browser window.
    // We pass our creation reference to CEF - don't release it!
    // CEF takes ownership of this reference and will release it when the
    // browser closes.
    cef_browser_host_create_browser(&window_info, &client_handler->client, &url,
                                    &browser_settings, NULL, NULL);

#if defined(OS_WIN)
    cef_string_clear(&window_name);
#endif
  }

  cef_string_clear(&url);
  // Note: We DON'T release the client_handler here.
  // We transferred our creation reference to CEF via
  // cef_browser_host_create_browser. CEF will release it when the browser
  // closes.
}

// Returns the default client handler for Chrome style UI.
cef_client_t* CEF_CALLBACK browser_process_handler_get_default_client(
    cef_browser_process_handler_t* self) {
  // Return the global instance (matches C++ SimpleApp::GetDefaultClient).
  simple_handler_t* instance = simple_handler_get_instance();
  if (instance) {
    // Add reference before returning (CEF will release it).
    instance->client.base.add_ref(&instance->client.base);
    return &instance->client;
  }

  return NULL;
}

// Creates a browser process handler instance.
simple_browser_process_handler_t* browser_process_handler_create(void) {
  simple_browser_process_handler_t* handler =
      (simple_browser_process_handler_t*)calloc(
          1, sizeof(simple_browser_process_handler_t));
  CHECK(handler);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&handler->handler.base,
                           cef_browser_process_handler_t,
                           browser_process_handler);

  // Set callbacks.
  handler->handler.on_context_initialized =
      browser_process_handler_on_context_initialized;
  handler->handler.get_default_client =
      browser_process_handler_get_default_client;

  // Initialize with ref count of 1.
  atomic_store(&handler->ref_count, 1);

  return handler;
}

// Creates the application instance.
simple_app_t* simple_app_create(void) {
  simple_app_t* app = (simple_app_t*)calloc(1, sizeof(simple_app_t));
  CHECK(app);

  // Initialize base structure.
  INIT_CEF_BASE_REFCOUNTED(&app->app.base, cef_app_t, simple_app);

  // Set callbacks.
  app->app.get_browser_process_handler = simple_app_get_browser_process_handler;

  // Create the browser process handler.
  app->browser_process_handler = browser_process_handler_create();
  CHECK(app->browser_process_handler);

  // Initialize with ref count of 1.
  atomic_store(&app->ref_count, 1);

  return app;
}
