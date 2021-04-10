// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_TEST_UTIL_H_
#define CEF_TESTS_CEFTESTS_TEST_UTIL_H_
#pragma once

#include "include/cef_process_message.h"
#include "include/cef_request.h"
#include "include/cef_request_context.h"
#include "include/cef_response.h"
#include "include/cef_values.h"
#include "tests/ceftests/test_suite.h"

// Test that CefRequest::HeaderMap objects are equal. Multiple values with the
// same key are allowed, but not duplicate entries with the same key/value. If
// |allowExtras| is true then additional header fields will be allowed in
// |map2|.
void TestMapEqual(const CefRequest::HeaderMap& map1,
                  const CefRequest::HeaderMap& map2,
                  bool allowExtras);

// Test that the CefRequest::HeaderMap object contains no duplicate entries.
void TestMapNoDuplicates(const CefRequest::HeaderMap& map);

// Test that CefPostDataElement objects are equal
void TestPostDataElementEqual(CefRefPtr<CefPostDataElement> elem1,
                              CefRefPtr<CefPostDataElement> elem2);

// Test that CefPostData objects are equal
void TestPostDataEqual(CefRefPtr<CefPostData> postData1,
                       CefRefPtr<CefPostData> postData2);

// Test that CefRequest objects are equal
// If |allowExtras| is true then additional header fields will be allowed in
// |request2|.
void TestRequestEqual(CefRefPtr<CefRequest> request1,
                      CefRefPtr<CefRequest> request2,
                      bool allowExtras);

// Test that CefResponse objects are equal
// If |allowExtras| is true then additional header fields will be allowed in
// |response2|.
void TestResponseEqual(CefRefPtr<CefResponse> response1,
                       CefRefPtr<CefResponse> response2,
                       bool allowExtras);

// Test if two binary values are equal.
void TestBinaryEqual(CefRefPtr<CefBinaryValue> val1,
                     CefRefPtr<CefBinaryValue> val2);

// Test if two list values are equal.
void TestListEqual(CefRefPtr<CefListValue> val1, CefRefPtr<CefListValue> val2);

// Test if two dictionary values are equal.
void TestDictionaryEqual(CefRefPtr<CefDictionaryValue> val1,
                         CefRefPtr<CefDictionaryValue> val2);

// Test if two process message values are equal.
void TestProcessMessageEqual(CefRefPtr<CefProcessMessage> val1,
                             CefRefPtr<CefProcessMessage> val2);

// Test if two CefString vectors are equal.
void TestStringVectorEqual(const std::vector<CefString>& val1,
                           const std::vector<CefString>& val2);

enum TestRequestContextMode {
  TEST_RC_MODE_NONE,
  TEST_RC_MODE_GLOBAL,
  TEST_RC_MODE_GLOBAL_WITH_HANDLER,
  TEST_RC_MODE_CUSTOM,
  TEST_RC_MODE_CUSTOM_WITH_HANDLER,
};

inline bool IsTestRequestContextModeCustom(TestRequestContextMode mode) {
  return mode == TEST_RC_MODE_CUSTOM ||
         mode == TEST_RC_MODE_CUSTOM_WITH_HANDLER;
}

// Returns true if the old CefResourceHandler API should be tested.
bool TestOldResourceAPI();

// Returns true if the Chrome runtime is enabled.
bool IsChromeRuntimeEnabled();

// Returns true if requests for |url| should be ignored by tests.
bool IgnoreURL(const std::string& url);

// Return a RequestContext object matching the specified |mode|.
// |cache_path| may be specified for CUSTOM modes.
// Use the RC_TEST_GROUP_BASE macro to test all valid combinations.
CefRefPtr<CefRequestContext> CreateTestRequestContext(
    TestRequestContextMode mode,
    const std::string& cache_path);

