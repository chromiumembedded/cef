// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/native/menu_runner_mac.h"

#include "libcef/browser/browser_host_impl.h"

#include "base/message_loop/message_loop.h"
#include "base/compiler_specific.h"
#import "base/mac/scoped_sending_event.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/gfx/geometry/point.h"

CefMenuRunnerMac::CefMenuRunnerMac() {
}

CefMenuRunnerMac::~CefMenuRunnerMac() {
}

bool CefMenuRunnerMac::RunContextMenu(
    CefBrowserHostImpl* browser,
    CefMenuModelImpl* model,
    const content::ContextMenuParams& params) {
  // Create a menu controller based on the model.
  menu_controller_.reset([[MenuController alloc] initWithModel:model->model()
                                        useWithPopUpButtonCell:NO]);

  // Keep the menu controller alive (by adding an additional retain) until after
  // the menu has been dismissed. Otherwise it will crash if the browser is
  // destroyed (and consequently the menu controller is destroyed) while the
  // menu is still pending.
  base::scoped_nsobject<MenuController> menu_controller_ref(menu_controller_);

  // Make sure events can be pumped while the menu is up.
  base::MessageLoop::ScopedNestableTaskAllower allow(
      base::MessageLoop::current());

  // One of the events that could be pumped is |window.close()|.
  // User-initiated event-tracking loops protect against this by
  // setting flags in -[CrApplication sendEvent:], but since
  // web-content menus are initiated by IPC message the setup has to
  // be done manually.
  base::mac::ScopedSendingEvent sendingEventScoper;

  // Show the menu. Blocks until the menu is dismissed.
  if (browser->IsWindowless()) {
    // Don't show the menu unless a native window handle exists.
    if (!browser->GetWindowHandle())
      return false;

    const gfx::Point& screen_point =
        browser->GetScreenPoint(gfx::Point(params.x, params.y));
    NSPoint screen_position = NSPointFromCGPoint(screen_point.ToCGPoint());
    [[menu_controller_ menu] popUpMenuPositioningItem:nil
                                           atLocation:screen_position
                                               inView:nil];
  } else {
    NSView* parent_view = browser->web_contents()->GetContentNativeView();

    // Synthesize an event for the click, as there is no certainty that
    // [NSApp currentEvent] will return a valid event.
    NSEvent* currentEvent = [NSApp currentEvent];
    NSWindow* window = [parent_view window];

    NSPoint position = [window mouseLocationOutsideOfEventStream];

    NSTimeInterval eventTime = [currentEvent timestamp];
    NSEvent* clickEvent = [NSEvent mouseEventWithType:NSRightMouseDown
                                             location:position
                                        modifierFlags:NSRightMouseDownMask
                                            timestamp:eventTime
                                         windowNumber:[window windowNumber]
                                              context:nil
                                          eventNumber:0
                                           clickCount:1
                                             pressure:1.0];

    [NSMenu popUpContextMenu:[menu_controller_ menu]
                   withEvent:clickEvent
                     forView:parent_view];
  }

  return true;
}

void CefMenuRunnerMac::CancelContextMenu() {
  if (menu_controller_.get())
    [menu_controller_ cancel];
}

