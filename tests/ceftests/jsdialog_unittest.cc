// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <map>

#include "include/base/cef_callback.h"
#include "include/cef_devtools_message_observer.h"
#include "include/cef_parser.h"
#include "include/test/cef_test_helpers.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char* kStartUrl = "https://tests/JSDialogTestHandler.Start";
const char* kEndUrl = "https://tests/JSDialogTestHandler.End?r=";
const char* kDialogNavigationUrl =
    "https://tests/JSDialogTestHandler.Navigated";

// Exercise the default Views dialog implementation instead of handling or
// suppressing the dialog in CefJSDialogHandler. In Alloy, these dialogs share
// the TabInterface modal UI scope used by print preview.
class DefaultJSDialogTestHandler : public TestHandler,
                                   public CefDevToolsMessageObserver {
 public:
  DefaultJSDialogTestHandler(JSDialogType type, bool accept, bool navigate)
      : type_(type), accept_(accept), navigate_(navigate) {}

  void RunTest() override {
    SetUseViews(true);
    AddResource(kStartUrl,
                "<!doctype html><title>Default JS dialog test</title>",
                "text/html");
    AddResource(kDialogNavigationUrl,
                "<!doctype html><title>Navigated JS dialog test</title>",
                "text/html");
    SetTestTimeout();
    CreateReusableBrowser(kStartUrl);
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool is_loading,
                            bool can_go_back,
                            bool can_go_forward) override {
    if (is_loading) {
      return;
    }
    const auto url = browser->GetMainFrame()->GetURL();
    if (navigate_ && url == kDialogNavigationUrl) {
      navigation_loaded_ = true;
      MaybeCompleteRound();
      return;
    }
    if (started_ || url != kStartUrl) {
      return;
    }
    started_ = true;
    registration_ = browser->GetHost()->AddDevToolsMessageObserver(this);
    enable_id_ = Execute("Page.enable", nullptr);
  }

  bool OnJSDialog(CefRefPtr<CefBrowser> browser,
                  const CefString& origin_url,
                  JSDialogType dialog_type,
                  const CefString& message_text,
                  const CefString& default_prompt_text,
                  CefRefPtr<CefJSDialogCallback> callback,
                  bool& suppress_message) override {
    EXPECT_EQ(type_, dialog_type);
    EXPECT_EQ(navigation_loaded_ ? kDialogNavigationUrl : kStartUrl,
              origin_url.ToString());
    EXPECT_EQ("Default dialog test", message_text.ToString());
    EXPECT_EQ(type_ == JSDIALOGTYPE_PROMPT ? "default input" : "",
              default_prompt_text.ToString());
    EXPECT_FALSE(suppress_message);
    ++default_dialog_count_;
    // Do not execute or retain the callback. Let Chromium create the dialog.
    return false;
  }

  void OnDevToolsEvent(CefRefPtr<CefBrowser> browser,
                       const CefString& method,
                       const void* params,
                       size_t params_size) override {
    if (finishing_ || (method != "Page.javascriptDialogOpening" &&
                       method != "Page.javascriptDialogClosed")) {
      return;
    }
    auto value = Parse(params, params_size);
    ASSERT_TRUE(value);
    if (method == "Page.javascriptDialogOpening") {
      EXPECT_EQ(TypeName(), value->GetString("type").ToString());
      EXPECT_TRUE(value->GetBool("hasBrowserHandler"));
      ++opening_count_;
      if (navigate_ && completed_count_ == 0) {
        navigation_script_id_ = script_id_;
        CefPostTask(TID_UI, base::BindOnce(
                                &DefaultJSDialogTestHandler::Navigate, this));
        return;
      }
      auto handle = CefDictionaryValue::Create();
      handle->SetBool("accept", accept_);
      if (type_ == JSDIALOGTYPE_PROMPT && accept_) {
        handle->SetString("promptText", "entered input");
      }
      // The opening event precedes creation of the default dialog. Execute
      // on a later UI task, after the manager has shown it. If the dialog was
      // suppressed, this command fails with "No dialog is showing".
      handle_id_ = Execute("Page.handleJavaScriptDialog", handle);
    } else {
      const bool navigation_dismissal = navigate_ && completed_count_ == 0;
      if (navigation_dismissal) {
        // Suppression before the posted navigation must not pass this test.
        EXPECT_TRUE(navigation_started_);
      }
      EXPECT_EQ(navigation_dismissal ? false : accept_,
                value->GetBool("result"));
      ++closed_count_;
      MaybeCompleteRound();
    }
  }

  void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                              int message_id,
                              bool success,
                              const void* result,
                              size_t result_size) override {
    // Commands can respond synchronously, before Execute returns the allocated
    // ID. Process results after recording that ID below.
    CefPostTask(TID_UI,
                base::BindOnce(&DefaultJSDialogTestHandler::ProcessMethodResult,
                               this, message_id, success,
                               std::string(static_cast<const char*>(result),
                                           result_size)));
  }

  void ProcessMethodResult(int message_id,
                           bool success,
                           const std::string& result) {
    if (timed_out_) {
      return;
    }
    if (finishing_) {
      if (message_id == cleanup_id_) {
        EXPECT_TRUE(success) << result;
        registration_ = nullptr;
        TestHandler::DestroyTest();
      }
      return;
    }
    auto entry = message_ids_.find(message_id);
    if (entry == message_ids_.end()) {
      return;
    }
    message_id = entry->second;
    message_ids_.erase(entry);
    if (message_id == navigation_script_id_) {
      // Navigation may destroy the old execution context before its pending
      // Runtime.evaluate completes. The new document load is the liveness
      // check for this round.
      return;
    }
    ASSERT_TRUE(success) << result;
    if (message_id == enable_id_) {
      OpenDialog();
      return;
    }
    if (message_id == script_id_) {
      auto value = Parse(result.data(), result.size());
      ASSERT_TRUE(value);
      ASSERT_FALSE(value->HasKey("exceptionDetails"));
      EXPECT_EQ(ExpectedResult(),
                value->GetDictionary("result")->GetString("value").ToString());
      got_script_result_ = true;
    } else if (message_id == handle_id_) {
      got_handle_result_ = true;
    }
    MaybeCompleteRound();
  }

  void MaybeCompleteRound() {
    const bool round_complete = navigate_ && completed_count_ == 0
                                    ? navigation_loaded_ && closed_count_ == 1
                                    : got_script_result_ && got_handle_result_;
    if (round_complete && !finishing_) {
      ++completed_count_;
      EXPECT_EQ(completed_count_, opening_count_);
      EXPECT_EQ(completed_count_, default_dialog_count_);
      EXPECT_EQ(completed_count_, closed_count_);
      if (completed_count_ == 2) {
        DestroyTest();
      } else {
        // A second dialog verifies that dismissal releases the modal scope.
        OpenDialog();
      }
    }
  }

  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                 TerminationStatus status,
                                 int error_code,
                                 const CefString& error_string) override {
    ADD_FAILURE() << "Default dialog renderer crashed: " << status << " "
                  << error_string.ToString();
    DestroyTest();
  }

  void OnTestTimeout(int timeout_ms, bool treat_as_error) override {
    timed_out_ = true;
    TestHandler::OnTestTimeout(timeout_ms, treat_as_error);
  }

  void DestroyTest() override {
    if (timed_out_) {
      // A failed cleanup must discard the lease instead of stranding the
      // shared pool while waiting for another protocol response.
      finishing_ = true;
      registration_ = nullptr;
      TestHandler::DestroyTest();
      return;
    }
    if (finishing_) {
      return;
    }
    finishing_ = true;
    EXPECT_EQ(2, completed_count_);
    CefPostTask(TID_UI, base::BindOnce(
                            &DefaultJSDialogTestHandler::CompleteTest, this));
  }

 private:
  void Navigate() {
    if (!finishing_) {
      navigation_started_ = true;
      GetBrowser()->GetMainFrame()->LoadURL(kDialogNavigationUrl);
    }
  }

  const char* TypeName() const {
    switch (type_) {
      case JSDIALOGTYPE_ALERT:
        return "alert";
      case JSDIALOGTYPE_CONFIRM:
        return "confirm";
      case JSDIALOGTYPE_PROMPT:
        return "prompt";
      case JSDIALOGTYPE_NUM_VALUES:
        break;
    }
    NOTREACHED();
    return "";
  }

  std::string ExpectedResult() const {
    if (type_ == JSDIALOGTYPE_ALERT) {
      return "undefined";
    }
    if (type_ == JSDIALOGTYPE_CONFIRM) {
      return accept_ ? "true" : "false";
    }
    return accept_ ? "entered input" : "null";
  }

  void OpenDialog() {
    got_script_result_ = false;
    got_handle_result_ = false;
    auto params = CefDictionaryValue::Create();
    params->SetString(
        "expression",
        std::string("String(") + TypeName() + "('Default dialog test'" +
            (type_ == JSDIALOGTYPE_PROMPT ? ", 'default input'" : "") + "))");
    params->SetBool("userGesture", true);
    script_id_ = Execute("Runtime.evaluate", params);
  }

  static CefRefPtr<CefDictionaryValue> Parse(const void* data, size_t size) {
    auto value = CefParseJSON(data, size, JSON_PARSER_RFC);
    return value && value->GetType() == VTYPE_DICTIONARY
               ? value->GetDictionary()
               : nullptr;
  }

  int Execute(const std::string& method, CefRefPtr<CefDictionaryValue> params) {
    const int id = next_id_++;
    // Avoid synchronously reentering the DevTools observer from a callback.
    CefPostTask(TID_UI, base::BindOnce(&DefaultJSDialogTestHandler::ExecuteOnUI,
                                       this, id, method, params));
    return id;
  }

  void ExecuteOnUI(int id,
                   const std::string& method,
                   CefRefPtr<CefDictionaryValue> params) {
    if (!finishing_) {
      // The browser's DevTools session and ID allocator survive pool leases.
      const int actual_id =
          GetBrowser()->GetHost()->ExecuteDevToolsMethod(0, method, params);
      EXPECT_GT(actual_id, 0);
      message_ids_[actual_id] = id;
    }
  }

  void CompleteTest() {
    if (timed_out_) {
      return;
    }
    if (!registration_) {
      TestHandler::DestroyTest();
      return;
    }
    // Stop dialog notifications and wait for the acknowledgement before
    // releasing the observer and returning this browser to the shared pool.
    cleanup_id_ = GetBrowser()->GetHost()->ExecuteDevToolsMethod(
        0, "Page.disable", nullptr);
    EXPECT_GT(cleanup_id_, 0);
  }

  const JSDialogType type_;
  const bool accept_;
  const bool navigate_;
  bool navigation_started_ = false;
  bool navigation_loaded_ = false;
  bool started_ = false;
  bool finishing_ = false;
  bool timed_out_ = false;
  bool got_script_result_ = false;
  bool got_handle_result_ = false;
  int next_id_ = 1;
  int enable_id_ = 0;
  int cleanup_id_ = 0;
  std::map<int, int> message_ids_;
  int script_id_ = 0;
  int navigation_script_id_ = 0;
  int handle_id_ = 0;
  int opening_count_ = 0;
  int default_dialog_count_ = 0;
  int closed_count_ = 0;
  int completed_count_ = 0;
  CefRefPtr<CefRegistration> registration_;

  IMPLEMENT_REFCOUNTING(DefaultJSDialogTestHandler);
};

