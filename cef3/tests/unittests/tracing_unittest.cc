// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "include/cef_task.h"
#include "include/cef_trace.h"
#include "include/cef_trace_event.h"
#include "tests/unittests/test_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

enum TracingTestType {
  CEF_TRACE_EVENT0,
  CEF_TRACE_EVENT1,
  CEF_TRACE_EVENT2,
  CEF_TRACE_EVENT_INSTANT0,
  CEF_TRACE_EVENT_INSTANT1,
  CEF_TRACE_EVENT_INSTANT2,
  CEF_TRACE_EVENT_COPY_INSTANT0,
  CEF_TRACE_EVENT_COPY_INSTANT1,
  CEF_TRACE_EVENT_COPY_INSTANT2,
  CEF_TRACE_EVENT_BEGIN0,
  CEF_TRACE_EVENT_BEGIN1,
  CEF_TRACE_EVENT_BEGIN2,
  CEF_TRACE_EVENT_COPY_BEGIN0,
  CEF_TRACE_EVENT_COPY_BEGIN1,
  CEF_TRACE_EVENT_COPY_BEGIN2,
  CEF_TRACE_EVENT_END0,
  CEF_TRACE_EVENT_END1,
  CEF_TRACE_EVENT_END2,
  CEF_TRACE_EVENT_COPY_END0,
  CEF_TRACE_EVENT_COPY_END1,
  CEF_TRACE_EVENT_COPY_END2,
  CEF_TRACE_EVENT_IF_LONGER_THAN0,
  CEF_TRACE_EVENT_IF_LONGER_THAN1,
  CEF_TRACE_EVENT_IF_LONGER_THAN2,
  CEF_TRACE_COUNTER1,
  CEF_TRACE_COPY_COUNTER1,
  CEF_TRACE_COUNTER2,
  CEF_TRACE_COPY_COUNTER2,
  CEF_TRACE_COUNTER_ID1,
  CEF_TRACE_COPY_COUNTER_ID1,
  CEF_TRACE_COUNTER_ID2,
  CEF_TRACE_COPY_COUNTER_ID2,
  CEF_TRACE_EVENT_ASYNC_BEGIN0,
  CEF_TRACE_EVENT_ASYNC_BEGIN1,
  CEF_TRACE_EVENT_ASYNC_BEGIN2,
  CEF_TRACE_EVENT_COPY_ASYNC_BEGIN0,
  CEF_TRACE_EVENT_COPY_ASYNC_BEGIN1,
  CEF_TRACE_EVENT_COPY_ASYNC_BEGIN2,
  CEF_TRACE_EVENT_ASYNC_STEP0,
  CEF_TRACE_EVENT_ASYNC_STEP1,
  CEF_TRACE_EVENT_COPY_ASYNC_STEP0,
  CEF_TRACE_EVENT_COPY_ASYNC_STEP1,
  CEF_TRACE_EVENT_ASYNC_END0,
  CEF_TRACE_EVENT_ASYNC_END1,
  CEF_TRACE_EVENT_ASYNC_END2,
  CEF_TRACE_EVENT_COPY_ASYNC_END0,
  CEF_TRACE_EVENT_COPY_ASYNC_END1,
  CEF_TRACE_EVENT_COPY_ASYNC_END2
};

const char kTraceTestCategory[] = "test_category";

// Used to test begin tracing on the UI thread.
class BeginTracingTask : public CefTask {
 public:
  explicit BeginTracingTask(CefRefPtr<CefTraceClient> client)
      : client_(client) {
  }

  virtual void Execute(CefThreadId threadId) OVERRIDE {
    EXPECT_TRUE(CefBeginTracing(client_, kTraceTestCategory));
  }

 private:
  virtual ~BeginTracingTask() {}

  CefRefPtr<CefTraceClient> client_;

  IMPLEMENT_REFCOUNTING(BeginTracingTask);
};

// Used to test end tracing on the UI thread.
class EndTracingTask : public CefTask {
 public:
  EndTracingTask() {}

