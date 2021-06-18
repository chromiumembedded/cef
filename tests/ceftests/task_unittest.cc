// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/cef_command_line.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

void WaitForEvent(CefRefPtr<CefWaitableEvent> event) {
  if (CefCommandLine::GetGlobalCommandLine()->HasSwitch(
          "disable-test-timeout")) {
    event->Wait();
  } else {
    EXPECT_TRUE(event->TimedWait(1000));
  }
}

void GetForCurrentThread(bool* ran_test, CefRefPtr<CefWaitableEvent> event) {
  // Currently on the FILE thread.
  CefRefPtr<CefTaskRunner> runner = CefTaskRunner::GetForCurrentThread();
  EXPECT_TRUE(runner.get());
  EXPECT_TRUE(runner->BelongsToCurrentThread());
  EXPECT_TRUE(runner->BelongsToThread(TID_FILE_USER_VISIBLE));
  EXPECT_FALSE(runner->BelongsToThread(TID_IO));
  EXPECT_TRUE(runner->IsSame(runner));

  CefRefPtr<CefTaskRunner> runner2 = CefTaskRunner::GetForCurrentThread();
  EXPECT_TRUE(runner2.get());
  EXPECT_TRUE(runner->IsSame(runner2));
  EXPECT_TRUE(runner2->IsSame(runner));

  // Not on the IO thread.
  CefRefPtr<CefTaskRunner> runner3 = CefTaskRunner::GetForThread(TID_IO);
  EXPECT_TRUE(runner3.get());
  EXPECT_FALSE(runner->IsSame(runner3));
  EXPECT_FALSE(runner3->IsSame(runner));

  *ran_test = true;
  event->Signal();
}

void GetForThread(bool* ran_test, CefRefPtr<CefWaitableEvent> event) {
  // Currently on the FILE thread.
  CefRefPtr<CefTaskRunner> runner = CefTaskRunner::GetForThread(TID_IO);
  EXPECT_TRUE(runner.get());
  EXPECT_FALSE(runner->BelongsToCurrentThread());
  EXPECT_TRUE(runner->BelongsToThread(TID_IO));
  EXPECT_FALSE(runner->BelongsToThread(TID_FILE_USER_VISIBLE));
  EXPECT_TRUE(runner->IsSame(runner));

  CefRefPtr<CefTaskRunner> runner2 = CefTaskRunner::GetForThread(TID_IO);
  EXPECT_TRUE(runner2.get());
  EXPECT_TRUE(runner->IsSame(runner2));
  EXPECT_TRUE(runner2->IsSame(runner));

  CefRefPtr<CefTaskRunner> runner3 =
      CefTaskRunner::GetForThread(TID_FILE_USER_VISIBLE);
  EXPECT_TRUE(runner3.get());
  EXPECT_FALSE(runner->IsSame(runner3));
  EXPECT_FALSE(runner3->IsSame(runner));

  *ran_test = true;
  event->Signal();
}

void PostTaskEvent1(bool* ran_test,
                    CefRefPtr<CefWaitableEvent> event,
                    CefRefPtr<CefTaskRunner> runner) {
  // Currently on the IO thread.
  EXPECT_TRUE(runner->BelongsToCurrentThread());
  EXPECT_TRUE(runner->BelongsToThread(TID_IO));
  EXPECT_FALSE(runner->BelongsToThread(TID_FILE_USER_VISIBLE));

  // Current thread should be the IO thread.
  CefRefPtr<CefTaskRunner> runner2 = CefTaskRunner::GetForCurrentThread();
  EXPECT_TRUE(runner2.get());
  EXPECT_TRUE(runner2->BelongsToCurrentThread());
  EXPECT_TRUE(runner2->BelongsToThread(TID_IO));
  EXPECT_FALSE(runner2->BelongsToThread(TID_FILE_USER_VISIBLE));
  EXPECT_TRUE(runner->IsSame(runner2));
  EXPECT_TRUE(runner2->IsSame(runner));

  // Current thread should be the IO thread.
  CefRefPtr<CefTaskRunner> runner3 = CefTaskRunner::GetForThread(TID_IO);
  EXPECT_TRUE(runner3.get());
  EXPECT_TRUE(runner3->BelongsToCurrentThread());
  EXPECT_TRUE(runner3->BelongsToThread(TID_IO));
  EXPECT_FALSE(runner3->BelongsToThread(TID_FILE_USER_VISIBLE));
  EXPECT_TRUE(runner->IsSame(runner3));
  EXPECT_TRUE(runner3->IsSame(runner));

  // Current thread should not be the FILE thread.
  CefRefPtr<CefTaskRunner> runner4 =
      CefTaskRunner::GetForThread(TID_FILE_USER_VISIBLE);
  EXPECT_TRUE(runner4.get());
  EXPECT_FALSE(runner4->BelongsToCurrentThread());
  EXPECT_FALSE(runner4->BelongsToThread(TID_IO));
  EXPECT_TRUE(runner4->BelongsToThread(TID_FILE_USER_VISIBLE));
  EXPECT_FALSE(runner->IsSame(runner4));
  EXPECT_FALSE(runner4->IsSame(runner));

  *ran_test = true;
  event->Signal();
}

