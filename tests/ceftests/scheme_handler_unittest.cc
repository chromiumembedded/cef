// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "include/base/cef_callback.h"
#include "include/cef_callback.h"
#include "include/cef_origin_whitelist.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_suite.h"
#include "tests/ceftests/test_util.h"

namespace {

class TestResults {
 public:
  TestResults() : status_code(200), sub_status_code(200), delay(0) {}

  void reset() {
    url.clear();
    html.clear();
    status_code = 200;
    response_error_code = ERR_NONE;
    expected_error_code = ERR_NONE;
    redirect_url.clear();
    sub_url.clear();
    sub_html.clear();
    sub_status_code = 200;
    sub_allow_origin.clear();
    exit_url.clear();
    accept_language.clear();
    console_messages.clear();
    delay = 0;
    got_request.reset();
    got_read.reset();
    got_output.reset();
    got_sub_output.reset();
    got_redirect.reset();
    got_error.reset();
    got_sub_error.reset();
    got_sub_request.reset();
    got_sub_read.reset();
    got_sub_success.reset();
    got_exit_request.reset();
  }

  std::string url;
  std::string html;
  int status_code;

  // Error code set on the response.
  cef_errorcode_t response_error_code;
  // Error code expected in OnLoadError.
  cef_errorcode_t expected_error_code;

  // Used for testing redirects
  std::string redirect_url;

  // Used for testing XHR requests
  std::string sub_url;
  std::string sub_html;
  int sub_status_code;
  std::string sub_allow_origin;
  std::string sub_redirect_url;
  std::string exit_url;

  // Used for testing per-browser Accept-Language.
  std::string accept_language;

  // Used for testing received console messages.
  std::vector<std::string> console_messages;

  // Delay for returning scheme handler results.
  int delay;

  TrackCallback got_request, got_read, got_output, got_sub_output, got_redirect,
      got_error, got_sub_error, got_sub_redirect, got_sub_request, got_sub_read,
      got_sub_success, got_exit_request;
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

  void PopulateBrowserSettings(CefBrowserSettings* settings) override {
    if (!test_results_->accept_language.empty()) {
      CefString(&settings->accept_language_list) =
          test_results_->accept_language;
    }
  }

  void RunTest() override {
    CreateBrowser(test_results_->url);

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
    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return RV_CANCEL;
    }

    const std::string& newUrl = request->GetURL();
    if (IsExitURL(newUrl)) {
      test_results_->got_exit_request.yes();
      // XHR tests use an exit URL to destroy the test.
      if (newUrl.find("SUCCESS") != std::string::npos)
        test_results_->got_sub_success.yes();
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
    if (url == test_results_->url)
      test_results_->got_output.yes();
    else if (url == test_results_->sub_url)
      test_results_->got_sub_output.yes();
    else if (IsExitURL(url))
      return;

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
    if (url == test_results_->url)
      test_results_->got_error.yes();
    else if (url == test_results_->sub_url)
      test_results_->got_sub_error.yes();
    else if (IsExitURL(url))
      return;

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
    if (!test_results_->console_messages.empty()) {
      std::vector<std::string>::iterator it =
          test_results_->console_messages.begin();
      for (; it != test_results_->console_messages.end(); ++it) {
        const std::string& possible = *it;
        const std::string& actual = message.ToString();
        if (actual.find(possible) == 0U) {
          expected = true;
          test_results_->console_messages.erase(it);
          break;
        }
      }
    }

    EXPECT_TRUE(expected) << "Unexpected console message: "
                          << message.ToString();
    return false;
  }

 protected:
  TestResults* test_results_;

  IMPLEMENT_REFCOUNTING(TestSchemeHandler);
};

class ClientSchemeHandlerOld : public CefResourceHandler {
 public:
  explicit ClientSchemeHandlerOld(TestResults* tr)
      : test_results_(tr), offset_(0), is_sub_(false), has_delayed_(false) {}

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    bool handled = false;

    std::string url = request->GetURL();
    is_sub_ =
        (!test_results_->sub_url.empty() && test_results_->sub_url == url);

    if (is_sub_) {
      test_results_->got_sub_request.yes();

      if (!test_results_->sub_html.empty())
        handled = true;
    } else {
      EXPECT_EQ(url, test_results_->url);

      test_results_->got_request.yes();

      if (!test_results_->html.empty())
        handled = true;
    }

    std::string accept_language;
    CefRequest::HeaderMap headerMap;
    CefRequest::HeaderMap::iterator headerIter;
    request->GetHeaderMap(headerMap);
    headerIter = headerMap.find("Accept-Language");
    if (headerIter != headerMap.end())
      accept_language = headerIter->second;
    EXPECT_TRUE(!accept_language.empty());

