// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefsimple_capi/simple_handler.h"

#import <Cocoa/Cocoa.h>

#include "include/capi/cef_browser_capi.h"
#include "include/internal/cef_types_mac.h"

static NSWindow* GetNSWindowForBrowser(cef_browser_t* browser) {
  cef_browser_host_t* host = browser->get_host(browser);
  if (!host) {
    return nil;
  }

  cef_window_handle_t handle = host->get_window_handle(host);
  host->base.release(&host->base);

  if (!handle) {
    return nil;
  }

  NSView* view = CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(handle);
  return [view window];
}

void simple_handler_platform_title_change(simple_handler_t* handler,
                                          cef_browser_t* browser,
                                          const cef_string_t* title) {
  NSWindow* window = GetNSWindowForBrowser(browser);
  if (!window) {
    return;
  }

  // Convert CEF string to UTF-8.
  cef_string_utf8_t title_utf8 = {};
  cef_string_to_utf8(title->str, title->length, &title_utf8);

  NSString* str = [NSString stringWithUTF8String:title_utf8.str];
  [window setTitle:str];

  cef_string_utf8_clear(&title_utf8);
}

void simple_handler_platform_show_window(simple_handler_t* handler,
                                         cef_browser_t* browser) {
  NSWindow* window = GetNSWindowForBrowser(browser);
  if (!window) {
    return;
  }

  [window makeKeyAndOrderFront:window];
}
