// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/window_test.h"

#import <Cocoa/Cocoa.h>

namespace window_test {

namespace {

NSWindow* GetWindow(CefWindowHandle handle) {
  NSView* view = (NSView*)handle;
  return [view window];
}

}  // namespace

void SetPos(CefWindowHandle handle, int x, int y, int width, int height) {
  NSWindow* window = GetWindow(handle);

  // Make sure the window isn't minimized or maximized.
  if ([window isMiniaturized])
    [window deminiaturize:nil];
  else if ([window isZoomed])
    [window performZoom:nil];

  // Retrieve information for the display that contains the window.
  NSScreen* screen = [window screen];
  if (screen == nil)
    screen = [NSScreen mainScreen];
  NSRect frame = [screen frame];
  NSRect visibleFrame = [screen visibleFrame];

  // Make sure the window is inside the display.
  CefRect display_rect(
      visibleFrame.origin.x,
      frame.size.height - visibleFrame.size.height - visibleFrame.origin.y,
      visibleFrame.size.width,
      visibleFrame.size.height);
  CefRect window_rect(x, y, width, height);
  ModifyBounds(display_rect, window_rect);

  NSRect newRect;
  newRect.origin.x = window_rect.x;
  newRect.origin.y = frame.size.height - window_rect.height - window_rect.y;
  newRect.size.width = window_rect.width;
  newRect.size.height = window_rect.height;
  [window setFrame:newRect display:YES];
}

void Minimize(CefWindowHandle handle) {
  [GetWindow(handle) performMiniaturize:nil];
}

void Maximize(CefWindowHandle handle) {
  [GetWindow(handle) performZoom:nil];
}

void Restore(CefWindowHandle handle) {
  NSWindow* window = GetWindow(handle);
  if ([window isMiniaturized])
    [window deminiaturize:nil];
  else if ([window isZoomed])
    [window performZoom:nil];
}

}  // namespace window_test