class DefaultJSDialogTest : public testing::Test {
 protected:
  void Run(cef_jsdialog_type_t type, bool accept, bool navigate = false) {
    CefRefPtr<DefaultJSDialogTestHandler> handler =
        new DefaultJSDialogTestHandler(type, accept, navigate);
    handler->ExecuteTest();
    ReleaseAndWaitForDestructor(handler);
  }
};

TEST_F(DefaultJSDialogTest, Alert) {
  Run(JSDIALOGTYPE_ALERT, true);
}

TEST_F(DefaultJSDialogTest, ConfirmAccept) {
  Run(JSDIALOGTYPE_CONFIRM, true);
}

TEST_F(DefaultJSDialogTest, ConfirmCancel) {
  Run(JSDIALOGTYPE_CONFIRM, false);
}

TEST_F(DefaultJSDialogTest, PromptAccept) {
  Run(JSDIALOGTYPE_PROMPT, true);
}

TEST_F(DefaultJSDialogTest, PromptCancel) {
  Run(JSDIALOGTYPE_PROMPT, false);
}

TEST_F(DefaultJSDialogTest, AlertDismissOnNavigation) {
  Run(JSDIALOGTYPE_ALERT, true, true);
}

TEST_F(DefaultJSDialogTest, ConfirmDismissOnNavigation) {
  Run(JSDIALOGTYPE_CONFIRM, true, true);
}

