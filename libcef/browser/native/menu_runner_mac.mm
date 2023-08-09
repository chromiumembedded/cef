// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/native/menu_runner_mac.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"

#import "base/mac/scoped_sending_event.h"
#include "base/task/current_thread.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/gfx/geometry/point.h"

CefMenuRunnerMac::CefMenuRunnerMac() {}

CefMenuRunnerMac::~CefMenuRunnerMac() {}

bool CefMenuRunnerMac::RunContextMenu(
    AlloyBrowserHostImpl* browser,
    CefMenuModelImpl* model,
    const content::ContextMenuParams& params) {
  // Create a menu controller based on the model.
  MenuControllerCocoa* menu_controller =
      [[MenuControllerCocoa alloc] initWithModel:model->model()
                                        delegate:nil
                          useWithPopUpButtonCell:NO];

  menu_controller_ = menu_controller;

  // Make sure events can be pumped while the menu is up.
  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;

  // One of the events that could be pumped is |window.close()|.
  // User-initiated event-tracking loops protect against this by
  // setting flags in -[CrApplication sendEvent:], but since
  // web-content menus are initiated by IPC message the setup has to
  // be done manually.
  base::mac::ScopedSendingEvent sendingEventScoper;

  // Show the menu. Blocks until the menu is dismissed.
  if (browser->IsWindowless()) {
    // Don't show the menu unless a native window handle exists.
    if (!browser->GetWindowHandle()) {
      return false;
    }

    const gfx::Point& screen_point = browser->GetScreenPoint(
        gfx::Point(params.x, params.y), /*want_dip_coords=*/true);
    NSPoint screen_position = NSPointFromCGPoint(screen_point.ToCGPoint());
    [[menu_controller_ menu] popUpMenuPositioningItem:nil
                                           atLocation:screen_position
                                               inView:nil];
  } else {
    NSView* parent_view =
        browser->web_contents()->GetContentNativeView().GetNativeNSView();

    // Synthesize an event for the click, as there is no certainty that
    // [NSApp currentEvent] will return a valid event.
    NSEvent* currentEvent = [NSApp currentEvent];
    NSWindow* window = [parent_view window];

    NSPoint position = [window mouseLocationOutsideOfEventStream];

    NSTimeInterval eventTime = [currentEvent timestamp];
    NSEvent* clickEvent = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                                             location:position
                                        modifierFlags:0
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
  if (menu_controller_) {
    menu_controller_ = nil;
  }
}