    if (!test_results_->accept_language.empty()) {
      // Value from CefBrowserSettings.accept_language set in
      // PopulateBrowserSettings().
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
    if (g_current_handler)
      g_current_handler->DestroyTest();
    return false;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    if (is_sub_) {
      response->SetStatus(test_results_->sub_status_code);

      if (!test_results_->sub_allow_origin.empty()) {
        // Set the Access-Control-Allow-Origin header to allow cross-domain
        // scripting.
        CefResponse::HeaderMap headers;
        headers.insert(std::make_pair("Access-Control-Allow-Origin",
                                      test_results_->sub_allow_origin));
        response->SetHeaderMap(headers);
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

  TestResults* test_results_;
  size_t offset_;
  bool is_sub_;
  bool has_delayed_;

  IMPLEMENT_REFCOUNTING(ClientSchemeHandlerOld);
  DISALLOW_COPY_AND_ASSIGN(ClientSchemeHandlerOld);
};

class ClientSchemeHandler : public CefResourceHandler {
 public:
  explicit ClientSchemeHandler(TestResults* tr)
      : test_results_(tr), offset_(0), is_sub_(false), has_delayed_(false) {}

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return false;
    }

    bool handled = false;

    std::string url = request->GetURL();
    is_sub_ =
        (!test_results_->sub_url.empty() && test_results_->sub_url == url);

    if (is_sub_) {
      test_results_->got_sub_request.yes();

      if (!test_results_->sub_html.empty())
        handled = true;
    } else {
      EXPECT_EQ(url, test_results_->url);

      test_results_->got_request.yes();

      if (!test_results_->html.empty())
        handled = true;
    }

    std::string accept_language;
    CefRequest::HeaderMap headerMap;
    CefRequest::HeaderMap::iterator headerIter;
    request->GetHeaderMap(headerMap);
    headerIter = headerMap.find("Accept-Language");
    if (headerIter != headerMap.end())
      accept_language = headerIter->second;
    EXPECT_TRUE(!accept_language.empty());

    if (!test_results_->accept_language.empty()) {
      // Value from CefBrowserSettings.accept_language set in
      // PopulateBrowserSettings().
      EXPECT_STREQ(test_results_->accept_language.data(),
                   accept_language.data());
    } else {
      // CEF_SETTINGS_ACCEPT_LANGUAGE value from
      // CefSettings.accept_language_list set in CefTestSuite::GetSettings()
      // and expanded internally by ComputeAcceptLanguageFromPref.
      EXPECT_STREQ("en-GB,en;q=0.9", accept_language.data());
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
    if (g_current_handler)
      g_current_handler->DestroyTest();
    return false;
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return false;
    }

    EXPECT_TRUE(false);  // Not reached.
    return false;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    if (is_sub_) {
      response->SetStatus(test_results_->sub_status_code);

      if (!test_results_->sub_allow_origin.empty()) {
        // Set the Access-Control-Allow-Origin header to allow cross-domain
        // scripting.
        CefResponse::HeaderMap headers;
        headers.insert(std::make_pair("Access-Control-Allow-Origin",
                                      test_results_->sub_allow_origin));
        response->SetHeaderMap(headers);
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

  TestResults* test_results_;
  size_t offset_;
  bool is_sub_;
  bool has_delayed_;

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

  TestResults* test_results_;

  IMPLEMENT_REFCOUNTING(ClientSchemeHandlerFactory);
  DISALLOW_COPY_AND_ASSIGN(ClientSchemeHandlerFactory);
};

// Global test results object.
TestResults g_TestResults;

// If |domain| is empty the scheme will be registered as non-standard.
void RegisterTestScheme(const std::string& scheme, const std::string& domain) {
  g_TestResults.reset();

  EXPECT_TRUE(CefRegisterSchemeHandlerFactory(
      scheme, domain, new ClientSchemeHandlerFactory(&g_TestResults)));
  WaitForIOThread();
}

void ClearTestSchemes() {
  EXPECT_TRUE(CefClearSchemeHandlerFactories());
  WaitForIOThread();
}

struct XHRTestSettings {
  XHRTestSettings() : synchronous(true) {}

  std::string url;
  std::string sub_url;
  std::string sub_allow_origin;
  std::string sub_redirect_url;
  bool synchronous;
};

void SetUpXHR(const XHRTestSettings& settings) {
  g_TestResults.sub_url = settings.sub_url;
  g_TestResults.sub_html = "SUCCESS";
  g_TestResults.sub_allow_origin = settings.sub_allow_origin;
  g_TestResults.sub_redirect_url = settings.sub_redirect_url;

  std::string request_url;
  if (!settings.sub_redirect_url.empty())
    request_url = settings.sub_redirect_url;
  else
    request_url = settings.sub_url;

  g_TestResults.url = settings.url;
  std::stringstream ss;
  ss << "<html><head>"
        "<script language=\"JavaScript\">"
        "function onResult(val) {"
        "  document.location = \"http://tests/exit?result=\"+val;"
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
  g_TestResults.html = ss.str();

  g_TestResults.exit_url = "http://tests/exit";
}

struct FetchTestSettings {
  FetchTestSettings() {}

  std::string url;
  std::string sub_url;
  std::string sub_allow_origin;
  std::string sub_redirect_url;
};

void SetUpFetch(const FetchTestSettings& settings) {
  g_TestResults.sub_url = settings.sub_url;
  g_TestResults.sub_html = "SUCCESS";
  g_TestResults.sub_allow_origin = settings.sub_allow_origin;
  g_TestResults.sub_redirect_url = settings.sub_redirect_url;

  std::string request_url;
  if (!settings.sub_redirect_url.empty())
    request_url = settings.sub_redirect_url;
  else
    request_url = settings.sub_url;

  g_TestResults.url = settings.url;
  std::stringstream ss;
  ss << "<html><head>"
        "<script language=\"JavaScript\">"
        "function onResult(val) {"
        "  document.location = \"http://tests/exit?result=\"+val;"
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
  g_TestResults.html = ss.str();

  g_TestResults.exit_url = "http://tests/exit";
}  // namespace

void SetUpXSS(const std::string& url,
              const std::string& sub_url,
              const std::string& domain = std::string()) {
  // 1. Load |url| which contains an iframe.
  // 2. The iframe loads |sub_url|.
  // 3. |sub_url| tries to call a JS function in |url|.
  // 4. |url| tries to call a JS function in |sub_url|.

  std::stringstream ss;
  std::string domain_line;
  if (!domain.empty())
    domain_line = "document.domain = '" + domain + "';";

  g_TestResults.sub_url = sub_url;
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
        "  document.location = \"http://tests/exit?result=\"+result;"
        "}"
        "</script>"
        "</head><body onload=\"execXSSRequest();\">"
        "Running execXSSRequest..."
        "</body></html>";
  g_TestResults.sub_html = ss.str();

  g_TestResults.url = url;
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
  g_TestResults.html = ss.str();

  g_TestResults.exit_url = "http://tests/exit";
}

}  // namespace

// Test that scheme registration/unregistration works as expected.
TEST(SchemeHandlerTest, Registration) {
  RegisterTestScheme("customstd", "test");
  g_TestResults.url = "customstd://test/run.html";
  g_TestResults.html =
      "<html><head></head><body><h1>Success!</h1></body></html>";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);

  // Unregister the handler.
  EXPECT_TRUE(CefRegisterSchemeHandlerFactory("customstd", "test", nullptr));
  WaitForIOThread();

  g_TestResults.got_request.reset();
  g_TestResults.got_read.reset();
  g_TestResults.got_output.reset();
  g_TestResults.expected_error_code = ERR_UNKNOWN_URL_SCHEME;
  handler->ExecuteTest();

  EXPECT_TRUE(g_TestResults.got_error);
  EXPECT_FALSE(g_TestResults.got_request);
  EXPECT_FALSE(g_TestResults.got_read);
  EXPECT_FALSE(g_TestResults.got_output);

  // Re-register the handler.
  EXPECT_TRUE(CefRegisterSchemeHandlerFactory(
      "customstd", "test", new ClientSchemeHandlerFactory(&g_TestResults)));
  WaitForIOThread();

  g_TestResults.got_error.reset();
  g_TestResults.expected_error_code = ERR_NONE;
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(g_TestResults.got_error);
  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);

  ClearTestSchemes();
}

// Test that a custom standard scheme can return normal results.
TEST(SchemeHandlerTest, CustomStandardNormalResponse) {
  RegisterTestScheme("customstd", "test");
  g_TestResults.url = "customstd://test/run.html";
  g_TestResults.html =
      "<html><head></head><body><h1>Success!</h1></body></html>";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);

