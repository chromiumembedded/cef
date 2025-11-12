// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_handler.h"

#if defined(CEF_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <string.h>
#endif

#include "include/capi/cef_browser_capi.h"

void simple_handler_platform_title_change(simple_handler_t* handler,
                                          cef_browser_t* browser,
                                          const cef_string_t* title) {
#if defined(CEF_X11)
  // Convert the title to UTF-8.
  cef_string_utf8_t title_utf8 = {};
  cef_string_to_utf8(title->str, title->length, &title_utf8);

  // Retrieve the X11 display shared with Chromium.
  Display* display = cef_get_xdisplay();
  if (!display) {
    cef_string_utf8_clear(&title_utf8);
    return;
  }

  // Retrieve the X11 window handle for the browser.
  cef_browser_host_t* host = browser->get_host(browser);
  if (!host) {
    cef_string_utf8_clear(&title_utf8);
    return;
  }

  cef_window_handle_t window = host->get_window_handle(host);
  if (window == 0) {
    host->base.release(&host->base);
    cef_string_utf8_clear(&title_utf8);
    return;
  }

  // Retrieve the atoms required by the below XChangeProperty call.
  const char* kAtoms[] = {"_NET_WM_NAME", "UTF8_STRING"};
  Atom atoms[2];
  int result = XInternAtoms(display, (char**)kAtoms, 2, False, atoms);
  if (result) {
    // Set the window title.
    XChangeProperty(display, window, atoms[0], atoms[1], 8, PropModeReplace,
                    (const unsigned char*)title_utf8.str, title_utf8.length);

    // Also set via XStoreName as fallback.
    XStoreName(display, window, title_utf8.str);
  }

  host->base.release(&host->base);
  cef_string_utf8_clear(&title_utf8);
#endif  // defined(CEF_X11)
}
