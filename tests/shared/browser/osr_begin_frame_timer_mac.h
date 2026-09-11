// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_SHARED_BROWSER_OSR_BEGIN_FRAME_TIMER_MAC_H_
#define CEF_TESTS_SHARED_BROWSER_OSR_BEGIN_FRAME_TIMER_MAC_H_
#pragma once

#import <Foundation/Foundation.h>

#include <functional>

namespace client {

// Drives external begin frames, including during Cocoa event tracking (live
// resize and menus). All methods must be called on the main thread. The owner
// must Stop before the browser closes; destruction also stops the timer.
class OsrBeginFrameTimerMac {
 public:
  OsrBeginFrameTimerMac() = default;
  ~OsrBeginFrameTimerMac();

  OsrBeginFrameTimerMac(const OsrBeginFrameTimerMac&) = delete;
  OsrBeginFrameTimerMac& operator=(const OsrBeginFrameTimerMac&) = delete;

  // Replaces any running timer. The callback is first called after one
  // interval.
  void Start(int frame_rate, std::function<void()> callback);
  void Stop();

 private:
  NSTimer* timer_ = nil;
};

}  // namespace client

#endif  // CEF_TESTS_SHARED_BROWSER_OSR_BEGIN_FRAME_TIMER_MAC_H_
