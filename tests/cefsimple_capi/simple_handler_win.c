// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_handler.h"

#include <windows.h>

#include "include/capi/cef_browser_capi.h"

void simple_handler_platform_title_change(simple_handler_t* handler,
                                          cef_browser_t* browser,
                                          const cef_string_t* title) {
  cef_browser_host_t* host = browser->get_host(browser);
  if (!host) {
    return;
  }

  cef_window_handle_t hwnd = host->get_window_handle(host);
  if (hwnd) {
    // CEF uses UTF-16 strings, which Windows SetWindowTextW expects.
    SetWindowTextW(hwnd, title->str);
  }

  host->base.release(&host->base);
}