  virtual void Execute(CefThreadId threadId) OVERRIDE {
    EXPECT_TRUE(CefEndTracingAsync());
  }

 private:
  virtual ~EndTracingTask() {}

  IMPLEMENT_REFCOUNTING(EndTracingTask);
};

class TracingTestHandler : public CefTraceClient {
 public:
  TracingTestHandler(TracingTestType type, const char* trace_type)
      : completion_event_(true, false),
        trace_type_(trace_type),
        type_(type) {
  }

  virtual void OnTraceDataCollected(const char* fragment,
                                    size_t fragment_size) OVERRIDE {
    if (!trace_data_.empty())
      trace_data_.append(",");
    trace_data_.append(fragment, fragment_size);
  }

  virtual void OnEndTracingComplete() OVERRIDE {
    EXPECT_TRUE(!trace_data_.empty());
    EXPECT_TRUE(trace_type_ != NULL);
    EXPECT_TRUE(strstr(trace_data_.c_str(), trace_type_) != NULL);
    completion_event_.Signal();
  }

  void RunTest() {
    // BeginTracing works only on the UI thread.
    CefPostTask(TID_UI, new BeginTracingTask(this));
    WaitForUIThread();

    switch (type_) {
      case CEF_TRACE_EVENT0:
        CEF_TRACE_EVENT0(kTraceTestCategory, "CEF_TRACE_EVENT0");
        break;
      case CEF_TRACE_EVENT1:
        CEF_TRACE_EVENT1(kTraceTestCategory, "CEF_TRACE_EVENT1", "arg1", 1);
        break;
      case CEF_TRACE_EVENT2:
        CEF_TRACE_EVENT2(kTraceTestCategory, "CEF_TRACE_EVENT2", "arg1", 1,
                         "arg2", 2);
        break;
      case CEF_TRACE_EVENT_INSTANT0:
        CEF_TRACE_EVENT_INSTANT0(kTraceTestCategory,
                                 "CEF_TRACE_EVENT_INSTANT0");
        break;
      case CEF_TRACE_EVENT_INSTANT1:
        CEF_TRACE_EVENT_INSTANT1(kTraceTestCategory, "CEF_TRACE_EVENT_INSTANT1",
                                 "arg1", 1);
        break;
      case CEF_TRACE_EVENT_INSTANT2:
        CEF_TRACE_EVENT_INSTANT2(kTraceTestCategory, "CEF_TRACE_EVENT_INSTANT2",
                                 "arg1", 1, "arg2", 2);
        break;
      case CEF_TRACE_EVENT_COPY_INSTANT0:
        CEF_TRACE_EVENT_COPY_INSTANT0(kTraceTestCategory,
                                      "CEF_TRACE_EVENT_COPY_INSTANT0");
        break;
      case CEF_TRACE_EVENT_COPY_INSTANT1:
        CEF_TRACE_EVENT_COPY_INSTANT1(kTraceTestCategory,
                                      "CEF_TRACE_EVENT_COPY_INSTANT1",
                                      "arg1", 1);
        break;
      case CEF_TRACE_EVENT_COPY_INSTANT2:
        CEF_TRACE_EVENT_COPY_INSTANT2(kTraceTestCategory,
                                      "CEF_TRACE_EVENT_COPY_INSTANT2",
                                      "arg1", 1, "arg2", 2);
        break;
      case CEF_TRACE_EVENT_BEGIN0:
        CEF_TRACE_EVENT_BEGIN0(kTraceTestCategory, "CEF_TRACE_EVENT_BEGIN0");
        break;
      case CEF_TRACE_EVENT_BEGIN1:
        CEF_TRACE_EVENT_BEGIN1(kTraceTestCategory, "CEF_TRACE_EVENT_BEGIN1",
                               "arg1", 1);
        break;
      case CEF_TRACE_EVENT_BEGIN2:
        CEF_TRACE_EVENT_BEGIN2(kTraceTestCategory, "CEF_TRACE_EVENT_BEGIN2",
                               "arg1", 1, "arg2", 2);
        break;
      case CEF_TRACE_EVENT_COPY_BEGIN0:
        CEF_TRACE_EVENT_COPY_BEGIN0(kTraceTestCategory,
                                    "CEF_TRACE_EVENT_COPY_BEGIN0");
        break;
      case CEF_TRACE_EVENT_COPY_BEGIN1:
        CEF_TRACE_EVENT_COPY_BEGIN1(kTraceTestCategory,
                                    "CEF_TRACE_EVENT_COPY_BEGIN1",
                                    "arg1", 1);
        break;
      case CEF_TRACE_EVENT_COPY_BEGIN2:
        CEF_TRACE_EVENT_COPY_BEGIN2(kTraceTestCategory,
                                    "CEF_TRACE_EVENT_COPY_BEGIN2",
                                    "arg1", 1, "arg2", 2);
        break;
      case CEF_TRACE_EVENT_END0:
        CEF_TRACE_EVENT_END0(kTraceTestCategory, "CEF_TRACE_EVENT_END0");
        break;
      case CEF_TRACE_EVENT_END1:
        CEF_TRACE_EVENT_END1(kTraceTestCategory, "CEF_TRACE_EVENT_END1",
                             "arg1", 1);
        break;
      case CEF_TRACE_EVENT_END2:
        CEF_TRACE_EVENT_END2(kTraceTestCategory, "CEF_TRACE_EVENT_END2",
                             "arg1", 1, "arg2", 2);
        break;
      case CEF_TRACE_EVENT_COPY_END0:
        CEF_TRACE_EVENT_COPY_END0(kTraceTestCategory,
                                  "CEF_TRACE_EVENT_COPY_END0");
        break;
      case CEF_TRACE_EVENT_COPY_END1:
        CEF_TRACE_EVENT_COPY_END1(kTraceTestCategory,
                                  "CEF_TRACE_EVENT_COPY_END1", "arg1", 1);
        break;
      case CEF_TRACE_EVENT_COPY_END2:
        CEF_TRACE_EVENT_COPY_END2(kTraceTestCategory,
                                  "CEF_TRACE_EVENT_COPY_END2", "arg1", 1,
                                  "arg2", 2);
        break;
      case CEF_TRACE_EVENT_IF_LONGER_THAN0:
        CEF_TRACE_EVENT_IF_LONGER_THAN0(0, kTraceTestCategory,
                                        "CEF_TRACE_EVENT_IF_LONGER_THAN0");
        break;
      case CEF_TRACE_EVENT_IF_LONGER_THAN1:
        CEF_TRACE_EVENT_IF_LONGER_THAN1(0, kTraceTestCategory,
                                        "CEF_TRACE_EVENT_IF_LONGER_THAN1",
                                        "arg1", 1);
      case CEF_TRACE_EVENT_IF_LONGER_THAN2:
        CEF_TRACE_EVENT_IF_LONGER_THAN2(0, kTraceTestCategory,
                                        "CEF_TRACE_EVENT_IF_LONGER_THAN2",
                                        "arg1", 1, "arg2", 2);
        break;
      case CEF_TRACE_COUNTER1:
        CEF_TRACE_COUNTER1(kTraceTestCategory, "CEF_TRACE_COUNTER1", 5);
        break;
      case CEF_TRACE_COPY_COUNTER1:
        CEF_TRACE_COPY_COUNTER1(kTraceTestCategory, "CEF_TRACE_COPY_COUNTER1",
                                5);
        break;
      case CEF_TRACE_COUNTER2:
        CEF_TRACE_COUNTER2(kTraceTestCategory, "CEF_TRACE_COUNTER2", "val1", 5,
                           "val2", 10);
        break;
      case CEF_TRACE_COPY_COUNTER2:
        CEF_TRACE_COPY_COUNTER2(kTraceTestCategory, "CEF_TRACE_COPY_COUNTER2",
                                "val1", 5, "val2", 10);
        break;
      case CEF_TRACE_COUNTER_ID1:
        CEF_TRACE_COUNTER_ID1(kTraceTestCategory, "CEF_TRACE_COUNTER_ID1", 100,
                              5);
        break;
      case CEF_TRACE_COPY_COUNTER_ID1:
        CEF_TRACE_COPY_COUNTER_ID1(kTraceTestCategory,
                                   "CEF_TRACE_COPY_COUNTER_ID1", 100, 5);
        break;
      case CEF_TRACE_COUNTER_ID2:
        CEF_TRACE_COUNTER_ID2(kTraceTestCategory, "CEF_TRACE_COUNTER_ID2", 100,
                              "val1", 5, "val2", 10);
        break;
      case CEF_TRACE_COPY_COUNTER_ID2:
        CEF_TRACE_COPY_COUNTER_ID2(kTraceTestCategory,
                                   "CEF_TRACE_COPY_COUNTER_ID2", 100,
                                   "val1", 5, "val2", 10);
        break;
      case CEF_TRACE_EVENT_ASYNC_BEGIN0:
        CEF_TRACE_EVENT_ASYNC_BEGIN0(kTraceTestCategory,
                                     "CEF_TRACE_EVENT_ASYNC_BEGIN0", 100);
        break;
      case CEF_TRACE_EVENT_ASYNC_BEGIN1:
        CEF_TRACE_EVENT_ASYNC_BEGIN1(kTraceTestCategory,
                                     "CEF_TRACE_EVENT_ASYNC_BEGIN1", 100,
                                     "arg1", 1);
        break;
      case CEF_TRACE_EVENT_ASYNC_BEGIN2:
        CEF_TRACE_EVENT_ASYNC_BEGIN2(kTraceTestCategory,
                                     "CEF_TRACE_EVENT_ASYNC_BEGIN2",
                                     100, "arg1", 1, "arg2", 2);
        break;
      case CEF_TRACE_EVENT_COPY_ASYNC_BEGIN0:
        CEF_TRACE_EVENT_COPY_ASYNC_BEGIN0(kTraceTestCategory,
                                          "CEF_TRACE_EVENT_COPY_ASYNC_BEGIN0",
                                          100);
        break;
      case CEF_TRACE_EVENT_COPY_ASYNC_BEGIN1:
        CEF_TRACE_EVENT_COPY_ASYNC_BEGIN1(kTraceTestCategory,
                                          "CEF_TRACE_EVENT_COPY_ASYNC_BEGIN1",
                                          100, "arg1", 1);
        break;
      case CEF_TRACE_EVENT_COPY_ASYNC_BEGIN2:
        CEF_TRACE_EVENT_COPY_ASYNC_BEGIN2(kTraceTestCategory,
                                          "CEF_TRACE_EVENT_COPY_ASYNC_BEGIN2",
                                          100, "arg1", 1, "arg2", 2);
        break;
      case CEF_TRACE_EVENT_ASYNC_STEP0:
        CEF_TRACE_EVENT_ASYNC_STEP0(kTraceTestCategory,
                                    "CEF_TRACE_EVENT_ASYNC_STEP0", 100,
                                    1000);
        break;
      case CEF_TRACE_EVENT_ASYNC_STEP1:
        CEF_TRACE_EVENT_ASYNC_STEP1(kTraceTestCategory,
                                    "CEF_TRACE_EVENT_ASYNC_STEP1", 100,
                                    1000, "arg1", 1);
        break;
      case CEF_TRACE_EVENT_COPY_ASYNC_STEP0:
        CEF_TRACE_EVENT_COPY_ASYNC_STEP0(kTraceTestCategory,
                                         "CEF_TRACE_EVENT_COPY_ASYNC_STEP0",
                                         100, 1000);
        break;
      case CEF_TRACE_EVENT_COPY_ASYNC_STEP1:
        CEF_TRACE_EVENT_COPY_ASYNC_STEP1(kTraceTestCategory,
                                         "CEF_TRACE_EVENT_COPY_ASYNC_STEP1",
                                         100, 1000, "arg1", 1);
        break;
      case CEF_TRACE_EVENT_ASYNC_END0:
        CEF_TRACE_EVENT_ASYNC_END0(kTraceTestCategory,
                                   "CEF_TRACE_EVENT_ASYNC_END0", 100);
        break;
      case CEF_TRACE_EVENT_ASYNC_END1:
        CEF_TRACE_EVENT_ASYNC_END1(kTraceTestCategory,
                                   "CEF_TRACE_EVENT_ASYNC_END1", 100,
                                   "arg1", 1);
        break;
      case CEF_TRACE_EVENT_ASYNC_END2:
        CEF_TRACE_EVENT_ASYNC_END2(kTraceTestCategory,
                                   "CEF_TRACE_EVENT_ASYNC_END2", 100,
                                   "arg1", 1, "arg2", 2);
      case CEF_TRACE_EVENT_COPY_ASYNC_END0:
        CEF_TRACE_EVENT_COPY_ASYNC_END0(kTraceTestCategory,
                                        "CEF_TRACE_EVENT_COPY_ASYNC_END0",
                                        100);
        break;
      case CEF_TRACE_EVENT_COPY_ASYNC_END1:
        CEF_TRACE_EVENT_COPY_ASYNC_END1(kTraceTestCategory,
                                        "CEF_TRACE_EVENT_COPY_ASYNC_END1",
                                        100, "arg1", 1);
        break;
      case CEF_TRACE_EVENT_COPY_ASYNC_END2:
        CEF_TRACE_EVENT_COPY_ASYNC_END2(kTraceTestCategory,
                                        "CEF_TRACE_EVENT_COPY_ASYNC_END2",
                                        100, "arg1", 1, "arg2", 2);
        break;
    }

    // Run EndTracingAsync on the UI thread.
    CefPostTask(TID_UI, new EndTracingTask());
    WaitForUIThread();
  }

