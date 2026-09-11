// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/osr_begin_frame_timer_mac.h"

#include "include/base/cef_logging.h"

namespace client {

OsrBeginFrameTimerMac::~OsrBeginFrameTimerMac() {
  Stop();
}

void OsrBeginFrameTimerMac::Start(int frame_rate,
                                  std::function<void()> callback) {
  DCHECK([NSThread isMainThread]);
  CHECK_GT(frame_rate, 0);
  CHECK(callback);
  Stop();
  const NSTimeInterval interval = 1.0 / frame_rate;
  timer_ = [[NSTimer alloc]
      initWithFireDate:[NSDate dateWithTimeIntervalSinceNow:interval]
              interval:interval
               repeats:YES
                 block:^(NSTimer*) {
                   callback();
                 }];
  [[NSRunLoop mainRunLoop] addTimer:timer_ forMode:NSRunLoopCommonModes];
}

void OsrBeginFrameTimerMac::Stop() {
  DCHECK([NSThread isMainThread]);
  [timer_ invalidate];
#if !__has_feature(objc_arc)
  [timer_ release];
#endif
  timer_ = nil;
}

}  // namespace client
