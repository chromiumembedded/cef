// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_app.h"
#import "include/cef_application_mac.h"

#if !__has_feature(objc_arc)
// Memory AutoRelease pool.
static NSAutoreleasePool* g_autopool = nil;
#endif

// Provide the CefAppProtocol implementation required by CEF.
@interface TestApplication : NSApplication <CefAppProtocol> {
 @private
  BOOL handlingSendEvent_;
}
@end

@implementation TestApplication
- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  CefScopedSendingEvent sendingEventScoper;
  [super sendEvent:event];
}
@end

void PlatformInit() {
#if !__has_feature(objc_arc)
  // Initialize the AutoRelease pool.
  g_autopool = [[NSAutoreleasePool alloc] init];
#endif

  // Initialize the TestApplication instance.
  [TestApplication sharedApplication];
}

void PlatformCleanup() {
#if !__has_feature(objc_arc)
  // Release the AutoRelease pool.
  [g_autopool release];
#endif
}
