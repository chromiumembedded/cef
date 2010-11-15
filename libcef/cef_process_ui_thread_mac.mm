// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cef.h"
#include "cef_process_ui_thread.h"
#include "browser_webkit_glue.h"
#include "base/chrome_application_mac.h"
#include "third_party/WebKit/WebKit/mac/WebCoreSupport/WebSystemInterface.h"

namespace {
  
// Memory autorelease pool.
NSAutoreleasePool* g_autopool;

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
                              kCFRunLoopBeforeTimers | kCFRunLoopBeforeWaiting,
                              YES, /* repeat */
                              0,
                              &RunLoopObserver,
                              NULL);
  if (observer) {
    CFRunLoopAddObserver(CFRunLoopGetCurrent(), observer,
                         kCFRunLoopCommonModes);
  }

  webkit_glue::InitializeDataPak();
}

void CefProcessUIThread::PlatformCleanUp() {
  [g_autopool release];
}

