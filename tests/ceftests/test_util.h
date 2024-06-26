// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_TEST_UTIL_H_
#define CEF_TESTS_CEFTESTS_TEST_UTIL_H_
#pragma once

#include <optional>

#include "include/base/cef_callback.h"
#include "include/cef_process_message.h"
#include "include/cef_request.h"
#include "include/cef_request_context.h"
#include "include/cef_response.h"
#include "include/cef_values.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "tests/ceftests/test_suite.h"

CefTime CefTimeFrom(CefBaseTime value);
CefBaseTime CefBaseTimeFrom(const CefTime& value);

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
  TEST_RC_MODE_CUSTOM_WITH_HANDLER,
};

inline bool IsTestRequestContextModeCustom(TestRequestContextMode mode) {
  return mode == TEST_RC_MODE_CUSTOM_WITH_HANDLER;
}

// Returns true if the old CefResourceHandler API should be tested.
bool TestOldResourceAPI();

// Returns true if Views should be used as a the global default.
bool UseViewsGlobal();

// Returns true if Alloy style browser should be used as the global default.
bool UseAlloyStyleBrowserGlobal();

// Returns true if Alloy style window should be used as the global default.
// Only used in combination with Views.
bool UseAlloyStyleWindowGlobal();

// Determine the Views window title based on the style of |window| and
// optionally |browser_view|.
std::string ComputeViewsWindowTitle(CefRefPtr<CefWindow> window,
                                    CefRefPtr<CefBrowserView> browser_view);

// Determine the native window title based on |use_alloy_style|.
std::string ComputeNativeWindowTitle(bool use_alloy_style);

// Returns true if BFCache is enabled.
bool IsBFCacheEnabled();

// Returns true if same-site BFCache is enabled.
bool IsSameSiteBFCacheEnabled();

// Returns true if requests for |url| should be ignored by tests.
bool IgnoreURL(const std::string& url);

// Returns |timeout_ms| as scaled by the current configuration, or std::nullopt
// if timeouts are disabled.
std::optional<int> GetConfiguredTestTimeout(int timeout_ms);

// Send a mouse click event with the necessary delay to avoid having events
// dropped or rate limited.
void SendMouseClickEvent(CefRefPtr<CefBrowser> browser,
                         const CefMouseEvent& mouse_event,
                         cef_mouse_button_type_t mouse_button_type = MBT_LEFT);

// Allow |parent_url| to create popups that bypass the popup blocker. If
// |parent_url| is empty the default value will be configured.
void GrantPopupPermission(CefRefPtr<CefRequestContext> request_context,
                          const std::string& parent_url);

// Create a CefRequestContext object matching the specified |mode|. |cache_path|
// may be specified for CUSTOM modes. |init_callback| will be executed
// asynchronously on the UI thread. Use the RC_TEST_GROUP_ALL macro to test all
// valid combinations.
using RCInitCallback = base::OnceCallback<void(CefRefPtr<CefRequestContext>)>;
void CreateTestRequestContext(TestRequestContextMode mode,
                              const std::string& cache_path,
                              RCInitCallback init_callback);

// Run a single test without additional test modes.
#define RC_TEST_SINGLE(test_case_name, test_name, test_class, rc_mode,   \
                       with_cache_path)                                  \
  TEST(test_case_name, test_name) {                                      \
    CefScopedTempDir scoped_temp_dir;                                    \
    std::string cache_path;                                              \
    if (with_cache_path) {                                               \
      EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDirUnderPath(          \
          CefTestSuite::GetInstance()->root_cache_path()));              \
      cache_path = scoped_temp_dir.GetPath();                            \
    }                                                                    \
    CefRefPtr<test_class> handler = new test_class(rc_mode, cache_path); \
    handler->ExecuteTest();                                              \
    ReleaseAndWaitForDestructor(handler);                                \
    if (!scoped_temp_dir.IsEmpty()) {                                    \
      scoped_temp_dir.Take();                                            \
    }                                                                    \
  }

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
  RC_TEST_BASE(test_case_name, test_name##RCCustomInMemoryWithHandler,     \
               test_class, test_mode, TEST_RC_MODE_CUSTOM_WITH_HANDLER, false)

// RequestContextModes that operate on disk.
#define RC_TEST_GROUP_ON_DISK(test_case_name, test_name, test_class, \
                              test_mode)                             \
  RC_TEST_BASE(test_case_name, test_name##RCCustomOnDiskWithHandler, \
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
//        CreateTestRequestContext(rc_mode_, rc_cache_path_,
//            base::BindOnce(&MyTestHandler::RunTestContinue, this));
//     }
//
//     void RunTestContinue(CefRefPtr<CefRequestContext> request_context) {
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
