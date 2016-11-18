// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_bind.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

void GetForCurrentThread(bool* ran_test) {
  // Currently on the FILE thread.
  CefRefPtr<CefTaskRunner> runner = CefTaskRunner::GetForCurrentThread();
  EXPECT_TRUE(runner.get());
  EXPECT_TRUE(runner->BelongsToCurrentThread());
  EXPECT_TRUE(runner->BelongsToThread(TID_FILE));
  EXPECT_FALSE(runner->BelongsToThread(TID_DB));
  EXPECT_TRUE(runner->IsSame(runner));

  CefRefPtr<CefTaskRunner> runner2 = CefTaskRunner::GetForCurrentThread();
  EXPECT_TRUE(runner2.get());
  EXPECT_TRUE(runner->IsSame(runner2));
  EXPECT_TRUE(runner2->IsSame(runner));

  // Not on the DB thread.
  CefRefPtr<CefTaskRunner> runner3 = CefTaskRunner::GetForThread(TID_DB);
  EXPECT_TRUE(runner3.get());
  EXPECT_FALSE(runner->IsSame(runner3));
  EXPECT_FALSE(runner3->IsSame(runner));

  *ran_test = true;
}

void GetForThread(bool* ran_test) {
  // Currently on the FILE thread.
  CefRefPtr<CefTaskRunner> runner = CefTaskRunner::GetForThread(TID_DB);
  EXPECT_TRUE(runner.get());
  EXPECT_FALSE(runner->BelongsToCurrentThread());
  EXPECT_TRUE(runner->BelongsToThread(TID_DB));
  EXPECT_FALSE(runner->BelongsToThread(TID_FILE));
  EXPECT_TRUE(runner->IsSame(runner));

  CefRefPtr<CefTaskRunner> runner2 = CefTaskRunner::GetForThread(TID_DB);
  EXPECT_TRUE(runner2.get());
  EXPECT_TRUE(runner->IsSame(runner2));
  EXPECT_TRUE(runner2->IsSame(runner));

  CefRefPtr<CefTaskRunner> runner3 = CefTaskRunner::GetForThread(TID_FILE);
  EXPECT_TRUE(runner3.get());
  EXPECT_FALSE(runner->IsSame(runner3));
  EXPECT_FALSE(runner3->IsSame(runner));

 *ran_test = true;
}

void PostTaskEvent1(bool* got_it, CefRefPtr<CefTaskRunner> runner) {
  // Currently on the DB thread.
  EXPECT_TRUE(runner->BelongsToCurrentThread());
  EXPECT_TRUE(runner->BelongsToThread(TID_DB));
  EXPECT_FALSE(runner->BelongsToThread(TID_FILE));

  // Current thread should be the DB thread.
  CefRefPtr<CefTaskRunner> runner2 = CefTaskRunner::GetForCurrentThread();
  EXPECT_TRUE(runner2.get());
  EXPECT_TRUE(runner2->BelongsToCurrentThread());
  EXPECT_TRUE(runner2->BelongsToThread(TID_DB));
  EXPECT_FALSE(runner2->BelongsToThread(TID_FILE));
  EXPECT_TRUE(runner->IsSame(runner2));
  EXPECT_TRUE(runner2->IsSame(runner));

  // Current thread should be the DB thread.
  CefRefPtr<CefTaskRunner> runner3 = CefTaskRunner::GetForThread(TID_DB);
  EXPECT_TRUE(runner3.get());
  EXPECT_TRUE(runner3->BelongsToCurrentThread());
  EXPECT_TRUE(runner3->BelongsToThread(TID_DB));
  EXPECT_FALSE(runner3->BelongsToThread(TID_FILE));
  EXPECT_TRUE(runner->IsSame(runner3));
  EXPECT_TRUE(runner3->IsSame(runner));

  // Current thread should not be the FILE thread.
  CefRefPtr<CefTaskRunner> runner4 = CefTaskRunner::GetForThread(TID_FILE);
  EXPECT_TRUE(runner4.get());
  EXPECT_FALSE(runner4->BelongsToCurrentThread());
  EXPECT_FALSE(runner4->BelongsToThread(TID_DB));
  EXPECT_TRUE(runner4->BelongsToThread(TID_FILE));
  EXPECT_FALSE(runner->IsSame(runner4));
  EXPECT_FALSE(runner4->IsSame(runner));

  *got_it = true;
}

