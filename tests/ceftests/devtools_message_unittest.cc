// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <sstream>

#include "include/base/cef_callback.h"
#include "include/base/cef_callback_helpers.h"
#include "include/cef_callback.h"
#include "include/cef_devtools_message_observer.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_closure_task.h"

#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char kTestUrl1[] = "http://tests/DevToolsMessage1";
const char kTestUrl2[] = "http://tests/DevToolsMessage2";

class DevToolsMessageTestHandler : public TestHandler {
 public:
  DevToolsMessageTestHandler() {}

  void RunTest() override {
    // Add HTML resources.
    AddResource(kTestUrl1, "<html><body>Test1</body></html>", "text/html");
    AddResource(kTestUrl2, "<html><body>Test2</body></html>", "text/html");

    // Create the browser.
    CreateBrowser(kTestUrl1);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    // STEP 1: Add the DevTools observer. Wait for the 1st load.
    registration_ = browser->GetHost()->AddDevToolsMessageObserver(
        new TestMessageObserver(this));
    EXPECT_TRUE(registration_);
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (!isLoading) {
      load_ct_++;
      if (load_ct_ == 1) {
        // STEP 2: 1st load has completed. Now enable page domain notifications
        // and wait for the method result.
        ExecuteMethod(
            "Page.enable", "",
            base::BindOnce(&DevToolsMessageTestHandler::Navigate, this));
      } else if (load_ct_ == 2) {
        MaybeDestroyTest();
      }
    }
  }

  void DestroyTest() override {
    // STEP 7: Remove the DevTools observer. This should result in the observer
    // object being destroyed.
    EXPECT_TRUE(registration_);
    registration_ = nullptr;
    EXPECT_TRUE(observer_destroyed_);

    // Each send message variant should be called at least a single time.
    EXPECT_GE(method_send_ct_, 1);
    EXPECT_GE(method_execute_ct_, 1);

    // All sent messages should receive a result callback.
    EXPECT_EQ(expected_method_ct_, method_send_ct_ + method_execute_ct_);
    EXPECT_EQ(expected_method_ct_, result_ct_);
    EXPECT_EQ(expected_method_ct_, last_result_id_);

    // Every received message should parse successfully to a result or event
    // callback.
    EXPECT_EQ(message_ct_, result_ct_ + event_ct_);

    // Should receive 1 or more events (probably just 1, but who knows?).
    EXPECT_GE(event_ct_, 1);

    // OnLoadingStateChange(isLoading=false) should be called twice.
    EXPECT_EQ(expected_load_ct_, load_ct_);

    // Should get callbacks for agent attached but not detached.
    EXPECT_EQ(1, attached_ct_);
    EXPECT_EQ(0, detached_ct_);

    TestHandler::DestroyTest();
  }

 private:
  struct MethodResult {
    int message_id;
    bool success;
    std::string result;
  };

  struct Event {
    std::string method;
    std::string params;
  };

  class TestMessageObserver : public CefDevToolsMessageObserver {
   public:
    explicit TestMessageObserver(DevToolsMessageTestHandler* handler)
        : handler_(handler) {}

    virtual ~TestMessageObserver() { handler_->observer_destroyed_.yes(); }

    bool OnDevToolsMessage(CefRefPtr<CefBrowser> browser,
                           const void* message,
                           size_t message_size) override {
      EXPECT_TRUE(browser.get());
      EXPECT_EQ(handler_->GetBrowserId(), browser->GetIdentifier());
      handler_->message_ct_++;
      return false;
    }

    void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                                int message_id,
                                bool success,
                                const void* result,
                                size_t result_size) override {
      EXPECT_TRUE(browser.get());
      EXPECT_EQ(handler_->GetBrowserId(), browser->GetIdentifier());
      handler_->result_ct_++;

      MethodResult mr;
      mr.message_id = message_id;
      mr.success = success;
      if (result) {
        // Intentionally truncating at small size.
        mr.result = std::string(static_cast<const char*>(result),
                                std::min(static_cast<int>(result_size), 80));
      }
      handler_->OnMethodResult(mr);
    }

