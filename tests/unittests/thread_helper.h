// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_UNITTESTS_THREAD_HELPER_H_
#define CEF_TESTS_UNITTESTS_THREAD_HELPER_H_
#pragma once

#include "include/base/cef_bind.h"
#include "include/cef_task.h"

#include "base/synchronization/waitable_event.h"

// Helper for signaling |event|.
void SignalEvent(base::WaitableEvent* event);

// Post a task to the specified thread and wait for the task to execute as
// indication that all previously pending tasks on that thread have completed.
void WaitForThread(CefThreadId thread_id);
void WaitForThread(CefRefPtr<CefTaskRunner> task_runner);

#define WaitForIOThread() WaitForThread(TID_IO)
#define WaitForUIThread() WaitForThread(TID_UI)
#define WaitForDBThread() WaitForThread(TID_DB)

// Assert that execution is occuring on the named thread.
#define EXPECT_UI_THREAD()       EXPECT_TRUE(CefCurrentlyOn(TID_UI));
#define EXPECT_IO_THREAD()       EXPECT_TRUE(CefCurrentlyOn(TID_IO));
#define EXPECT_FILE_THREAD()     EXPECT_TRUE(CefCurrentlyOn(TID_FILE));
#define EXPECT_RENDERER_THREAD() EXPECT_TRUE(CefCurrentlyOn(TID_RENDERER));

// Executes |test_impl| on the specified |thread_id|. |event| will be signaled
// once execution is complete.
void RunOnThread(CefThreadId thread_id,
                 const base::Callback<void(void)>& test_impl,
                 base::WaitableEvent* event);

#define NAMED_THREAD_TEST(thread_id, test_case_name, test_name)  \
    TEST(test_case_name, test_name) { \
      base::WaitableEvent event(false, false); \
      RunOnThread(thread_id, base::Bind(test_name##Impl), &event); \
      event.Wait(); \
    }

// Execute "test_case_name.test_name" test on the named thread. The test
// implementation is "void test_nameImpl()".
#define UI_THREAD_TEST(test_case_name, test_name)  \
    NAMED_THREAD_TEST(TID_UI, test_case_name, test_name)

// Like RunOnThread() but |test_impl| is responsible for signaling |event|.
void RunOnThreadAsync(
    CefThreadId thread_id,
    const base::Callback<void(base::WaitableEvent*)>& test_impl,
    base::WaitableEvent* event);

#define NAMED_THREAD_TEST_ASYNC(thread_id, test_case_name, test_name)  \
    TEST(test_case_name, test_name) { \
      base::WaitableEvent event(false, false); \
      RunOnThreadAsync(thread_id, base::Bind(test_name##Impl), &event); \
      event.Wait(); \
    }

// Execute "test_case_name.test_name" test on the named thread. The test
// implementation is "void test_nameImpl(base::WaitableEvent* event)".
#define UI_THREAD_TEST_ASYNC(test_case_name, test_name)  \
    NAMED_THREAD_TEST_ASYNC(TID_UI, test_case_name, test_name)

#endif  // CEF_TESTS_UNITTESTS_THREAD_HELPER_H_
