// Copyright (c) 2025 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was ported from Chromium source file:
// base/synchronization/lock_unittest.cc
// Chromium commit: 70909488d8cc2
//
// ---------------------------------------------------------------------------

#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>

#include "include/base/cef_bind.h"
#include "include/base/cef_callback.h"
#include "include/base/cef_lock.h"
#include "include/cef_thread.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace base {

// Basic test to make sure that Acquire()/Release()/Try() don't crash ----------

namespace {

// Random delay up to max_ms milliseconds.
void RandomSleep(int max_ms) {
  int ms = rand() % (max_ms + 1);
  if (ms > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  }
}

}  // namespace

TEST(LockTest, Basic) {
  Lock lock;

  CefRefPtr<CefThread> thread = CefThread::CreateThread("LockTest.Basic");
  ASSERT_TRUE(thread.get());
  ASSERT_TRUE(thread->GetTaskRunner().get());

  std::atomic<int> thread_acquired{0};
  CefRefPtr<CefWaitableEvent> done_event =
      CefWaitableEvent::CreateWaitableEvent(false, false);

  thread->GetTaskRunner()->PostTask(CefCreateClosureTask(BindOnce(
      [](Lock* lock, std::atomic<int>* acquired,
         CefRefPtr<CefWaitableEvent> done) {
        for (int i = 0; i < 10; i++) {
          lock->Acquire();
          (*acquired)++;
          lock->Release();
        }
        for (int i = 0; i < 10; i++) {
          lock->Acquire();
          (*acquired)++;
          RandomSleep(20);
          lock->Release();
        }
        for (int i = 0; i < 10; i++) {
          if (lock->Try()) {
            (*acquired)++;
            RandomSleep(20);
            lock->Release();
          }
        }
        done->Signal();
      },
      &lock, &thread_acquired, done_event)));

  int acquired = 0;
  for (int i = 0; i < 5; i++) {
    lock.Acquire();
    acquired++;
    lock.Release();
  }
  for (int i = 0; i < 10; i++) {
    lock.Acquire();
    acquired++;
    RandomSleep(20);
    lock.Release();
  }
  for (int i = 0; i < 10; i++) {
    if (lock.Try()) {
      acquired++;
      RandomSleep(20);
      lock.Release();
    }
  }
  for (int i = 0; i < 5; i++) {
    lock.Acquire();
    acquired++;
    RandomSleep(20);
    lock.Release();
  }

  done_event->Wait();
  thread->Stop();

  EXPECT_GE(acquired, 20);
  EXPECT_GE(thread_acquired.load(), 20);
}

// Test that Try() works as expected -------------------------------------------

TEST(LockTest, TryLock) {
  Lock lock;

  ASSERT_TRUE(lock.Try());
  lock.AssertAcquired();

  // This thread will not be able to get the lock.
  {
    CefRefPtr<CefThread> thread = CefThread::CreateThread("LockTest.TryLock.1");
    ASSERT_TRUE(thread.get());

    std::atomic<bool> got_lock{false};
    CefRefPtr<CefWaitableEvent> done_event =
        CefWaitableEvent::CreateWaitableEvent(false, false);

    thread->GetTaskRunner()->PostTask(CefCreateClosureTask(BindOnce(
        [](Lock* lock, std::atomic<bool>* got_lock,
           CefRefPtr<CefWaitableEvent> done) {
          // The local variable is required for the static analyzer to see that
          // the lock is properly released.
          bool acquired = lock->Try();
          *got_lock = acquired;
          if (acquired) {
            lock->Release();
          }
          done->Signal();
        },
        &lock, &got_lock, done_event)));

    done_event->Wait();
    thread->Stop();

    ASSERT_FALSE(got_lock.load());
  }

  lock.Release();

  // This thread will....
  {
    CefRefPtr<CefThread> thread = CefThread::CreateThread("LockTest.TryLock.2");
    ASSERT_TRUE(thread.get());

    std::atomic<bool> got_lock{false};
    CefRefPtr<CefWaitableEvent> done_event =
        CefWaitableEvent::CreateWaitableEvent(false, false);

    thread->GetTaskRunner()->PostTask(CefCreateClosureTask(BindOnce(
        [](Lock* lock, std::atomic<bool>* got_lock,
           CefRefPtr<CefWaitableEvent> done) {
          bool acquired = lock->Try();
          *got_lock = acquired;
          if (acquired) {
            lock->Release();
          }
          done->Signal();
        },
        &lock, &got_lock, done_event)));

    done_event->Wait();
    thread->Stop();

    ASSERT_TRUE(got_lock.load());
    // But it released it....
    ASSERT_TRUE(lock.Try());
    lock.AssertAcquired();
  }

  lock.Release();
}

// Tests that locks actually exclude -------------------------------------------

