// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/message_router_unittest_utils.h"

namespace {

const char kSingleQueryRequest[] = "request_context";
const char kSingleQueryResponse[] = "success_response";
const int kSingleQueryErrorCode = 5;
const char kSingleQueryErrorMessage[] = "error_message";

// Test a single query in a single page load.
class SingleQueryTestHandler : public SingleLoadTestHandler {
 public:
  enum TestType {
    SUCCESS,
    FAILURE,
    CANCEL,
  };

  SingleQueryTestHandler(TestType type, bool sync_callback)
      : test_type_(type), sync_callback_(sync_callback) {}

  std::string GetMainHTML() override {
    std::string html;

    std::stringstream ss;
    ss << kSingleQueryErrorCode;
    const std::string& errorCodeStr = ss.str();

    html =
        "<html><body><script>\n"
        // No requests should exist.
        "window.mrtAssertTotalCount(" LINESTR
        ",0);\n"
        "window.mrtAssertBrowserCount(" LINESTR
        ",0);\n"
        "window.mrtAssertContextCount(" LINESTR
        ",0);\n"
        // Send the query.
        "var request_id = window.mrtQuery({\n"
        "  request: '" +
        std::string(kSingleQueryRequest) +
        "',\n"
        "  persistent: false,\n"
        "  onSuccess: function(response) {\n"
        // Request should be removed before callback is executed.
        "    window.mrtAssertTotalCount(" LINESTR
        ",0);\n"
        "    window.mrtAssertBrowserCount(" LINESTR
        ",0);\n"
        "    window.mrtAssertContextCount(" LINESTR
        ",0);\n"
        "    if (response == '" +
        std::string(kSingleQueryResponse) +
        "')\n"
        "      window.mrtNotify('success');\n"
        "    else\n"
        "      window.mrtNotify('error-onSuccess');\n"
        "  },\n"
        "  onFailure: function(error_code, error_message) {\n"
        // Request should be removed before callback is executed.
        "    window.mrtAssertTotalCount(" LINESTR
        ",0);\n"
        "    window.mrtAssertBrowserCount(" LINESTR
        ",0);\n"
        "    window.mrtAssertContextCount(" LINESTR
        ",0);\n"
        "    if (error_code == " +
        errorCodeStr + " && error_message == '" +
        std::string(kSingleQueryErrorMessage) +
        "')\n"
        "      window.mrtNotify('failure');\n"
        "    else\n"
        "      window.mrtNotify('error-onFailure');\n"
        "  }\n"
        "});\n"
        // Request should exist.
        "window.mrtAssertTotalCount(" LINESTR
        ",1);\n"
        "window.mrtAssertBrowserCount(" LINESTR
        ",1);\n"
        "window.mrtAssertContextCount(" LINESTR ",1);\n";

    if (test_type_ == CANCEL) {
      html +=
          "window.mrtQueryCancel(request_id);\n"
          // Request should be removed immediately.
          "window.mrtAssertTotalCount(" LINESTR
          ",0);\n"
          "window.mrtAssertBrowserCount(" LINESTR
          ",0);\n"
          "window.mrtAssertContextCount(" LINESTR
          ",0);\n"
          "window.mrtNotify('cancel');\n";
    }

    html += "</script></body></html>";
    return html;
  }

  void OnNotify(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                const std::string& message) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);

    // OnNotify only be called once.
    EXPECT_FALSE(got_notify_);
    got_notify_.yes();

    if (test_type_ == SUCCESS) {
      EXPECT_STREQ("success", message.c_str());
    } else if (test_type_ == FAILURE) {
      EXPECT_STREQ("failure", message.c_str());
    } else if (test_type_ == CANCEL) {
      EXPECT_STREQ("cancel", message.c_str());
    }