TEST_F(DefaultJSDialogTest, PromptDismissOnNavigation) {
  Run(JSDIALOGTYPE_PROMPT, true, true);
}

class JSDialogTestHandler : public TestHandler {
 public:
  enum TestType {
    TYPE_ALERT,
    TYPE_CONFIRM,
    TYPE_PROMPT,
    TYPE_ONBEFOREUNLOAD,
  };
  enum TestMode {
    MODE_SUPPRESS,
    MODE_RUN_IMMEDIATE,
    MODE_RUN_DELAYED,
  };

  JSDialogTestHandler(TestType type,
                      TestMode mode,
                      bool success,
                      const std::string& user_input,
                      const std::string& result)
      : type_(type),
        mode_(mode),
        success_(success),
        user_input_(user_input),
        result_(result) {}

  void RunTest() override {
    std::string content = "<html><head><body>START<script>";
    if (type_ == TYPE_ALERT) {
      content +=
          "alert('My alert message'); "
          "document.location='" +
          std::string(kEndUrl) + "';";
    } else if (type_ == TYPE_CONFIRM) {
      content +=
          "var r = confirm('My confirm message')?'ok':'cancel'; "
          "document.location='" +
          std::string(kEndUrl) + "'+r;";
    } else if (type_ == TYPE_PROMPT) {
      content +=
          "var r = prompt('My prompt message','my default'); "
          "document.location='" +
          std::string(kEndUrl) + "'+r;";
    } else if (type_ == TYPE_ONBEFOREUNLOAD) {
      content +=
          "window.onbeforeunload=function() {"
          " return 'My unload message'; };";
    }
    content += "</script></body></html>";

    AddResource(kStartUrl, content, "text/html");
    AddResource(kEndUrl, "<html><body>END</body></html>", "text/html");

    // Create the browser
    CreateReusableBrowser(kStartUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (!frame->IsMain()) {
      return;
    }

    std::string url = frame->GetURL();
    if (url.find(kEndUrl) == 0) {
      got_onloadend_.yes();

      std::string result = url.substr(strlen(kEndUrl));
      EXPECT_STREQ(result_.c_str(), result.c_str());

      DestroyTest();
    } else if (type_ == TYPE_ONBEFOREUNLOAD) {
      // Send the page a user gesture to enable firing of the onbefureunload
      // handler. See https://crbug.com/707007.
      CefExecuteJavaScriptWithUserGestureForTests(frame, CefString());

      // Trigger the onunload handler.
      frame->LoadURL(kEndUrl);
    }
  }

  void Continue(CefRefPtr<CefJSDialogCallback> callback) {
    callback->Continue(success_, user_input_);
  }

  bool OnJSDialog(CefRefPtr<CefBrowser> browser,
                  const CefString& origin_url,
                  JSDialogType dialog_type,
                  const CefString& message_text,
                  const CefString& default_prompt_text,
                  CefRefPtr<CefJSDialogCallback> callback,
                  bool& suppress_message) override {
    got_onjsdialog_.yes();

    EXPECT_STREQ(kStartUrl, origin_url.ToString().c_str());

    if (type_ == TYPE_ALERT) {
      EXPECT_EQ(JSDIALOGTYPE_ALERT, dialog_type);
      EXPECT_STREQ("My alert message", message_text.ToString().c_str());
      EXPECT_TRUE(default_prompt_text.empty());
    } else if (type_ == TYPE_CONFIRM) {
      EXPECT_EQ(JSDIALOGTYPE_CONFIRM, dialog_type);
      EXPECT_STREQ("My confirm message", message_text.ToString().c_str());
      EXPECT_TRUE(default_prompt_text.empty());
    } else if (type_ == TYPE_PROMPT) {
      EXPECT_EQ(JSDIALOGTYPE_PROMPT, dialog_type);
      EXPECT_STREQ("My prompt message", message_text.ToString().c_str());
      EXPECT_STREQ("my default", default_prompt_text.ToString().c_str());
    }

    EXPECT_FALSE(suppress_message);

    if (mode_ == MODE_SUPPRESS) {
      // Suppress the dialog.
      suppress_message = true;
      return false;
    } else if (mode_ == MODE_RUN_IMMEDIATE) {
      // Continue immediately.
      callback->Continue(success_, user_input_);
    } else if (mode_ == MODE_RUN_DELAYED) {
      // Continue asynchronously.
      CefPostTask(TID_UI, base::BindOnce(&JSDialogTestHandler::Continue, this,
                                         callback));
    }

    return true;
  }

  bool OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser,
                            const CefString& message_text,
                            bool is_reload,
                            CefRefPtr<CefJSDialogCallback> callback) override {
    got_onbeforeunloaddialog_.yes();

    if (type_ == TYPE_ONBEFOREUNLOAD) {
      // The message is no longer configurable via JavaScript.
      // See https://crbug.com/587940.
      EXPECT_STREQ("Is it OK to leave/reload this page?",
                   message_text.ToString().c_str());
      EXPECT_FALSE(is_reload);
    }

    if (mode_ == MODE_RUN_IMMEDIATE) {
      // Continue immediately.
      callback->Continue(success_, user_input_);
    } else if (mode_ == MODE_RUN_DELAYED) {
      // Continue asynchronously.
      CefPostTask(TID_UI, base::BindOnce(&JSDialogTestHandler::Continue, this,
                                         callback));
    }

    return true;
  }