  ClearTestSchemes();
}

// Test that a custom standard scheme can return normal results with delayed
// responses.
TEST(SchemeHandlerTest, CustomStandardNormalResponseDelayed) {
  RegisterTestScheme("customstd", "test");
  g_TestResults.url = "customstd://test/run.html";
  g_TestResults.html =
      "<html><head></head><body><h1>Success!</h1></body></html>";
  g_TestResults.delay = 100;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);

  ClearTestSchemes();
}

// Test that a custom nonstandard scheme can return normal results.
TEST(SchemeHandlerTest, CustomNonStandardNormalResponse) {
  RegisterTestScheme("customnonstd", std::string());
  g_TestResults.url = "customnonstd:some%20value";
  g_TestResults.html =
      "<html><head></head><body><h1>Success!</h1></body></html>";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);

  ClearTestSchemes();
}

// Test that a custom standard scheme can return an error code.
TEST(SchemeHandlerTest, CustomStandardErrorResponse) {
  RegisterTestScheme("customstd", "test");
  g_TestResults.url = "customstd://test/run.html";
  g_TestResults.html = "<html><head></head><body><h1>404</h1></body></html>";
  g_TestResults.status_code = 404;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);

  ClearTestSchemes();
}

// Test that a custom standard scheme can return a CEF error code in the
// response.
TEST(SchemeHandlerTest, CustomStandardErrorCodeResponse) {
  RegisterTestScheme("customstd", "test");
  g_TestResults.url = "customstd://test/run.html";
  g_TestResults.response_error_code = ERR_FILE_TOO_BIG;
  g_TestResults.expected_error_code = ERR_FILE_TOO_BIG;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_FALSE(g_TestResults.got_read);
  EXPECT_FALSE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_error);

  ClearTestSchemes();
}

// Test that a custom nonstandard scheme can return an error code.
TEST(SchemeHandlerTest, CustomNonStandardErrorResponse) {
  RegisterTestScheme("customnonstd", std::string());
  g_TestResults.url = "customnonstd:some%20value";
  g_TestResults.html = "<html><head></head><body><h1>404</h1></body></html>";
  g_TestResults.status_code = 404;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);

  ClearTestSchemes();
}

