// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "include/base/cef_callback.h"
#include "include/cef_callback.h"
#include "include/cef_origin_whitelist.h"
#include "include/cef_request_context.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_scheme.h"
#include "include/test/cef_test_helpers.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_suite.h"
#include "tests/ceftests/test_util.h"

namespace {

class TestResults {
 public:
  TestResults() = default;

  // Used for running tests in a custom request context.
  CefRefPtr<CefRequestContext> request_context;

  std::string url;
  std::string html;
  int status_code = 200;

  // Error code set on the response.
  cef_errorcode_t response_error_code = ERR_NONE;
  // Error code expected in OnLoadError.
  cef_errorcode_t expected_error_code = ERR_NONE;

  // Used for testing redirects
  std::string redirect_url;

  // Used for testing XHR requests
  std::string sub_url;
  std::string sub_html;
  int sub_status_code = 200;
  std::string sub_allow_origin;
  std::string sub_redirect_url;
  std::string exit_url;

  // Used for testing XSS requests
  bool needs_same_origin_policy_relaxation = false;

  // Used for testing Accept-Language.
  std::string accept_language;

  // Used for testing received console messages.
  std::vector<std::string> console_messages;

  // Delay for returning scheme handler results.
  int delay = 0;

  TrackCallback got_request, got_read, got_output, got_sub_output, got_redirect,
      got_error, got_sub_error, got_sub_redirect, got_sub_request, got_sub_read,
      git_exit_success, got_exit_request;
};

// Current scheme handler object. Used when destroying the test from
// ClientSchemeHandler::ProcessRequest().
class TestSchemeHandler;
TestSchemeHandler* g_current_handler = nullptr;

class TestSchemeHandler : public TestHandler {
 public:
  explicit TestSchemeHandler(TestResults* tr) : test_results_(tr) {
    g_current_handler = this;
  }

  void RunTest() override {
    CreateBrowser(test_results_->url, test_results_->request_context);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  // Necessary to make the method public in order to destroy the test from
  // ClientSchemeHandler::ProcessRequest().
  void DestroyTest() override {
    EXPECT_TRUE(test_results_->console_messages.empty())
        << "Did not receive expected console message: "
        << test_results_->console_messages.front();

    TestHandler::DestroyTest();
  }

  void DestroyTestIfDone() {
    if (!test_results_->exit_url.empty() && !test_results_->got_exit_request) {
      return;
    }

    if (!test_results_->sub_url.empty() &&
        !(test_results_->got_sub_output || test_results_->got_sub_error ||
          test_results_->got_exit_request)) {
      return;
    }

    if (!(test_results_->got_output || test_results_->got_error)) {
      return;
    }

    DestroyTest();
  }

  bool IsExitURL(const std::string& url) const {
    return !test_results_->exit_url.empty() &&
           url.find(test_results_->exit_url) != std::string::npos;
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    if (request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return RV_CANCEL;
    }

    const std::string& newUrl = request->GetURL();
    if (IsExitURL(newUrl)) {
      test_results_->got_exit_request.yes();
      // XHR tests use an exit URL to destroy the test.
      if (newUrl.find("SUCCESS") != std::string::npos) {
        test_results_->git_exit_success.yes();
      }
      DestroyTestIfDone();
      return RV_CANCEL;
    }

    if (!test_results_->sub_redirect_url.empty() &&
        newUrl == test_results_->sub_redirect_url) {
      test_results_->got_sub_redirect.yes();
      // Redirect to the sub URL.
      request->SetURL(test_results_->sub_url);
    } else if (newUrl == test_results_->redirect_url) {
      test_results_->got_redirect.yes();

      // No read should have occurred for the redirect.
      EXPECT_TRUE(test_results_->got_request);
      EXPECT_FALSE(test_results_->got_read);

      // Now loading the redirect URL.
      test_results_->url = test_results_->redirect_url;
      test_results_->redirect_url.clear();
    }

    return RV_CONTINUE;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    const std::string& url = frame->GetURL();
    if (url == test_results_->url) {
      test_results_->got_output.yes();
    } else if (url == test_results_->sub_url) {
      test_results_->got_sub_output.yes();
    } else if (IsExitURL(url)) {
      return;
    }

    if (url == test_results_->url || test_results_->status_code != 200) {
      // Test that the status code is correct.
      EXPECT_EQ(httpStatusCode, test_results_->status_code);
    }

    DestroyTestIfDone();
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    const std::string& url = failedUrl;
    if (url == test_results_->url) {
      test_results_->got_error.yes();
    } else if (url == test_results_->sub_url) {
      test_results_->got_sub_error.yes();
    } else if (IsExitURL(url)) {
      return;
    }

    // Tests sometimes also fail with ERR_ABORTED or ERR_UNKNOWN_URL_SCHEME.
    if (!(test_results_->expected_error_code == 0 &&
          (errorCode == ERR_ABORTED || errorCode == ERR_UNKNOWN_URL_SCHEME))) {
      EXPECT_EQ(test_results_->expected_error_code, errorCode)
          << failedUrl.ToString();
    }

    DestroyTestIfDone();
  }

  bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                        cef_log_severity_t level,
                        const CefString& message,
                        const CefString& source,
                        int line) override {
    bool expected = false;
    const std::string& actual = message.ToString();

    auto it = std::find_if(test_results_->console_messages.begin(),
                           test_results_->console_messages.end(),
                           [&actual](const std::string& possible) {
                             return actual.find(possible) == 0U;
                           });

    if (it != test_results_->console_messages.end()) {
      expected = true;
      test_results_->console_messages.erase(it);
    }

    EXPECT_TRUE(expected) << "Unexpected console message: "
                          << message.ToString();
    return false;
  }

 protected:
  TestResults* const test_results_;

  IMPLEMENT_REFCOUNTING(TestSchemeHandler);
};

class ClientSchemeHandlerOld : public CefResourceHandler {
 public:
  explicit ClientSchemeHandlerOld(TestResults* tr) : test_results_(tr) {}

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    bool handled = false;

    std::string url = request->GetURL();
    is_sub_ =
        (!test_results_->sub_url.empty() && test_results_->sub_url == url);

    if (is_sub_) {
      test_results_->got_sub_request.yes();

      if (!test_results_->sub_html.empty()) {
        handled = true;
      }
    } else {
      EXPECT_EQ(url, test_results_->url);

      test_results_->got_request.yes();

      if (!test_results_->html.empty()) {
        handled = true;
      }
    }