  void OnResetDialogState(CefRefPtr<CefBrowser> browser) override {
    got_onresetdialogstate_.yes();
  }

  void OnDialogClosed(CefRefPtr<CefBrowser> browser) override {
    got_ondialogclosed_.yes();
    EXPECT_TRUE(browser.get());
    EXPECT_TRUE(browser->IsSame(GetBrowser()));
  }

  TestType type_;
  TestMode mode_;
  bool success_;
  std::string user_input_;
  std::string result_;

  TrackCallback got_onjsdialog_;
  TrackCallback got_onbeforeunloaddialog_;
  TrackCallback got_onresetdialogstate_;
  TrackCallback got_ondialogclosed_;
  TrackCallback got_onloadend_;

  IMPLEMENT_REFCOUNTING(JSDialogTestHandler);
};

}  // namespace

// Alert dialog with suppression.
TEST(JSDialogTest, AlertSuppress) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_ALERT, JSDialogTestHandler::MODE_SUPPRESS,
      true,  // success
      "",    // user_input
      "");   // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_FALSE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Alert dialog with immediate callback.
TEST(JSDialogTest, AlertRunImmediate) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_ALERT, JSDialogTestHandler::MODE_RUN_IMMEDIATE,
      true,  // success
      "",    // user_input
      "");   // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Alert dialog with delayed callback.