    DestroyTestIfDone();
  }

  void ExecuteCallback() {
    EXPECT_TRUE(callback_.get());
    if (test_type_ == SUCCESS) {
      callback_->Success(kSingleQueryResponse);
    } else if (test_type_ == FAILURE) {
      callback_->Failure(kSingleQueryErrorCode, kSingleQueryErrorMessage);
    } else {
      ADD_FAILURE();  // Not reached.
    }
    callback_ = nullptr;
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);
    EXPECT_NE(0, query_id);
    EXPECT_FALSE(persistent);
    EXPECT_STREQ(kSingleQueryRequest, request.ToString().c_str());

    got_on_query_.yes();

    query_id_ = query_id;
    callback_ = callback;

    if (test_type_ == SUCCESS || test_type_ == FAILURE) {
      if (sync_callback_) {
        ExecuteCallback();
      } else {
        CefPostTask(
            TID_UI,
            base::BindOnce(&SingleQueryTestHandler::ExecuteCallback, this));
      }
    }

    return true;
  }

  void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64_t query_id) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);
    EXPECT_EQ(test_type_, CANCEL);
    EXPECT_EQ(query_id_, query_id);
    EXPECT_TRUE(got_on_query_);
    EXPECT_TRUE(callback_.get());

    got_on_query_canceled_.yes();
    callback_ = nullptr;

    DestroyTestIfDone();
  }

  void DestroyTestIfDone() {
    bool destroy_test = false;
    if (test_type_ == CANCEL) {
      destroy_test = got_notify_ && got_on_query_canceled_;
    } else {
      destroy_test = got_notify_;
    }
    if (destroy_test) {
      DestroyTest();
    }
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_notify_);
    EXPECT_TRUE(got_on_query_);
    EXPECT_FALSE(callback_.get());

    if (test_type_ == CANCEL) {
      EXPECT_TRUE(got_on_query_canceled_);
    } else {
      EXPECT_FALSE(got_on_query_canceled_);
    }

    TestHandler::DestroyTest();
  }

 private:
  const TestType test_type_;
  const bool sync_callback_;

  int64_t query_id_ = 0;
  CefRefPtr<Callback> callback_;

  TrackCallback got_on_query_;
  TrackCallback got_on_query_canceled_;
  TrackCallback got_notify_;
};

}  // namespace

