// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_VIEWS_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_VIEWS_H_

#include <stdatomic.h>

#include "include/capi/views/cef_browser_view_capi.h"
#include "include/capi/views/cef_window_capi.h"

// Forward declaration.
typedef struct _simple_window_delegate_t simple_window_delegate_t;

//
// Browser view delegate structure.
// Implements cef_browser_view_delegate_t callbacks.
//
typedef struct _simple_browser_view_delegate_t {
  cef_browser_view_delegate_t delegate;

  // Reference count for this object.
  atomic_int ref_count;

  // Runtime style for the browser.
  cef_runtime_style_t runtime_style;
} simple_browser_view_delegate_t;

//
// Window delegate structure.
// Implements cef_window_delegate_t callbacks.
//
typedef struct _simple_window_delegate_t {
  cef_window_delegate_t delegate;

  // Reference count for this object.
  atomic_int ref_count;

  // Browser view to add to the window (owned by this structure).
  // We keep CEF's reference from the creation call.
  cef_browser_view_t* browser_view;

  // Runtime style for the window.
  cef_runtime_style_t runtime_style;

  // Initial show state for the window.
  cef_show_state_t initial_show_state;
} simple_window_delegate_t;

//
// Creates a browser view delegate.
// Caller is responsible for releasing the reference when done.
//
simple_browser_view_delegate_t* browser_view_delegate_create(
    cef_runtime_style_t runtime_style);

//
// Creates a window delegate.
// Takes ownership of the browser_view reference.
// Caller is responsible for releasing the reference when done.
//
simple_window_delegate_t* window_delegate_create(
    cef_browser_view_t* browser_view,
    cef_runtime_style_t runtime_style,
    cef_show_state_t initial_show_state);

#endif  // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_VIEWS_H_