TEST(JSDialogTest, AlertRunDelayed) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_ALERT, JSDialogTestHandler::MODE_RUN_DELAYED,
      true,  // success
      "",    // user_input
      "");   // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Confirm dialog with suppression.
TEST(JSDialogTest, ConfirmSuppress) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_CONFIRM, JSDialogTestHandler::MODE_SUPPRESS,
      true,       // success
      "",         // user_input
      "cancel");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_FALSE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Confirm dialog run immediately return OK.
TEST(JSDialogTest, ConfirmRunImmediateOk) {
  CefRefPtr<JSDialogTestHandler> handler =
      new JSDialogTestHandler(JSDialogTestHandler::TYPE_CONFIRM,
                              JSDialogTestHandler::MODE_RUN_IMMEDIATE,
                              true,   // success
                              "",     // user_input
                              "ok");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Confirm dialog run immediately return Cancel.
TEST(JSDialogTest, ConfirmRunImmediateCancel) {
  CefRefPtr<JSDialogTestHandler> handler =
      new JSDialogTestHandler(JSDialogTestHandler::TYPE_CONFIRM,
                              JSDialogTestHandler::MODE_RUN_IMMEDIATE,
                              false,      // success
                              "",         // user_input
                              "cancel");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Confirm dialog run delayed return OK.
TEST(JSDialogTest, ConfirmRunDelayedOk) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_CONFIRM, JSDialogTestHandler::MODE_RUN_DELAYED,
      true,   // success
      "",     // user_input
      "ok");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Confirm dialog run delayed return Cancel.
TEST(JSDialogTest, ConfirmRunDelayedCancel) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_CONFIRM, JSDialogTestHandler::MODE_RUN_DELAYED,
      false,      // success
      "",         // user_input
      "cancel");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Prompt dialog with suppression.
