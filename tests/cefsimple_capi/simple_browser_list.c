// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_browser_list.h"

#include <stdlib.h>
#include <string.h>

#include "tests/cefsimple_capi/simple_utils.h"

void browser_list_init(browser_list_t* list) {
  CHECK(list);
  list->browsers = NULL;
  list->count = 0;
  list->capacity = 0;
}

int browser_list_add(browser_list_t* list, cef_browser_t* browser) {
  CHECK(list);
  CHECK(browser);

  // Ensure capacity.
  if (list->count >= list->capacity) {
    size_t new_capacity = list->capacity == 0 ? 4 : list->capacity * 2;
    cef_browser_t** new_list = (cef_browser_t**)realloc(
        list->browsers, new_capacity * sizeof(cef_browser_t*));
    if (!new_list) {
      return 0;  // Allocation failed.
    }
    list->browsers = new_list;
    list->capacity = new_capacity;
  }

  // Add a reference for the list.
  // The list now owns one reference to this browser.
  browser->base.add_ref(&browser->base);

  // Store the browser pointer.
  list->browsers[list->count++] = browser;
  return 1;
}

void browser_list_remove(browser_list_t* list, cef_browser_t* browser) {
  CHECK(list);
  CHECK(browser);

  // Find and remove the browser.
  for (size_t i = 0; i < list->count; ++i) {
    // Add a reference before calling is_same, since CEF functions take
    // ownership of parameters passed to them.
    browser->base.add_ref(&browser->base);

    if (list->browsers[i]->is_same(list->browsers[i], browser)) {
      // Release the list's reference to this browser.
      list->browsers[i]->base.release(&list->browsers[i]->base);

      // Shift remaining elements.
      for (size_t j = i; j < list->count - 1; ++j) {
        list->browsers[j] = list->browsers[j + 1];
      }
      list->count--;
      break;
    }
  }
}

void browser_list_destroy(browser_list_t* list) {
  CHECK(list);

  // Release all browsers in the list.
  for (size_t i = 0; i < list->count; ++i) {
    list->browsers[i]->base.release(&list->browsers[i]->base);
  }

  // Free the array.
  free(list->browsers);

  // Reset to empty state.
  list->browsers = NULL;
  list->count = 0;
  list->capacity = 0;
}

size_t browser_list_count(const browser_list_t* list) {
  CHECK(list);
  return list->count;
}

cef_browser_t* browser_list_get(const browser_list_t* list, size_t index) {
  CHECK(list);
  if (index >= list->count) {
    return NULL;
  }
  return list->browsers[index];
}