// Test that custom standard scheme handling fails when the scheme name is
// incorrect.
TEST(SchemeHandlerTest, CustomStandardNameNotHandled) {
  RegisterTestScheme("customstd", "test");
  g_TestResults.url = "customstd2://test/run.html";
  g_TestResults.expected_error_code = ERR_UNKNOWN_URL_SCHEME;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(g_TestResults.got_request);
  EXPECT_FALSE(g_TestResults.got_read);
  EXPECT_FALSE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_error);

  ClearTestSchemes();
}

// Test that custom nonstandard scheme handling fails when the scheme name is
// incorrect.
TEST(SchemeHandlerTest, CustomNonStandardNameNotHandled) {
  RegisterTestScheme("customnonstd", std::string());
  g_TestResults.url = "customnonstd2:some%20value";
  g_TestResults.expected_error_code = ERR_UNKNOWN_URL_SCHEME;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(g_TestResults.got_request);
  EXPECT_FALSE(g_TestResults.got_read);
  EXPECT_FALSE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_error);

  ClearTestSchemes();
}

// Test that custom standard scheme handling fails when the domain name is
// incorrect.
TEST(SchemeHandlerTest, CustomStandardDomainNotHandled) {
  RegisterTestScheme("customstd", "test");
  g_TestResults.url = "customstd://noexist/run.html";
  g_TestResults.expected_error_code = ERR_UNKNOWN_URL_SCHEME;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(g_TestResults.got_request);
  EXPECT_FALSE(g_TestResults.got_read);
  EXPECT_FALSE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_error);

  ClearTestSchemes();
}

// Test that a custom standard scheme can return no response.
TEST(SchemeHandlerTest, CustomStandardNoResponse) {
  RegisterTestScheme("customstd", "test");
  g_TestResults.url = "customstd://test/run.html";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_FALSE(g_TestResults.got_read);
  EXPECT_FALSE(g_TestResults.got_output);

  ClearTestSchemes();
}

// Test that a custom nonstandard scheme can return no response.
TEST(SchemeHandlerTest, CustomNonStandardNoResponse) {
  RegisterTestScheme("customnonstd", std::string());
  g_TestResults.url = "customnonstd:some%20value";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_FALSE(g_TestResults.got_read);
  EXPECT_FALSE(g_TestResults.got_output);

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate redirects.
TEST(SchemeHandlerTest, CustomStandardRedirect) {
  RegisterTestScheme("customstd", "test");
  g_TestResults.url = "customstd://test/run.html";
  g_TestResults.redirect_url = "customstd://test/redirect.html";
  g_TestResults.html =
      "<html><head></head><body><h1>Redirected</h1></body></html>";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_redirect);

  ClearTestSchemes();
}