    std::string accept_language;
    CefRequest::HeaderMap headerMap;
    CefRequest::HeaderMap::iterator headerIter;
    request->GetHeaderMap(headerMap);
    headerIter = headerMap.find("Accept-Language");
    if (headerIter != headerMap.end()) {
      accept_language = headerIter->second;
    }
    EXPECT_TRUE(!accept_language.empty());

    if (!test_results_->accept_language.empty()) {
      // Value from CefRequestContextSettings.accept_language_list.
      EXPECT_STREQ(test_results_->accept_language.data(),
                   accept_language.data());
    } else {
      // CEF_SETTINGS_ACCEPT_LANGUAGE value from
      // CefSettings.accept_language_list set in CefTestSuite::GetSettings()
      // and expanded internally by ComputeAcceptLanguageFromPref.
      EXPECT_STREQ("en-GB,en;q=0.9", accept_language.data());
    }

    if (handled) {
      if (test_results_->delay > 0) {
        // Continue after the delay.
        CefPostDelayedTask(
            TID_IO, base::BindOnce(&CefCallback::Continue, callback.get()),
            test_results_->delay);
      } else {
        // Continue immediately.
        callback->Continue();
      }
      return true;
    } else if (test_results_->response_error_code != ERR_NONE) {
      // Propagate the error code.
      callback->Continue();
      return true;
    }

    // Response was canceled.
    if (g_current_handler) {
      g_current_handler->DestroyTest();
    }
    return false;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    CefResponse::HeaderMap headers;

    if (is_sub_) {
      response->SetStatus(test_results_->sub_status_code);

      if (!test_results_->sub_allow_origin.empty()) {
        // Set the Access-Control-Allow-Origin header to allow cross-domain
        // scripting.
        headers.insert(std::make_pair("Access-Control-Allow-Origin",
                                      test_results_->sub_allow_origin));
      }

      if (!test_results_->sub_html.empty()) {
        response->SetMimeType("text/html");
        response_length = test_results_->sub_html.size();
      }
    } else if (!test_results_->redirect_url.empty()) {
      redirectUrl = test_results_->redirect_url;
    } else if (test_results_->response_error_code != ERR_NONE) {
      response->SetError(test_results_->response_error_code);
    } else {
      response->SetStatus(test_results_->status_code);

      if (!test_results_->html.empty()) {
        response->SetMimeType("text/html");
        response_length = test_results_->html.size();
      }
    }

    if (test_results_->needs_same_origin_policy_relaxation) {
      // Apply same-origin policy relaxation for document.domain.
      headers.insert(std::make_pair("Origin-Agent-Cluster", "?0"));
    }

    if (!headers.empty()) {
      response->SetHeaderMap(headers);
    }
  }

  void Cancel() override { EXPECT_TRUE(CefCurrentlyOn(TID_IO)); }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    if (test_results_->delay > 0) {
      if (!has_delayed_) {
        // Continue after a delay.
        CefPostDelayedTask(
            TID_IO,
            base::BindOnce(&ClientSchemeHandlerOld::ContinueAfterDelay, this,
                           callback),
            test_results_->delay);
        bytes_read = 0;
        return true;
      }

      has_delayed_ = false;
    }

    std::string* data;

    if (is_sub_) {
      test_results_->got_sub_read.yes();
      data = &test_results_->sub_html;
    } else {
      test_results_->got_read.yes();
      data = &test_results_->html;
    }

    bool has_data = false;
    bytes_read = 0;

    size_t size = data->size();
    if (offset_ < size) {
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(size - offset_));
      memcpy(data_out, data->c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

 private:
  void ContinueAfterDelay(CefRefPtr<CefCallback> callback) {
    has_delayed_ = true;
    callback->Continue();
  }

  TestResults* const test_results_;
  size_t offset_ = 0;
  bool is_sub_ = false;
  bool has_delayed_ = false;

  IMPLEMENT_REFCOUNTING(ClientSchemeHandlerOld);
  DISALLOW_COPY_AND_ASSIGN(ClientSchemeHandlerOld);
};

class ClientSchemeHandler : public CefResourceHandler {
 public:
  explicit ClientSchemeHandler(TestResults* tr) : test_results_(tr) {}

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    if (request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return false;
    }

    bool handled = false;

    std::string url = request->GetURL();
    is_sub_ =
        (!test_results_->sub_url.empty() && test_results_->sub_url == url);

    if (is_sub_) {
      test_results_->got_sub_request.yes();

      if (!test_results_->sub_html.empty()) {
        handled = true;
      }
    } else {
      EXPECT_EQ(url, test_results_->url);

      test_results_->got_request.yes();

      if (!test_results_->html.empty()) {
        handled = true;
      }
    }

    std::string accept_language;
    CefRequest::HeaderMap headerMap;
    CefRequest::HeaderMap::iterator headerIter;
    request->GetHeaderMap(headerMap);
    headerIter = headerMap.find("Accept-Language");
    if (headerIter != headerMap.end()) {
      accept_language = headerIter->second;
    }
    EXPECT_TRUE(!accept_language.empty());

    if (!test_results_->accept_language.empty()) {
      // Value from CefRequestContextSettings.accept_language_list.
      EXPECT_STREQ(test_results_->accept_language.data(),
                   accept_language.data());
    } else {
      // CEF_SETTINGS_ACCEPT_LANGUAGE value from
      // CefSettings.accept_language_list set in CefTestSuite::GetSettings()
      // and expanded internally by ComputeAcceptLanguageFromPref.
      if (CefIsFeatureEnabledForTests("ReduceAcceptLanguage")) {
        EXPECT_TRUE(accept_language == "en-GB" ||
                    accept_language == "en-GB,en;q=0.9")
            << accept_language;
      } else {
        EXPECT_STREQ("en-GB,en;q=0.9", accept_language.data());
      }
    }

    // Continue or cancel the request immediately based on the return value.
    handle_request = true;

    if (handled) {
      if (test_results_->delay > 0) {
        // Continue after the delay.
        handle_request = false;
        CefPostDelayedTask(
            TID_FILE_USER_BLOCKING,
            base::BindOnce(&CefCallback::Continue, callback.get()),
            test_results_->delay);
      }
      return true;
    } else if (test_results_->response_error_code != ERR_NONE) {
      // Propagate the error code.
      return true;
    }

    // Response was canceled.
    if (g_current_handler) {
      g_current_handler->DestroyTest();
    }
    return false;
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    if (request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return false;
    }

    EXPECT_TRUE(false);  // Not reached.
    return false;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    CefResponse::HeaderMap headers;