  void ExecuteTest() {
    // Run the test.
    RunTest();

    // Wait for the test to complete.
    completion_event_.Wait();
  }

 private:
  virtual ~TracingTestHandler() {}

  // Handle used to notify when the test is complete.
  base::WaitableEvent completion_event_;

  const char* trace_type_;
  TracingTestType type_;
  std::string trace_data_;

  IMPLEMENT_REFCOUNTING(TracingTestHandler);
};

// Helper for defining tracing tests.
#define TRACING_TEST(name, test_type) \
    TEST(TracingTest, name) { \
      CefRefPtr<TracingTestHandler> handler = \
          new TracingTestHandler(test_type, #test_type); \
      handler->ExecuteTest(); \
    }

// Define the tests.
TRACING_TEST(TraceEvent0, CEF_TRACE_EVENT0);
TRACING_TEST(TraceEvent1, CEF_TRACE_EVENT1);
TRACING_TEST(TraceEvent2, CEF_TRACE_EVENT2);
TRACING_TEST(TraceEventInstant0, CEF_TRACE_EVENT_INSTANT0);
TRACING_TEST(TraceEventInstant1, CEF_TRACE_EVENT_INSTANT1);
TRACING_TEST(TraceEventInstant2, CEF_TRACE_EVENT_INSTANT2);
TRACING_TEST(TraceEventCopyInstant0, CEF_TRACE_EVENT_COPY_INSTANT0);
TRACING_TEST(TraceEventCopyInstant1, CEF_TRACE_EVENT_COPY_INSTANT1);
TRACING_TEST(TraceEventCopyInstant2, CEF_TRACE_EVENT_COPY_INSTANT2);
TRACING_TEST(TraceEventBegin0, CEF_TRACE_EVENT_BEGIN0);
TRACING_TEST(TraceEventBegin1, CEF_TRACE_EVENT_BEGIN1);
TRACING_TEST(TraceEventBegin2, CEF_TRACE_EVENT_BEGIN2);
TRACING_TEST(TraceEventCopyBegin0, CEF_TRACE_EVENT_COPY_BEGIN0);
TRACING_TEST(TraceEventCopyBegin1, CEF_TRACE_EVENT_COPY_BEGIN1);
TRACING_TEST(TraceEventCopyBegin2, CEF_TRACE_EVENT_COPY_BEGIN2);
TRACING_TEST(TraceEventEnd0, CEF_TRACE_EVENT_END0);
TRACING_TEST(TraceEventEnd1, CEF_TRACE_EVENT_END1);
TRACING_TEST(TraceEventEnd2, CEF_TRACE_EVENT_END2);
TRACING_TEST(TraceEventCopyEnd0, CEF_TRACE_EVENT_COPY_END0);
TRACING_TEST(TraceEventCopyEnd1, CEF_TRACE_EVENT_COPY_END1);
TRACING_TEST(TraceEventCopyEnd2, CEF_TRACE_EVENT_COPY_END1);
TRACING_TEST(TraceEventIfLongerThan0, CEF_TRACE_EVENT_IF_LONGER_THAN0);
TRACING_TEST(TraceEventIfLongerThan1, CEF_TRACE_EVENT_IF_LONGER_THAN1);
TRACING_TEST(TraceEventIfLongerThan2, CEF_TRACE_EVENT_IF_LONGER_THAN2);
TRACING_TEST(TraceCounter1, CEF_TRACE_COUNTER1);
TRACING_TEST(TraceCopyCounter1, CEF_TRACE_COPY_COUNTER1);
TRACING_TEST(TraceCounter2, CEF_TRACE_COUNTER2);
TRACING_TEST(TraceCopyCounter2, CEF_TRACE_COPY_COUNTER2);
TRACING_TEST(TraceCounterId1, CEF_TRACE_COUNTER_ID1);
TRACING_TEST(TraceCopyCounterId1, CEF_TRACE_COPY_COUNTER_ID1);
TRACING_TEST(TraceCounterId2, CEF_TRACE_COUNTER_ID2);
TRACING_TEST(TraceCopyCounterId2, CEF_TRACE_COPY_COUNTER_ID1);
TRACING_TEST(TraceTraceEventAsyncBegin0, CEF_TRACE_EVENT_ASYNC_BEGIN0);
TRACING_TEST(TraceTraceEventAsyncBegin1, CEF_TRACE_EVENT_ASYNC_BEGIN1);
TRACING_TEST(TraceTraceEventAsyncBegin2, CEF_TRACE_EVENT_ASYNC_BEGIN2);
TRACING_TEST(TraceTraceEventCopyAsyncBegin0,
             CEF_TRACE_EVENT_COPY_ASYNC_BEGIN0);
TRACING_TEST(TraceTraceEventCopyAsyncBegin1,
             CEF_TRACE_EVENT_COPY_ASYNC_BEGIN1);
TRACING_TEST(TraceTraceEventCopyAsyncBegin2,
             CEF_TRACE_EVENT_COPY_ASYNC_BEGIN2);
TRACING_TEST(TraceTraceEventAsyncStep0, CEF_TRACE_EVENT_ASYNC_STEP0);
TRACING_TEST(TraceTraceEventAsyncStep1, CEF_TRACE_EVENT_ASYNC_STEP1);
TRACING_TEST(TraceEventCopyAsyncStep0, CEF_TRACE_EVENT_COPY_ASYNC_STEP0);
TRACING_TEST(TraceEventCopyAsyncStep1, CEF_TRACE_EVENT_COPY_ASYNC_STEP1);
TRACING_TEST(TraceEventAsyncEnd0, CEF_TRACE_EVENT_ASYNC_END0);
TRACING_TEST(TraceEventAsyncEnd1, CEF_TRACE_EVENT_ASYNC_END1);
TRACING_TEST(TraceEventAsyncEnd2, CEF_TRACE_EVENT_ASYNC_END2);
TRACING_TEST(TraceEventCopyAsyncEnd0, CEF_TRACE_EVENT_COPY_ASYNC_END0);