// Test that a custom nonstandard scheme can generate redirects.
TEST(SchemeHandlerTest, CustomNonStandardRedirect) {
  RegisterTestScheme("customnonstd", std::string());
  g_TestResults.url = "customnonstd:some%20value";
  g_TestResults.redirect_url = "customnonstd:some%20other%20value";
  g_TestResults.html =
      "<html><head></head><body><h1>Redirected</h1></body></html>";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_redirect);

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate same origin XHR requests.
TEST(SchemeHandlerTest, CustomStandardXHRSameOriginSync) {
  RegisterTestScheme("customstd", "test");

  XHRTestSettings settings;
  settings.url = "customstd://test/run.html";
  settings.sub_url = "customstd://test/xhr.html";
  SetUpXHR(settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate same origin XHR requests.
TEST(SchemeHandlerTest, CustomStandardXHRSameOriginAsync) {
  RegisterTestScheme("customstd", "test");

  XHRTestSettings settings;
  settings.url = "customstd://test/run.html";
  settings.sub_url = "customstd://test/xhr.html";
  settings.synchronous = false;
  SetUpXHR(settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that custom nonstandard schemes are treated as unique origins that
// cannot generate XHR requests.
TEST(SchemeHandlerTest, CustomNonStandardXHRSameOriginSync) {
  RegisterTestScheme("customnonstd", std::string());

  XHRTestSettings settings;
  settings.url = "customnonstd:some%20value";
  settings.sub_url = "customnonstd:xhr%20value";
  SetUpXHR(settings);

  g_TestResults.console_messages.push_back(
      "Access to XMLHttpRequest at 'customnonstd:xhr%20value' from origin "
      "'null' has been blocked by CORS policy: Cross origin requests are only "
      "supported for protocol schemes:");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_FALSE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that custom nonstandard schemes are treated as unique origins that
// cannot generate XHR requests.
TEST(SchemeHandlerTest, CustomNonStandardXHRSameOriginAsync) {
  RegisterTestScheme("customnonstd", std::string());

  XHRTestSettings settings;
  settings.url = "customnonstd:some%20value";
  settings.sub_url = "customnonstd:xhr%20value";
  settings.synchronous = false;
  SetUpXHR(settings);

  g_TestResults.console_messages.push_back(
      "Access to XMLHttpRequest at 'customnonstd:xhr%20value' from origin "
      "'null' has been blocked by CORS policy: Cross origin requests are only "
      "supported for protocol schemes:");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_FALSE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a non fetch enabled custom standard scheme can't generate same
// origin Fetch requests.
TEST(SchemeHandlerTest, CustomStandardFetchSameOrigin) {
  RegisterTestScheme("customstd", "test");

  FetchTestSettings settings;
  settings.url = "customstd://test/run.html";
  settings.sub_url = "customstd://test/fetch.html";
  SetUpFetch(settings);

  g_TestResults.console_messages.push_back(
      "Fetch API cannot load customstd://test/fetch.html. URL scheme "
      "\"customstd\" is not supported.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_FALSE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a fetch enabled custom standard scheme can generate same origin
// Fetch requests.
TEST(SchemeHandlerTest, FetchCustomStandardFetchSameOrigin) {
  RegisterTestScheme("customstdfetch", "test");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test/run.html";
  settings.sub_url = "customstdfetch://test/fetch.html";
  SetUpFetch(settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that custom nonstandard schemes are treated as unique origins that
// cannot generate Fetch requests.
TEST(SchemeHandlerTest, CustomNonStandardFetchSameOrigin) {
  RegisterTestScheme("customnonstd", std::string());

  FetchTestSettings settings;
  settings.url = "customnonstd:some%20value";
  settings.sub_url = "customnonstd:xhr%20value";
  SetUpFetch(settings);

  g_TestResults.console_messages.push_back(
      "Fetch API cannot load customnonstd:xhr%20value. URL scheme "
      "\"customnonstd\" is not supported.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_FALSE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate same origin XSS requests.
TEST(SchemeHandlerTest, CustomStandardXSSSameOrigin) {
  RegisterTestScheme("customstd", "test");
  SetUpXSS("customstd://test/run.html", "customstd://test/iframe.html");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that custom nonstandard schemes are treated as unique origins that
// cannot generate XSS requests.
TEST(SchemeHandlerTest, CustomNonStandardXSSSameOrigin) {
  RegisterTestScheme("customnonstd", std::string());
  SetUpXSS("customnonstd:some%20value", "customnonstd:xhr%20value");

  g_TestResults.console_messages.push_back(
      "Error: Blocked a frame with origin \"null\" from accessing a "
      "cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme cannot generate cross-domain XHR requests
// by default. Behavior should be the same as with HTTP.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginSync) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  SetUpXHR(settings);

  g_TestResults.console_messages.push_back(
      "Access to XMLHttpRequest at 'customstd://test2/xhr.html' from origin "
      "'customstd://test1' has been blocked by CORS policy: No "
      "'Access-Control-Allow-Origin' header is present on the requested "
      "resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme cannot generate cross-domain XHR requests
// by default. Behavior should be the same as with HTTP.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginAsync) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.synchronous = false;
  SetUpXHR(settings);

  g_TestResults.console_messages.push_back(
      "Access to XMLHttpRequest at 'customstd://test2/xhr.html' from origin "
      "'customstd://test1' has been blocked by CORS policy: No "
      "'Access-Control-Allow-Origin' header is present on the requested "
      "resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme cannot generate cross-domain Fetch
// requests by default. Behavior should be the same as with HTTP.
TEST(SchemeHandlerTest, CustomStandardFetchDifferentOrigin) {
  RegisterTestScheme("customstdfetch", "test1");
  RegisterTestScheme("customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  SetUpFetch(settings);

  g_TestResults.console_messages.push_back(
      "Access to fetch at 'customstdfetch://test2/fetch.html' from origin "
      "'customstdfetch://test1' has been blocked by CORS policy: No "
      "'Access-Control-Allow-Origin' header is present on the requested "
      "resource. If an opaque response serves your needs, set the request's "
      "mode to 'no-cors' to fetch the resource with CORS disabled.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme cannot generate cross-domain XSS requests
// by default.
TEST(SchemeHandlerTest, CustomStandardXSSDifferentOrigin) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");
  SetUpXSS("customstd://test1/run.html", "customstd://test2/iframe.html");

  g_TestResults.console_messages.push_back(
      "Error: Blocked a frame with origin \"customstd://test2\" from accessing "
      "a cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a cross-protocol iframe load succeeds, and that the custom
// standard scheme cannot generate XSS requests to the HTTP protocol by default.
TEST(SchemeHandlerTest, CustomStandardXSSDifferentProtocolHttp) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("http", "test2");
  SetUpXSS("customstd://test1/run.html", "http://test2/iframe.html");

  g_TestResults.console_messages.push_back(
      "Error: Blocked a frame with origin \"http://test2\" from accessing a "
      "cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a cross-protocol iframe load succeeds, and that the custom
// standard scheme cannot generate XSS requests to a non-standard scheme by
// default.
TEST(SchemeHandlerTest, CustomStandardXSSDifferentProtocolCustomNonStandard) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customnonstd", std::string());
  SetUpXSS("customstd://test1/run.html", "customnonstd:some%20value");

  g_TestResults.console_messages.push_back(
      "Error: Blocked a frame with origin \"null\" from accessing a "
      "cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a cross-protocol iframe load succeeds, and that the HTTP protocol
// cannot generate XSS requests to the custom standard scheme by default.
TEST(SchemeHandlerTest, HttpXSSDifferentProtocolCustomStandard) {
  RegisterTestScheme("http", "test1");
  RegisterTestScheme("customstd", "test2");
  SetUpXSS("http://test1/run.html", "customstd://test2/iframe.html");

  g_TestResults.console_messages.push_back(
      "Error: Blocked a frame with origin \"customstd://test2\" from accessing "
      "a cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a cross-protocol iframe load succeeds, and that the HTTP protocol
// cannot generate XSS requests to the custom non-standard scheme by default.
TEST(SchemeHandlerTest, HttpXSSDifferentProtocolCustomNonStandard) {
  RegisterTestScheme("http", "test1");
  RegisterTestScheme("customnonstd", std::string());
  SetUpXSS("http://test1/run.html", "customnonstd:some%20value");

  g_TestResults.console_messages.push_back(
      "Error: Blocked a frame with origin \"null\" from accessing a "
      "cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that an HTTP scheme cannot generate cross-domain XHR requests by
// default.
TEST(SchemeHandlerTest, HttpXHRDifferentOriginSync) {
  RegisterTestScheme("http", "test1");
  RegisterTestScheme("http", "test2");

  XHRTestSettings settings;
  settings.url = "http://test1/run.html";
  settings.sub_url = "http://test2/xhr.html";
  SetUpXHR(settings);

  g_TestResults.console_messages.push_back(
      "Access to XMLHttpRequest at 'http://test2/xhr.html' from origin "
      "'http://test1' has been blocked by CORS policy: No "
      "'Access-Control-Allow-Origin' header is present on the requested "
      "resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that an HTTP scheme cannot generate cross-domain XHR requests by
// default.
TEST(SchemeHandlerTest, HttpXHRDifferentOriginAsync) {
  RegisterTestScheme("http", "test1");
  RegisterTestScheme("http", "test2");

  XHRTestSettings settings;
  settings.url = "http://test1/run.html";
  settings.sub_url = "http://test2/xhr.html";
  settings.synchronous = false;
  SetUpXHR(settings);

  g_TestResults.console_messages.push_back(
      "Access to XMLHttpRequest at 'http://test2/xhr.html' from origin "
      "'http://test1' has been blocked by CORS policy: No "
      "'Access-Control-Allow-Origin' header is present on the requested "
      "resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that an HTTP scheme cannot generate cross-domain Fetch requests by
// default.
TEST(SchemeHandlerTest, HttpFetchDifferentOriginAsync) {
  RegisterTestScheme("http", "test1");
  RegisterTestScheme("http", "test2");

  FetchTestSettings settings;
  settings.url = "http://test1/run.html";
  settings.sub_url = "http://test2/fetch.html";
  SetUpFetch(settings);

  g_TestResults.console_messages.push_back(
      "Access to fetch at 'http://test2/fetch.html' from origin 'http://test1' "
      "has been blocked by CORS policy: No 'Access-Control-Allow-Origin' "
      "header is present on the requested resource. If an opaque response "
      "serves your needs, set the request's mode to 'no-cors' to fetch the "
      "resource with CORS disabled.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that an HTTP scheme cannot generate cross-domain XSS requests by
// default.
TEST(SchemeHandlerTest, HttpXSSDifferentOrigin) {
  RegisterTestScheme("http", "test1");
  RegisterTestScheme("http", "test2");
  SetUpXSS("http://test1/run.html", "http://test2/xss.html");

  g_TestResults.console_messages.push_back(
      "Error: Blocked a frame with origin \"http://test2\" from accessing a "
      "cross-origin frame.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate cross-domain XHR requests
// when setting the Access-Control-Allow-Origin header. Should behave the same
// as HTTP.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithHeaderSync) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_allow_origin = "customstd://test1";
  SetUpXHR(settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate cross-domain XHR requests
// when setting the Access-Control-Allow-Origin header. Should behave the same
// as HTTP.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithHeaderAsync) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_allow_origin = "customstd://test1";
  settings.synchronous = false;
  SetUpXHR(settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate cross-domain Fetch requests
// when setting the Access-Control-Allow-Origin header. Should behave the same
// as HTTP.
TEST(SchemeHandlerTest, CustomStandardFetchDifferentOriginWithHeader) {
  RegisterTestScheme("customstdfetch", "test1");
  RegisterTestScheme("customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  settings.sub_allow_origin = "customstdfetch://test1";
  SetUpFetch(settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate cross-domain XHR requests
// when using the cross-origin whitelist.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithWhitelistSync1) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  SetUpXHR(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2", false));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Same as above but origin whitelist matches any domain.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithWhitelistSync2) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  SetUpXHR(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              CefString(), true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Same as above but origin whitelist matches sub-domains.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithWhitelistSync3) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "a.test2.foo");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://a.test2.foo/xhr.html";
  SetUpXHR(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2.foo", true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate cross-domain XHR requests
// when using the cross-origin whitelist.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithWhitelistAsync1) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.synchronous = false;
  SetUpXHR(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2", false));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Same as above but origin whitelist matches any domain.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithWhitelistAsync2) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.synchronous = false;
  SetUpXHR(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              CefString(), true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Same as above but origin whitelist matches sub-domains.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginWithWhitelistAsync3) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "a.test2.foo");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://a.test2.foo/xhr.html";
  settings.synchronous = false;
  SetUpXHR(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2.foo", true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate cross-domain Fetch requests
// when using the cross-origin whitelist.
TEST(SchemeHandlerTest, CustomStandardFetchDifferentOriginWithWhitelist1) {
  RegisterTestScheme("customstdfetch", "test1");
  RegisterTestScheme("customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  SetUpFetch(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry(
      "customstdfetch://test1", "customstdfetch", "test2", false));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Same as above but origin whitelist matches any domain.
TEST(SchemeHandlerTest, CustomStandardFetchDifferentOriginWithWhitelist2) {
  RegisterTestScheme("customstdfetch", "test1");
  RegisterTestScheme("customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  SetUpFetch(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry(
      "customstdfetch://test1", "customstdfetch", CefString(), true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Same as above but origin whitelist matches sub-domains.
TEST(SchemeHandlerTest, CustomStandardFetchDifferentOriginWithWhitelist3) {
  RegisterTestScheme("customstdfetch", "test1");
  RegisterTestScheme("customstdfetch", "a.test2.foo");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://a.test2.foo/fetch.html";
  SetUpFetch(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry(
      "customstdfetch://test1", "customstdfetch", "test2.foo", true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Test that an HTTP scheme can generate cross-domain XHR requests when setting
// the Access-Control-Allow-Origin header.
TEST(SchemeHandlerTest, HttpXHRDifferentOriginWithHeaderSync) {
  RegisterTestScheme("http", "test1");
  RegisterTestScheme("http", "test2");

  XHRTestSettings settings;
  settings.url = "http://test1/run.html";
  settings.sub_url = "http://test2/xhr.html";
  settings.sub_allow_origin = "http://test1";
  SetUpXHR(settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that an HTTP scheme can generate cross-domain XHR requests when setting
// the Access-Control-Allow-Origin header.
TEST(SchemeHandlerTest, HttpXHRDifferentOriginWithHeaderAsync) {
  RegisterTestScheme("http", "test1");
  RegisterTestScheme("http", "test2");

  XHRTestSettings settings;
  settings.url = "http://test1/run.html";
  settings.sub_url = "http://test2/xhr.html";
  settings.sub_allow_origin = "http://test1";
  settings.synchronous = false;
  SetUpXHR(settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that an HTTP scheme can generate cross-domain XHR requests when setting
// the Access-Control-Allow-Origin header.
TEST(SchemeHandlerTest, HttpFetchDifferentOriginWithHeader) {
  RegisterTestScheme("http", "test1");
  RegisterTestScheme("http", "test2");

  FetchTestSettings settings;
  settings.url = "http://test1/run.html";
  settings.sub_url = "http://test2/fetch.html";
  settings.sub_allow_origin = "http://test1";
  SetUpFetch(settings);

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate cross-domain XSS requests
// when using document.domain.
TEST(SchemeHandlerTest, CustomStandardXSSDifferentOriginWithDomain) {
  RegisterTestScheme("customstd", "a.test.com");
  RegisterTestScheme("customstd", "b.test.com");
  SetUpXSS("customstd://a.test.com/run.html",
           "customstd://b.test.com/iframe.html", "test.com");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that an HTTP scheme can generate cross-domain XSS requests when using
// document.domain.
TEST(SchemeHandlerTest, HttpXSSDifferentOriginWithDomain) {
  RegisterTestScheme("http", "a.test.com");
  RegisterTestScheme("http", "b.test.com");
  SetUpXSS("http://a.test.com/run.html", "http://b.test.com/iframe.html",
           "test.com");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme cannot generate cross-domain XHR requests
// that perform redirects.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginRedirectSync) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_redirect_url = "customstd://test1/xhr.html";
  SetUpXHR(settings);

  g_TestResults.console_messages.push_back(
      "Access to XMLHttpRequest at 'customstd://test2/xhr.html' (redirected "
      "from 'customstd://test1/xhr.html') from origin 'customstd://test1' has "
      "been blocked by CORS policy: No 'Access-Control-Allow-Origin' header is "
      "present on the requested resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_redirect);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme cannot generate cross-domain XHR requests
// that perform redirects.
TEST(SchemeHandlerTest, CustomStandardXHRDifferentOriginRedirectAsync) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_redirect_url = "customstd://test1/xhr.html";
  settings.synchronous = false;
  SetUpXHR(settings);

  g_TestResults.console_messages.push_back(
      "Access to XMLHttpRequest at 'customstd://test2/xhr.html' (redirected "
      "from 'customstd://test1/xhr.html') from origin 'customstd://test1' has "
      "been blocked by CORS policy: No 'Access-Control-Allow-Origin' header is "
      "present on the requested resource.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_redirect);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme cannot generate cross-domain Fetch
// requests that perform redirects.
TEST(SchemeHandlerTest, CustomStandardFetchDifferentOriginRedirect) {
  RegisterTestScheme("customstdfetch", "test1");
  RegisterTestScheme("customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  settings.sub_redirect_url = "customstdfetch://test1/fetch.html";
  SetUpFetch(settings);

  g_TestResults.console_messages.push_back(
      "Access to fetch at 'customstdfetch://test2/fetch.html' (redirected from "
      "'customstdfetch://test1/fetch.html') from origin "
      "'customstdfetch://test1' has been blocked by CORS policy: No "
      "'Access-Control-Allow-Origin' header is present on the requested "
      "resource. If an opaque response serves your needs, set the request's "
      "mode to 'no-cors' to fetch the resource with CORS disabled.");

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_redirect);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_FALSE(g_TestResults.got_sub_read);
  EXPECT_FALSE(g_TestResults.got_sub_success);

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate cross-domain XHR requests
// that perform redirects when using the cross-origin whitelist.
TEST(SchemeHandlerTest,
     CustomStandardXHRDifferentOriginRedirectWithWhitelistSync) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_redirect_url = "customstd://test1/xhr.html";
  SetUpXHR(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2", false));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_redirect);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate cross-domain XHR requests
// that perform redirects when using the cross-origin whitelist.
TEST(SchemeHandlerTest,
     CustomStandardXHRDifferentOriginRedirectWithWhitelistAsync1) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_redirect_url = "customstd://test1/xhr.html";
  settings.synchronous = false;
  SetUpXHR(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2", false));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_redirect);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Same as above but origin whitelist matches any domain.
TEST(SchemeHandlerTest,
     CustomStandardXHRDifferentOriginRedirectWithWhitelistAsync2) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "test2");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://test2/xhr.html";
  settings.sub_redirect_url = "customstd://test1/xhr.html";
  settings.synchronous = false;
  SetUpXHR(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              CefString(), true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_redirect);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Same as above but origin whitelist matches sub-domains.
TEST(SchemeHandlerTest,
     CustomStandardXHRDifferentOriginRedirectWithWhitelistAsync3) {
  RegisterTestScheme("customstd", "test1");
  RegisterTestScheme("customstd", "a.test2.foo");

  XHRTestSettings settings;
  settings.url = "customstd://test1/run.html";
  settings.sub_url = "customstd://a.test2.foo/xhr.html";
  settings.sub_redirect_url = "customstd://test1/xhr.html";
  settings.synchronous = false;
  SetUpXHR(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry("customstd://test1", "customstd",
                                              "test2.foo", true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_redirect);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Test that a custom standard scheme can generate cross-domain Fetch requests
// that perform redirects when using the cross-origin whitelist.
TEST(SchemeHandlerTest,
     CustomStandardFetchDifferentOriginRedirectWithWhitelist1) {
  RegisterTestScheme("customstdfetch", "test1");
  RegisterTestScheme("customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  settings.sub_redirect_url = "customstdfetch://test1/fetch.html";
  SetUpFetch(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry(
      "customstdfetch://test1", "customstdfetch", "test2", false));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_redirect);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Same as above but origin whitelist matches any domain.
TEST(SchemeHandlerTest,
     CustomStandardFetchDifferentOriginRedirectWithWhitelist2) {
  RegisterTestScheme("customstdfetch", "test1");
  RegisterTestScheme("customstdfetch", "test2");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://test2/fetch.html";
  settings.sub_redirect_url = "customstdfetch://test1/fetch.html";
  SetUpFetch(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry(
      "customstdfetch://test1", "customstdfetch", CefString(), true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_redirect);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Same as above but origin whitelist matches sub-domains.
TEST(SchemeHandlerTest,
     CustomStandardFetchDifferentOriginRedirectWithWhitelist3) {
  RegisterTestScheme("customstdfetch", "test1");
  RegisterTestScheme("customstdfetch", "a.test2.foo");

  FetchTestSettings settings;
  settings.url = "customstdfetch://test1/run.html";
  settings.sub_url = "customstdfetch://a.test2.foo/fetch.html";
  settings.sub_redirect_url = "customstdfetch://test1/fetch.html";
  SetUpFetch(settings);

  EXPECT_TRUE(CefAddCrossOriginWhitelistEntry(
      "customstdfetch://test1", "customstdfetch", "test2.foo", true));
  WaitForUIThread();

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_sub_redirect);
  EXPECT_TRUE(g_TestResults.got_sub_request);
  EXPECT_TRUE(g_TestResults.got_sub_read);
  EXPECT_TRUE(g_TestResults.got_sub_success);

  EXPECT_TRUE(CefClearCrossOriginWhitelist());
  WaitForUIThread();

  ClearTestSchemes();
}

// Test per-browser setting of Accept-Language.
TEST(SchemeHandlerTest, AcceptLanguage) {
  RegisterTestScheme("customstd", "test");
  g_TestResults.url = "customstd://test/run.html";
  g_TestResults.html =
      "<html><head></head><body><h1>Success!</h1></body></html>";

  // Value that will be set via CefBrowserSettings.accept_language in
  // PopulateBrowserSettings().
  g_TestResults.accept_language = "uk";

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);

  ClearTestSchemes();
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