    if (is_sub_) {
      response->SetStatus(test_results_->sub_status_code);

      if (!test_results_->sub_allow_origin.empty()) {
        // Set the Access-Control-Allow-Origin header to allow cross-domain
        // scripting.
        headers.insert(std::make_pair("Access-Control-Allow-Origin",
                                      test_results_->sub_allow_origin));
      }

      if (!test_results_->sub_html.empty()) {
        response->SetMimeType("text/html");
        response_length = test_results_->sub_html.size();
      }
    } else if (!test_results_->redirect_url.empty()) {
      redirectUrl = test_results_->redirect_url;
    } else if (test_results_->response_error_code != ERR_NONE) {
      response->SetError(test_results_->response_error_code);
    } else {
      response->SetStatus(test_results_->status_code);

      if (!test_results_->html.empty()) {
        response->SetMimeType("text/html");
        response_length = test_results_->html.size();
      }
    }

    if (test_results_->needs_same_origin_policy_relaxation) {
      // Apply same-origin policy relaxation for document.domain.
      headers.insert(std::make_pair("Origin-Agent-Cluster", "?0"));
    }

    if (!headers.empty()) {
      response->SetHeaderMap(headers);
    }
  }

  void Cancel() override { EXPECT_TRUE(CefCurrentlyOn(TID_IO)); }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    if (test_results_->delay > 0) {
      if (!has_delayed_) {
        // Continue after a delay.
        CefPostDelayedTask(
            TID_FILE_USER_BLOCKING,
            base::BindOnce(&ClientSchemeHandler::ContinueAfterDelay, this,
                           data_out, bytes_to_read, callback),
            test_results_->delay);
        bytes_read = 0;
        return true;
      }

      has_delayed_ = false;
    }

    return GetData(data_out, bytes_to_read, bytes_read);
  }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(false);  // Not reached.
    bytes_read = -2;
    return false;
  }

 private:
  void ContinueAfterDelay(void* data_out,
                          int bytes_to_read,
                          CefRefPtr<CefResourceReadCallback> callback) {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    has_delayed_ = true;

    int bytes_read = 0;
    GetData(data_out, bytes_to_read, bytes_read);
    callback->Continue(bytes_read);
  }

  bool GetData(void* data_out, int bytes_to_read, int& bytes_read) {
    std::string* data;

    if (is_sub_) {
      test_results_->got_sub_read.yes();
      data = &test_results_->sub_html;
    } else {
      test_results_->got_read.yes();
      data = &test_results_->html;
    }

    // Default to response complete.
    bool has_data = false;
    bytes_read = 0;

    size_t size = data->size();
    if (offset_ < size) {
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(size - offset_));
      memcpy(data_out, data->c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

  TestResults* const test_results_;
  size_t offset_ = 0;
  bool is_sub_ = false;
  bool has_delayed_ = false;

  IMPLEMENT_REFCOUNTING(ClientSchemeHandler);
  DISALLOW_COPY_AND_ASSIGN(ClientSchemeHandler);
};

class ClientSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  explicit ClientSchemeHandlerFactory(TestResults* tr) : test_results_(tr) {}

  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    if (TestOldResourceAPI()) {
      return new ClientSchemeHandlerOld(test_results_);
    }
    return new ClientSchemeHandler(test_results_);
  }

  TestResults* const test_results_;

  IMPLEMENT_REFCOUNTING(ClientSchemeHandlerFactory);
  DISALLOW_COPY_AND_ASSIGN(ClientSchemeHandlerFactory);
};

// If |domain| is empty the scheme will be registered as non-standard.
void RegisterTestScheme(TestResults* test_results,
                        const std::string& scheme,
                        const std::string& domain) {
  if (test_results->request_context) {
    EXPECT_TRUE(test_results->request_context->RegisterSchemeHandlerFactory(
        scheme, domain, new ClientSchemeHandlerFactory(test_results)));
  } else {
    EXPECT_TRUE(CefRegisterSchemeHandlerFactory(
        scheme, domain, new ClientSchemeHandlerFactory(test_results)));
  }
  WaitForIOThread();
}

void ClearTestSchemes(TestResults* test_results) {
  if (test_results->request_context) {
    EXPECT_TRUE(test_results->request_context->ClearSchemeHandlerFactories());
  } else {
    EXPECT_TRUE(CefClearSchemeHandlerFactories());
  }
  WaitForIOThread();
}

struct XHRTestSettings {
  XHRTestSettings() = default;

  std::string url;
  std::string sub_url;
  std::string sub_allow_origin;
  std::string sub_redirect_url;
  bool synchronous = true;
};

void SetUpXHR(TestResults* test_results, const XHRTestSettings& settings) {
  test_results->sub_url = settings.sub_url;
  test_results->sub_html = "SUCCESS";
  test_results->sub_allow_origin = settings.sub_allow_origin;
  test_results->sub_redirect_url = settings.sub_redirect_url;

  std::string request_url;
  if (!settings.sub_redirect_url.empty()) {
    request_url = settings.sub_redirect_url;
  } else {
    request_url = settings.sub_url;
  }

  test_results->url = settings.url;
  std::stringstream ss;
  ss << "<html><head>"
        "<script language=\"JavaScript\">"
        "function onResult(val) {"
        "  document.location = \"https://tests/exit?result=\"+val;"
        "}"
        "function execXMLHttpRequest() {";
  if (settings.synchronous) {
    ss << "var result = 'FAILURE';"
          "try {"
          "  xhr = new XMLHttpRequest();"
          "  xhr.open(\"GET\", \""
       << request_url.c_str()
       << "\", false);"
          "  xhr.send();"
          "  result = xhr.responseText;"
          "} catch(e) {}"
          "onResult(result)";
  } else {
    ss << "xhr = new XMLHttpRequest();"
          "xhr.open(\"GET\", \""
       << request_url.c_str()
       << "\", true);"
          "xhr.onload = function(e) {"
          "  if (xhr.readyState === 4) {"
          "    if (xhr.status === 200) {"
          "      onResult(xhr.responseText);"
          "    } else {"
          "      console.log('XMLHttpRequest failed with status ' + "
          "xhr.status);"
          "      onResult('FAILURE');"
          "    }"
          "  }"
          "};"
          "xhr.onerror = function(e) {"
          "  onResult('FAILURE');"
          "};"
          "xhr.send()";
  }
  ss << "}"
        "</script>"
        "</head><body onload=\"execXMLHttpRequest();\">"
        "Running execXMLHttpRequest..."
        "</body></html>";
  test_results->html = ss.str();

  test_results->exit_url = "https://tests/exit";
}

