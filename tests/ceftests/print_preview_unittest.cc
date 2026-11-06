// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <map>

#include "include/cef_command_line.h"
#include "include/cef_devtools_message_observer.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/shared/browser/client_app_browser.h"

namespace {

constexpr char kTestUrl[] = "https://tests/print-preview.html";
constexpr char kNavigationUrl[] = "https://tests/print-preview-navigated.html";

class PrintPreviewBrowserTest : public client::ClientAppBrowser::Delegate {
 public:
  void OnBeforeCommandLineProcessing(
      CefRefPtr<client::ClientAppBrowser> app,
      CefRefPtr<CefCommandLine> command_line) override {
    // Alloy defaults to the native print dialog unless explicitly enabled.
    command_line->AppendSwitch("enable-print-preview");
  }

 private:
  IMPLEMENT_REFCOUNTING(PrintPreviewBrowserTest);
};

// Observe and drive the non-CEF preview WebContents through a DevTools target
// session attached using the existing browser-host DevTools API.
class PrintPreviewTestHandler : public TestHandler,
                                public CefDevToolsMessageObserver {
 public:
  explicit PrintPreviewTestHandler(bool navigate = false)
      : navigate_(navigate) {}

  void RunTest() override {
    SetUseViews(true);
    AddResource(kTestUrl,
                "<!doctype html><title>Print preview test</title>"
                "<h1>Print preview regression test</h1><p>PDF content</p>",
                "text/html");
    AddResource(kNavigationUrl,
                "<!doctype html><title>Navigated print preview test</title>"
                "<h1>Print preview after navigation</h1>",
                "text/html");
    SetTestTimeout(15000);
    CreateReusableBrowser(kTestUrl);
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool is_loading,
                            bool can_go_back,
                            bool can_go_forward) override {
    if (is_loading) {
      return;
    }
    const auto url = browser->GetMainFrame()->GetURL();
    if (navigate_ && url == kNavigationUrl) {
      navigation_loaded_ = true;
      MaybeVerifyRenderer();
      return;
    }
    if (started_ || url != kTestUrl) {
      return;
    }
    // Wait for the requested document to finish loading, so printing cannot
    // race its initial navigation (or the initial about:blank document).
    started_ = true;
    registration_ = browser->GetHost()->AddDevToolsMessageObserver(this);
    auto params = CefDictionaryValue::Create();
    params->SetBool("discover", true);
    discover_id_ = next_id_;
    Execute("Target.setDiscoverTargets", params);
  }

  void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                              int message_id,
                              bool success,
                              const void* result,
                              size_t result_size) override {
    // Execute can synchronously respond before returning the allocated ID.
    CefPostTask(TID_UI,
                base::BindOnce(&PrintPreviewTestHandler::ProcessMethodResult,
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
    ASSERT_TRUE(success) << result;
    auto value = Parse(result.data(), result.size());
    ASSERT_TRUE(value);
    if (message_id == discover_id_) {
      GetBrowser()->GetHost()->Print();
    } else if (message_id == attach_id_) {
      session_id_ = value->GetString("sessionId");
      ASSERT_FALSE(session_id_.empty());
      SendToPreview(1, "Runtime.enable");
    } else if (message_id == alive_id_) {
      awaiting_alive_ = false;
      EXPECT_FALSE(value->HasKey("exceptionDetails"));
      EXPECT_EQ(navigation_loaded_ ? "Navigated print preview test"
                                   : "Print preview test",
                value->GetDictionary("result")->GetString("value").ToString());
      if (++dismissed_count_ == 2) {
        DestroyTest();
      } else {
        // Reopening also verifies that dismissal releases the modal UI scope.
        GetBrowser()->GetHost()->Print();
      }
    }
  }

  void OnDevToolsEvent(CefRefPtr<CefBrowser> browser,
                       const CefString& method,
                       const void* params,
                       size_t params_size) override {
    if (finishing_) {
      return;
    }
    auto value = Parse(params, params_size);
    ASSERT_TRUE(value);
    if (method == "Target.targetCreated" ||
        method == "Target.targetInfoChanged") {
      auto info = value->GetDictionary("targetInfo");
      if (preview_id_.empty() && info->GetString("url") == "chrome://print/") {
        preview_id_ = info->GetString("targetId");
        auto attach = CefDictionaryValue::Create();
        attach->SetString("targetId", preview_id_);
        attach->SetBool("flatten", false);
        attach_id_ = next_id_;
        Execute("Target.attachToTarget", attach);
      }
    } else if (method == "Target.receivedMessageFromTarget" &&
               value->GetString("sessionId") == session_id_) {
      const std::string message = value->GetString("message");
      auto nested = Parse(message.data(), message.size());
      ASSERT_TRUE(nested);
      OnPreviewMessage(nested);
    } else if (method == "Target.targetDestroyed" &&
               value->GetString("targetId") == preview_id_) {
      EXPECT_EQ(dismissed_count_ + 1, loaded_count_);
      preview_id_.clear();
      session_id_.clear();
      waiting_for_pdf_ = false;
      MaybeVerifyRenderer();
    } else if (method == "Target.targetCrashed") {
      ADD_FAILURE() << "DevTools target crashed during print preview";
      DestroyTest();
    }
  }

  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                 TerminationStatus status,
                                 int error_code,
                                 const CefString& error_string) override {
    ADD_FAILURE() << "Print initiator crashed: " << status << " "
                  << error_string.ToString();
    DestroyTest();
  }

  void OnTestTimeout(int timeout_ms, bool treat_as_error) override {
    timed_out_ = true;
    TestHandler::OnTestTimeout(timeout_ms, treat_as_error);
  }

  void DestroyTest() override {
    if (timed_out_) {
      finishing_ = true;
      registration_ = nullptr;
      TestHandler::DestroyTest();
      return;
    }
    if (finishing_) {
      return;
    }
    finishing_ = true;
    EXPECT_EQ(2, loaded_count_);
    EXPECT_EQ(2, dismissed_count_);
    CefPostTask(TID_UI,
                base::BindOnce(&PrintPreviewTestHandler::CompleteTest, this));
  }

 private:
  void Navigate() {
    if (!finishing_) {
      GetBrowser()->GetMainFrame()->LoadURL(kNavigationUrl);
    }
  }

  void MaybeVerifyRenderer() {
    if (finishing_ || awaiting_alive_ || !preview_id_.empty() ||
        loaded_count_ != dismissed_count_ + 1 ||
        (navigate_ && dismissed_count_ == 0 && !navigation_loaded_)) {
      return;
    }
    // Wait for both preview destruction and the new document load when
    // navigating. Then verify that the initiating browser remains responsive.
    awaiting_alive_ = true;
    auto evaluate = CefDictionaryValue::Create();
    evaluate->SetString("expression", "document.title");
    alive_id_ = next_id_;
    Execute("Runtime.evaluate", evaluate);
  }

  void CompleteTest() {
    if (timed_out_) {
      return;
    }
    if (!registration_) {
      TestHandler::DestroyTest();
      return;
    }
    // Both preview targets have closed, so their nested sessions are gone.
    // Stop global discovery before releasing this browser to another test.
    auto params = CefDictionaryValue::Create();
    params->SetBool("discover", false);
    cleanup_id_ = GetBrowser()->GetHost()->ExecuteDevToolsMethod(
        0, "Target.setDiscoverTargets", params);
    EXPECT_GT(cleanup_id_, 0);
  }

  static CefRefPtr<CefDictionaryValue> Parse(const void* data, size_t size) {
    auto value = CefParseJSON(data, size, JSON_PARSER_RFC);
    return value && value->GetType() == VTYPE_DICTIONARY
               ? value->GetDictionary()
               : nullptr;
  }

  void Execute(const std::string& method,
               CefRefPtr<CefDictionaryValue> params) {
    const int id = next_id_++;
    // Protocol commands can synchronously dispatch responses and events. Do
    // not reenter the DevTools observer from one of its own callbacks.
    CefPostTask(TID_UI, base::BindOnce(&PrintPreviewTestHandler::ExecuteOnUI,
                                       this, id, method, params));
  }

  void ExecuteOnUI(int id,
                   const std::string& method,
                   CefRefPtr<CefDictionaryValue> params) {
    if (finishing_) {
      return;
    }
    const int actual_id =
        GetBrowser()->GetHost()->ExecuteDevToolsMethod(0, method, params);
    EXPECT_GT(actual_id, 0);
    message_ids_[actual_id] = id;
  }

  void SendToPreview(int id,
                     const std::string& method,
                     CefRefPtr<CefDictionaryValue> params = nullptr) {
    auto message = CefDictionaryValue::Create();
    message->SetInt("id", id);
    message->SetString("method", method);
    if (params) {
      message->SetDictionary("params", params);
    }
    auto value = CefValue::Create();
    value->SetDictionary(message);
    auto send = CefDictionaryValue::Create();
    send->SetString("sessionId", session_id_);
    send->SetString("message", CefWriteJSON(value, JSON_WRITER_DEFAULT));
    Execute("Target.sendMessageToTarget", send);
  }

  void OnPreviewMessage(CefRefPtr<CefDictionaryValue> message) {
    ASSERT_FALSE(message->HasKey("error"))
        << message->GetDictionary("error")->GetString("message").ToString();
    if (message->GetString("method") == "Runtime.executionContextCreated") {
      auto context = message->GetDictionary("params")->GetDictionary("context");
      auto auxiliary = context->GetDictionary("auxData");
      if (!waiting_for_pdf_ && auxiliary && auxiliary->GetBool("isDefault") &&
          context->GetString("origin").ToString().starts_with(
              "chrome://print")) {
        waiting_for_pdf_ = true;
        auto evaluate = CefDictionaryValue::Create();
        evaluate->SetInt("contextId", context->GetInt("id"));
        evaluate->SetBool("awaitPromise", true);
        evaluate->SetBool("returnByValue", true);
        evaluate->SetString("expression", R"JS((async () => {
          if (document.readyState === 'loading') {
            await new Promise(resolve =>
                document.addEventListener('DOMContentLoaded', resolve,
                                          {once: true}));
          }
          await customElements.whenDefined('print-preview-app');
          const app = document.querySelector('print-preview-app');
          await app.updateComplete;
          const preview = app.shadowRoot.querySelector('#previewArea');
          // previewLoaded() requires both document readiness and a successful
          // documentLoaded notification from the PDF viewer.
          await new Promise((resolve, reject) => {
            const check = () => {
              if (preview.previewState === 'error') {
                reject(new Error('PDF preview failed'));
              } else if (preview.previewLoaded() &&
                         preview.previewState === 'display-preview') {
                preview.removeEventListener('preview-state-changed', check);
                resolve();
              }
            };
            preview.addEventListener('preview-state-changed', check);
            check();
          });
          // Allow the loaded PDF and removal of the loading overlay to paint.
          await new Promise(resolve => requestAnimationFrame(() =>
              requestAnimationFrame(resolve)));
          const bounds = preview.getBoundingClientRect();
          return document.visibilityState === 'visible' &&
                 bounds.width > 0 && bounds.height > 0 &&
                 preview.previewLoaded();
        })())JS");
        SendToPreview(2, "Runtime.evaluate", evaluate);
      }
    } else if (message->GetInt("id") == 2) {
      auto result = message->GetDictionary("result");
      ASSERT_TRUE(result);
      ASSERT_FALSE(result->HasKey("exceptionDetails"));
      ASSERT_TRUE(result->GetDictionary("result")->GetBool("value"));
      ++loaded_count_;
      if (navigate_ && loaded_count_ == 1) {
        CefPostTask(TID_UI,
                    base::BindOnce(&PrintPreviewTestHandler::Navigate, this));
        return;
      }
      auto cancel = CefDictionaryValue::Create();
      cancel->SetString("expression", R"JS(
        document.querySelector('print-preview-app').shadowRoot
            .querySelector('print-preview-sidebar').shadowRoot
            .querySelector('print-preview-button-strip').shadowRoot
            .querySelector('.cancel-button').click();
      )JS");
      SendToPreview(3, "Runtime.evaluate", cancel);
    }
  }

  const bool navigate_;
  bool navigation_loaded_ = false;
  bool awaiting_alive_ = false;
  bool started_ = false;
  bool finishing_ = false;
  bool timed_out_ = false;
  bool waiting_for_pdf_ = false;
  int next_id_ = 1;
  int discover_id_ = 0;
  int cleanup_id_ = 0;
  std::map<int, int> message_ids_;
  int attach_id_ = 0;
  int alive_id_ = 0;
  int loaded_count_ = 0;
  int dismissed_count_ = 0;
  std::string preview_id_;
  std::string session_id_;
  CefRefPtr<CefRegistration> registration_;

  IMPLEMENT_REFCOUNTING(PrintPreviewTestHandler);
};

TEST(PrintPreviewTest, DisplayPdfAndDismiss) {
  CefRefPtr<PrintPreviewTestHandler> handler = new PrintPreviewTestHandler;
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(PrintPreviewTest, DismissOnNavigation) {
  CefRefPtr<PrintPreviewTestHandler> handler =
      new PrintPreviewTestHandler(true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

}  // namespace

void CreatePrintPreviewBrowserTests(
    client::ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new PrintPreviewBrowserTest);
}