    void OnDevToolsEvent(CefRefPtr<CefBrowser> browser,
                         const CefString& method,
                         const void* params,
                         size_t params_size) override {
      EXPECT_TRUE(browser.get());
      EXPECT_EQ(handler_->GetBrowserId(), browser->GetIdentifier());
      handler_->event_ct_++;

      Event ev;
      ev.method = method;
      if (params) {
        // Intentionally truncating at small size.
        ev.params = std::string(static_cast<const char*>(params),
                                std::min(static_cast<int>(params_size), 80));
      }
      handler_->OnEvent(ev);
    }

    void OnDevToolsAgentAttached(CefRefPtr<CefBrowser> browser) override {
      EXPECT_TRUE(browser.get());
      EXPECT_EQ(handler_->GetBrowserId(), browser->GetIdentifier());
      handler_->attached_ct_++;
    }

    void OnDevToolsAgentDetached(CefRefPtr<CefBrowser> browser) override {
      EXPECT_TRUE(browser.get());
      EXPECT_EQ(handler_->GetBrowserId(), browser->GetIdentifier());
      handler_->detached_ct_++;
    }

   private:
    DevToolsMessageTestHandler* handler_;

    IMPLEMENT_REFCOUNTING(TestMessageObserver);
    DISALLOW_COPY_AND_ASSIGN(TestMessageObserver);
  };

  // Execute a DevTools method. Expected results will be verified in
  // OnMethodResult, and |next_step| will then be executed.
  // |expected_result| can be a fragment that the result should start with.
  void ExecuteMethod(const std::string& method,
                     const std::string& params,
                     base::OnceClosure next_step,
                     const std::string& expected_result = "{}",
                     bool expected_success = true) {
    CHECK(!method.empty());
    CHECK(!next_step.is_null());

    int message_id = next_message_id_++;

    std::stringstream message;
    message << "{\"id\":" << message_id << ",\"method\":\"" << method << "\"";
    if (!params.empty()) {
      message << ",\"params\":" << params;
    }
    message << "}";

    // Set expected result state.
    pending_message_ = message.str();
    pending_result_next_ = std::move(next_step);
    pending_result_ = {message_id, expected_success, expected_result};

    if (message_id % 2 == 0) {
      // Use the less structured method.
      method_send_ct_++;
      GetBrowser()->GetHost()->SendDevToolsMessage(pending_message_.data(),
                                                   pending_message_.size());
    } else {
      // Use the more structured method.
      method_execute_ct_++;
      CefRefPtr<CefDictionaryValue> dict;
      if (!params.empty()) {
        CefRefPtr<CefValue> value =
            CefParseJSON(params.data(), params.size(), JSON_PARSER_RFC);
        EXPECT_TRUE(value && value->GetType() == VTYPE_DICTIONARY)
            << "failed to parse: " << params;
        if (value && value->GetType() == VTYPE_DICTIONARY) {
          dict = value->GetDictionary();
        }
      }
      GetBrowser()->GetHost()->ExecuteDevToolsMethod(message_id, method, dict);
    }
  }

  // Every call to ExecuteMethod should result in a single call to this method
  // with the same message_id.
  void OnMethodResult(const MethodResult& result) {
    EXPECT_EQ(pending_result_.message_id, result.message_id)
        << "with message=" << pending_message_;
    if (result.message_id != pending_result_.message_id)
      return;

    EXPECT_EQ(pending_result_.success, result.success)
        << "with message=" << pending_message_;

    EXPECT_TRUE(result.result.find(pending_result_.result) == 0)
        << "with message=" << pending_message_
        << "\nand actual result=" << result.result
        << "\nand expected result=" << pending_result_.result;

    last_result_id_ = result.message_id;

    // Continue asynchronously to allow the callstack to unwind.
    CefPostTask(TID_UI, std::move(pending_result_next_));

    // Clear expected result state.
    pending_message_.clear();
    pending_result_ = {};
  }

  void OnEvent(const Event& event) {
    if (event.method != pending_event_.method)
      return;

    EXPECT_TRUE(event.params.find(pending_event_.params) == 0)
        << "with method=" << event.method
        << "\nand actual params=" << event.params
        << "\nand expected params=" << pending_event_.params;

    // Continue asynchronously to allow the callstack to unwind.
    CefPostTask(TID_UI, std::move(pending_event_next_));

    // Clear expected result state.
    pending_event_ = {};
  }

  void Navigate() {
    pending_event_ = {"Page.frameNavigated", "{\"frame\":"};
    pending_event_next_ =
        base::BindOnce(&DevToolsMessageTestHandler::AfterNavigate, this);

    std::stringstream params;
    params << "{\"url\":\"" << kTestUrl2 << "\"}";

    // STEP 3: Page domain notifications are enabled. Now start a new
    // navigation (but do nothing on method result) and wait for the
    // "Page.frameNavigated" event.
    ExecuteMethod("Page.navigate", params.str(), base::DoNothing(),
                  /*expected_result=*/"{\"frameId\":");
  }

  void AfterNavigate() {
    // STEP 4: Got the "Page.frameNavigated" event. Now disable page domain
    // notifications.
    ExecuteMethod(
        "Page.disable", "",
        base::BindOnce(&DevToolsMessageTestHandler::AfterPageDisabled, this));
  }

  void AfterPageDisabled() {
    // STEP 5: Got the the "Page.disable" method result. Now call a non-existant
    // method to verify an error result, and then destroy the test when done.
    ExecuteMethod(
        "Foo.doesNotExist", "",
        base::BindOnce(&DevToolsMessageTestHandler::MaybeDestroyTest, this),
        /*expected_result=*/
        "{\"code\":-32601,\"message\":\"'Foo.doesNotExist' wasn't found\"}",
        /*expected_success=*/false);
  }

  void MaybeDestroyTest() {
    if (result_ct_ == expected_method_ct_ && load_ct_ == expected_load_ct_) {
      // STEP 6: Got confirmation of all expected method results and load
      // events. Now destroy the test.
      DestroyTest();
    }
  }

  // Total # of times we're planning to call ExecuteMethod.
  const int expected_method_ct_ = 4;

  // Total # of times we're expecting OnLoadingStateChange(isLoading=false) to
  // be called.
  const int expected_load_ct_ = 2;

  // In ExecuteMethod:
  //   Next message ID to use.
  int next_message_id_ = 1;
  //   Last message that was sent (used for debug messages only).
  std::string pending_message_;
  //   SendDevToolsMessage call count.
  int method_send_ct_ = 0;
  //   ExecuteDevToolsMethod call count.
  int method_execute_ct_ = 0;

  // Expect |pending_result_.message_id| in OnMethodResult.
  // The result should start with the |pending_result_.result| fragment.
  MethodResult pending_result_;
  //   Tracks the last message ID received.
  int last_result_id_ = -1;
  //   When received, execute this callback.
  base::OnceClosure pending_result_next_;

  // Wait for |pending_event_.method| in OnEvent.
  // The params should start with the |pending_event_.params| fragment.
  Event pending_event_;
  //   When received, execute this callback.
  base::OnceClosure pending_event_next_;

  CefRefPtr<CefRegistration> registration_;

  // Observer callback count.
  int message_ct_ = 0;
  int result_ct_ = 0;
  int event_ct_ = 0;
  int attached_ct_ = 0;
  int detached_ct_ = 0;

  // OnLoadingStateChange(isLoading=false) count.
  int load_ct_ = 0;

  TrackCallback observer_destroyed_;

  IMPLEMENT_REFCOUNTING(DevToolsMessageTestHandler);
  DISALLOW_COPY_AND_ASSIGN(DevToolsMessageTestHandler);
};

}  // namespace

// Test everything related to DevTools messages:
// - CefDevToolsMessageObserver registration and life span.
// - SendDevToolsMessage/ExecuteDevToolsMethod calls.
// - CefDevToolsMessageObserver callbacks for method results and events.
TEST(DevToolsMessageTest, Messages) {
  CefRefPtr<DevToolsMessageTestHandler> handler =
      new DevToolsMessageTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