void PostOnceTask1(bool* ran_test, CefRefPtr<CefWaitableEvent> event) {
  // Currently on the FILE thread.
  CefRefPtr<CefTaskRunner> runner = CefTaskRunner::GetForThread(TID_IO);
  EXPECT_TRUE(runner.get());
  EXPECT_FALSE(runner->BelongsToCurrentThread());
  EXPECT_TRUE(runner->BelongsToThread(TID_IO));

  runner->PostTask(CefCreateClosureTask(
      base::BindOnce(&PostTaskEvent1, ran_test, event, runner)));
}

void PostRepeatingTask1(bool* ran_test, CefRefPtr<CefWaitableEvent> event) {
  // Currently on the FILE thread.
  CefRefPtr<CefTaskRunner> runner = CefTaskRunner::GetForThread(TID_IO);
  EXPECT_TRUE(runner.get());
  EXPECT_FALSE(runner->BelongsToCurrentThread());
  EXPECT_TRUE(runner->BelongsToThread(TID_IO));

  runner->PostTask(CefCreateClosureTask(
      base::BindRepeating(&PostTaskEvent1, ran_test, event, runner)));
}

void PostOnceDelayedTask1(bool* ran_test, CefRefPtr<CefWaitableEvent> event) {
  // Currently on the FILE thread.
  CefRefPtr<CefTaskRunner> runner = CefTaskRunner::GetForThread(TID_IO);
  EXPECT_TRUE(runner.get());
  EXPECT_FALSE(runner->BelongsToCurrentThread());
  EXPECT_TRUE(runner->BelongsToThread(TID_IO));

  runner->PostDelayedTask(CefCreateClosureTask(base::BindOnce(
                              &PostTaskEvent1, ran_test, event, runner)),
                          0);
}

void PostRepeatingDelayedTask1(bool* ran_test,
                               CefRefPtr<CefWaitableEvent> event) {
  // Currently on the FILE thread.
  CefRefPtr<CefTaskRunner> runner = CefTaskRunner::GetForThread(TID_IO);
  EXPECT_TRUE(runner.get());
  EXPECT_FALSE(runner->BelongsToCurrentThread());
  EXPECT_TRUE(runner->BelongsToThread(TID_IO));

  runner->PostDelayedTask(CefCreateClosureTask(base::BindRepeating(
                              &PostTaskEvent1, ran_test, event, runner)),
                          0);
}

void PostTaskEvent2(bool* ran_test, CefRefPtr<CefWaitableEvent> event) {
  EXPECT_TRUE(CefCurrentlyOn(TID_IO));
  EXPECT_FALSE(CefCurrentlyOn(TID_FILE_USER_VISIBLE));

  *ran_test = true;
  event->Signal();
}

void PostOnceTask2(bool* ran_test, CefRefPtr<CefWaitableEvent> event) {
  // Currently on the FILE thread.
  EXPECT_FALSE(CefCurrentlyOn(TID_IO));

  CefPostTask(TID_IO, CefCreateClosureTask(
                          base::BindOnce(&PostTaskEvent2, ran_test, event)));
}

