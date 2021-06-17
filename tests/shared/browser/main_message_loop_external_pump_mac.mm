// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/main_message_loop_external_pump.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <memory>

#include "include/cef_app.h"

@class EventHandler;

namespace client {

class MainMessageLoopExternalPumpMac : public MainMessageLoopExternalPump {
 public:
  MainMessageLoopExternalPumpMac();
  ~MainMessageLoopExternalPumpMac();

  // MainMessageLoopStd methods:
  void Quit() override;
  int Run() override;

  // MainMessageLoopExternalPump methods:
  void OnScheduleMessagePumpWork(int64 delay_ms) override;

  // Internal methods used for processing the event callbacks. They are public
  // for simplicity but should not be used directly.
  void HandleScheduleWork(int64 delay_ms);
  void HandleTimerTimeout();

 protected:
  // MainMessageLoopExternalPump methods:
  void SetTimer(int64 delay_ms) override;
  void KillTimer() override;
  bool IsTimerPending() override { return timer_ != nil; }

 private:
  // Owner thread that will run events.
  NSThread* owner_thread_;

  // Pending work timer.
  NSTimer* timer_;

  // Used to handle event callbacks on the owner thread.
  EventHandler* event_handler_;
};

}  // namespace client

// Object that handles event callbacks on the owner thread.
@interface EventHandler : NSObject {
 @private
  client::MainMessageLoopExternalPumpMac* pump_;
}

- (id)initWithPump:(client::MainMessageLoopExternalPumpMac*)pump;
- (void)scheduleWork:(NSNumber*)delay_ms;
- (void)timerTimeout:(id)obj;
@end

@implementation EventHandler

- (id)initWithPump:(client::MainMessageLoopExternalPumpMac*)pump {
  if (self = [super init]) {
    pump_ = pump;
  }
  return self;
}

- (void)scheduleWork:(NSNumber*)delay_ms {
  pump_->HandleScheduleWork([delay_ms integerValue]);
}

- (void)timerTimeout:(id)obj {
  pump_->HandleTimerTimeout();
}

@end

namespace client {

MainMessageLoopExternalPumpMac::MainMessageLoopExternalPumpMac()
    : owner_thread_([NSThread currentThread]), timer_(nil) {
#if !__has_feature(objc_arc)
  [owner_thread_ retain];
#endif  // !__has_feature(objc_arc)
  event_handler_ = [[EventHandler alloc] initWithPump:this];
}

MainMessageLoopExternalPumpMac::~MainMessageLoopExternalPumpMac() {
  KillTimer();
#if !__has_feature(objc_arc)
  [owner_thread_ release];
  [event_handler_ release];
#endif  // !__has_feature(objc_arc)
  owner_thread_ = nil;
  event_handler_ = nil;
}

void MainMessageLoopExternalPumpMac::Quit() {
  [NSApp stop:nil];
}

int MainMessageLoopExternalPumpMac::Run() {
  // Run the message loop.
  [NSApp run];

  KillTimer();

  // We need to run the message pump until it is idle. However we don't have
  // that information here so we run the message loop "for a while".
  for (int i = 0; i < 10; ++i) {
    // Let default runloop observers run.
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, 1);

    // Do some work.
    CefDoMessageLoopWork();

    // Sleep to allow the CEF proc to do work.
    [NSThread sleepForTimeInterval:0.05];
  }

  return 0;
}

void MainMessageLoopExternalPumpMac::OnScheduleMessagePumpWork(int64 delay_ms) {
  // This method may be called on any thread.
  NSNumber* number = [NSNumber numberWithInt:static_cast<int>(delay_ms)];
  [event_handler_ performSelector:@selector(scheduleWork:)
                         onThread:owner_thread_
                       withObject:number
                    waitUntilDone:NO];
}

void MainMessageLoopExternalPumpMac::HandleScheduleWork(int64 delay_ms) {
  OnScheduleWork(delay_ms);
}

void MainMessageLoopExternalPumpMac::HandleTimerTimeout() {
  OnTimerTimeout();
}

void MainMessageLoopExternalPumpMac::SetTimer(int64 delay_ms) {
  DCHECK_GT(delay_ms, 0);
  DCHECK(!timer_);

  const double delay_s = static_cast<double>(delay_ms) / 1000.0;
  timer_ = [NSTimer timerWithTimeInterval:delay_s
                                   target:event_handler_
                                 selector:@selector(timerTimeout:)
                                 userInfo:nil
                                  repeats:NO];
#if !__has_feature(objc_arc)
  [timer_ retain];
#endif  // !__has_feature(objc_arc)

  // Add the timer to default and tracking runloop modes.
  NSRunLoop* owner_runloop = [NSRunLoop currentRunLoop];
  [owner_runloop addTimer:timer_ forMode:NSRunLoopCommonModes];
  [owner_runloop addTimer:timer_ forMode:NSEventTrackingRunLoopMode];
}

void MainMessageLoopExternalPumpMac::KillTimer() {
  if (timer_ != nil) {
    [timer_ invalidate];
#if !__has_feature(objc_arc)
    [timer_ release];
#endif  // !__has_feature(objc_arc)
    timer_ = nil;
  }
}

// static
std::unique_ptr<MainMessageLoopExternalPump>
MainMessageLoopExternalPump::Create() {
  return std::make_unique<MainMessageLoopExternalPumpMac>();
}

}  // namespace client
