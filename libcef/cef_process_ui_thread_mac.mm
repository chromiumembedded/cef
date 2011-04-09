// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cef.h"
#include "cef_process_ui_thread.h"
#include "browser_webkit_glue.h"
#include "base/message_pump_mac.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/mac/WebCoreSupport/WebSystemInterface.h"

// CrAppProtocol implementation.
@interface CrApplication : NSApplication<CrAppProtocol> {
 @private
  BOOL handlingSendEvent_;
}
- (BOOL)isHandlingSendEvent;
@end

@implementation CrApplication
- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)sendEvent:(NSEvent*)event {
  BOOL wasHandlingSendEvent = handlingSendEvent_;
  handlingSendEvent_ = YES;
  [super sendEvent:event];
  handlingSendEvent_ = wasHandlingSendEvent;
}
@end

namespace {

// Memory autorelease pool.
static NSAutoreleasePool* g_autopool = nil;

void RunLoopObserver(CFRunLoopObserverRef observer, CFRunLoopActivity activity,
                     void* info)
{
  CefDoMessageLoopWork();
}
  
} // namespace

void CefProcessUIThread::PlatformInit() {
  // Initialize the CrApplication instance.
  [CrApplication sharedApplication];
  g_autopool = [[NSAutoreleasePool alloc] init];

  InitWebCoreSystemInterface();

  // Register the run loop observer.
  CFRunLoopObserverRef observer =
      CFRunLoopObserverCreate(NULL,
                              kCFRunLoopBeforeWaiting,
                              YES, /* repeat */
                              0,
                              &RunLoopObserver,
                              NULL);
  if (observer) {
    CFRunLoopAddObserver(CFRunLoopGetCurrent(), observer,
                         kCFRunLoopCommonModes);
  }

  webkit_glue::InitializeDataPak();
  
  // On Mac, the select popup menus are rendered by the browser.
  WebKit::WebView::setUseExternalPopupMenus(true);
}

void CefProcessUIThread::PlatformCleanUp() {
  [g_autopool release];
}