struct FetchTestSettings {
  FetchTestSettings() = default;

  std::string url;
  std::string sub_url;
  std::string sub_allow_origin;
  std::string sub_redirect_url;
};

void SetUpFetch(TestResults* test_results, const FetchTestSettings& settings) {
  test_results->sub_url = settings.sub_url;
  test_results->sub_html = "SUCCESS";
  test_results->sub_allow_origin = settings.sub_allow_origin;
  test_results->sub_redirect_url = settings.sub_redirect_url;

  std::string request_url;
  if (!settings.sub_redirect_url.empty()) {
    request_url = settings.sub_redirect_url;
  } else {
    request_url = settings.sub_url;
  }

  test_results->url = settings.url;
  std::stringstream ss;
  ss << "<html><head>"
        "<script language=\"JavaScript\">"
        "function onResult(val) {"
        "  document.location = \"https://tests/exit?result=\"+val;"
        "}"
        "function execFetchHttpRequest() {";
  ss << "fetch('" << request_url.c_str()
     << "')"
        ".then(function(response) {"
        "  if (response.status === 200) {"
        "      response.text().then(function(text) {"
        "          onResult(text);"
        "      }).catch(function(e) {"
        "          onResult('FAILURE');        "
        "      });"
        "  } else {"
        "      onResult('FAILURE');"
        "  }"
        "}).catch(function(e) {"
        "  onResult('FAILURE');"
        "});"
     << "}"
        "</script>"
        "</head><body onload=\"execFetchHttpRequest();\">"
        "Running execFetchHttpRequest..."
        "</body></html>";
  test_results->html = ss.str();

  test_results->exit_url = "https://tests/exit";
}  // namespace

void SetUpXSS(TestResults* test_results,
              const std::string& url,
              const std::string& sub_url,
              const std::string& domain = std::string()) {
  // 1. Load |url| which contains an iframe.
  // 2. The iframe loads |sub_url|.
  // 3. |sub_url| tries to call a JS function in |url|.
  // 4. |url| tries to call a JS function in |sub_url|.

  std::stringstream ss;
  std::string domain_line;
  if (!domain.empty()) {
    domain_line = "document.domain = '" + domain + "';";
    if (url.find("http") == 0 && sub_url.find("http") == 0) {
      test_results->needs_same_origin_policy_relaxation = true;
    }
  }

  test_results->sub_url = sub_url;
  ss << "<html><head>"
        "<script language=\"JavaScript\">"
     << domain_line
     << "function getResult() {"
        "  return 'SUCCESS';"
        "}"
        "function execXSSRequest() {"
        "  var result = 'FAILURE';"
        "  try {"
        "    result = parent.getResult();"
        "  } catch(e) { console.log(e.stack); }"
        "  document.location = \"https://tests/exit?result=\"+result;"
        "}"
        "</script>"
        "</head><body onload=\"execXSSRequest();\">"
        "Running execXSSRequest..."
        "</body></html>";
  test_results->sub_html = ss.str();

  test_results->url = url;
  ss.str("");
  ss << "<html><head>"
        "<script language=\"JavaScript\">"
     << domain_line
     << ""
        "function getResult() {"
        "  try {"
        "    return document.getElementById('s').contentWindow.getResult();"
        "  } catch(e) { console.log(e.stack); }"
        "  return 'FAILURE';"
        "}"
        "</script>"
        "</head><body>"
        "<iframe src=\""
     << sub_url.c_str()
     << "\" id=\"s\">"
        "</body></html>";
  test_results->html = ss.str();

  test_results->exit_url = "https://tests/exit";
}

}  // namespace

// Test that scheme registration/unregistration works as expected.
TEST(SchemeHandlerTest, Registration) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");
  test_results.url = "customstd://test/run.html";
  test_results.html =
      "<html><head></head><body><h1>Success!</h1></body></html>";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);

  // Unregister the handler.
  EXPECT_TRUE(CefRegisterSchemeHandlerFactory("customstd", "test", nullptr));
  WaitForIOThread();

  test_results.got_request.reset();
  test_results.got_read.reset();
  test_results.got_output.reset();
  test_results.expected_error_code = ERR_UNKNOWN_URL_SCHEME;
  handler->ExecuteTest();

  EXPECT_TRUE(test_results.got_error);
  EXPECT_FALSE(test_results.got_request);
  EXPECT_FALSE(test_results.got_read);
  EXPECT_FALSE(test_results.got_output);

  // Re-register the handler.
  EXPECT_TRUE(CefRegisterSchemeHandlerFactory(
      "customstd", "test", new ClientSchemeHandlerFactory(&test_results)));
  WaitForIOThread();

  test_results.got_error.reset();
  test_results.expected_error_code = ERR_NONE;
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_results.got_error);
  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can return normal results.
TEST(SchemeHandlerTest, CustomStandardNormalResponse) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");
  test_results.url = "customstd://test/run.html";
  test_results.html =
      "<html><head></head><body><h1>Success!</h1></body></html>";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can return normal results with delayed
// responses.
TEST(SchemeHandlerTest, CustomStandardNormalResponseDelayed) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");
  test_results.url = "customstd://test/run.html";
  test_results.html =
      "<html><head></head><body><h1>Success!</h1></body></html>";
  test_results.delay = 100;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);

  ClearTestSchemes(&test_results);
}

// Test that a custom nonstandard scheme can return normal results.
TEST(SchemeHandlerTest, CustomNonStandardNormalResponse) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customnonstd", std::string());
  test_results.url = "customnonstd:some%20value";
  test_results.html =
      "<html><head></head><body><h1>Success!</h1></body></html>";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can return an error code.
TEST(SchemeHandlerTest, CustomStandardErrorResponse) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");
  test_results.url = "customstd://test/run.html";
  test_results.html = "<html><head></head><body><h1>404</h1></body></html>";
  test_results.status_code = 404;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can return a CEF error code in the
// response.
TEST(SchemeHandlerTest, CustomStandardErrorCodeResponse) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");
  test_results.url = "customstd://test/run.html";
  test_results.response_error_code = ERR_FILE_TOO_BIG;
  test_results.expected_error_code = ERR_FILE_TOO_BIG;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_FALSE(test_results.got_read);
  EXPECT_FALSE(test_results.got_output);
  EXPECT_TRUE(test_results.got_error);

  ClearTestSchemes(&test_results);
}