void PostTask1(bool* ran_test) {
  // Currently on the FILE thread.
  CefRefPtr<CefTaskRunner> runner = CefTaskRunner::GetForThread(TID_DB);
  EXPECT_TRUE(runner.get());
  EXPECT_FALSE(runner->BelongsToCurrentThread());
  EXPECT_TRUE(runner->BelongsToThread(TID_DB));
  
  bool got_it = false;
  runner->PostTask(
      CefCreateClosureTask(base::Bind(&PostTaskEvent1, &got_it, runner)));

  WaitForThread(runner);
  EXPECT_TRUE(got_it);

  *ran_test = true;
}

void PostDelayedTask1(bool* ran_test) {
  // Currently on the FILE thread.
  CefRefPtr<CefTaskRunner> runner = CefTaskRunner::GetForThread(TID_DB);
  EXPECT_TRUE(runner.get());
  EXPECT_FALSE(runner->BelongsToCurrentThread());
  EXPECT_TRUE(runner->BelongsToThread(TID_DB));
  
  bool got_it = false;
  runner->PostDelayedTask(
      CefCreateClosureTask(base::Bind(&PostTaskEvent1, &got_it, runner)), 0);

  WaitForThread(runner);
  EXPECT_TRUE(got_it);

  *ran_test = true;
}

void PostTaskEvent2(bool* got_it) {
  EXPECT_TRUE(CefCurrentlyOn(TID_DB));
  EXPECT_FALSE(CefCurrentlyOn(TID_FILE));

  *got_it = true;
}

void PostTask2(bool* ran_test) {
  // Currently on the FILE thread.
  EXPECT_FALSE(CefCurrentlyOn(TID_DB));

  bool got_it = false;
  CefPostTask(TID_DB,
      CefCreateClosureTask(base::Bind(&PostTaskEvent2, &got_it)));

  WaitForThread(TID_DB);
  EXPECT_TRUE(got_it);

  *ran_test = true;
}

void PostDelayedTask2(bool* ran_test) {
  // Currently on the FILE thread.
  EXPECT_FALSE(CefCurrentlyOn(TID_DB));

  bool got_it = false;
  CefPostDelayedTask(TID_DB,
      CefCreateClosureTask(base::Bind(&PostTaskEvent2, &got_it)), 0);

  WaitForThread(TID_DB);
  EXPECT_TRUE(got_it);

  *ran_test = true;
}

}  // namespace

TEST(TaskTest, GetForCurrentThread) {
  bool ran_test = false;
  CefPostTask(TID_FILE,
      CefCreateClosureTask(base::Bind(&GetForCurrentThread, &ran_test)));
  WaitForThread(TID_FILE);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, GetForThread) {
  bool ran_test = false;
  CefPostTask(TID_FILE,
      CefCreateClosureTask(base::Bind(&GetForThread, &ran_test)));
  WaitForThread(TID_FILE);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, PostTask1) {
  bool ran_test = false;
  CefPostTask(TID_FILE, CefCreateClosureTask(base::Bind(&PostTask1, &ran_test)));
  WaitForThread(TID_FILE);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, PostDelayedTask1) {
  bool ran_test = false;
  CefPostTask(TID_FILE,
      CefCreateClosureTask(base::Bind(&PostDelayedTask1, &ran_test)));
  WaitForThread(TID_FILE);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, PostTask2) {
  bool ran_test = false;
  CefPostTask(TID_FILE, CefCreateClosureTask(base::Bind(&PostTask2, &ran_test)));
  WaitForThread(TID_FILE);
  EXPECT_TRUE(ran_test);
}

TEST(TaskTest, PostDelayedTask2) {
  bool ran_test = false;
  CefPostTask(TID_FILE,
      CefCreateClosureTask(base::Bind(&PostDelayedTask2, &ran_test)));
  WaitForThread(TID_FILE);
  EXPECT_TRUE(ran_test);
}
