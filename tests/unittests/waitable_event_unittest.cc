// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "include/base/cef_bind.h"
#include "include/cef_thread.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/gtest/include/gtest/gtest.h"

// Test manual reset.
TEST(WaitableEventTest, ManualReset) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(false, false);

  EXPECT_FALSE(event->IsSignaled());

  event->Signal();
  EXPECT_TRUE(event->IsSignaled());
  EXPECT_TRUE(event->IsSignaled());

  event->Reset();
  EXPECT_FALSE(event->IsSignaled());
  EXPECT_FALSE(event->TimedWait(10));

  event->Signal();
  event->Wait();
  EXPECT_TRUE(event->TimedWait(10));
}

// Test automatic reset.
TEST(WaitableEventTest, AutomaticReset) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  EXPECT_FALSE(event->IsSignaled());

  event->Signal();
  EXPECT_TRUE(event->IsSignaled());
  EXPECT_FALSE(event->IsSignaled());

  event->Reset();
  EXPECT_FALSE(event->IsSignaled());
  EXPECT_FALSE(event->TimedWait(10));

  event->Signal();
  event->Wait();
  EXPECT_FALSE(event->TimedWait(10));

  event->Signal();
  EXPECT_TRUE(event->TimedWait(10));
}

namespace {

void SignalEvent(CefWaitableEvent* event) {
  event->Signal();
}

}  // namespace

// Tests that a WaitableEvent can be safely deleted when |Wait| is done without
// additional synchronization.
TEST(WaitableEventTest, WaitAndDelete) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  CefRefPtr<CefThread> thread = CefThread::CreateThread("waitable_event_test");
  thread->GetTaskRunner()->PostDelayedTask(
      CefCreateClosureTask(base::Bind(SignalEvent,
                                      base::Unretained(event.get()))), 10);

  event->Wait();
  event = nullptr;

  thread->Stop();
  thread = nullptr;
}