// Test that a custom nonstandard scheme can return an error code.
TEST(SchemeHandlerTest, CustomNonStandardErrorResponse) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customnonstd", std::string());
  test_results.url = "customnonstd:some%20value";
  test_results.html = "<html><head></head><body><h1>404</h1></body></html>";
  test_results.status_code = 404;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);

  ClearTestSchemes(&test_results);
}

// Test that custom standard scheme handling fails when the scheme name is
// incorrect.
TEST(SchemeHandlerTest, CustomStandardNameNotHandled) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");
  test_results.url = "customstd2://test/run.html";
  test_results.expected_error_code = ERR_UNKNOWN_URL_SCHEME;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_results.got_request);
  EXPECT_FALSE(test_results.got_read);
  EXPECT_FALSE(test_results.got_output);
  EXPECT_TRUE(test_results.got_error);

  ClearTestSchemes(&test_results);
}

// Test that custom nonstandard scheme handling fails when the scheme name is
// incorrect.
TEST(SchemeHandlerTest, CustomNonStandardNameNotHandled) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customnonstd", std::string());
  test_results.url = "customnonstd2:some%20value";
  test_results.expected_error_code = ERR_UNKNOWN_URL_SCHEME;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_results.got_request);
  EXPECT_FALSE(test_results.got_read);
  EXPECT_FALSE(test_results.got_output);
  EXPECT_TRUE(test_results.got_error);

  ClearTestSchemes(&test_results);
}

// Test that custom standard scheme handling fails when the domain name is
// incorrect.
TEST(SchemeHandlerTest, CustomStandardDomainNotHandled) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");
  test_results.url = "customstd://noexist/run.html";
  test_results.expected_error_code = ERR_UNKNOWN_URL_SCHEME;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_results.got_request);
  EXPECT_FALSE(test_results.got_read);
  EXPECT_FALSE(test_results.got_output);
  EXPECT_TRUE(test_results.got_error);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can return no response.
TEST(SchemeHandlerTest, CustomStandardNoResponse) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");
  test_results.url = "customstd://test/run.html";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_FALSE(test_results.got_read);
  EXPECT_FALSE(test_results.got_output);

  ClearTestSchemes(&test_results);
}

// Test that a custom nonstandard scheme can return no response.
TEST(SchemeHandlerTest, CustomNonStandardNoResponse) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customnonstd", std::string());
  test_results.url = "customnonstd:some%20value";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_FALSE(test_results.got_read);
  EXPECT_FALSE(test_results.got_output);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate redirects.
TEST(SchemeHandlerTest, CustomStandardRedirect) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");
  test_results.url = "customstd://test/run.html";
  test_results.redirect_url = "customstd://test/redirect.html";
  test_results.html =
      "<html><head></head><body><h1>Redirected</h1></body></html>";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_redirect);

  ClearTestSchemes(&test_results);
}

// Test that a custom nonstandard scheme can generate redirects.
TEST(SchemeHandlerTest, CustomNonStandardRedirect) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customnonstd", std::string());
  test_results.url = "customnonstd:some%20value";
  test_results.redirect_url = "customnonstd:some%20other%20value";
  test_results.html =
      "<html><head></head><body><h1>Redirected</h1></body></html>";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_redirect);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate same origin XHR requests.