namespace {

// Static helper which can also be called from the main thread.
void DoMutexStuff(Lock* lock, std::atomic<int>* value) {
  for (int i = 0; i < 40; i++) {
    lock->Acquire();
    int v = value->load();
    RandomSleep(10);
    value->store(v + 1);
    lock->Release();
  }
}

}  // namespace

TEST(LockTest, MutexTwoThreads) {
  Lock lock;
  std::atomic<int> value{0};

  CefRefPtr<CefThread> thread =
      CefThread::CreateThread("LockTest.MutexTwoThreads");
  ASSERT_TRUE(thread.get());

  CefRefPtr<CefWaitableEvent> done_event =
      CefWaitableEvent::CreateWaitableEvent(false, false);

  thread->GetTaskRunner()->PostTask(CefCreateClosureTask(BindOnce(
      [](Lock* lock, std::atomic<int>* value,
         CefRefPtr<CefWaitableEvent> done) {
        DoMutexStuff(lock, value);
        done->Signal();
      },
      &lock, &value, done_event)));

  DoMutexStuff(&lock, &value);

  done_event->Wait();
  thread->Stop();

  EXPECT_EQ(2 * 40, value.load());
}

TEST(LockTest, MutexFourThreads) {
  Lock lock;
  std::atomic<int> value{0};

  CefRefPtr<CefThread> thread1 =
      CefThread::CreateThread("LockTest.MutexFourThreads.1");
  CefRefPtr<CefThread> thread2 =
      CefThread::CreateThread("LockTest.MutexFourThreads.2");
  CefRefPtr<CefThread> thread3 =
      CefThread::CreateThread("LockTest.MutexFourThreads.3");
  ASSERT_TRUE(thread1.get());
  ASSERT_TRUE(thread2.get());
  ASSERT_TRUE(thread3.get());

  CefRefPtr<CefWaitableEvent> done_event1 =
      CefWaitableEvent::CreateWaitableEvent(false, false);
  CefRefPtr<CefWaitableEvent> done_event2 =
      CefWaitableEvent::CreateWaitableEvent(false, false);
  CefRefPtr<CefWaitableEvent> done_event3 =
      CefWaitableEvent::CreateWaitableEvent(false, false);

  thread1->GetTaskRunner()->PostTask(CefCreateClosureTask(BindOnce(
      [](Lock* lock, std::atomic<int>* value,
         CefRefPtr<CefWaitableEvent> done) {
        DoMutexStuff(lock, value);
        done->Signal();
      },
      &lock, &value, done_event1)));

  thread2->GetTaskRunner()->PostTask(CefCreateClosureTask(BindOnce(
      [](Lock* lock, std::atomic<int>* value,
         CefRefPtr<CefWaitableEvent> done) {
        DoMutexStuff(lock, value);
        done->Signal();
      },
      &lock, &value, done_event2)));

  thread3->GetTaskRunner()->PostTask(CefCreateClosureTask(BindOnce(
      [](Lock* lock, std::atomic<int>* value,
         CefRefPtr<CefWaitableEvent> done) {
        DoMutexStuff(lock, value);
        done->Signal();
      },
      &lock, &value, done_event3)));

  DoMutexStuff(&lock, &value);

  done_event1->Wait();
  done_event2->Wait();
  done_event3->Wait();

  thread1->Stop();
  thread2->Stop();
  thread3->Stop();

  EXPECT_EQ(4 * 40, value.load());
}

}  // namespace base

// =============================================================================
// SKIPPED TESTS from Chromium's base/synchronization/lock_unittest.cc
// =============================================================================
// LockTest.InvariantIsCalled - Requires base::FunctionRef, LIFETIME_BOUND
// LockTest.AutoLockMaybe - AutoLockMaybe not exposed in CEF
// LockTest.AutoLockMaybeNull - AutoLockMaybe not exposed in CEF
// LockTest.ReleasableAutoLockExplicitRelease - ReleasableAutoLock not exposed
// LockTest.ReleasableAutoLockImplicitRelease - ReleasableAutoLock not exposed
// TryLockTest.CorrectlyCheckIsAcquired - Requires GUARDED_BY thread annotation
// LockTest.GetTrackedLocksHeldByCurrentThread - Lock tracking infrastructure
// LockTest.GetTrackedLocksHeldByCurrentThread_AutoLock - Lock tracking
// LockTest.GetTrackedLocksHeldByCurrentThread_MovableAutoLock - Lock tracking
// LockTest.GetTrackedLocksHeldByCurrentThread_AutoTryLock - Lock tracking
// LockTest.GetTrackedLocksHeldByCurrentThread_AutoLockMaybe - Lock tracking
// LockTest.GetTrackedLocksHeldByCurrentThreadOverCapacity - Lock tracking
// LockTest.TrackingDisabled - Lock tracking
// LockTest.PriorityIsInherited - Platform-specific priority inheritance