// Test that a single query with successful result delivered synchronously.
TEST(MessageRouterTest, SingleQuerySuccessSyncCallback) {
  CefRefPtr<SingleQueryTestHandler> handler =
      new SingleQueryTestHandler(SingleQueryTestHandler::SUCCESS, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that a single query with successful result delivered asynchronously.
TEST(MessageRouterTest, SingleQuerySuccessAsyncCallback) {
  CefRefPtr<SingleQueryTestHandler> handler =
      new SingleQueryTestHandler(SingleQueryTestHandler::SUCCESS, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that a single query with failure result delivered synchronously.
TEST(MessageRouterTest, SingleQueryFailureSyncCallback) {
  CefRefPtr<SingleQueryTestHandler> handler =
      new SingleQueryTestHandler(SingleQueryTestHandler::FAILURE, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that a single query with failure result delivered asynchronously.
TEST(MessageRouterTest, SingleQueryFailureAsyncCallback) {
  CefRefPtr<SingleQueryTestHandler> handler =
      new SingleQueryTestHandler(SingleQueryTestHandler::FAILURE, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that a single query with cancellation.
TEST(MessageRouterTest, SingleQueryCancel) {
  CefRefPtr<SingleQueryTestHandler> handler =
      new SingleQueryTestHandler(SingleQueryTestHandler::CANCEL, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const int kSinglePersistentQueryResponseCount = 10;

// Test a single persistent query in a single page load.
class SinglePersistentQueryTestHandler : public SingleLoadTestHandler {
 public:
  enum TestType {
    SUCCESS,
    FAILURE,
  };

  SinglePersistentQueryTestHandler(TestType test_type, bool sync_callback)
      : test_type_(test_type), sync_callback_(sync_callback) {}

  std::string GetMainHTML() override {
    std::string html;

    std::stringstream ss;
    ss << kSinglePersistentQueryResponseCount;
    const std::string& responseCountStr = ss.str();
    ss.str("");
    ss << kSingleQueryErrorCode;
    const std::string& errorCodeStr = ss.str();

    html =
        "<html><body><script>\n"
        // No requests should exist.
        "window.mrtAssertTotalCount(" LINESTR
        ",0);\n"
        "window.mrtAssertBrowserCount(" LINESTR
        ",0);\n"
        "window.mrtAssertContextCount(" LINESTR
        ",0);\n"
        // Keep track of the number of responses.
        "var count = 0;\n"
        // Send the query.
        "var request_id = window.mrtQuery({\n"
        "  request: '" +
        std::string(kSingleQueryRequest) +
        "',\n"
        "  persistent: true,\n"
        "  onSuccess: function(response) {\n"
        // Request should not be removed.
        "    window.mrtAssertTotalCount(" LINESTR
        ",1);\n"
        "    window.mrtAssertBrowserCount(" LINESTR
        ",1);\n"
        "    window.mrtAssertContextCount(" LINESTR
        ",1);\n"
        "    if (response == '" +
        std::string(kSingleQueryResponse) +
        "') {\n"
        "      if (++count == " +
        responseCountStr +
        ") {\n"
        "        window.mrtNotify('success');\n"
        "        window.mrtQueryCancel(request_id);\n"
        // Request should be removed immediately.
        "        window.mrtAssertTotalCount(" LINESTR
        ",0);\n"
        "        window.mrtAssertBrowserCount(" LINESTR
        ",0);\n"
        "        window.mrtAssertContextCount(" LINESTR
        ",0);\n"
        "      }\n"
        "    } else {\n"
        "      window.mrtNotify('error-onSuccess');\n"
        "    }\n"
        "  },\n"
        "  onFailure: function(error_code, error_message) {\n"
        // Request should be removed before callback is executed.
        "    window.mrtAssertTotalCount(" LINESTR
        ",0);\n"
        "    window.mrtAssertBrowserCount(" LINESTR
        ",0);\n"
        "    window.mrtAssertContextCount(" LINESTR
        ",0);\n"
        "    if (error_code == " +
        errorCodeStr + " && error_message == '" +
        std::string(kSingleQueryErrorMessage) +
        "') {\n"
        "      window.mrtNotify('failure');\n"
        "    } else {\n"
        "      window.mrtNotify('error-onFailure');\n"
        "    }\n"
        "  }\n"
        "});\n"
        // Request should exist.
        "window.mrtAssertTotalCount(" LINESTR
        ",1);\n"
        "window.mrtAssertBrowserCount(" LINESTR
        ",1);\n"
        "window.mrtAssertContextCount(" LINESTR ",1);\n";

    html += "</script></body></html>";
    return html;
  }

  void OnNotify(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                const std::string& message) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);

    if (test_type_ == SUCCESS) {
      EXPECT_STREQ("success", message.c_str());
    } else if (test_type_ == FAILURE) {
      EXPECT_STREQ("failure", message.c_str());
    }

    got_notify_.yes();

    DestroyTestIfDone();
  }

  void ExecuteCallback() {
    EXPECT_TRUE(callback_.get());
    if (test_type_ == SUCCESS) {
      callback_->Success(kSingleQueryResponse);
    } else {
      callback_->Failure(kSingleQueryErrorCode, kSingleQueryErrorMessage);
      callback_ = nullptr;
    }
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);
    EXPECT_NE(0, query_id);
    EXPECT_TRUE(persistent);
    EXPECT_STREQ(kSingleQueryRequest, request.ToString().c_str());

    got_on_query_.yes();

    query_id_ = query_id;
    callback_ = callback;

    int repeat =
        (test_type_ == SUCCESS ? kSinglePersistentQueryResponseCount : 1);

    for (int i = 0; i < repeat; ++i) {
      if (sync_callback_) {
        ExecuteCallback();
      } else {
        CefPostTask(
            TID_UI,
            base::BindOnce(&SinglePersistentQueryTestHandler::ExecuteCallback,
                           this));
      }
    }

    return true;
  }

  void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64_t query_id) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);
    EXPECT_EQ(query_id_, query_id);
    EXPECT_TRUE(got_on_query_);
    EXPECT_TRUE(callback_.get());

    got_on_query_canceled_.yes();
    callback_ = nullptr;

    DestroyTestIfDone();
  }

  void DestroyTestIfDone() {
    bool destroy_test = false;
    if (test_type_ == SUCCESS) {
      if (got_on_query_ && got_on_query_canceled_ && got_notify_) {
        destroy_test = true;
      }
    } else if (got_on_query_ && got_notify_) {
      destroy_test = true;
    }

    if (destroy_test) {
      DestroyTest();
    }
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_notify_);
    EXPECT_TRUE(got_on_query_);
    EXPECT_FALSE(callback_.get());

    if (test_type_ == SUCCESS) {
      EXPECT_TRUE(got_on_query_canceled_);
    } else {
      EXPECT_FALSE(got_on_query_canceled_);
    }

    TestHandler::DestroyTest();
  }

 private:
  const TestType test_type_;
  const bool sync_callback_;

  int64_t query_id_ = 0;
  CefRefPtr<Callback> callback_;

  TrackCallback got_on_query_;
  TrackCallback got_on_query_canceled_;
  TrackCallback got_notify_;
};

}  // namespace

// Test that a single query with successful result delivered synchronously.
TEST(MessageRouterTest, SinglePersistentQuerySuccessSyncCallback) {
  CefRefPtr<SinglePersistentQueryTestHandler> handler =
      new SinglePersistentQueryTestHandler(
          SinglePersistentQueryTestHandler::SUCCESS, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that a single query with successful result delivered asynchronously.
TEST(MessageRouterTest, SinglePersistentQuerySuccessAsyncCallback) {
  CefRefPtr<SinglePersistentQueryTestHandler> handler =
      new SinglePersistentQueryTestHandler(
          SinglePersistentQueryTestHandler::SUCCESS, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that a single query with failure result delivered synchronously.
TEST(MessageRouterTest, SinglePersistentQueryFailureSyncCallback) {
  CefRefPtr<SinglePersistentQueryTestHandler> handler =
      new SinglePersistentQueryTestHandler(
          SinglePersistentQueryTestHandler::FAILURE, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that a single query with failure result delivered asynchronously.
TEST(MessageRouterTest, SinglePersistentQueryFailureAsyncCallback) {
  CefRefPtr<SinglePersistentQueryTestHandler> handler =
      new SinglePersistentQueryTestHandler(
          SinglePersistentQueryTestHandler::FAILURE, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

// Test a single unhandled query in a single page load.
class SingleUnhandledQueryTestHandler : public SingleLoadTestHandler {
 public:
  SingleUnhandledQueryTestHandler() = default;

  std::string GetMainHTML() override {
    std::string html;

    html =
        "<html><body><script>\n"
        // No requests should exist.
        "window.mrtAssertTotalCount(" LINESTR
        ",0);\n"
        "window.mrtAssertBrowserCount(" LINESTR
        ",0);\n"
        "window.mrtAssertContextCount(" LINESTR
        ",0);\n"
        // Keep track of the number of responses.
        "var count = 0;\n"
        // Send the query.
        "var request_id = window.mrtQuery({\n"
        "  request: '" +
        std::string(kSingleQueryRequest) +
        "',\n"
        "  persistent: false,\n"
        "  onSuccess: function(response) {\n"
        "    window.mrtNotify('error-onSuccess');\n"
        "  },\n"
        "  onFailure: function(error_code, error_message) {\n"
        // Request should be removed before callback is executed.
        "    window.mrtAssertTotalCount(" LINESTR
        ",0);\n"
        "    window.mrtAssertBrowserCount(" LINESTR
        ",0);\n"
        "    window.mrtAssertContextCount(" LINESTR
        ",0);\n"
        "    if (error_code == -1 && "
        "error_message == 'The query has been canceled') {\n"
        "      window.mrtNotify('failure');\n"
        "    } else {\n"
        "      window.mrtNotify('error-onFailure');\n"
        "    }\n"
        "  }\n"
        "});\n"
        // Request should exist.
        "window.mrtAssertTotalCount(" LINESTR
        ",1);\n"
        "window.mrtAssertBrowserCount(" LINESTR
        ",1);\n"
        "window.mrtAssertContextCount(" LINESTR ",1);\n";

    html += "</script></body></html>";
    return html;
  }

  void OnNotify(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                const std::string& message) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);
    EXPECT_STREQ("failure", message.c_str());

    got_notify_.yes();

    DestroyTest();
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);
    EXPECT_NE(0, query_id);
    EXPECT_FALSE(persistent);
    EXPECT_STREQ(kSingleQueryRequest, request.ToString().c_str());

    got_on_query_.yes();

    return false;
  }

  void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64_t query_id) override {
    EXPECT_FALSE(true);  // Not reached.
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_on_query_);
    EXPECT_TRUE(got_notify_);

    TestHandler::DestroyTest();
  }

 private:
  TrackCallback got_on_query_;
  TrackCallback got_notify_;
};

}  // namespace

// Test that a single unhandled query results in a call to onFailure.
TEST(MessageRouterTest, SingleUnhandledQuery) {
  CefRefPtr<SingleUnhandledQueryTestHandler> handler =
      new SingleUnhandledQueryTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