TEST(SchemeHandlerTest, CustomStandardXHRSameOriginSync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");

  XHRTestSettings settings;
  settings.url = "customstd://test/run.html";
  settings.sub_url = "customstd://test/xhr.html";
  SetUpXHR(&test_results, settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate same origin XHR requests.
TEST(SchemeHandlerTest, CustomStandardXHRSameOriginAsync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");

  XHRTestSettings settings;
  settings.url = "customstd://test/run.html";
  settings.sub_url = "customstd://test/xhr.html";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that custom nonstandard schemes are treated as unique origins that
// cannot generate XHR requests.
TEST(SchemeHandlerTest, CustomNonStandardXHRSameOriginSync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customnonstd", std::string());

  XHRTestSettings settings;
  settings.url = "customnonstd:some%20value";
  settings.sub_url = "customnonstd:xhr%20value";
  SetUpXHR(&test_results, settings);

  test_results.console_messages.push_back(
      "Access to XMLHttpRequest at 'customnonstd:xhr%20value' from origin "
      "'null' has been blocked by CORS policy: Cross origin requests are only "
      "supported for protocol schemes:");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_FALSE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that custom nonstandard schemes are treated as unique origins that
// cannot generate XHR requests.
TEST(SchemeHandlerTest, CustomNonStandardXHRSameOriginAsync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customnonstd", std::string());

  XHRTestSettings settings;
  settings.url = "customnonstd:some%20value";
  settings.sub_url = "customnonstd:xhr%20value";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  test_results.console_messages.push_back(
      "Access to XMLHttpRequest at 'customnonstd:xhr%20value' from origin "
      "'null' has been blocked by CORS policy: Cross origin requests are only "
      "supported for protocol schemes:");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_FALSE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a non fetch enabled custom standard scheme can't generate same
// origin Fetch requests.
TEST(SchemeHandlerTest, CustomStandardFetchSameOrigin) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");

  FetchTestSettings settings;
  settings.url = "customstd://test/run.html";
  settings.sub_url = "customstd://test/fetch.html";
  SetUpFetch(&test_results, settings);

  test_results.console_messages.push_back(
      "Fetch API cannot load customstd://test/fetch.html. URL scheme "
      "\"customstd\" is not supported.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_FALSE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a fetch enabled custom standard scheme can generate same origin
// Fetch requests.
TEST(SchemeHandlerTest, FetchCustomStandardFetchSameOrigin) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstdfetch", "test");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test/run.html";
  settings.sub_url = "customstdfetch://test/fetch.html";
  SetUpFetch(&test_results, settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that custom nonstandard schemes are treated as unique origins that
// cannot generate Fetch requests.
TEST(SchemeHandlerTest, CustomNonStandardFetchSameOrigin) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customnonstd", std::string());

  FetchTestSettings settings;
  settings.url = "customnonstd:some%20value";
  settings.sub_url = "customnonstd:xhr%20value";
  SetUpFetch(&test_results, settings);

  test_results.console_messages.push_back(
      "Fetch API cannot load customnonstd:xhr%20value. URL scheme "
      "\"customnonstd\" is not supported.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_FALSE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate same origin XSS requests.
TEST(SchemeHandlerTest, CustomStandardXSSSameOrigin) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test");
  SetUpXSS(&test_results, "customstd://test/run.html",
           "customstd://test/iframe.html");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that custom nonstandard schemes are treated as unique origins that
// cannot generate XSS requests.
TEST(SchemeHandlerTest, CustomNonStandardXSSSameOrigin) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customnonstd", std::string());
  SetUpXSS(&test_results, "customnonstd:some%20value",
           "customnonstd:xhr%20value");

  test_results.console_messages.push_back(
      "SecurityError: Failed to read a named property 'getResult' from "
      "'Window': Blocked a frame with origin \"null\" from accessing a "
      "cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme cannot generate cross-domain XHR requests
// by default. Behavior should be the same as with HTTP.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginSync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  SetUpXHR(&test_results, settings);

  test_results.console_messages.push_back(
      "Access to XMLHttpRequest at 'customstd://test2/xhr.html' from origin "
      "'customstd://test1' has been blocked by CORS policy: No "
      "'Access-Control-Allow-Origin' header is present on the requested "
      "resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme cannot generate cross-domain XHR requests
// by default. Behavior should be the same as with HTTP.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginAsync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  test_results.console_messages.push_back(
      "Access to XMLHttpRequest at 'customstd://test2/xhr.html' from origin "
      "'customstd://test1' has been blocked by CORS policy: No "
      "'Access-Control-Allow-Origin' header is present on the requested "
      "resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme cannot generate cross-domain Fetch
// requests by default. Behavior should be the same as with HTTP.
TEST(SchemeHandlerTest, CustomStandardFetchDifferentOrigin) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstdfetch", "test1");
  RegisterTestScheme(&test_results, "customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  SetUpFetch(&test_results, settings);

  test_results.console_messages.push_back(
      "Access to fetch at 'customstdfetch://test2/fetch.html' from origin "
      "'customstdfetch://test1' has been blocked by CORS policy: No "
      "'Access-Control-Allow-Origin' header is present on the requested "
      "resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme cannot generate cross-domain XSS requests
// by default.
TEST(SchemeHandlerTest, CustomStandardXSSDifferentOrigin) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");
  SetUpXSS(&test_results, "customstd://test1/run.html",
           "customstd://test2/iframe.html");

  test_results.console_messages.push_back(
      "SecurityError: Failed to read a named property 'getResult' from "
      "'Window': Blocked a frame with origin \"customstd://test2\" from "
      "accessing a cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a cross-protocol iframe load succeeds, and that the custom
// standard scheme cannot generate XSS requests to the HTTP protocol by default.
TEST(SchemeHandlerTest, CustomStandardXSSDifferentProtocolHttp) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "https", "test2");
  SetUpXSS(&test_results, "customstd://test1/run.html",
           "https://test2/iframe.html");

  test_results.console_messages.push_back(
      "SecurityError: Failed to read a named property 'getResult' from "
      "'Window': Blocked a frame with origin \"https://test2\" from accessing "
      "a cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a cross-protocol iframe load succeeds, and that the custom
// standard scheme cannot generate XSS requests to a non-standard scheme by
// default.
TEST(SchemeHandlerTest, CustomStandardXSSDifferentProtocolCustomNonStandard) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customnonstd", std::string());
  SetUpXSS(&test_results, "customstd://test1/run.html",
           "customnonstd:some%20value");

  test_results.console_messages.push_back(
      "SecurityError: Failed to read a named property 'getResult' from "
      "'Window': Blocked a frame with origin \"null\" from accessing a "
      "cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a cross-protocol iframe load succeeds, and that the HTTP protocol
// cannot generate XSS requests to the custom standard scheme by default.
TEST(SchemeHandlerTest, HttpXSSDifferentProtocolCustomStandard) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "https", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");
  SetUpXSS(&test_results, "https://test1/run.html",
           "customstd://test2/iframe.html");

  test_results.console_messages.push_back(
      "SecurityError: Failed to read a named property 'getResult' from "
      "'Window': Blocked a frame with origin \"customstd://test2\" from "
      "accessing a cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a cross-protocol iframe load succeeds, and that the HTTP protocol
// cannot generate XSS requests to the custom non-standard scheme by default.
TEST(SchemeHandlerTest, HttpXSSDifferentProtocolCustomNonStandard) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "https", "test1");
  RegisterTestScheme(&test_results, "customnonstd", std::string());
  SetUpXSS(&test_results, "https://test1/run.html",
           "customnonstd:some%20value");

  test_results.console_messages.push_back(
      "SecurityError: Failed to read a named property 'getResult' from "
      "'Window': Blocked a frame with origin \"null\" from accessing a "
      "cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that an HTTP scheme cannot generate cross-domain XHR requests by
// default.
TEST(SchemeHandlerTest, HttpXHRDifferentOriginSync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "https", "test1");
  RegisterTestScheme(&test_results, "https", "test2");

  XHRTestSettings settings;
  settings.url = "https://test1/run.html";
  settings.sub_url = "https://test2/xhr.html";
  SetUpXHR(&test_results, settings);

  test_results.console_messages.push_back(
      "Access to XMLHttpRequest at 'https://test2/xhr.html' from origin "
      "'https://test1' has been blocked by CORS policy: No "
      "'Access-Control-Allow-Origin' header is present on the requested "
      "resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that an HTTP scheme cannot generate cross-domain XHR requests by
// default.
TEST(SchemeHandlerTest, HttpXHRDifferentOriginAsync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "https", "test1");
  RegisterTestScheme(&test_results, "https", "test2");

  XHRTestSettings settings;
  settings.url = "https://test1/run.html";
  settings.sub_url = "https://test2/xhr.html";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  test_results.console_messages.push_back(
      "Access to XMLHttpRequest at 'https://test2/xhr.html' from origin "
      "'https://test1' has been blocked by CORS policy: No "
      "'Access-Control-Allow-Origin' header is present on the requested "
      "resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that an HTTP scheme cannot generate cross-domain Fetch requests by
// default.
TEST(SchemeHandlerTest, HttpFetchDifferentOriginAsync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "https", "test1");
  RegisterTestScheme(&test_results, "https", "test2");

  FetchTestSettings settings;
  settings.url = "https://test1/run.html";
  settings.sub_url = "https://test2/fetch.html";
  SetUpFetch(&test_results, settings);

  test_results.console_messages.push_back(
      "Access to fetch at 'https://test2/fetch.html' from origin "
      "'https://test1' "
      "has been blocked by CORS policy: No 'Access-Control-Allow-Origin' "
      "header is present on the requested resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that an HTTP scheme cannot generate cross-domain XSS requests by
// default.
TEST(SchemeHandlerTest, HttpXSSDifferentOrigin) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "https", "test1");
  RegisterTestScheme(&test_results, "https", "test2");
  SetUpXSS(&test_results, "https://test1/run.html", "https://test2/xss.html");

  test_results.console_messages.push_back(
      "SecurityError: Failed to read a named property 'getResult' from "
      "'Window': Blocked a frame with origin \"https://test2\" from accessing "
      "a cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate cross-domain XHR requests
// when setting the Access-Control-Allow-Origin header. Should behave the same
// as HTTP.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithHeaderSync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_allow_origin = "customstd://test1";
  SetUpXHR(&test_results, settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate cross-domain XHR requests
// when setting the Access-Control-Allow-Origin header. Should behave the same
// as HTTP.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithHeaderAsync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_allow_origin = "customstd://test1";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate cross-domain Fetch requests
// when setting the Access-Control-Allow-Origin header. Should behave the same
// as HTTP.
TEST(SchemeHandlerTest, CustomStandardFetchDifferentOriginWithHeader) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstdfetch", "test1");
  RegisterTestScheme(&test_results, "customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  settings.sub_allow_origin = "customstdfetch://test1";
  SetUpFetch(&test_results, settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate cross-domain XHR requests
// when using the cross-origin whitelist.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithWhitelistSync1) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  SetUpXHR(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2", false));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Same as above but origin whitelist matches any domain.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithWhitelistSync2) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  SetUpXHR(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              CefString(), true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Same as above but origin whitelist matches sub-domains.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithWhitelistSync3) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "a.test2.foo");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://a.test2.foo/xhr.html";
  SetUpXHR(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2.foo", true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate cross-domain XHR requests
// when using the cross-origin whitelist.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithWhitelistAsync1) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2", false));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Same as above but origin whitelist matches any domain.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithWhitelistAsync2) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              CefString(), true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Same as above but origin whitelist matches sub-domains.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithWhitelistAsync3) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "a.test2.foo");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://a.test2.foo/xhr.html";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2.foo", true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate cross-domain Fetch requests
// when using the cross-origin whitelist.
TEST(SchemeHandlerTest, CustomStandardFetchDifferentOriginWithWhitelist1) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstdfetch", "test1");
  RegisterTestScheme(&test_results, "customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  SetUpFetch(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry(
      "customstdfetch://test1", "customstdfetch", "test2", false));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Same as above but origin whitelist matches any domain.
TEST(SchemeHandlerTest, CustomStandardFetchDifferentOriginWithWhitelist2) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstdfetch", "test1");
  RegisterTestScheme(&test_results, "customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  SetUpFetch(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry(
      "customstdfetch://test1", "customstdfetch", CefString(), true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Same as above but origin whitelist matches sub-domains.
TEST(SchemeHandlerTest, CustomStandardFetchDifferentOriginWithWhitelist3) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstdfetch", "test1");
  RegisterTestScheme(&test_results, "customstdfetch", "a.test2.foo");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://a.test2.foo/fetch.html";
  SetUpFetch(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry(
      "customstdfetch://test1", "customstdfetch", "test2.foo", true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Test that an HTTP scheme can generate cross-domain XHR requests when setting
// the Access-Control-Allow-Origin header.
TEST(SchemeHandlerTest, HttpXHRDifferentOriginWithHeaderSync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "https", "test1");
  RegisterTestScheme(&test_results, "https", "test2");

  XHRTestSettings settings;
  settings.url = "https://test1/run.html";
  settings.sub_url = "https://test2/xhr.html";
  settings.sub_allow_origin = "https://test1";
  SetUpXHR(&test_results, settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that an HTTP scheme can generate cross-domain XHR requests when setting
// the Access-Control-Allow-Origin header.
TEST(SchemeHandlerTest, HttpXHRDifferentOriginWithHeaderAsync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "https", "test1");
  RegisterTestScheme(&test_results, "https", "test2");

  XHRTestSettings settings;
  settings.url = "https://test1/run.html";
  settings.sub_url = "https://test2/xhr.html";
  settings.sub_allow_origin = "https://test1";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that an HTTP scheme can generate cross-domain XHR requests when setting
// the Access-Control-Allow-Origin header.
TEST(SchemeHandlerTest, HttpFetchDifferentOriginWithHeader) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "https", "test1");
  RegisterTestScheme(&test_results, "https", "test2");

  FetchTestSettings settings;
  settings.url = "https://test1/run.html";
  settings.sub_url = "https://test2/fetch.html";
  settings.sub_allow_origin = "https://test1";
  SetUpFetch(&test_results, settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate cross-domain XSS requests
// when using document.domain.
TEST(SchemeHandlerTest, CustomStandardXSSDifferentOriginWithDomain) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "a.test.com");
  RegisterTestScheme(&test_results, "customstd", "b.test.com");
  SetUpXSS(&test_results, "customstd://a.test.com/run.html",
           "customstd://b.test.com/iframe.html", "test.com");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that an HTTP scheme can generate cross-domain XSS requests when using
// document.domain.
TEST(SchemeHandlerTest, HttpXSSDifferentOriginWithDomain) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "https", "a.test.com");
  RegisterTestScheme(&test_results, "https", "b.test.com");
  SetUpXSS(&test_results, "https://a.test.com/run.html",
           "https://b.test.com/iframe.html", "test.com");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme cannot generate cross-domain XHR requests
// that perform redirects.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginRedirectSync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_redirect_url = "customstd://test1/xhr.html";
  SetUpXHR(&test_results, settings);

  test_results.console_messages.push_back(
      "Access to XMLHttpRequest at 'customstd://test2/xhr.html' (redirected "
      "from 'customstd://test1/xhr.html') from origin 'customstd://test1' has "
      "been blocked by CORS policy: No 'Access-Control-Allow-Origin' header is "
      "present on the requested resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_redirect);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme cannot generate cross-domain XHR requests
// that perform redirects.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginRedirectAsync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_redirect_url = "customstd://test1/xhr.html";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  test_results.console_messages.push_back(
      "Access to XMLHttpRequest at 'customstd://test2/xhr.html' (redirected "
      "from 'customstd://test1/xhr.html') from origin 'customstd://test1' has "
      "been blocked by CORS policy: No 'Access-Control-Allow-Origin' header is "
      "present on the requested resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_redirect);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme cannot generate cross-domain Fetch
// requests that perform redirects.
TEST(SchemeHandlerTest, CustomStandardFetchDifferentOriginRedirect) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstdfetch", "test1");
  RegisterTestScheme(&test_results, "customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  settings.sub_redirect_url = "customstdfetch://test1/fetch.html";
  SetUpFetch(&test_results, settings);

  test_results.console_messages.push_back(
      "Access to fetch at 'customstdfetch://test2/fetch.html' (redirected from "
      "'customstdfetch://test1/fetch.html') from origin "
      "'customstdfetch://test1' has been blocked by CORS policy: No "
      "'Access-Control-Allow-Origin' header is present on the requested "
      "resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_redirect);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_FALSE(test_results.got_sub_read);
  EXPECT_FALSE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate cross-domain XHR requests
// that perform redirects when using the cross-origin whitelist.
TEST(SchemeHandlerTest,
     CustomStandardXHRDifferentOriginRedirectWithWhitelistSync) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_redirect_url = "customstd://test1/xhr.html";
  SetUpXHR(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2", false));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_redirect);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate cross-domain XHR requests
// that perform redirects when using the cross-origin whitelist.
TEST(SchemeHandlerTest,
     CustomStandardXHRDifferentOriginRedirectWithWhitelistAsync1) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_redirect_url = "customstd://test1/xhr.html";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2", false));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_redirect);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Same as above but origin whitelist matches any domain.
TEST(SchemeHandlerTest,
     CustomStandardXHRDifferentOriginRedirectWithWhitelistAsync2) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_redirect_url = "customstd://test1/xhr.html";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              CefString(), true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_redirect);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Same as above but origin whitelist matches sub-domains.
TEST(SchemeHandlerTest,
     CustomStandardXHRDifferentOriginRedirectWithWhitelistAsync3) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstd", "test1");
  RegisterTestScheme(&test_results, "customstd", "a.test2.foo");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://a.test2.foo/xhr.html";
  settings.sub_redirect_url = "customstd://test1/xhr.html";
  settings.synchronous = false;
  SetUpXHR(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2.foo", true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_redirect);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Test that a custom standard scheme can generate cross-domain Fetch requests
// that perform redirects when using the cross-origin whitelist.
TEST(SchemeHandlerTest,
     CustomStandardFetchDifferentOriginRedirectWithWhitelist1) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstdfetch", "test1");
  RegisterTestScheme(&test_results, "customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  settings.sub_redirect_url = "customstdfetch://test1/fetch.html";
  SetUpFetch(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry(
      "customstdfetch://test1", "customstdfetch", "test2", false));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_redirect);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Same as above but origin whitelist matches any domain.
TEST(SchemeHandlerTest,
     CustomStandardFetchDifferentOriginRedirectWithWhitelist2) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstdfetch", "test1");
  RegisterTestScheme(&test_results, "customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  settings.sub_redirect_url = "customstdfetch://test1/fetch.html";
  SetUpFetch(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry(
      "customstdfetch://test1", "customstdfetch", CefString(), true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_redirect);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Same as above but origin whitelist matches sub-domains.
TEST(SchemeHandlerTest,
     CustomStandardFetchDifferentOriginRedirectWithWhitelist3) {
  TestResults test_results;
  RegisterTestScheme(&test_results, "customstdfetch", "test1");
  RegisterTestScheme(&test_results, "customstdfetch", "a.test2.foo");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://a.test2.foo/fetch.html";
  settings.sub_redirect_url = "customstdfetch://test1/fetch.html";
  SetUpFetch(&test_results, settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry(
      "customstdfetch://test1", "customstdfetch", "test2.foo", true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.got_sub_redirect);
  EXPECT_TRUE(test_results.got_sub_request);
  EXPECT_TRUE(test_results.got_sub_read);
  EXPECT_TRUE(test_results.git_exit_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes(&test_results);
}

// Test that CefRequestContextSettings.accept_language_list configures both
// the Accept-Language request header and `navigator.language` JS attribute.
TEST(SchemeHandlerTest, AcceptLanguage) {
  TestResults test_results;

  // Value that will be set via CefRequestContextSettings.accept_language_list.
  test_results.accept_language = "uk";

  // Create an in-memory request context with custom settings.
  CefRequestContextSettings settings;
  CefString(&settings.accept_language_list) = test_results.accept_language;
  test_results.request_context =
      CefRequestContext::CreateContext(settings, nullptr);

  RegisterTestScheme(&test_results, "customstd", "test");
  test_results.url = "customstd://test/run.html";
  test_results.html =
      "<html><head></head><body>"
      "<script>"
      "if (navigator.language == 'uk') {"
      "  result = 'SUCCESS';"
      "} else {"
      "  result = 'FAILURE';"
      "  console.log('navigator.language: '+navigator.language+' != uk');"
      "}"
      "document.location = 'https://tests/exit?result='+result;"
      "</script>"
      "</body></html>";
  test_results.exit_url = "https://tests/exit";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&test_results);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_results.got_request);
  EXPECT_TRUE(test_results.got_read);
  EXPECT_TRUE(test_results.got_output);
  EXPECT_TRUE(test_results.git_exit_success);

  ClearTestSchemes(&test_results);
}

// Entry point for registering custom schemes.
// Called from client_app_delegates.cc.
void RegisterSchemeHandlerCustomSchemes(
    CefRawPtr<CefSchemeRegistrar> registrar) {
  // Registering the custom standard schemes as secure because requests from
  // non-secure origins to the loopback address will be blocked by
  // https://chromestatus.com/feature/5436853517811712.

  // Add a custom standard scheme.
  registrar->AddCustomScheme("customstd", CEF_SCHEME_OPTION_STANDARD |
                                              CEF_SCHEME_OPTION_SECURE |
                                              CEF_SCHEME_OPTION_CORS_ENABLED);
  // Also used in cors_unittest.cc.
  registrar->AddCustomScheme(
      "customstdfetch", CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE |
                            CEF_SCHEME_OPTION_CORS_ENABLED |
                            CEF_SCHEME_OPTION_FETCH_ENABLED);
  // Add a custom non-standard scheme.
  registrar->AddCustomScheme("customnonstd", CEF_SCHEME_OPTION_NONE);
  registrar->AddCustomScheme("customnonstdfetch",
                             CEF_SCHEME_OPTION_FETCH_ENABLED);
}

// Entry point for registering cookieable schemes.
// Called from client_app_delegates.cc.
void RegisterSchemeHandlerCookieableSchemes(
    std::vector<std::string>& cookieable_schemes) {
  cookieable_schemes.push_back("customstd");
  // Also used in cors_unittest.cc.
  cookieable_schemes.push_back("customstdfetch");
}