// Helper macro for testing a single RequestContextMode value.
// See RC_TEST_GROUP_ALL documentation for example usage.
#define RC_TEST_BASE(test_case_name, test_name, test_class, test_mode, \
                     rc_mode, with_cache_path)                         \
  TEST(test_case_name, test_name) {                                    \
    CefScopedTempDir scoped_temp_dir;                                  \
    std::string cache_path;                                            \
    if (with_cache_path) {                                             \
      EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDirUnderPath(        \
          CefTestSuite::GetInstance()->root_cache_path()));            \
      cache_path = scoped_temp_dir.GetPath();                          \
    }                                                                  \
    CefRefPtr<test_class> handler =                                    \
        new test_class(test_class::test_mode, rc_mode, cache_path);    \
    handler->ExecuteTest();                                            \
    ReleaseAndWaitForDestructor(handler);                              \
    if (!scoped_temp_dir.IsEmpty()) {                                  \
      scoped_temp_dir.Take();                                          \
    }                                                                  \
  }

// RequestContextModes that operate in memory.
#define RC_TEST_GROUP_IN_MEMORY(test_case_name, test_name, test_class,     \
                                test_mode)                                 \
  RC_TEST_BASE(test_case_name, test_name##RCNone, test_class, test_mode,   \
               TEST_RC_MODE_NONE, false)                                   \
  RC_TEST_BASE(test_case_name, test_name##RCGlobal, test_class, test_mode, \
               TEST_RC_MODE_GLOBAL, false)                                 \
  RC_TEST_BASE(test_case_name, test_name##RCGlobalWithHandler, test_class, \
               test_mode, TEST_RC_MODE_GLOBAL_WITH_HANDLER, false)         \
  RC_TEST_BASE(test_case_name, test_name##RCCustomInMemory, test_class,    \
               test_mode, TEST_RC_MODE_CUSTOM, false)                      \
  RC_TEST_BASE(test_case_name, test_name##RCCustomInMemoryWithHandler,     \
               test_class, test_mode, TEST_RC_MODE_CUSTOM_WITH_HANDLER, false)

// RequestContextModes that operate on disk.
#define RC_TEST_GROUP_ON_DISK(test_case_name, test_name, test_class,  \
                              test_mode)                              \
  RC_TEST_BASE(test_case_name, test_name##RCCustomOnDisk, test_class, \
               test_mode, TEST_RC_MODE_CUSTOM, true)                  \
  RC_TEST_BASE(test_case_name, test_name##RCCustomOnDiskWithHandler,  \
               test_class, test_mode, TEST_RC_MODE_CUSTOM_WITH_HANDLER, true)

// Helper macro for testing all valid combinations of RequestContextMode values.
// For example:
//
//   // Test handler implementation.
//   class MyTestHandler : public TestHandler {
//    public:
//     // Test modes supported by MyTestHandler.
//     enum TestMode {
//       FIRST,
//       SECOND,
//     };
//
//     // Constructor always accepts three arguments.
//     MyTestHandler(TestMode test_mode,
//                   TestRequestContextMode rc_mode,
//                   const std::string& rc_cache_path)
//         : test_mode_(test_mode), rc_mode_(rc_mode),
//           rc_cache_path_(rc_cache_path) {}
//
//     void RunTest() override {
//        // Create a RequestContext with the specified attributes.
//        CefRefPtr<CefRequestContext> request_context =
//            CreateTestRequestContext(rc_mode_, rc_cache_path_);
//
//        // Do something with |test_mode_| and |request_context|...
//     }
//
//    private:
//     const TestMode test_mode_;
//     const TestRequestContextMode rc_mode_;
//     const std::string rc_cache_path_;
//
//     IMPLEMENT_REFCOUNTING(MyTestHandler);
//   };
//
//   // Helper macro for defining tests using MyTestHandler.
//   #define MY_TEST_GROUP(test_name, test_mode) \
//     RC_TEST_GROUP_ALL(MyTest, test_name, MyTestHandler, test_mode)
//
//   // Implementation for MyTest.First* tests.
//   MY_TEST_GROUP(First, FIRST);
//   // Implementation for MyTest.Second* tests.
//   MY_TEST_GROUP(Second, SECOND);
//
#define RC_TEST_GROUP_ALL(test_case_name, test_name, test_class, test_mode) \
  RC_TEST_GROUP_IN_MEMORY(test_case_name, test_name, test_class, test_mode) \
  RC_TEST_GROUP_ON_DISK(test_case_name, test_name, test_class, test_mode)

#endif  // CEF_TESTS_CEFTESTS_TEST_UTIL_H_