TEST(JSDialogTest, PromptSuppress) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_PROMPT, JSDialogTestHandler::MODE_SUPPRESS,
      true,          // success
      "some_value",  // user_input
      "null");       // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_FALSE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Prompt dialog run immediately return OK.
TEST(JSDialogTest, PromptRunImmediateOk) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_PROMPT, JSDialogTestHandler::MODE_RUN_IMMEDIATE,
      true,           // success
      "some_value",   // user_input
      "some_value");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Prompt dialog run immediately return Cancel.
TEST(JSDialogTest, PromptRunImmediateCancel) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_PROMPT, JSDialogTestHandler::MODE_RUN_IMMEDIATE,
      false,         // success
      "some_value",  // user_input
      "null");       // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Prompt dialog run delayed return OK.
TEST(JSDialogTest, PromptRunDelayedOk) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_PROMPT, JSDialogTestHandler::MODE_RUN_DELAYED,
      true,           // success
      "some_value",   // user_input
      "some_value");  // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// Prompt dialog run delayed return Cancel.
TEST(JSDialogTest, PromptRunDelayedCancel) {
  CefRefPtr<JSDialogTestHandler> handler = new JSDialogTestHandler(
      JSDialogTestHandler::TYPE_PROMPT, JSDialogTestHandler::MODE_RUN_DELAYED,
      false,         // success
      "some_value",  // user_input
      "null");       // result
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_onjsdialog_);
  EXPECT_FALSE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// OnBeforeUnload dialog with immediate callback.
TEST(JSDialogTest, OnBeforeUnloadRunImmediate) {
  CefRefPtr<JSDialogTestHandler> handler =
      new JSDialogTestHandler(JSDialogTestHandler::TYPE_ONBEFOREUNLOAD,
                              JSDialogTestHandler::MODE_RUN_IMMEDIATE,
                              true,  // success
                              "",    // user_input
                              "");   // result
  handler->ExecuteTest();

  EXPECT_FALSE(handler->got_onjsdialog_);
  EXPECT_TRUE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}

// OnBeforeUnload dialog with delayed callback.
TEST(JSDialogTest, OnBeforeUnloadRunDelayed) {
  CefRefPtr<JSDialogTestHandler> handler =
      new JSDialogTestHandler(JSDialogTestHandler::TYPE_ONBEFOREUNLOAD,
                              JSDialogTestHandler::MODE_RUN_DELAYED,
                              true,  // success
                              "",    // user_input
                              "");   // result
  handler->ExecuteTest();

  EXPECT_FALSE(handler->got_onjsdialog_);
  EXPECT_TRUE(handler->got_onbeforeunloaddialog_);
  EXPECT_TRUE(handler->got_onresetdialogstate_);
  EXPECT_TRUE(handler->got_ondialogclosed_);
  EXPECT_TRUE(handler->got_onloadend_);

  ReleaseAndWaitForDestructor(handler);
}