void PostRepeatingTask2(bool* ran_test, CefRefPtr<CefWaitableEvent> event) {
  // Currently on the FILE thread.
  EXPECT_FALSE(CefCurrentlyOn(TID_IO));

  CefPostTask(TID_IO, CefCreateClosureTask(base::BindRepeating(
                          &PostTaskEvent2, ran_test, event)));
}

void PostOnceDelayedTask2(bool* ran_test, CefRefPtr<CefWaitableEvent> event) {
  // Currently on the FILE thread.
  EXPECT_FALSE(CefCurrentlyOn(TID_IO));

  CefPostDelayedTask(
      TID_IO,
      CefCreateClosureTask(base::BindOnce(&PostTaskEvent2, ran_test, event)),
      0);
}

void PostRepeatingDelayedTask2(bool* ran_test,
                               CefRefPtr<CefWaitableEvent> event) {
  // Currently on the FILE thread.
  EXPECT_FALSE(CefCurrentlyOn(TID_IO));

  CefPostDelayedTask(TID_IO,
                     CefCreateClosureTask(
                         base::BindRepeating(&PostTaskEvent2, ran_test, event)),
                     0);
}

}  // namespace

TEST(TaskTest, GetOnceForCurrentThread) {
  bool ran_test = false;
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_FILE_USER_VISIBLE,
              CefCreateClosureTask(
                  base::BindOnce(&GetForCurrentThread, &ran_test, event)));
  WaitForEvent(event);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, GetRepeatingForCurrentThread) {
  bool ran_test = false;
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_FILE_USER_VISIBLE,
              CefCreateClosureTask(
                  base::BindRepeating(&GetForCurrentThread, &ran_test, event)));
  WaitForEvent(event);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, GetOnceForThread) {
  bool ran_test = false;
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_FILE_USER_VISIBLE, CefCreateClosureTask(base::BindOnce(
                                         &GetForThread, &ran_test, event)));
  WaitForEvent(event);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, GetRepeatingForThread) {
  bool ran_test = false;
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_FILE_USER_VISIBLE, CefCreateClosureTask(base::BindRepeating(
                                         &GetForThread, &ran_test, event)));
  WaitForEvent(event);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, PostOnceTask1) {
  bool ran_test = false;
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_FILE_USER_VISIBLE, CefCreateClosureTask(base::BindOnce(
                                         &PostOnceTask1, &ran_test, event)));
  WaitForEvent(event);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, PostRepeatingTask1) {
  bool ran_test = false;
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_FILE_USER_VISIBLE,
              CefCreateClosureTask(
                  base::BindRepeating(&PostRepeatingTask1, &ran_test, event)));
  WaitForEvent(event);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, PostOnceDelayedTask1) {
  bool ran_test = false;
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_FILE_USER_VISIBLE,
              CefCreateClosureTask(
                  base::BindOnce(&PostOnceDelayedTask1, &ran_test, event)));
  WaitForEvent(event);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, PostRepeatingDelayedTask1) {
  bool ran_test = false;
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_FILE_USER_VISIBLE,
              CefCreateClosureTask(base::BindRepeating(
                  &PostRepeatingDelayedTask1, &ran_test, event)));
  WaitForEvent(event);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, PostOnceTask2) {
  bool ran_test = false;
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_FILE_USER_VISIBLE, CefCreateClosureTask(base::BindOnce(
                                         &PostOnceTask2, &ran_test, event)));
  WaitForEvent(event);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, PostRepeatingTask2) {
  bool ran_test = false;
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_FILE_USER_VISIBLE,
              CefCreateClosureTask(
                  base::BindRepeating(&PostRepeatingTask2, &ran_test, event)));
  WaitForEvent(event);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, PostOnceDelayedTask2) {
  bool ran_test = false;
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_FILE_USER_VISIBLE,
              CefCreateClosureTask(
                  base::BindOnce(&PostOnceDelayedTask2, &ran_test, event)));
  WaitForEvent(event);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, PostRepeatingDelayedTask2) {
  bool ran_test = false;
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(TID_FILE_USER_VISIBLE,
              CefCreateClosureTask(base::BindRepeating(
                  &PostRepeatingDelayedTask2, &ran_test, event)));
  WaitForEvent(event);
  EXPECT_TRUE(ran_test);
}
