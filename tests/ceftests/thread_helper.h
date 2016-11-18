// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_UNITTESTS_THREAD_HELPER_H_
#define CEF_TESTS_UNITTESTS_THREAD_HELPER_H_
#pragma once

#include "include/base/cef_bind.h"
#include "include/cef_task.h"
#include "include/cef_waitable_event.h"

// Helper for signaling |event|.
void SignalEvent(CefRefPtr<CefWaitableEvent> event);

// Post a task to the specified thread and wait for the task to execute as
// indication that all previously pending tasks on that thread have completed.
void WaitForThread(CefThreadId thread_id, int64 delay_ms = 0);
void WaitForThread(CefRefPtr<CefTaskRunner> task_runner, int64 delay_ms = 0);

#define WaitForIOThread() WaitForThread(TID_IO)
#define WaitForUIThread() WaitForThread(TID_UI)
#define WaitForDBThread() WaitForThread(TID_DB)
#define WaitForIOThreadWithDelay(delay_ms) WaitForThread(TID_IO, delay_ms)
#define WaitForUIThreadWithDelay(delay_ms) WaitForThread(TID_UI, delay_ms)
#define WaitForDBThreadWithDelay(delay_ms) WaitForThread(TID_DB, delay_ms)

// Assert that execution is occuring on the named thread.
#define EXPECT_UI_THREAD()       EXPECT_TRUE(CefCurrentlyOn(TID_UI));
#define EXPECT_IO_THREAD()       EXPECT_TRUE(CefCurrentlyOn(TID_IO));
#define EXPECT_FILE_THREAD()     EXPECT_TRUE(CefCurrentlyOn(TID_FILE));
#define EXPECT_RENDERER_THREAD() EXPECT_TRUE(CefCurrentlyOn(TID_RENDERER));

// Executes |test_impl| on the specified |thread_id|. |event| will be signaled
// once execution is complete.
void RunOnThread(CefThreadId thread_id,
                 const base::Callback<void(void)>& test_impl,
                 CefRefPtr<CefWaitableEvent> event);

#define NAMED_THREAD_TEST(thread_id, test_case_name, test_name)  \
    TEST(test_case_name, test_name) { \
      CefRefPtr<CefWaitableEvent> event = \
          CefWaitableEvent::CreateWaitableEvent(true, false); \
      RunOnThread(thread_id, base::Bind(test_name##Impl), event); \
      event->Wait(); \
    }

// Execute "test_case_name.test_name" test on the named thread. The test
// implementation is "void test_nameImpl()".
#define UI_THREAD_TEST(test_case_name, test_name)  \
    NAMED_THREAD_TEST(TID_UI, test_case_name, test_name)

// Like RunOnThread() but |test_impl| is responsible for signaling |event|.
void RunOnThreadAsync(
    CefThreadId thread_id,
    const base::Callback<void(CefRefPtr<CefWaitableEvent>)>& test_impl,
    CefRefPtr<CefWaitableEvent>);

#define NAMED_THREAD_TEST_ASYNC(thread_id, test_case_name, test_name)  \
    TEST(test_case_name, test_name) { \
      CefRefPtr<CefWaitableEvent> event = \
          CefWaitableEvent::CreateWaitableEvent(true, false); \
      RunOnThreadAsync(thread_id, base::Bind(test_name##Impl), event); \
      event->Wait(); \
    }

// Execute "test_case_name.test_name" test on the named thread. The test
// implementation is "void test_nameImpl(CefRefPtr<CefWaitableEvent> event)".
#define UI_THREAD_TEST_ASYNC(test_case_name, test_name)  \
    NAMED_THREAD_TEST_ASYNC(TID_UI, test_case_name, test_name)

#endif  // CEF_TESTS_UNITTESTS_THREAD_HELPER_H_
