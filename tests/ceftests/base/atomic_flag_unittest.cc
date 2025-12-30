// Copyright (c) 2025 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was ported from Chromium source file:
// base/synchronization/atomic_flag_unittest.cc
// Chromium commit: 70909488d8cc2
//
// ---------------------------------------------------------------------------

#include <chrono>
#include <thread>

#include "include/base/cef_atomic_flag.h"
#include "include/base/cef_bind.h"
#include "include/base/cef_callback.h"
#include "include/cef_task.h"
#include "include/cef_thread.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Busy waits (to explicitly avoid using synchronization constructs that would
// defeat the purpose of testing atomics) until |tested_flag| is set and then
// verifies that non-atomic |*expected_after_flag| is true and sets |*done_flag|
// before returning if it's non-null.
void BusyWaitUntilFlagIsSet(AtomicFlag* tested_flag,
                            bool* expected_after_flag,
                            AtomicFlag* done_flag) {
  while (!tested_flag->IsSet()) {
    std::this_thread::yield();
  }

  EXPECT_TRUE(*expected_after_flag);
  if (done_flag) {
    done_flag->Set();
  }
}

}  // namespace

TEST(AtomicFlagTest, SimpleSingleThreadedTest) {
  AtomicFlag flag;
  ASSERT_FALSE(flag.IsSet());
  flag.Set();
  ASSERT_TRUE(flag.IsSet());
}

TEST(AtomicFlagTest, DoubleSetTest) {
  AtomicFlag flag;
  ASSERT_FALSE(flag.IsSet());
  flag.Set();
  ASSERT_TRUE(flag.IsSet());
  flag.Set();
  ASSERT_TRUE(flag.IsSet());
}

TEST(AtomicFlagTest, ReadFromDifferentThread) {
  // |tested_flag| is the one being tested below.
  AtomicFlag tested_flag;
  // |expected_after_flag| is used to confirm that sequential consistency is
  // obtained around |tested_flag|.
  bool expected_after_flag = false;
  // |reset_flag| is used to confirm the test flows as intended without using
  // synchronization constructs which would defeat the purpose of exercising
  // atomics.
  AtomicFlag reset_flag;

  CefRefPtr<CefThread> thread =
      CefThread::CreateThread("AtomicFlagTest.ReadFromDifferentThread");
  ASSERT_TRUE(thread.get());
  ASSERT_TRUE(thread->GetTaskRunner().get());

  thread->GetTaskRunner()->PostTask(
      CefCreateClosureTask(BindOnce(&BusyWaitUntilFlagIsSet, &tested_flag,
                                    &expected_after_flag, &reset_flag)));

  // To verify that IsSet() fetches the flag's value from memory every time it
  // is called (not just the first time that it is called on a thread), sleep
  // before setting the flag.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // |expected_after_flag| is used to verify that all memory operations
  // performed before |tested_flag| is Set() are visible to threads that can see
  // IsSet().
  expected_after_flag = true;
  tested_flag.Set();

  // Sleep again to give the busy loop time to observe the flag and verify
  // expectations.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // Use |reset_flag| to confirm that the above completed (which the rest of
  // this test assumes).
  while (!reset_flag.IsSet()) {
    std::this_thread::yield();
  }

  tested_flag.UnsafeResetForTesting();
  EXPECT_FALSE(tested_flag.IsSet());
  expected_after_flag = false;

  // Perform the same test again after the controlled UnsafeResetForTesting(),
  // |thread| is guaranteed to be synchronized past the
  // |UnsafeResetForTesting()| call when the task runs per the implicit
  // synchronization in the post task mechanism.
  thread->GetTaskRunner()->PostTask(CefCreateClosureTask(BindOnce(
      &BusyWaitUntilFlagIsSet, &tested_flag, &expected_after_flag, nullptr)));

  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  expected_after_flag = true;
  tested_flag.Set();

  // The |thread|'s destructor will block until the posted task completes, so
  // the test will time out if it fails to see the flag be set.
  thread->Stop();
}

}  // namespace base

// =============================================================================
// SKIPPED TESTS from Chromium's base/synchronization/atomic_flag_unittest.cc
// =============================================================================
// AtomicFlagTest.SetOnDifferentSequenceDeathTest - Death test
