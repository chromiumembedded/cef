// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <map>
#include <memory>
#include <sstream>

#include "include/base/cef_callback.h"
#include "include/cef_parser.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_scheme.h"
#include "include/cef_server.h"
#include "include/cef_task.h"
#include "include/cef_urlrequest.h"
#include "include/cef_waitable_event.h"
#include "include/test/cef_test_helpers.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_request.h"
#include "tests/ceftests/test_server_observer.h"
#include "tests/ceftests/test_suite.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/browser/file_util.h"
#include "tests/shared/common/string_util.h"

// How to add a new test:
// 1. Add a new value to the RequestTestMode enumeration.
// 2. Add methods to set up and run the test in RequestTestRunner.
// 3. Add a line for the test in the RequestTestRunner constructor.
// 4. Add lines for the test in the "Define the tests" section at the bottom of
//    the file.

namespace {

// Browser-side app delegate.
class URLRequestBrowserTest : public client::ClientAppBrowser::Delegate {
 public:
  URLRequestBrowserTest() = default;

  void OnBeforeCommandLineProcessing(
      CefRefPtr<client::ClientAppBrowser> app,
      CefRefPtr<CefCommandLine> command_line) override {
    // Delegate auth callbacks to GetAuthCredentials.
    command_line->AppendSwitch("disable-chrome-login-prompt");

    // Disable component extensions that require creation of a background
    // WebContents because they slow down test runs.
    command_line->AppendSwitch(
        "disable-component-extensions-with-background-pages");
  }

 private:
  IMPLEMENT_REFCOUNTING(URLRequestBrowserTest);
};

// TEST DATA

// Custom scheme handler backend.
const char kRequestSchemeCustom[] = "urcustom";
const char kRequestHostCustom[] = "test";

// Server backend.
const char* kRequestAddressServer = test_server::kHttpServerAddress;
const uint16_t kRequestPortServer = test_server::kHttpServerPort;
const char kRequestSchemeServer[] = "http";

const char kRequestSendCookieName[] = "urcookie_send";
const char kRequestSaveCookieName[] = "urcookie_save";

const char kCacheControlHeader[] = "cache-control";

enum RequestTestMode {
  REQTEST_GET = 0,
  REQTEST_GET_NODATA,
  REQTEST_GET_PARTIAL_CONTENT,
  REQTEST_GET_ALLOWCOOKIES,
  REQTEST_GET_REDIRECT,
  REQTEST_GET_REDIRECT_STOP,
  REQTEST_GET_REDIRECT_LOCATION,
  REQTEST_GET_REFERRER,
  REQTEST_GET_AUTH,
  REQTEST_POST,
  REQTEST_POST_FILE,
  REQTEST_POST_WITHPROGRESS,
  REQTEST_POST_REDIRECT,
  REQTEST_POST_REDIRECT_TOGET,
  REQTEST_HEAD,
  REQTEST_CACHE_WITH_CONTROL,
  REQTEST_CACHE_WITHOUT_CONTROL,
  REQTEST_CACHE_SKIP_FLAG,
  REQTEST_CACHE_SKIP_HEADER,
  REQTEST_CACHE_ONLY_FAILURE_FLAG,
  REQTEST_CACHE_ONLY_FAILURE_HEADER,
  REQTEST_CACHE_ONLY_SUCCESS_FLAG,
  REQTEST_CACHE_ONLY_SUCCESS_HEADER,
  REQTEST_CACHE_DISABLE_FLAG,
  REQTEST_CACHE_DISABLE_HEADER,
  REQTEST_INCOMPLETE_PROCESS_REQUEST,
  REQTEST_INCOMPLETE_READ_RESPONSE,
};

enum ContextTestMode {
  CONTEXT_GLOBAL,
  CONTEXT_INMEMORY,
  CONTEXT_ONDISK,
};

// Defines test expectations for a request.
struct RequestRunSettings {
  RequestRunSettings() = default;

  // Set expectations for request failure.
  void SetRequestFailureExpected(cef_errorcode_t error_code) {
    expect_upload_progress = false;
    expect_download_progress = false;
    expect_download_data = false;
    expected_status = UR_FAILED;
    expected_error_code = error_code;
    response = nullptr;
    response_data.clear();
  }

  // Request that will be sent.
  CefRefPtr<CefRequest> request;

  // Response that will be returned by the backend.
  CefRefPtr<CefResponse> response;

  // Optional response data that will be returned by the backend.
  std::string response_data;

  // Create an incomplete request to test shutdown behavior.
  enum IncompleteType {
    INCOMPLETE_NONE,
    INCOMPLETE_PROCESS_REQUEST,
    INCOMPLETE_READ_RESPONSE,
  };
  IncompleteType incomplete_type = INCOMPLETE_NONE;

  // If true upload progress notification will be expected.
  bool expect_upload_progress = false;

  // If true download progress notification will be expected.
  bool expect_download_progress = true;

  // If true download data will be expected.
  bool expect_download_data = true;

  // The offset from what we passed that we expect to receive.
  size_t expected_download_offset = 0;

  // Expected status value.
  CefURLRequest::Status expected_status = UR_SUCCESS;

  // Expected error code value.
  CefURLRequest::ErrorCode expected_error_code = ERR_NONE;

  // If true the request cookie should be sent to the server.
  bool expect_send_cookie = false;

  // If true the response cookie should be saved.
  bool expect_save_cookie = false;

  // If true the test will begin by requiring Basic authentication and then
  // continue with the actual request. The UR_FLAG_ALLOW_STORED_CREDENTIALS
  // flag must be set on the request. When using the global request context
  // CefRequestContext::ClearHttpAuthCredentials should be called to avoid
  // leaking state across test runs. Authentication is only supported with
  // browser-initiated requests and the server backend.
  bool expect_authentication = false;
  std::string username;
  std::string password;

  // If specified the test will begin with this redirect request and response.
  CefRefPtr<CefRequest> redirect_request;
  CefRefPtr<CefResponse> redirect_response;

  // If true the redirect is expected to be followed.
  bool expect_follow_redirect = true;

  // If true the response is expected to be served from cache.
  bool expect_response_was_cached = false;

  // The expected number of requests to send, or -1 if unspecified.
  // Used only with the server backend.
  int expected_send_count = -1;

  // The expected number of requests to receive, or -1 if unspecified.
  // Used only with the server backend.
  int expected_receive_count = -1;

  using NextRequestCallback =
      base::OnceCallback<void(int /* next_send_count */,
                              base::OnceClosure /* complete_callback */)>;

  // If non-null this callback will be executed before subsequent requests are
  // sent.
  NextRequestCallback setup_next_request;
};

// Manages the map of request URL to test expectations.
class RequestDataMap {
 public:
  struct Entry {
    enum Type {
      TYPE_UNKNOWN,
      TYPE_NORMAL,
      TYPE_REDIRECT,
    };

    explicit Entry(Type entry_type) : type(entry_type) {}

    Type type;

    // Used with TYPE_NORMAL.
    // |settings| is not owned by this object.
    RequestRunSettings* settings = nullptr;

    // Used with TYPE_REDIRECT.
    CefRefPtr<CefRequest> redirect_request;
    CefRefPtr<CefResponse> redirect_response;
  };

  RequestDataMap() : owner_task_runner_(CefTaskRunner::GetForCurrentThread()) {}

  // Pass ownership to the specified |task_runner| thread.
  // If |task_runner| is nullptr the test is considered destroyed.
  void SetOwnerTaskRunner(CefRefPtr<CefTaskRunner> task_runner) {
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());
    owner_task_runner_ = task_runner;
  }

  void AddSchemeHandler(RequestRunSettings* settings) {
    EXPECT_TRUE(settings);
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());

    const std::string& url = settings->request->GetURL();
    data_map_.insert(std::make_pair(url, settings));

    if (settings->redirect_request) {
      const std::string& redirect_url = settings->redirect_request->GetURL();
      redirect_data_map_.insert(std::make_pair(
          redirect_url, std::make_pair(settings->redirect_request,
                                       settings->redirect_response)));
    }
  }

  Entry Find(const std::string& url) {
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());

    Entry entry(Entry::TYPE_UNKNOWN);

    // Try to find a test match.
    {
      DataMap::const_iterator it = data_map_.find(url);
      if (it != data_map_.end()) {
        entry.type = Entry::TYPE_NORMAL;
        entry.settings = it->second;
        return entry;
      }
    }

    // Try to find a redirect match.
    {
      RedirectDataMap::const_iterator it = redirect_data_map_.find(url);
      if (it != redirect_data_map_.end()) {
        entry.type = Entry::TYPE_REDIRECT;
        entry.redirect_request = it->second.first;
        entry.redirect_response = it->second.second;
        return entry;
      }
    }

    // Unknown test.
    ADD_FAILURE() << "url: " << url;
    return entry;
  }

  size_t size() const { return data_map_.size() + redirect_data_map_.size(); }

 private:
  CefRefPtr<CefTaskRunner> owner_task_runner_;

  // The below members are only accessed on the |owner_task_runner_| thread.

  // RequestRunSettings pointer is not owned by this object.
  typedef std::map<std::string, RequestRunSettings*> DataMap;
  DataMap data_map_;

  typedef std::map<std::string,
                   std::pair<CefRefPtr<CefRequest>, CefRefPtr<CefResponse>>>
      RedirectDataMap;
  RedirectDataMap redirect_data_map_;
};

class TestCompletionCallback : public CefCompletionCallback {
 public:
  explicit TestCompletionCallback(base::OnceClosure complete_callback)
      : complete_callback_(std::move(complete_callback)) {
    EXPECT_FALSE(complete_callback_.is_null());
  }

  void OnComplete() override { std::move(complete_callback_).Run(); }

 private:
  base::OnceClosure complete_callback_;

  IMPLEMENT_REFCOUNTING(TestCompletionCallback);
};

std::string GetRequestScheme(bool server_backend) {
  return server_backend ? kRequestSchemeServer : kRequestSchemeCustom;
}

std::string GetRequestHost(bool server_backend, bool with_port) {
  if (server_backend) {
    if (with_port) {
      std::stringstream ss;
      ss << kRequestAddressServer << ":" << kRequestPortServer;
      return ss.str();
    } else {
      return kRequestAddressServer;
    }
  } else {
    return kRequestHostCustom;
  }
}

std::string GetRequestOrigin(bool server_backend) {
  return GetRequestScheme(server_backend) + "://" +
         GetRequestHost(server_backend, true);
}

void SetUploadData(CefRefPtr<CefRequest> request, const std::string& data) {
  CefRefPtr<CefPostData> postData = CefPostData::Create();
  CefRefPtr<CefPostDataElement> element = CefPostDataElement::Create();
  element->SetToBytes(data.size(), data.c_str());
  postData->AddElement(element);
  request->SetPostData(postData);
}

void SetUploadFile(CefRefPtr<CefRequest> request, const std::string& file) {
  CefRefPtr<CefPostData> postData = CefPostData::Create();
  CefRefPtr<CefPostDataElement> element = CefPostDataElement::Create();
  element->SetToFile(file);
  postData->AddElement(element);
  request->SetPostData(postData);
}

void GetUploadData(CefRefPtr<CefRequest> request, std::string& data) {
  CefRefPtr<CefPostData> postData = request->GetPostData();
  EXPECT_TRUE(postData.get());
  CefPostData::ElementVector elements;
  postData->GetElements(elements);
  EXPECT_EQ((size_t)1, elements.size());
  CefRefPtr<CefPostDataElement> element = elements[0];
  EXPECT_TRUE(element.get());

  size_t size = element->GetBytesCount();

  data.resize(size);
  EXPECT_EQ(size, data.size());
  EXPECT_EQ(size, element->GetBytes(size, const_cast<char*>(data.c_str())));
}

// Set a cookie so that we can test if it's sent with the request.
void SetTestCookie(CefRefPtr<CefRequestContext> request_context,
                   bool server_backend,
                   base::OnceClosure callback) {
  class Callback : public CefSetCookieCallback {
   public:
    explicit Callback(base::OnceClosure callback)
        : callback_(std::move(callback)) {
      EXPECT_FALSE(callback_.is_null());
    }

    void OnComplete(bool success) override {
      EXPECT_TRUE(success);
      std::move(callback_).Run();
    }

   private:
    base::OnceClosure callback_;

    IMPLEMENT_REFCOUNTING(Callback);
  };

  CefCookie cookie;
  CefString(&cookie.name) = kRequestSendCookieName;
  CefString(&cookie.value) = "send-cookie-value";
  CefString(&cookie.domain) = GetRequestHost(server_backend, false);
  CefString(&cookie.path) = "/";
  cookie.has_expires = false;
  EXPECT_TRUE(request_context->GetCookieManager(nullptr)->SetCookie(
      GetRequestOrigin(server_backend), cookie,
      new Callback(std::move(callback))));
}

using GetTestCookieCallback =
    base::OnceCallback<void(bool /* cookie exists */)>;

// Tests if the save cookie has been set. If set, it will be deleted at the same
// time.
void GetTestCookie(CefRefPtr<CefRequestContext> request_context,
                   bool server_backend,
                   GetTestCookieCallback callback) {
  class Visitor : public CefCookieVisitor {
   public:
    explicit Visitor(GetTestCookieCallback callback)
        : callback_(std::move(callback)) {
      EXPECT_FALSE(callback_.is_null());
    }
    ~Visitor() override { std::move(callback_).Run(cookie_exists_); }

    bool Visit(const CefCookie& cookie,
               int count,
               int total,
               bool& deleteCookie) override {
      std::string cookie_name = CefString(&cookie.name);
      if (cookie_name == kRequestSaveCookieName) {
        cookie_exists_ = true;
        deleteCookie = true;
        return false;
      }
      return true;
    }

   private:
    GetTestCookieCallback callback_;
    bool cookie_exists_ = false;

    IMPLEMENT_REFCOUNTING(Visitor);
  };

  CefRefPtr<CefCookieManager> cookie_manager =
      request_context->GetCookieManager(nullptr);
  cookie_manager->VisitUrlCookies(GetRequestOrigin(server_backend), true,
                                  new Visitor(std::move(callback)));
}

std::string GetHeaderValue(const CefRequest::HeaderMap& header_map,
                           const std::string& header_name_lower) {
  CefRequest::HeaderMap::const_iterator it = header_map.begin();
  for (; it != header_map.end(); ++it) {
    std::string name = client::AsciiStrToLower(it->first);
    if (name == header_name_lower) {
      return it->second;
    }
  }
  return std::string();
}

// Verify normal request expectations.
void VerifyNormalRequest(const RequestRunSettings* settings,
                         CefRefPtr<CefRequest> request,
                         bool server_backend) {
  // Shouldn't get here if we're not following redirects.
  EXPECT_TRUE(settings->expect_follow_redirect);

  // Verify that the request was sent correctly.
  TestRequestEqual(settings->request, request, true);

  CefRequest::HeaderMap headerMap;
  request->GetHeaderMap(headerMap);

  // Check if the default headers were sent.
  EXPECT_FALSE(GetHeaderValue(headerMap, "user-agent").empty());

  // CEF_SETTINGS_ACCEPT_LANGUAGE value from CefSettings.accept_language_list
  // set in CefTestSuite::GetSettings() and expanded internally by
  // ComputeAcceptLanguageFromPref.
  const std::string& accept_language =
      GetHeaderValue(headerMap, "accept-language");
  if (CefIsFeatureEnabledForTests("ReduceAcceptLanguage")) {
    EXPECT_TRUE(accept_language == "en-GB" ||
                accept_language == "en-GB,en;q=0.9")
        << accept_language;
  } else {
    EXPECT_STREQ("en-GB,en;q=0.9", accept_language.data());
  }

  if (server_backend) {
    EXPECT_FALSE(GetHeaderValue(headerMap, "accept-encoding").empty());
    EXPECT_STREQ(GetRequestHost(true, true).c_str(),
                 GetHeaderValue(headerMap, "host").c_str());
  }

  // Check if the request cookie was sent.
  const std::string& cookie_value = GetHeaderValue(headerMap, "cookie");
  bool has_send_cookie = false;
  if (!cookie_value.empty() &&
      cookie_value.find(kRequestSendCookieName) != std::string::npos) {
    has_send_cookie = true;
  }

  EXPECT_EQ(settings->expect_send_cookie, has_send_cookie);
}

// Populate normal response contents.
void GetNormalResponse(const RequestRunSettings* settings,
                       CefRefPtr<CefResponse> response) {
  EXPECT_TRUE(settings->response);
  if (!settings->response) {
    return;
  }

  response->SetStatus(settings->response->GetStatus());
  response->SetStatusText(settings->response->GetStatusText());
  response->SetMimeType(settings->response->GetMimeType());

  CefResponse::HeaderMap headerMap;
  settings->response->GetHeaderMap(headerMap);

  if (settings->expect_save_cookie) {
    std::stringstream ss;
    ss << kRequestSaveCookieName << "=" << "save-cookie-value";
    headerMap.insert(std::make_pair("Set-Cookie", ss.str()));
  }

  response->SetHeaderMap(headerMap);
}

// Based on https://en.wikipedia.org/wiki/Basic_access_authentication#Protocol
void GetAuthResponse(CefRefPtr<CefResponse> response) {
  response->SetStatus(401);
  response->SetStatusText("Unauthorized");
  response->SetMimeType("text/html");

  CefResponse::HeaderMap headerMap;
  headerMap.insert(
      std::make_pair("WWW-Authenticate", "Basic realm=\"Test Realm\""));
  response->SetHeaderMap(headerMap);
}

bool IsAuthorized(CefRefPtr<CefRequest> request,
                  const std::string& username,
                  const std::string& password) {
  const std::string& authHeader = request->GetHeaderByName("Authorization");
  if (authHeader.empty()) {
    return false;
  }

  if (authHeader.find("Basic ") == 0) {
    const std::string& base64 = authHeader.substr(6);
    CefRefPtr<CefBinaryValue> data = CefBase64Decode(base64);
    EXPECT_TRUE(data);
    if (!data) {
      LOG(ERROR) << "Failed to decode Authorization value: " << base64;
      return false;
    }

    std::string decoded;
    decoded.resize(data->GetSize());
    data->GetData(&decoded[0], data->GetSize(), 0);

    const std::string& expected = username + ":" + password;
    EXPECT_STREQ(expected.c_str(), decoded.c_str());
    return decoded == expected;
  }

  LOG(ERROR) << "Unexpected Authorization value: " << authHeader;
  return false;
}

// SCHEME HANDLER BACKEND

// Serves request responses.
class RequestSchemeHandlerOld : public CefResourceHandler {
 public:
  RequestSchemeHandlerOld(RequestRunSettings* settings,
                          base::OnceClosure destroy_callback)
      : settings_(settings), destroy_callback_(std::move(destroy_callback)) {}

  ~RequestSchemeHandlerOld() override {
    EXPECT_EQ(1, cancel_ct_);
    std::move(destroy_callback_).Run();
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();
    VerifyNormalRequest(settings_, request, false);

    // HEAD requests are identical to GET requests except no response data is
    // sent.
    if (request->GetMethod() != "HEAD") {
      response_data_ = settings_->response_data;
    }

    // Continue immediately.
    callback->Continue();
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    EXPECT_IO_THREAD();
    GetNormalResponse(settings_, response);
    response_length = response_data_.length();
  }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();

    bool has_data = false;
    bytes_read = 0;

    size_t size = response_data_.length();
    if (offset_ < size) {
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(size - offset_));
      memcpy(data_out, response_data_.c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

  void Cancel() override {
    EXPECT_IO_THREAD();
    cancel_ct_++;
  }

 private:
  // |settings_| is not owned by this object.
  RequestRunSettings* settings_;
  base::OnceClosure destroy_callback_;

  std::string response_data_;
  size_t offset_ = 0;

  int cancel_ct_ = 0;

  IMPLEMENT_REFCOUNTING(RequestSchemeHandlerOld);
  DISALLOW_COPY_AND_ASSIGN(RequestSchemeHandlerOld);
};

class RequestSchemeHandler : public CefResourceHandler {
 public:
  RequestSchemeHandler(RequestRunSettings* settings,
                       base::OnceClosure destroy_callback)
      : settings_(settings), destroy_callback_(std::move(destroy_callback)) {}

  ~RequestSchemeHandler() override {
    EXPECT_EQ(1, cancel_ct_);
    std::move(destroy_callback_).Run();
  }

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));
    VerifyNormalRequest(settings_, request, false);

    // HEAD requests are identical to GET requests except no response data is
    // sent.
    if (request->GetMethod() != "HEAD") {
      response_data_ = settings_->response_data;
    }

    // Continue immediately.
    handle_request = true;
    return true;
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(false);  // Not reached.
    return false;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    EXPECT_IO_THREAD();
    GetNormalResponse(settings_, response);
    response_length = response_data_.length() - offset_;
  }

  bool Skip(int64_t bytes_to_skip,
            int64_t& bytes_skipped,
            CefRefPtr<CefResourceSkipCallback> callback) override {
    size_t size = response_data_.length();
    if (offset_ < size) {
      bytes_skipped =
          std::min(bytes_to_skip, static_cast<int64_t>(size - offset_));
      offset_ += bytes_skipped;
    } else {
      bytes_skipped = ERR_FAILED;
    }

    return bytes_skipped > 0;
  }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    // Default to response complete.
    bool has_data = false;
    bytes_read = 0;

    size_t size = response_data_.length();
    if (offset_ < size) {
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(size - offset_));
      memcpy(data_out, response_data_.c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(false);  // Not reached.
    bytes_read = -2;
    return false;
  }

  void Cancel() override {
    EXPECT_IO_THREAD();
    cancel_ct_++;
  }

 private:
  // |settings_| is not owned by this object.
  RequestRunSettings* settings_;
  base::OnceClosure destroy_callback_;

  std::string response_data_;
  size_t offset_ = 0;

  int cancel_ct_ = 0;

  IMPLEMENT_REFCOUNTING(RequestSchemeHandler);
  DISALLOW_COPY_AND_ASSIGN(RequestSchemeHandler);
};

// Serves redirect request responses.
class RequestRedirectSchemeHandlerOld : public CefResourceHandler {
 public:
  RequestRedirectSchemeHandlerOld(CefRefPtr<CefRequest> request,
                                  CefRefPtr<CefResponse> response,
                                  base::OnceClosure destroy_callback)
      : request_(request),
        response_(response),
        destroy_callback_(std::move(destroy_callback)) {}

  ~RequestRedirectSchemeHandlerOld() override {
    EXPECT_EQ(1, cancel_ct_);
    std::move(destroy_callback_).Run();
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();

    // Verify that the request was sent correctly.
    TestRequestEqual(request_, request, true);

    // Continue immediately.
    callback->Continue();
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    EXPECT_IO_THREAD();

    response->SetStatus(response_->GetStatus());
    response->SetStatusText(response_->GetStatusText());
    response->SetMimeType(response_->GetMimeType());

    CefResponse::HeaderMap headerMap;
    response_->GetHeaderMap(headerMap);
    response->SetHeaderMap(headerMap);

    response_length = 0;
  }

  bool ReadResponse(void* response_data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();
    NOTREACHED();
    return false;
  }

  void Cancel() override {
    EXPECT_IO_THREAD();
    cancel_ct_++;
  }

 private:
  CefRefPtr<CefRequest> request_;
  CefRefPtr<CefResponse> response_;
  base::OnceClosure destroy_callback_;

  int cancel_ct_ = 0;

  IMPLEMENT_REFCOUNTING(RequestRedirectSchemeHandlerOld);
  DISALLOW_COPY_AND_ASSIGN(RequestRedirectSchemeHandlerOld);
};

class RequestRedirectSchemeHandler : public CefResourceHandler {
 public:
  RequestRedirectSchemeHandler(CefRefPtr<CefRequest> request,
                               CefRefPtr<CefResponse> response,
                               base::OnceClosure destroy_callback)
      : request_(request),
        response_(response),
        destroy_callback_(std::move(destroy_callback)) {}

  ~RequestRedirectSchemeHandler() override {
    EXPECT_EQ(1, cancel_ct_);
    std::move(destroy_callback_).Run();
  }

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    // Verify that the request was sent correctly.
    TestRequestEqual(request_, request, true);

    // Continue immediately.
    handle_request = true;
    return true;
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(false);  // Not reached.
    return false;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    EXPECT_IO_THREAD();

    response->SetStatus(response_->GetStatus());
    response->SetStatusText(response_->GetStatusText());
    response->SetMimeType(response_->GetMimeType());

    CefResponse::HeaderMap headerMap;
    response_->GetHeaderMap(headerMap);
    response->SetHeaderMap(headerMap);

    response_length = 0;
  }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    EXPECT_TRUE(false);  // Not reached.
    bytes_read = -1;
    return false;
  }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(false);  // Not reached.
    bytes_read = -2;
    return false;
  }

  void Cancel() override {
    EXPECT_IO_THREAD();
    cancel_ct_++;
  }

 private:
  CefRefPtr<CefRequest> request_;
  CefRefPtr<CefResponse> response_;
  base::OnceClosure destroy_callback_;

  int cancel_ct_ = 0;

  IMPLEMENT_REFCOUNTING(RequestRedirectSchemeHandler);
  DISALLOW_COPY_AND_ASSIGN(RequestRedirectSchemeHandler);
};

// Resource handler implementation that never completes. Used to test
// destruction handling behavior for in-progress requests.
class IncompleteSchemeHandlerOld : public CefResourceHandler {
 public:
  IncompleteSchemeHandlerOld(RequestRunSettings* settings,
                             base::OnceClosure destroy_callback)
      : settings_(settings), destroy_callback_(std::move(destroy_callback)) {
    EXPECT_NE(settings_->incomplete_type, RequestRunSettings::INCOMPLETE_NONE);
  }

  ~IncompleteSchemeHandlerOld() override {
    EXPECT_EQ(1, process_request_ct_);
    EXPECT_EQ(1, cancel_ct_);

    if (settings_->incomplete_type ==
        RequestRunSettings::INCOMPLETE_READ_RESPONSE) {
      EXPECT_EQ(1, get_response_headers_ct_);
      EXPECT_EQ(1, read_response_ct_);
    } else {
      EXPECT_EQ(0, get_response_headers_ct_);
      EXPECT_EQ(0, read_response_ct_);
    }

    std::move(destroy_callback_).Run();
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();

    process_request_ct_++;

    if (settings_->incomplete_type ==
        RequestRunSettings::INCOMPLETE_PROCESS_REQUEST) {
      // Never release or execute this callback.
      incomplete_callback_ = callback;
    } else {
      callback->Continue();
    }
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(settings_->incomplete_type,
              RequestRunSettings::INCOMPLETE_READ_RESPONSE);

    get_response_headers_ct_++;

    response->SetStatus(settings_->response->GetStatus());
    response->SetStatusText(settings_->response->GetStatusText());
    response->SetMimeType(settings_->response->GetMimeType());

    CefResponse::HeaderMap headerMap;
    settings_->response->GetHeaderMap(headerMap);
    settings_->response->SetHeaderMap(headerMap);

    response_length = static_cast<int64_t>(settings_->response_data.size());
  }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(settings_->incomplete_type,
              RequestRunSettings::INCOMPLETE_READ_RESPONSE);

    read_response_ct_++;

    // Never release or execute this callback.
    incomplete_callback_ = callback;
    bytes_read = 0;
    return true;
  }

  void Cancel() override {
    EXPECT_IO_THREAD();
    cancel_ct_++;
  }

 private:
  RequestRunSettings* const settings_;
  base::OnceClosure destroy_callback_;

  int process_request_ct_ = 0;
  int get_response_headers_ct_ = 0;
  int read_response_ct_ = 0;
  int cancel_ct_ = 0;

  CefRefPtr<CefCallback> incomplete_callback_;

  IMPLEMENT_REFCOUNTING(IncompleteSchemeHandlerOld);
  DISALLOW_COPY_AND_ASSIGN(IncompleteSchemeHandlerOld);
};

class IncompleteSchemeHandler : public CefResourceHandler {
 public:
  IncompleteSchemeHandler(RequestRunSettings* settings,
                          base::OnceClosure destroy_callback)
      : settings_(settings), destroy_callback_(std::move(destroy_callback)) {
    EXPECT_NE(settings_->incomplete_type, RequestRunSettings::INCOMPLETE_NONE);
  }

  ~IncompleteSchemeHandler() override {
    EXPECT_EQ(1, open_ct_);
    EXPECT_EQ(1, cancel_ct_);

    if (settings_->incomplete_type ==
        RequestRunSettings::INCOMPLETE_READ_RESPONSE) {
      EXPECT_EQ(1, get_response_headers_ct_);
      EXPECT_EQ(1, read_ct_);
    } else {
      EXPECT_EQ(0, get_response_headers_ct_);
      EXPECT_EQ(0, read_ct_);
    }

    std::move(destroy_callback_).Run();
  }

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    open_ct_++;

    if (settings_->incomplete_type ==
        RequestRunSettings::INCOMPLETE_PROCESS_REQUEST) {
      // Never release or execute this callback.
      incomplete_open_callback_ = callback;
    } else {
      // Continue immediately.
      handle_request = true;
    }
    return true;
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(false);  // Not reached.
    return false;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
                          CefString& redirectUrl) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(settings_->incomplete_type,
              RequestRunSettings::INCOMPLETE_READ_RESPONSE);

    get_response_headers_ct_++;

    response->SetStatus(settings_->response->GetStatus());
    response->SetStatusText(settings_->response->GetStatusText());
    response->SetMimeType(settings_->response->GetMimeType());

    CefResponse::HeaderMap headerMap;
    settings_->response->GetHeaderMap(headerMap);
    settings_->response->SetHeaderMap(headerMap);

    response_length = static_cast<int64_t>(settings_->response_data.size());
  }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));
    EXPECT_EQ(settings_->incomplete_type,
              RequestRunSettings::INCOMPLETE_READ_RESPONSE);

    read_ct_++;

    // Never release or execute this callback.
    incomplete_read_callback_ = callback;
    bytes_read = 0;
    return true;
  }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(false);  // Not reached.
    bytes_read = -2;
    return false;
  }

  void Cancel() override {
    EXPECT_IO_THREAD();
    cancel_ct_++;
  }

 private:
  RequestRunSettings* const settings_;
  base::OnceClosure destroy_callback_;

  int open_ct_ = 0;
  int get_response_headers_ct_ = 0;
  int read_ct_ = 0;
  int cancel_ct_ = 0;

  CefRefPtr<CefCallback> incomplete_open_callback_;
  CefRefPtr<CefResourceReadCallback> incomplete_read_callback_;

  IMPLEMENT_REFCOUNTING(IncompleteSchemeHandler);
  DISALLOW_COPY_AND_ASSIGN(IncompleteSchemeHandler);
};

class RequestSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  RequestSchemeHandlerFactory() = default;

  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();

    handler_create_ct_++;
    auto destroy_callback =
        base::BindOnce(&RequestSchemeHandlerFactory::OnHandlerDestroyed, this);

    RequestDataMap::Entry entry = data_map_.Find(request->GetURL());
    if (entry.type == RequestDataMap::Entry::TYPE_NORMAL) {
      if (entry.settings->incomplete_type ==
          RequestRunSettings::INCOMPLETE_NONE) {
        if (TestOldResourceAPI()) {
          return new RequestSchemeHandlerOld(entry.settings,
                                             std::move(destroy_callback));
        }
        return new RequestSchemeHandler(entry.settings,
                                        std::move(destroy_callback));
      }

      if (TestOldResourceAPI()) {
        return new IncompleteSchemeHandlerOld(entry.settings,
                                              std::move(destroy_callback));
      }
      return new IncompleteSchemeHandler(entry.settings,
                                         std::move(destroy_callback));
    } else if (entry.type == RequestDataMap::Entry::TYPE_REDIRECT) {
      if (TestOldResourceAPI()) {
        return new RequestRedirectSchemeHandlerOld(entry.redirect_request,
                                                   entry.redirect_response,
                                                   std::move(destroy_callback));
      }
      return new RequestRedirectSchemeHandler(entry.redirect_request,
                                              entry.redirect_response,
                                              std::move(destroy_callback));
    }

    // Unknown test.
    ADD_FAILURE();
    return nullptr;
  }

  void SetOwnerTaskRunner(CefRefPtr<CefTaskRunner> task_runner) {
    data_map_.SetOwnerTaskRunner(task_runner);
  }

  void AddSchemeHandler(RequestRunSettings* settings) {
    const std::string& scheme = GetRequestScheme(false);

    // Verify that the scheme is correct.
    const std::string& url = settings->request->GetURL();
    EXPECT_EQ(0U, url.find(scheme));

    if (settings->redirect_request) {
      // Verify that the scheme is correct.
      const std::string& redirect_url = settings->redirect_request->GetURL();
      EXPECT_EQ(0U, redirect_url.find(scheme));
    }

    data_map_.AddSchemeHandler(settings);
  }

  void OnHandlerDestroyed() {
    if (!CefCurrentlyOn(TID_IO)) {
      CefPostTask(TID_IO,
                  base::BindOnce(
                      &RequestSchemeHandlerFactory::OnHandlerDestroyed, this));
      return;
    }

    handler_destroy_ct_++;

    MaybeShutdown();
  }

  void Shutdown(base::OnceClosure complete_callback) {
    if (!CefCurrentlyOn(TID_IO)) {
      CefPostTask(TID_IO, base::BindOnce(&RequestSchemeHandlerFactory::Shutdown,
                                         this, std::move(complete_callback)));
      return;
    }

    EXPECT_TRUE(shutdown_callback_.is_null());
    shutdown_callback_ = std::move(complete_callback);

    data_map_.SetOwnerTaskRunner(nullptr);

    MaybeShutdown();
  }

 private:
  void MaybeShutdown() {
    if (!shutdown_callback_.is_null() &&
        handler_create_ct_ == handler_destroy_ct_) {
      std::move(shutdown_callback_).Run();
    }
  }

  RequestDataMap data_map_;

  int handler_create_ct_ = 0;
  int handler_destroy_ct_ = 0;
  base::OnceClosure shutdown_callback_;

  IMPLEMENT_REFCOUNTING(RequestSchemeHandlerFactory);
  DISALLOW_COPY_AND_ASSIGN(RequestSchemeHandlerFactory);
};

// SERVER BACKEND

// HTTP server handler.
class RequestServerHandler : public test_server::ObserverHelper {
 public:
  RequestServerHandler() = default;

  ~RequestServerHandler() override { RunCompleteCallback(false); }

  // Must be called before CreateServer().
  void AddSchemeHandler(RequestRunSettings* settings) {
    EXPECT_FALSE(initialized_);
    data_map_.AddSchemeHandler(settings);
  }

  // Must be called before CreateServer().
  void SetExpectedRequestCount(int count) {
    EXPECT_FALSE(initialized_);
    expected_http_request_ct_ = count;
  }

  // |complete_callback| will be executed on the UI thread after the server is
  // started.
  void CreateServer(base::OnceClosure complete_callback) {
    EXPECT_UI_THREAD();

    if (expected_http_request_ct_ < 0) {
      // Default to the assumption of one request per registered URL.
      SetExpectedRequestCount(static_cast<int>(data_map_.size()));
    }

    EXPECT_FALSE(initialized_);
    initialized_ = true;

    EXPECT_TRUE(complete_callback_.is_null());
    complete_callback_ = std::move(complete_callback);

    Initialize(/*https_server=*/false);
  }

  // Results in a call to VerifyResults() and eventual execution of the
  // |complete_callback| on the UI thread via RequestServerHandler destruction.
  void ShutdownServer(base::OnceClosure complete_callback) {
    EXPECT_UI_THREAD();

    EXPECT_TRUE(complete_callback_.is_null());
    complete_callback_ = std::move(complete_callback);

    Shutdown();
  }

  void OnInitialized(const std::string& server_origin) override {
    EXPECT_UI_THREAD();
    EXPECT_STREQ(server_origin.c_str(), GetRequestOrigin(true).c_str());
    EXPECT_FALSE(got_initialized_);
    got_initialized_.yes();

    RunCompleteCallback(true);
  }

  void OnShutdown() override {
    EXPECT_UI_THREAD();
    EXPECT_FALSE(got_shutdown_);
    got_shutdown_.yes();

    data_map_.SetOwnerTaskRunner(nullptr);

    VerifyResults();

    delete this;
  }

  bool OnTestServerRequest(CefRefPtr<CefRequest> request,
                           const ResponseCallback& response_callback) override {
    EXPECT_UI_THREAD();

    // Log the requests for better error reporting.
    request_log_ += request->GetMethod().ToString() + " " +
                    request->GetURL().ToString() + "\n";

    actual_http_request_ct_++;

    return HandleRequest(request, response_callback);
  }

 private:
  void VerifyResults() {
    EXPECT_TRUE(got_initialized_);
    EXPECT_TRUE(got_shutdown_);
    EXPECT_EQ(expected_http_request_ct_, actual_http_request_ct_)
        << request_log_;
  }

  bool HandleRequest(CefRefPtr<CefRequest> request,
                     const ResponseCallback& response_callback) {
    RequestDataMap::Entry entry = data_map_.Find(request->GetURL());
    if (entry.type == RequestDataMap::Entry::TYPE_NORMAL) {
      const bool needs_auth = entry.settings->expect_authentication &&
                              !IsAuthorized(request, entry.settings->username,
                                            entry.settings->password);
      if (needs_auth) {
        return HandleAuthRequest(request, response_callback);
      }

      return HandleNormalRequest(request, response_callback, entry.settings);
    } else if (entry.type == RequestDataMap::Entry::TYPE_REDIRECT) {
      return HandleRedirectRequest(request, response_callback,
                                   entry.redirect_request,
                                   entry.redirect_response);
    } else {
      // Unknown test.
      ADD_FAILURE() << "url: " << request->GetURL().ToString();
    }
    return false;
  }

  static bool HandleAuthRequest(CefRefPtr<CefRequest> request,
                                const ResponseCallback& response_callback) {
    CefRefPtr<CefResponse> response = CefResponse::Create();
    GetAuthResponse(response);
    response_callback.Run(response, std::string());
    return true;
  }

  static bool HandleNormalRequest(CefRefPtr<CefRequest> request,
                                  const ResponseCallback& response_callback,
                                  RequestRunSettings* settings) {
    VerifyNormalRequest(settings, request, true);

    CefRefPtr<CefResponse> response = CefResponse::Create();
    GetNormalResponse(settings, response);

    // HEAD requests are identical to GET requests except no response data is
    // sent.
    std::string response_data;
    if (request->GetMethod() != "HEAD") {
      size_t expected_offset = settings->expected_download_offset;
      response_data = settings->response_data.substr(expected_offset);
    }

    response_callback.Run(response, response_data);
    return true;
  }

  static bool HandleRedirectRequest(CefRefPtr<CefRequest> request,
                                    const ResponseCallback& response_callback,
                                    CefRefPtr<CefRequest> redirect_request,
                                    CefRefPtr<CefResponse> redirect_response) {
    if (redirect_response->GetStatus() == 302) {
      // Simulate wrong copying of POST-specific headers Content-Type and
      // Content-Length. A 302 redirect should end up in a GET request and
      // these headers should not propagate from a 302 POST-to-GET redirect.
      CefResponse::HeaderMap redirectHeaderMap;
      redirect_response->GetHeaderMap(redirectHeaderMap);
      redirectHeaderMap.insert(
          std::make_pair("content-type", "application/x-www-form-urlencoded"));
      redirectHeaderMap.insert(std::make_pair("content-length", "0"));
      redirect_response->SetHeaderMap(redirectHeaderMap);
    }

    // Verify that the request was sent correctly.
    TestRequestEqual(redirect_request, request, true);

    response_callback.Run(redirect_response, std::string());
    return true;
  }

  void RunCompleteCallback(bool startup) {
    EXPECT_UI_THREAD();

    if (startup) {
      // Transfer DataMap ownership to the UI thread.
      data_map_.SetOwnerTaskRunner(CefTaskRunner::GetForCurrentThread());
    }

    EXPECT_FALSE(complete_callback_.is_null());
    std::move(complete_callback_).Run();
  }

  RequestDataMap data_map_;

  bool initialized_ = false;

  // Only accessed on the UI thread.
  base::OnceClosure complete_callback_;

  // After initialization the below members are only accessed on the server
  // thread.

  TrackCallback got_initialized_;
  TrackCallback got_shutdown_;

  int expected_http_request_ct_ = -1;
  int actual_http_request_ct_ = 0;

  std::string request_log_;
};

// SHARED TEST RUNNER

// Executes the tests.
class RequestTestRunner : public base::RefCountedThreadSafe<RequestTestRunner> {
 public:
  using TestCallback = base::RepeatingCallback<void(base::OnceClosure)>;

  RequestTestRunner(bool is_server_backend,
                    bool use_frame_method,
                    base::OnceClosure incomplete_request_callback)
      : is_server_backend_(is_server_backend),
        use_frame_method_(use_frame_method),
        incomplete_request_callback_(std::move(incomplete_request_callback)) {
    owner_task_runner_ = CefTaskRunner::GetForCurrentThread();
    EXPECT_TRUE(owner_task_runner_.get());
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());
  }

  void Initialize() {
// Helper macro for registering test callbacks.
#define REGISTER_TEST(test_mode, setup_method, run_method)                  \
  RegisterTest(test_mode,                                                   \
               base::BindRepeating(&RequestTestRunner::setup_method, this), \
               base::BindRepeating(&RequestTestRunner::run_method, this));

    // Register the test callbacks.
    REGISTER_TEST(REQTEST_GET, SetupGetTest, SingleRunTest);
    REGISTER_TEST(REQTEST_GET_NODATA, SetupGetNoDataTest, SingleRunTest);
    REGISTER_TEST(REQTEST_GET_PARTIAL_CONTENT, SetupGetPartialContentTest,
                  SingleRunTest);
    REGISTER_TEST(REQTEST_GET_ALLOWCOOKIES, SetupGetAllowCookiesTest,
                  SingleRunTest);
    REGISTER_TEST(REQTEST_GET_REDIRECT, SetupGetRedirectTest, SingleRunTest);
    REGISTER_TEST(REQTEST_GET_REDIRECT_STOP, SetupGetRedirectStopTest,
                  SingleRunTest);
    REGISTER_TEST(REQTEST_GET_REDIRECT_LOCATION, SetupGetRedirectLocationTest,
                  SingleRunTest);
    REGISTER_TEST(REQTEST_GET_REFERRER, SetupGetReferrerTest, SingleRunTest);
    REGISTER_TEST(REQTEST_GET_AUTH, SetupGetAuthTest, SingleRunTest);
    REGISTER_TEST(REQTEST_POST, SetupPostTest, SingleRunTest);
    REGISTER_TEST(REQTEST_POST_FILE, SetupPostFileTest, SingleRunTest);
    REGISTER_TEST(REQTEST_POST_WITHPROGRESS, SetupPostWithProgressTest,
                  SingleRunTest);
    REGISTER_TEST(REQTEST_POST_REDIRECT, SetupPostRedirectTest, SingleRunTest);
    REGISTER_TEST(REQTEST_POST_REDIRECT_TOGET, SetupPostRedirectToGetTest,
                  SingleRunTest);
    REGISTER_TEST(REQTEST_HEAD, SetupHeadTest, SingleRunTest);
    REGISTER_TEST(REQTEST_CACHE_WITH_CONTROL, SetupCacheWithControlTest,
                  MultipleRunTest);
    REGISTER_TEST(REQTEST_CACHE_WITHOUT_CONTROL, SetupCacheWithoutControlTest,
                  MultipleRunTest);
    REGISTER_TEST(REQTEST_CACHE_SKIP_FLAG, SetupCacheSkipFlagTest,
                  MultipleRunTest);
    REGISTER_TEST(REQTEST_CACHE_SKIP_HEADER, SetupCacheSkipHeaderTest,
                  MultipleRunTest);
    REGISTER_TEST(REQTEST_CACHE_ONLY_FAILURE_FLAG,
                  SetupCacheOnlyFailureFlagTest, MultipleRunTest);
    REGISTER_TEST(REQTEST_CACHE_ONLY_FAILURE_HEADER,
                  SetupCacheOnlyFailureHeaderTest, MultipleRunTest);
    REGISTER_TEST(REQTEST_CACHE_ONLY_SUCCESS_FLAG,
                  SetupCacheOnlySuccessFlagTest, MultipleRunTest);
    REGISTER_TEST(REQTEST_CACHE_ONLY_SUCCESS_HEADER,
                  SetupCacheOnlySuccessHeaderTest, MultipleRunTest);
    REGISTER_TEST(REQTEST_CACHE_DISABLE_FLAG, SetupCacheDisableFlagTest,
                  MultipleRunTest);
    REGISTER_TEST(REQTEST_CACHE_DISABLE_HEADER, SetupCacheDisableHeaderTest,
                  MultipleRunTest);
    REGISTER_TEST(REQTEST_INCOMPLETE_PROCESS_REQUEST,
                  SetupIncompleteProcessRequestTest, SingleRunTest);
    REGISTER_TEST(REQTEST_INCOMPLETE_READ_RESPONSE,
                  SetupIncompleteReadResponseTest, SingleRunTest);
  }

  void Destroy() {
    owner_task_runner_ = nullptr;
    request_context_ = nullptr;
    incomplete_request_callback_.Reset();
  }

  // Called in the browser process to set the request context that will be used
  // when creating the URL request.
  void SetRequestContext(CefRefPtr<CefRequestContext> request_context) {
    request_context_ = request_context;
  }
  CefRefPtr<CefRequestContext> GetRequestContext() const {
    return request_context_;
  }

  // Called to setup the test.
  void SetupTest(RequestTestMode test_mode,
                 base::OnceClosure complete_callback) {
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());

    TestMap::const_iterator it = test_map_.find(test_mode);
    if (it != test_map_.end()) {
      auto safe_complete_callback =
          base::BindOnce(&RequestTestRunner::CompleteOnCorrectThread, this,
                         std::move(complete_callback));
      it->second.setup.Run(base::BindOnce(&RequestTestRunner::SetupContinue,
                                          this,
                                          std::move(safe_complete_callback)));
    } else {
      // Unknown test.
      ADD_FAILURE();
      std::move(complete_callback).Run();
    }
  }

  // Called to run the test.
  void RunTest(RequestTestMode test_mode,
               CefRefPtr<CefFrame> frame,
               base::OnceClosure complete_callback) {
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());

    frame_ = frame;

    TestMap::const_iterator it = test_map_.find(test_mode);
    if (it != test_map_.end()) {
      auto safe_complete_callback =
          base::BindOnce(&RequestTestRunner::CompleteOnCorrectThread, this,
                         std::move(complete_callback));
      it->second.run.Run(std::move(safe_complete_callback));
    } else {
      // Unknown test.
      ADD_FAILURE();
      std::move(complete_callback).Run();
    }
  }

  // Called to shut down the test.
  void ShutdownTest(base::OnceClosure complete_callback) {
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());

    auto safe_complete_callback =
        base::BindOnce(&RequestTestRunner::CompleteOnCorrectThread, this,
                       std::move(complete_callback));

    if (!post_file_tmpdir_.IsEmpty()) {
      CefPostTask(TID_FILE_USER_VISIBLE,
                  base::BindOnce(&RequestTestRunner::RunDeleteTempDirectory,
                                 this, std::move(safe_complete_callback)));
      return;
    }

    // Continue with test shutdown.
    RunShutdown(std::move(safe_complete_callback));
  }

 private:
  // Continued after |settings_| is populated for the test.
  void SetupContinue(base::OnceClosure complete_callback) {
    if (!owner_task_runner_->BelongsToCurrentThread()) {
      owner_task_runner_->PostTask(CefCreateClosureTask(
          base::BindOnce(&RequestTestRunner::SetupContinue, this,
                         std::move(complete_callback))));
      return;
    }

    SetupTestBackend(std::move(complete_callback));
  }

  std::string GetTestPath(const std::string& name) {
    return std::string("/Browser") + name;
  }

  std::string GetTestURL(const std::string& name) {
    // Avoid name duplication between tests running in different processes.
    // Otherwise we'll get unexpected state leakage (cache hits) when running
    // multiple tests.
    return GetRequestOrigin(is_server_backend_) + GetTestPath(name);
  }

  void SetupGetTestShared() {
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(GetTestURL("GetTest.html"));
    settings_.request->SetMethod("GET");

    settings_.response = CefResponse::Create();
    settings_.response->SetMimeType("text/html");
    settings_.response->SetStatus(200);
    settings_.response->SetStatusText("OK");

    settings_.response_data = "GET TEST SUCCESS";
  }

  void SetupGetTest(base::OnceClosure complete_callback) {
    SetupGetTestShared();
    std::move(complete_callback).Run();
  }

  void SetupGetNoDataTest(base::OnceClosure complete_callback) {
    // Start with the normal get test.
    SetupGetTestShared();

    // Disable download data notifications.
    settings_.request->SetFlags(UR_FLAG_NO_DOWNLOAD_DATA);

    settings_.expect_download_data = false;

    std::move(complete_callback).Run();
  }

  void SetupGetPartialContentTest(base::OnceClosure complete_callback) {
    // Start with the normal get test.
    SetupGetTestShared();

    // Skip first 4 bytes of content and expect to receive the rest
    settings_.request->SetHeaderByName("Range", "bytes=4-", true);
    settings_.response->SetHeaderByName("Content-Range", "bytes 4-8/8", true);
    settings_.response->SetStatus(206);
    settings_.response->SetStatusText("Partial Content");
    settings_.expected_download_offset = 4;

    std::move(complete_callback).Run();
  }

  void SetupGetAllowCookiesTest(base::OnceClosure complete_callback) {
    // Start with the normal get test.
    SetupGetTestShared();

    // Send cookies.
    settings_.request->SetFlags(UR_FLAG_ALLOW_STORED_CREDENTIALS);

    settings_.expect_save_cookie = true;
    settings_.expect_send_cookie = true;

    std::move(complete_callback).Run();
  }

  void SetupGetRedirectTest(base::OnceClosure complete_callback) {
    // Start with the normal get test.
    SetupGetTestShared();

    // Add a redirect request.
    settings_.redirect_request = CefRequest::Create();
    settings_.redirect_request->SetURL(GetTestURL("redirect.html"));
    settings_.redirect_request->SetMethod("GET");

    settings_.redirect_response = CefResponse::Create();
    settings_.redirect_response->SetMimeType("text/html");
    settings_.redirect_response->SetStatus(302);
    settings_.redirect_response->SetStatusText("Found");

    CefResponse::HeaderMap headerMap;
    headerMap.insert(std::make_pair("Location", settings_.request->GetURL()));
    settings_.redirect_response->SetHeaderMap(headerMap);

    std::move(complete_callback).Run();
  }

  void SetupGetRedirectStopTest(base::OnceClosure complete_callback) {
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(GetTestURL("GetTest.html"));
    settings_.request->SetMethod("GET");

    // With the test server only the status is expected
    // on stop redirects.
    settings_.response = CefResponse::Create();
    settings_.response->SetStatus(302);
    settings_.response->SetStatusText("Found");

    // Add a redirect request.
    settings_.redirect_request = CefRequest::Create();
    settings_.redirect_request->SetURL(GetTestURL("redirect.html"));
    settings_.redirect_request->SetMethod("GET");
    settings_.redirect_request->SetFlags(UR_FLAG_STOP_ON_REDIRECT);

    settings_.redirect_response = CefResponse::Create();
    settings_.redirect_response->SetMimeType("text/html");
    settings_.redirect_response->SetStatus(302);
    settings_.redirect_response->SetStatusText("Found");

    CefResponse::HeaderMap headerMap;
    headerMap.insert(std::make_pair("Location", settings_.request->GetURL()));
    settings_.redirect_response->SetHeaderMap(headerMap);

    settings_.expected_status = UR_CANCELED;
    settings_.expected_error_code = ERR_ABORTED;
    settings_.expect_download_data = false;
    settings_.expect_download_progress = false;
    settings_.expected_send_count = 1;
    settings_.expected_receive_count = 1;

    std::move(complete_callback).Run();
  }

  void SetupGetRedirectLocationTest(base::OnceClosure complete_callback) {
    // Start with the normal get test.
    SetupGetTestShared();

    // Add a redirect request.
    settings_.redirect_request = CefRequest::Create();
    settings_.redirect_request->SetURL(GetTestURL("redirect.html"));
    settings_.redirect_request->SetMethod("GET");

    settings_.redirect_response = CefResponse::Create();
    settings_.redirect_response->SetMimeType("text/html");
    settings_.redirect_response->SetStatus(302);
    settings_.redirect_response->SetStatusText("Found");

    CefResponse::HeaderMap headerMap;
    headerMap.insert(std::make_pair("LoCaTioN", GetTestPath("GetTest.html")));
    settings_.redirect_response->SetHeaderMap(headerMap);

    std::move(complete_callback).Run();
  }

  void SetupGetReferrerTest(base::OnceClosure complete_callback) {
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(GetTestURL("GetTest.html"));
    settings_.request->SetMethod("GET");

    // The referrer URL must be HTTP or HTTPS. This is enforced by
    // GURL::GetAsReferrer() called from URLRequest::SetReferrer().
    settings_.request->SetReferrer("https://tests.com/referrer.html",
                                   REFERRER_POLICY_DEFAULT);

    settings_.response = CefResponse::Create();
    settings_.response->SetMimeType("text/html");
    settings_.response->SetStatus(200);
    settings_.response->SetStatusText("OK");

    settings_.response_data = "GET TEST SUCCESS";

    std::move(complete_callback).Run();
  }

  void SetupGetAuthTest(base::OnceClosure complete_callback) {
    // Start with the normal get test.
    SetupGetTestShared();

    // Require Basic authentication.
    settings_.expect_authentication = true;
    settings_.username = "user";
    settings_.password = "pass";

    // This flag is required to support credentials, which means we'll also get
    // the cookies.
    settings_.request->SetFlags(UR_FLAG_ALLOW_STORED_CREDENTIALS);
    settings_.expect_save_cookie = true;
    settings_.expect_send_cookie = true;

    // The authentication request will come first, then the actual request.
    settings_.expected_receive_count = 2;
    settings_.expected_send_count = 2;

    std::move(complete_callback).Run();
  }

  void SetupPostTestShared() {
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(GetTestURL("PostTest.html"));
    settings_.request->SetMethod("POST");
    SetUploadData(settings_.request, "the_post_data");

    settings_.response = CefResponse::Create();
    settings_.response->SetMimeType("text/html");
    settings_.response->SetStatus(200);
    settings_.response->SetStatusText("OK");

    settings_.response_data = "POST TEST SUCCESS";
  }

  void SetupPostTest(base::OnceClosure complete_callback) {
    SetupPostTestShared();
    std::move(complete_callback).Run();
  }

  void SetupPostFileTest(base::OnceClosure complete_callback) {
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(GetTestURL("PostFileTest.html"));
    settings_.request->SetMethod("POST");

    settings_.response = CefResponse::Create();
    settings_.response->SetMimeType("text/html");
    settings_.response->SetStatus(200);
    settings_.response->SetStatusText("OK");

    settings_.response_data = "POST TEST SUCCESS";

    CefPostTask(TID_FILE_USER_VISIBLE,
                base::BindOnce(&RequestTestRunner::SetupPostFileTestContinue,
                               this, std::move(complete_callback)));
  }

  void SetupPostFileTestContinue(base::OnceClosure complete_callback) {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE_USER_VISIBLE));

    EXPECT_TRUE(post_file_tmpdir_.CreateUniqueTempDir());
    const std::string& path =
        client::file_util::JoinPath(post_file_tmpdir_.GetPath(), "example.txt");
    const char content[] = "HELLO FRIEND!";
    int write_ct =
        client::file_util::WriteFile(path, content, sizeof(content) - 1);
    EXPECT_EQ(static_cast<int>(sizeof(content) - 1), write_ct);
    SetUploadFile(settings_.request, path);

    std::move(complete_callback).Run();
  }

  void SetupPostWithProgressTest(base::OnceClosure complete_callback) {
    // Start with the normal post test.
    SetupPostTestShared();

    // Enable upload progress notifications.
    settings_.request->SetFlags(UR_FLAG_REPORT_UPLOAD_PROGRESS);

    settings_.expect_upload_progress = true;

    std::move(complete_callback).Run();
  }

  void SetupPostRedirectTest(base::OnceClosure complete_callback) {
    // Start with the normal post test.
    SetupPostTestShared();

    // Add a redirect request.
    settings_.redirect_request = CefRequest::Create();
    settings_.redirect_request->SetURL(GetTestURL("redirect.html"));
    settings_.redirect_request->SetMethod("POST");
    SetUploadData(settings_.redirect_request, "the_post_data");

    settings_.redirect_response = CefResponse::Create();
    settings_.redirect_response->SetMimeType("text/html");
    // Only 307 is supported for redirecting the same method and post data.
    settings_.redirect_response->SetStatus(307);
    settings_.redirect_response->SetStatusText("Found");

    CefResponse::HeaderMap headerMap;
    headerMap.insert(std::make_pair("Location", settings_.request->GetURL()));
    settings_.redirect_response->SetHeaderMap(headerMap);

    std::move(complete_callback).Run();
  }

  void SetupPostRedirectToGetTest(base::OnceClosure complete_callback) {
    // Start with the normal post test.
    SetupPostTestShared();

    // The expected result after redirect is a GET request without POST data.
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(GetTestURL("PostTest.html"));
    settings_.request->SetMethod("GET");

    // Add a redirect request.
    settings_.redirect_request = CefRequest::Create();
    settings_.redirect_request->SetURL(GetTestURL("redirect.html"));
    settings_.redirect_request->SetMethod("POST");
    SetUploadData(settings_.redirect_request, "the_post_data");

    settings_.redirect_response = CefResponse::Create();
    settings_.redirect_response->SetMimeType("text/html");
    // Redirect codes other than 307 will cause conversion to GET and removal
    // of POST data.
    settings_.redirect_response->SetStatus(302);
    settings_.redirect_response->SetStatusText("Found");

    CefResponse::HeaderMap headerMap;
    headerMap.insert(std::make_pair("Location", settings_.request->GetURL()));
    settings_.redirect_response->SetHeaderMap(headerMap);

    std::move(complete_callback).Run();
  }

  void SetupHeadTest(base::OnceClosure complete_callback) {
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(GetTestURL("HeadTest.html"));
    settings_.request->SetMethod("HEAD");

    settings_.response = CefResponse::Create();
    settings_.response->SetMimeType("text/html");
    settings_.response->SetStatus(200);
    settings_.response->SetStatusText("OK");

    // The backend will disregard this value when it returns the result.
    settings_.response_data = "HEAD TEST SUCCESS";

    settings_.expect_download_progress = false;
    settings_.expect_download_data = false;

    std::move(complete_callback).Run();
  }

  void SetupCacheShared(const std::string& name, bool with_cache_control) {
    // Start with the normal get test.
    SetupGetTestShared();

    // Specify a unique URL.
    settings_.request->SetURL(GetTestURL(name));

    if (with_cache_control) {
      // Allow the page to be cached for 10 seconds.
      CefResponse::HeaderMap headerMap;
      headerMap.insert(std::make_pair(kCacheControlHeader, "max-age=10"));
      settings_.response->SetHeaderMap(headerMap);
    }
  }

  void SetupCacheWithControlTest(base::OnceClosure complete_callback) {
    SetupCacheShared("CacheWithControlTest.html", true);

    // Send multiple requests. With the Cache-Control response header the 2nd+
    // should receive cached data.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 1;
    settings_.setup_next_request =
        base::BindOnce(&RequestTestRunner::SetupCacheWithControlTestNext, this);

    std::move(complete_callback).Run();
  }

  void SetupCacheWithControlTestNext(int next_send_count,
                                     base::OnceClosure complete_callback) {
    // Only handle from the cache.
    settings_.expect_response_was_cached = true;

    // The following requests will use the same setup, so no more callbacks
    // are required.
    EXPECT_TRUE(settings_.setup_next_request.is_null());

    std::move(complete_callback).Run();
  }

  void SetupCacheWithoutControlTest(base::OnceClosure complete_callback) {
    SetupCacheShared("CacheWithoutControlTest.html", false);

    // Send multiple requests. Without the Cache-Control response header all
    // should be received.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 3;

    std::move(complete_callback).Run();
  }

  void SetupCacheSkipFlagTest(base::OnceClosure complete_callback) {
    SetupCacheShared("CacheSkipFlagTest.html", true);

    // Skip the cache despite the the Cache-Control response header.
    // This will not read from the cache, but still write to the cache.
    settings_.request->SetFlags(UR_FLAG_SKIP_CACHE);

    // Send multiple requests. The 1st request will be handled normally,
    // but not result in any reads from the cache. The 2nd request will
    // expect a cached response and the 3nd request will skip the cache
    // again.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 2;
    settings_.setup_next_request =
        base::BindOnce(&RequestTestRunner::SetupCacheSkipFlagTestNext, this);

    std::move(complete_callback).Run();
  }

  void SetupCacheSkipFlagTestNext(int next_send_count,
                                  base::OnceClosure complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheSkipFlagTest.html", true);

    // Expect a cached response.
    settings_.expect_response_was_cached = true;
    settings_.setup_next_request =
        base::BindOnce(&RequestTestRunner::SetupCacheSkipFlagTestLast, this);

    std::move(complete_callback).Run();
  }

  void SetupCacheSkipFlagTestLast(int next_send_count,
                                  base::OnceClosure complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheSkipFlagTest.html", true);

    // Skip the cache despite the the Cache-Control response header.
    settings_.request->SetFlags(UR_FLAG_SKIP_CACHE);

    // Expect the cache to be skipped.
    settings_.expect_response_was_cached = false;
    EXPECT_TRUE(settings_.setup_next_request.is_null());

    std::move(complete_callback).Run();
  }

  void SetupCacheSkipHeaderTest(base::OnceClosure complete_callback) {
    SetupCacheShared("CacheSkipHeaderTest.html", true);

    // Skip the cache despite the the Cache-Control response header.
    // This will not read from the cache, but still write to the cache.
    CefRequest::HeaderMap headerMap;
    headerMap.insert(std::make_pair(kCacheControlHeader, "no-cache"));
    settings_.request->SetHeaderMap(headerMap);

    // Send multiple requests. The 1st request will be handled normally,
    // but not result in any reads from the cache. The 2nd request will
    // expect a cached response and the 3nd request will skip the cache
    // again.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 2;
    settings_.setup_next_request =
        base::BindOnce(&RequestTestRunner::SetupCacheSkipHeaderTestNext, this);

    std::move(complete_callback).Run();
  }

  void SetupCacheSkipHeaderTestNext(int next_send_count,
                                    base::OnceClosure complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheSkipHeaderTest.html", true);

    // Expect a cached response.
    settings_.expect_response_was_cached = true;
    settings_.setup_next_request =
        base::BindOnce(&RequestTestRunner::SetupCacheSkipHeaderTestLast, this);

    std::move(complete_callback).Run();
  }

  void SetupCacheSkipHeaderTestLast(int next_send_count,
                                    base::OnceClosure complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheSkipHeaderTest.html", true);

    // Skip the cache despite the the Cache-Control response header.
    settings_.request->SetFlags(UR_FLAG_SKIP_CACHE);

    // Expect the cache to be skipped.
    settings_.expect_response_was_cached = false;
    EXPECT_TRUE(settings_.setup_next_request.is_null());

    std::move(complete_callback).Run();
  }

  void SetupCacheOnlyFailureFlagTest(base::OnceClosure complete_callback) {
    SetupCacheShared("CacheOnlyFailureFlagTest.html", true);

    // Only handle from the cache.
    settings_.request->SetFlags(UR_FLAG_ONLY_FROM_CACHE);

    // Send multiple requests. All should fail because there's no entry in the
    // cache currently.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 0;

    // The request is expected to fail.
    settings_.SetRequestFailureExpected(ERR_CACHE_MISS);

    std::move(complete_callback).Run();
  }

  void SetupCacheOnlyFailureHeaderTest(base::OnceClosure complete_callback) {
    SetupCacheShared("CacheOnlyFailureFlagTest.html", true);

    // Only handle from the cache.
    CefRequest::HeaderMap headerMap;
    headerMap.insert(std::make_pair(kCacheControlHeader, "only-if-cached"));
    settings_.request->SetHeaderMap(headerMap);

    // Send multiple requests. All should fail because there's no entry in the
    // cache currently.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 0;

    // The request is expected to fail.
    settings_.SetRequestFailureExpected(ERR_CACHE_MISS);

    std::move(complete_callback).Run();
  }

  void SetupCacheOnlySuccessFlagTest(base::OnceClosure complete_callback) {
    SetupCacheShared("CacheOnlySuccessFlagTest.html", false);

    // Send multiple requests. The 1st request will be handled normally. The
    // 2nd+ requests will be configured by SetupCacheOnlySuccessFlagNext to
    // require cached data.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 1;
    settings_.setup_next_request =
        base::BindOnce(&RequestTestRunner::SetupCacheOnlySuccessFlagNext, this);

    std::move(complete_callback).Run();
  }

  void SetupCacheOnlySuccessFlagNext(int next_send_count,
                                     base::OnceClosure complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheOnlySuccessFlagTest.html", false);

    // Only handle from the cache.
    settings_.request->SetFlags(UR_FLAG_ONLY_FROM_CACHE);
    settings_.expect_response_was_cached = true;

    // The following requests will use the same setup, so no more callbacks
    // are required.
    EXPECT_TRUE(settings_.setup_next_request.is_null());

    std::move(complete_callback).Run();
  }

  void SetupCacheOnlySuccessHeaderTest(base::OnceClosure complete_callback) {
    SetupCacheShared("CacheOnlySuccessHeaderTest.html", false);

    // Send multiple requests. The 1st request will be handled normally. The
    // 2nd+ requests will be configured by SetupCacheOnlySuccessHeaderNext to
    // require cached data.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 1;
    settings_.setup_next_request = base::BindOnce(
        &RequestTestRunner::SetupCacheOnlySuccessHeaderNext, this);

    std::move(complete_callback).Run();
  }

  void SetupCacheOnlySuccessHeaderNext(int next_send_count,
                                       base::OnceClosure complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheOnlySuccessHeaderTest.html", false);

    // Only handle from the cache.
    CefRequest::HeaderMap headerMap;
    headerMap.insert(std::make_pair(kCacheControlHeader, "only-if-cached"));
    settings_.request->SetHeaderMap(headerMap);
    settings_.expect_response_was_cached = true;

    // The following requests will use the same setup, so no more callbacks
    // are required.
    EXPECT_TRUE(settings_.setup_next_request.is_null());

    std::move(complete_callback).Run();
  }

  void SetupCacheDisableFlagTest(base::OnceClosure complete_callback) {
    SetupCacheShared("CacheDisableFlagTest.html", true);

    // Disable the cache despite the the Cache-Control response header.
    settings_.request->SetFlags(UR_FLAG_DISABLE_CACHE);

    // Send multiple requests. The 1st request will be handled normally,
    // but not result in any reads from or writes to the cache.
    // Therefore all following requests that are set to be only handled
    // from the cache should fail.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 1;
    settings_.setup_next_request =
        base::BindOnce(&RequestTestRunner::SetupCacheDisableFlagTestNext, this);

    std::move(complete_callback).Run();
  }

  void SetupCacheDisableFlagTestNext(int next_send_count,
                                     base::OnceClosure complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheDisableFlagTest.html", true);

    // Only handle from the cache.
    settings_.request->SetFlags(UR_FLAG_ONLY_FROM_CACHE);

    // The request is expected to fail.
    settings_.SetRequestFailureExpected(ERR_CACHE_MISS);

    // The following requests will use the same setup, so no more callbacks
    // are required.
    EXPECT_TRUE(settings_.setup_next_request.is_null());

    std::move(complete_callback).Run();
  }

  void SetupCacheDisableHeaderTest(base::OnceClosure complete_callback) {
    SetupCacheShared("CacheDisableHeaderTest.html", true);

    // Disable the cache despite the the Cache-Control response header.
    CefRequest::HeaderMap headerMap;
    headerMap.insert(std::make_pair(kCacheControlHeader, "no-store"));
    settings_.request->SetHeaderMap(headerMap);

    // Send multiple requests. The 1st request will be handled normally,
    // but not result in any reads from or writes to the cache.
    // Therefore all following requests that are set to be only handled
    // from the cache should fail.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 1;
    settings_.setup_next_request = base::BindOnce(
        &RequestTestRunner::SetupCacheDisableHeaderTestNext, this);

    std::move(complete_callback).Run();
  }

  void SetupCacheDisableHeaderTestNext(int next_send_count,
                                       base::OnceClosure complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheDisableHeaderTest.html", true);

    // Only handle from the cache.
    CefRequest::HeaderMap headerMap;
    headerMap.insert(std::make_pair(kCacheControlHeader, "only-if-cached"));
    settings_.request->SetHeaderMap(headerMap);

    // The request is expected to fail.
    settings_.SetRequestFailureExpected(ERR_CACHE_MISS);

    // The following requests will use the same setup, so no more callbacks
    // are required.
    EXPECT_TRUE(settings_.setup_next_request.is_null());

    std::move(complete_callback).Run();
  }

  void SetupIncompleteProcessRequestTest(base::OnceClosure complete_callback) {
    // Start with the normal get test.
    SetupGetTestShared();

    settings_.incomplete_type = RequestRunSettings::INCOMPLETE_PROCESS_REQUEST;

    // There will be no response and the request will be aborted.
    settings_.response = CefResponse::Create();
    settings_.response_data.clear();
    settings_.expected_error_code = ERR_ABORTED;
    settings_.expected_status = UR_FAILED;
    settings_.expect_download_progress = false;
    settings_.expect_download_data = false;

    std::move(complete_callback).Run();
  }

  void SetupIncompleteReadResponseTest(base::OnceClosure complete_callback) {
    // Start with the normal get test.
    SetupGetTestShared();

    settings_.incomplete_type = RequestRunSettings::INCOMPLETE_READ_RESPONSE;

    // There will be a response but the request will be aborted without
    // receiving any data.
    settings_.response_data = test_server::kIncompleteDoNotSendData;
    settings_.expected_error_code = ERR_ABORTED;
    settings_.expected_status = UR_FAILED;
    settings_.expect_download_progress = true;
    settings_.expect_download_data = false;

    std::move(complete_callback).Run();
  }

  // Send a request. |complete_callback| will be executed on request completion.
  void SendRequest(test_request::RequestDoneCallback done_callback) {
    test_request::SendConfig config;

    if (settings_.redirect_request) {
      config.request_ = settings_.redirect_request;
    } else {
      config.request_ = settings_.request;
    }
    EXPECT_TRUE(config.request_.get());

    // Not delegating to CefRequestHandler::GetAuthCredentials.
    if (!use_frame_method_ && settings_.expect_authentication) {
      config.has_credentials_ = true;
      config.username_ = settings_.username;
      config.password_ = settings_.password;
    }

    if (use_frame_method_) {
      EXPECT_TRUE(frame_);
      config.frame_ = frame_;
    } else {
      config.request_context_ = request_context_;
    }

    test_request::Send(config, std::move(done_callback));

    if (settings_.incomplete_type != RequestRunSettings::INCOMPLETE_NONE) {
      std::move(incomplete_request_callback_).Run();
    }
  }

  // Verify a response.
  void VerifyResponse(const test_request::State* const client) {
    CefRefPtr<CefRequest> expected_request;
    CefRefPtr<CefResponse> expected_response;

    if (settings_.redirect_request) {
      expected_request = settings_.redirect_request;
    } else {
      expected_request = settings_.request;
    }

    if (settings_.redirect_response && !settings_.expect_follow_redirect) {
      // A redirect response was sent but the redirect is not expected to be
      // followed.
      expected_response = settings_.redirect_response;
    } else {
      expected_response = settings_.response;
    }

    TestRequestEqual(expected_request, client->request_, false);

    EXPECT_EQ(settings_.expected_status, client->status_);
    EXPECT_EQ(settings_.expected_error_code, client->error_code_);
    if (expected_response && client->response_) {
      TestResponseEqual(expected_response, client->response_, true);
    }

    EXPECT_EQ(settings_.expect_response_was_cached,
              client->response_was_cached_);

    EXPECT_EQ(1, client->request_complete_ct_);

    if (settings_.expect_upload_progress) {
      EXPECT_LE(1, client->upload_progress_ct_);

      std::string upload_data;
      GetUploadData(expected_request, upload_data);
      EXPECT_EQ((int64_t)upload_data.size(), client->upload_total_);
    } else {
      EXPECT_EQ(0, client->upload_progress_ct_);
      EXPECT_EQ(0, client->upload_total_);
    }

    if (settings_.expect_download_progress) {
      EXPECT_LE(1, client->download_progress_ct_);
      EXPECT_EQ((int64_t)(settings_.response_data.size() -
                          settings_.expected_download_offset),
                client->download_total_);
    } else {
      EXPECT_EQ(0, client->download_progress_ct_);
      EXPECT_EQ(0, client->download_total_);
    }

    if (settings_.expect_download_data) {
      size_t expected_offset = settings_.expected_download_offset;
      EXPECT_LE(1, client->download_data_ct_);
      EXPECT_STREQ(settings_.response_data.substr(expected_offset).c_str(),
                   client->download_data_.c_str());
    } else {
      EXPECT_EQ(0, client->download_data_ct_);
      EXPECT_TRUE(client->download_data_.empty());
    }

    if (settings_.expect_authentication) {
      EXPECT_EQ(1, client->auth_credentials_ct_);
    } else {
      EXPECT_EQ(0, client->auth_credentials_ct_);
    }
  }

  // Run a test with a single request.
  void SingleRunTest(base::OnceClosure complete_callback) {
    SendRequest(base::BindOnce(&RequestTestRunner::SingleRunTestComplete, this,
                               std::move(complete_callback)));
  }

  void SingleRunTestComplete(base::OnceClosure complete_callback,
                             const test_request::State& completed_client) {
    VerifyResponse(&completed_client);
    std::move(complete_callback).Run();
  }

  // Run a test with multiple requests.
  void MultipleRunTest(base::OnceClosure complete_callback) {
    EXPECT_GT(settings_.expected_send_count, 0);
    EXPECT_GE(settings_.expected_receive_count, 0);
    MultipleRunTestContinue(std::move(complete_callback), 1);
  }

  void MultipleRunTestContinue(base::OnceClosure complete_callback,
                               int send_count) {
    // Send the next request.
    SendRequest(base::BindOnce(&RequestTestRunner::MultipleRunTestNext, this,
                               std::move(complete_callback), send_count));
  }

  void MultipleRunTestNext(base::OnceClosure complete_callback,
                           int send_count,
                           const test_request::State& completed_client) {
    // Verify the completed request.
    VerifyResponse(&completed_client);

    if (send_count == settings_.expected_send_count) {
      // All requests complete.
      std::move(complete_callback).Run();
      return;
    }

    const int next_send_count = send_count + 1;
    auto continue_callback =
        base::BindOnce(&RequestTestRunner::MultipleRunTestContinue, this,
                       std::move(complete_callback), next_send_count);

    if (!settings_.setup_next_request.is_null()) {
      // Provide an opportunity to modify expectations before the next request.
      std::move(settings_.setup_next_request)
          .Run(next_send_count, std::move(continue_callback));
    } else {
      std::move(continue_callback).Run();
    }
  }

  // Register a test. Called in the constructor.
  void RegisterTest(RequestTestMode test_mode,
                    TestCallback setup,
                    TestCallback run) {
    TestEntry entry = {setup, run};
    test_map_.insert(std::make_pair(test_mode, entry));
  }

  void CompleteOnCorrectThread(base::OnceClosure complete_callback) {
    if (!owner_task_runner_->BelongsToCurrentThread()) {
      owner_task_runner_->PostTask(CefCreateClosureTask(
          base::BindOnce(&RequestTestRunner::CompleteOnCorrectThread, this,
                         std::move(complete_callback))));
      return;
    }

    std::move(complete_callback).Run();
  }

  void RunDeleteTempDirectory(base::OnceClosure complete_callback) {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE_USER_VISIBLE));

    EXPECT_TRUE(post_file_tmpdir_.Delete());
    EXPECT_TRUE(post_file_tmpdir_.IsEmpty());

    // Continue with test shutdown.
    RunShutdown(std::move(complete_callback));
  }

  void RunShutdown(base::OnceClosure complete_callback) {
    if (!owner_task_runner_->BelongsToCurrentThread()) {
      owner_task_runner_->PostTask(CefCreateClosureTask(
          base::BindOnce(&RequestTestRunner::RunShutdown, this,
                         std::move(complete_callback))));
      return;
    }

    ShutdownTestBackend(std::move(complete_callback));
  }

  // Create the backend for the current test. Called during test setup.
  void SetupTestBackend(base::OnceClosure complete_callback) {
    EXPECT_TRUE(settings_.request.get());
    EXPECT_TRUE(settings_.response.get() ||
                settings_.expected_status == UR_FAILED);

    if (is_server_backend_) {
      StartServer(std::move(complete_callback));
    } else {
      AddSchemeHandler(std::move(complete_callback));
    }
  }

  void StartServer(base::OnceClosure complete_callback) {
    EXPECT_FALSE(server_handler_);

    server_handler_ = new RequestServerHandler();

    server_handler_->AddSchemeHandler(&settings_);
    if (settings_.expected_receive_count >= 0) {
      server_handler_->SetExpectedRequestCount(
          settings_.expected_receive_count);
    }

    server_handler_->CreateServer(std::move(complete_callback));
  }

  void AddSchemeHandler(base::OnceClosure complete_callback) {
    EXPECT_FALSE(scheme_factory_);

    // Add the factory registration.
    scheme_factory_ = new RequestSchemeHandlerFactory();
    request_context_->RegisterSchemeHandlerFactory(GetRequestScheme(false),
                                                   GetRequestHost(false, false),
                                                   scheme_factory_.get());

    scheme_factory_->AddSchemeHandler(&settings_);

    // Any further calls will come from the IO thread.
    scheme_factory_->SetOwnerTaskRunner(CefTaskRunner::GetForThread(TID_IO));

    std::move(complete_callback).Run();
  }

  // Shutdown the backend for the current test. Called during test shutdown.
  void ShutdownTestBackend(base::OnceClosure complete_callback) {
    if (is_server_backend_) {
      ShutdownServer(std::move(complete_callback));
    } else {
      RemoveSchemeHandler(std::move(complete_callback));
    }
  }

  void ShutdownServer(base::OnceClosure complete_callback) {
    EXPECT_TRUE(server_handler_);

    // |server_handler_| will delete itself after shutdown.
    server_handler_->ShutdownServer(std::move(complete_callback));
    server_handler_ = nullptr;
  }

  void RemoveSchemeHandler(base::OnceClosure complete_callback) {
    EXPECT_TRUE(scheme_factory_);

    // Remove the factory registration.
    request_context_->RegisterSchemeHandlerFactory(
        GetRequestScheme(false), GetRequestHost(false, false), nullptr);
    scheme_factory_->Shutdown(std::move(complete_callback));
    scheme_factory_ = nullptr;
  }

  const bool is_server_backend_;
  const bool use_frame_method_;

  // Used with incomplete request tests.
  base::OnceClosure incomplete_request_callback_;

  // Primary thread runner (UI thread) for the object that owns us.
  CefRefPtr<CefTaskRunner> owner_task_runner_;

  CefRefPtr<CefRequestContext> request_context_;

  // Frame that originates the request. May be nullptr.
  CefRefPtr<CefFrame> frame_;

  struct TestEntry {
    TestCallback setup;
    TestCallback run;
  };
  typedef std::map<RequestTestMode, TestEntry> TestMap;
  TestMap test_map_;

  // Server backend.
  RequestServerHandler* server_handler_ = nullptr;

  // Scheme handler backend.
  std::string scheme_name_;
  CefRefPtr<RequestSchemeHandlerFactory> scheme_factory_;

  CefScopedTempDir post_file_tmpdir_;

 public:
  RequestRunSettings settings_;
};

class RequestTestHandler : public TestHandler {
 public:
  RequestTestHandler(RequestTestMode test_mode,
                     ContextTestMode context_mode,
                     bool test_server_backend,
                     bool test_frame_method)
      : test_mode_(test_mode),
        context_mode_(context_mode),
        test_server_backend_(test_server_backend),
        test_frame_method_(test_frame_method),
        test_url_(GetRequestOrigin(test_server_backend) +
                  "/URLRequestTest.Test") {}

  void RunTest() override {
    // Time out the test after a reasonable period of time.
    SetTestTimeout(5000);

    // Start pre-setup actions.
    PreSetupStart();
  }

  void PreSetupStart() {
    CefPostTask(TID_FILE_USER_VISIBLE,
                base::BindOnce(&RequestTestHandler::PreSetupFileTasks, this));
  }

  void PreSetupFileTasks() {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE_USER_VISIBLE));

    if (context_mode_ == CONTEXT_ONDISK) {
      EXPECT_TRUE(context_tmpdir_.CreateUniqueTempDirUnderPath(
          CefTestSuite::GetInstance()->root_cache_path()));
      context_tmpdir_path_ = context_tmpdir_.GetPath();
      EXPECT_FALSE(context_tmpdir_path_.empty());
    }

    CefPostTask(TID_UI,
                base::BindOnce(&RequestTestHandler::PreSetupContinue, this));
  }

  void PreSetupContinue() {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));

    test_runner_ = new RequestTestRunner(
        test_server_backend_, test_frame_method_,
        base::BindOnce(&RequestTestHandler::OnIncompleteRequest, this));
    test_runner_->Initialize();

    // Configure the number of times that SignalTestCompletion() will be called.
    // We need to call it at least 1 time if we don't create a browser.
    size_t completion_count = test_frame_method_ ? 0U : 1U;
    if (context_mode_ != CONTEXT_GLOBAL) {
      // Don't end the test until the temporary request context has been
      // destroyed.
      completion_count++;
    }
    if (completion_count > 0U) {
      SetSignalTestCompletionCount(completion_count);
    }

    // Get or create the request context.
    if (context_mode_ == CONTEXT_GLOBAL) {
      CefRefPtr<CefRequestContext> request_context =
          CefRequestContext::GetGlobalContext();
      EXPECT_TRUE(request_context.get());
      test_runner_->SetRequestContext(request_context);

      PreSetupComplete();
    } else {
      CefRequestContextSettings settings;

      if (context_mode_ == CONTEXT_ONDISK) {
        EXPECT_FALSE(context_tmpdir_.IsEmpty());
        CefString(&settings.cache_path) = context_tmpdir_path_;
      }

      if (!test_server_backend_) {
        // Set the schemes that are allowed to store cookies.
        CefString(&settings.cookieable_schemes_list) = GetRequestScheme(false);
      }

      // Create a new temporary request context. Calls OnContextInitialized.
      CefRequestContext::CreateContext(settings,
                                       new RequestContextHandler(this));
    }
  }

  void OnContextInitialized(CefRefPtr<CefRequestContext> request_context) {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_TRUE(request_context.get());
    test_runner_->SetRequestContext(request_context);
    PreSetupComplete();
  }

  void PreSetupComplete() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::BindOnce(&RequestTestHandler::PreSetupComplete, this));
      return;
    }

    // Setup the test. This will create the objects that we test against and
    // register the backend.
    test_runner_->SetupTest(
        test_mode_, base::BindOnce(&RequestTestHandler::OnSetupComplete, this));
  }

  // Browser process setup is complete.
  void OnSetupComplete() {
    // Start post-setup actions.
    SetTestCookie(test_runner_->GetRequestContext(), test_server_backend_,
                  base::BindOnce(&RequestTestHandler::PostSetupComplete, this));
  }

  void PostSetupComplete() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::BindOnce(&RequestTestHandler::PostSetupComplete, this));
      return;
    }

    if (test_frame_method_) {
      AddResource(test_url_, "<html><body>TEST</body></html>", "text/html");

      // Create the browser who's main frame will be the initiator for the
      // request.
      CreateBrowser(test_url_, test_runner_->GetRequestContext());
    } else {
      // Run the test now.
      test_running_ = true;
      test_runner_->RunTest(
          test_mode_, nullptr /* frame */,
          base::BindOnce(&RequestTestHandler::OnRunComplete, this));
    }
  }

  ReturnValue OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefRequest> request,
                                   CefRefPtr<CefCallback> callback) override {
    if (test_running_ && test_frame_method_) {
      EXPECT_TRUE(frame);
      EXPECT_STREQ(test_frame_->GetIdentifier().ToString().c_str(),
                   frame->GetIdentifier().ToString().c_str());
      test_frame_resource_load_ct_++;
    }

    return TestHandler::OnBeforeResourceLoad(browser, frame, request, callback);
  }

  bool GetAuthCredentials(CefRefPtr<CefBrowser> browser,
                          const CefString& origin_url,
                          bool isProxy,
                          const CefString& host,
                          int port,
                          const CefString& realm,
                          const CefString& scheme,
                          CefRefPtr<CefAuthCallback> callback) override {
    EXPECT_TRUE(test_frame_method_);
    auth_credentials_ct_++;
    if (test_runner_->settings_.expect_authentication) {
      callback->Continue(test_runner_->settings_.username,
                         test_runner_->settings_.password);
      return true;
    }
    return false;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (test_frame_method_) {
      // Run the test now.
      test_frame_ = frame;
      test_running_ = true;
      test_runner_->RunTest(
          test_mode_, frame,
          base::BindOnce(&RequestTestHandler::OnRunComplete, this));
      return;
    }
  }

  // Incomplete tests will not complete normally. Instead, we trigger a browser
  // close to abort in-progress requests.
  void OnIncompleteRequest() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI, base::BindOnce(
                              &RequestTestHandler::OnIncompleteRequest, this));
      return;
    }

    EXPECT_TRUE(test_frame_method_);
    EXPECT_NE(RequestRunSettings::INCOMPLETE_NONE,
              test_runner_->settings_.incomplete_type);

    // TestComplete will eventually be called from DestroyTest instead of being
    // triggered by browser destruction.
    CefPostDelayedTask(
        TID_UI, base::BindOnce(&TestHandler::CloseBrowser, GetBrowser(), false),
        1000);
  }

  // Test run is complete.
  void OnRunComplete() {
    GetTestCookie(test_runner_->GetRequestContext(), test_server_backend_,
                  base::BindOnce(&RequestTestHandler::PostRunComplete, this));
  }

  void PostRunComplete(bool has_save_cookie) {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI, base::BindOnce(&RequestTestHandler::PostRunComplete,
                                         this, has_save_cookie));
      return;
    }

    EXPECT_EQ(test_runner_->settings_.expect_save_cookie, has_save_cookie);

    // Shut down the browser side of the test.
    test_runner_->ShutdownTest(
        base::BindOnce(&RequestTestHandler::MaybeClearAuthCredentials, this));
  }

  void MaybeClearAuthCredentials() {
    if (test_runner_->settings_.expect_authentication &&
        context_mode_ == CONTEXT_GLOBAL) {
      // Clear the HTTP authentication cache to avoid leaking state between
      // test runs when using the global request context.
      test_runner_->GetRequestContext()->ClearHttpAuthCredentials(
          new TestCompletionCallback(
              base::BindOnce(&RequestTestHandler::DestroyTest, this)));
      return;
    }

    DestroyTest();
  }

  void DestroyTest() override {
    if (test_frame_method_) {
      // Expect at least 1 call to OnBeforeResourceLoad for every test.
      // Redirect tests may get multiple calls.
      EXPECT_LE(1, test_frame_resource_load_ct_);
    }

    // CefRequestHandler::GetAuthCredentials should be called after
    // CefURLRequestClient::GetAuthCredentials when the request has an
    // associated frame.
    if (test_frame_method_ && test_runner_->settings_.expect_authentication) {
      EXPECT_EQ(1, auth_credentials_ct_);
    } else {
      EXPECT_EQ(0, auth_credentials_ct_);
    }

    TestHandler::DestroyTest();

    // Release references to the context and handler.
    test_runner_->Destroy();

    // These tests don't create a browser that would signal implicitly.
    if (!test_frame_method_) {
      OnTestComplete();
    }
  }

  void OnTestComplete() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::BindOnce(&RequestTestHandler::OnTestComplete, this));
      return;
    }

    if (!context_tmpdir_.IsEmpty()) {
      // Temp directory will be deleted on application shutdown.
      context_tmpdir_.Take();
    }

    SignalTestCompletion();
  }

 private:
  // Used with temporary request contexts to signal test completion once the
  // temporary context has been destroyed.
  class RequestContextHandler : public CefRequestContextHandler {
   public:
    explicit RequestContextHandler(CefRefPtr<RequestTestHandler> test_handler)
        : test_handler_(test_handler) {}
    ~RequestContextHandler() override { test_handler_->OnTestComplete(); }

    void OnRequestContextInitialized(
        CefRefPtr<CefRequestContext> request_context) override {
      test_handler_->OnContextInitialized(request_context);
    }

   private:
    CefRefPtr<RequestTestHandler> test_handler_;

    IMPLEMENT_REFCOUNTING(RequestContextHandler);
  };

  const RequestTestMode test_mode_;
  const ContextTestMode context_mode_;
  const bool test_server_backend_;
  const bool test_frame_method_;
  const std::string test_url_;

  scoped_refptr<RequestTestRunner> test_runner_;

  bool test_running_ = false;

  CefRefPtr<CefFrame> test_frame_;
  int test_frame_resource_load_ct_ = 0;

  CefScopedTempDir context_tmpdir_;
  CefString context_tmpdir_path_;

 public:
  int auth_credentials_ct_ = 0;

  IMPLEMENT_REFCOUNTING(RequestTestHandler);
};

}  // namespace

// Entry point for registering custom schemes.
// Called from client_app_delegates.cc.
void RegisterURLRequestCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
  const std::string& scheme = GetRequestScheme(false);
  registrar->AddCustomScheme(
      scheme, CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_CORS_ENABLED);
}

// Entry point for registering cookieable schemes.
// Called from client_app_delegates.cc.
void RegisterURLRequestCookieableSchemes(
    std::vector<std::string>& cookieable_schemes) {
  const std::string& scheme = GetRequestScheme(false);
  cookieable_schemes.push_back(scheme);
}

// Helpers for defining URLRequest tests.
#define REQ_TEST(name, test_mode, context_mode, test_server_backend,      \
                 test_frame_method)                                       \
  TEST(URLRequestTest, name) {                                            \
    CefRefPtr<RequestTestHandler> handler = new RequestTestHandler(       \
        test_mode, context_mode, test_server_backend, test_frame_method); \
    handler->ExecuteTest();                                               \
    ReleaseAndWaitForDestructor(handler);                                 \
  }

// Define the tests.
#define REQ_TEST_SET_EX(suffix, context_mode, test_server_backend,             \
                        test_frame_method)                                     \
  REQ_TEST(BrowserGET##suffix, REQTEST_GET, context_mode, test_server_backend, \
           test_frame_method)                                                  \
  REQ_TEST(BrowserGETNoData##suffix, REQTEST_GET_NODATA, context_mode,         \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(BrowserGETPartialContent##suffix, REQTEST_GET_PARTIAL_CONTENT,      \
           context_mode, test_server_backend, test_frame_method)               \
  REQ_TEST(BrowserGETAllowCookies##suffix, REQTEST_GET_ALLOWCOOKIES,           \
           context_mode, test_server_backend, test_frame_method)               \
  REQ_TEST(BrowserGETRedirect##suffix, REQTEST_GET_REDIRECT, context_mode,     \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(BrowserGETRedirectStop##suffix, REQTEST_GET_REDIRECT_STOP,          \
           context_mode, test_server_backend, test_frame_method)               \
  REQ_TEST(BrowserGETRedirectLocation##suffix, REQTEST_GET_REDIRECT_LOCATION,  \
           context_mode, test_server_backend, test_frame_method)               \
  REQ_TEST(BrowserGETReferrer##suffix, REQTEST_GET_REFERRER, context_mode,     \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(BrowserPOST##suffix, REQTEST_POST, context_mode,                    \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(BrowserPOSTFile##suffix, REQTEST_POST_FILE, context_mode,           \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(BrowserPOSTWithProgress##suffix, REQTEST_POST_WITHPROGRESS,         \
           context_mode, test_server_backend, test_frame_method)               \
  REQ_TEST(BrowserPOSTRedirect##suffix, REQTEST_POST_REDIRECT, context_mode,   \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(BrowserPOSTRedirectToGET##suffix, REQTEST_POST_REDIRECT_TOGET,      \
           context_mode, test_server_backend, test_frame_method)               \
  REQ_TEST(BrowserHEAD##suffix, REQTEST_HEAD, context_mode,                    \
           test_server_backend, test_frame_method)

#define REQ_TEST_SET(suffix, test_frame_method)                           \
  REQ_TEST_SET_EX(ContextGlobalCustom##suffix, CONTEXT_GLOBAL, false,     \
                  test_frame_method)                                      \
  REQ_TEST_SET_EX(ContextInMemoryCustom##suffix, CONTEXT_INMEMORY, false, \
                  test_frame_method)                                      \
  REQ_TEST_SET_EX(ContextOnDiskCustom##suffix, CONTEXT_ONDISK, false,     \
                  test_frame_method)                                      \
  REQ_TEST_SET_EX(ContextGlobalServer##suffix, CONTEXT_GLOBAL, true,      \
                  test_frame_method)                                      \
  REQ_TEST_SET_EX(ContextInMemoryServer##suffix, CONTEXT_INMEMORY, true,  \
                  test_frame_method)                                      \
  REQ_TEST_SET_EX(ContextOnDiskServer##suffix, CONTEXT_ONDISK, true,      \
                  test_frame_method)

REQ_TEST_SET(WithoutFrame, false)
REQ_TEST_SET(WithFrame, true)

// Define tests that can only run with a frame.
#define REQ_TEST_FRAME_SET_EX(suffix, context_mode, test_server_backend) \
  REQ_TEST(BrowserIncompleteProcessRequest##suffix,                      \
           REQTEST_INCOMPLETE_PROCESS_REQUEST, context_mode,             \
           test_server_backend, true)                                    \
  REQ_TEST(BrowserIncompleteReadResponse##suffix,                        \
           REQTEST_INCOMPLETE_READ_RESPONSE, context_mode,               \
           test_server_backend, true)

#define REQ_TEST_FRAME_SET()                                                 \
  REQ_TEST_FRAME_SET_EX(ContextGlobalCustomWithFrame, CONTEXT_GLOBAL, false) \
  REQ_TEST_FRAME_SET_EX(ContextInMemoryCustomWithFrame, CONTEXT_INMEMORY,    \
                        false)                                               \
  REQ_TEST_FRAME_SET_EX(ContextOnDiskCustomWithFrame, CONTEXT_ONDISK, false) \
  REQ_TEST_FRAME_SET_EX(ContextGlobalServerWithFrame, CONTEXT_GLOBAL, true)  \
  REQ_TEST_FRAME_SET_EX(ContextInMemoryServerWithFrame, CONTEXT_INMEMORY,    \
                        true)                                                \
  REQ_TEST_FRAME_SET_EX(ContextOnDiskServerWithFrame, CONTEXT_ONDISK, true)

REQ_TEST_FRAME_SET()

// Cache and authentication tests can only be run with the server backend.
#define REQ_TEST_CACHE_SET_EX(suffix, context_mode, test_frame_method)         \
  REQ_TEST(BrowserGETCacheWithControl##suffix, REQTEST_CACHE_WITH_CONTROL,     \
           context_mode, true, test_frame_method)                              \
  REQ_TEST(BrowserGETCacheWithoutControl##suffix,                              \
           REQTEST_CACHE_WITHOUT_CONTROL, context_mode, true,                  \
           test_frame_method)                                                  \
  REQ_TEST(BrowserGETCacheSkipFlag##suffix, REQTEST_CACHE_SKIP_FLAG,           \
           context_mode, true, test_frame_method)                              \
  REQ_TEST(BrowserGETCacheSkipHeader##suffix, REQTEST_CACHE_SKIP_HEADER,       \
           context_mode, true, test_frame_method)                              \
  REQ_TEST(BrowserGETCacheOnlyFailureFlag##suffix,                             \
           REQTEST_CACHE_ONLY_FAILURE_FLAG, context_mode, true,                \
           test_frame_method)                                                  \
  REQ_TEST(BrowserGETCacheOnlyFailureHeader##suffix,                           \
           REQTEST_CACHE_ONLY_FAILURE_HEADER, context_mode, true,              \
           test_frame_method)                                                  \
  REQ_TEST(BrowserGETCacheOnlySuccessFlag##suffix,                             \
           REQTEST_CACHE_ONLY_SUCCESS_FLAG, context_mode, true,                \
           test_frame_method)                                                  \
  REQ_TEST(BrowserGETCacheOnlySuccessHeader##suffix,                           \
           REQTEST_CACHE_ONLY_SUCCESS_HEADER, context_mode, true,              \
           test_frame_method)                                                  \
  REQ_TEST(BrowserGETCacheDisableFlag##suffix, REQTEST_CACHE_DISABLE_FLAG,     \
           context_mode, true, test_frame_method)                              \
  REQ_TEST(BrowserGETCacheDisableHeader##suffix, REQTEST_CACHE_DISABLE_HEADER, \
           context_mode, true, test_frame_method)                              \
  REQ_TEST(BrowserGETAuth##suffix, REQTEST_GET_AUTH, context_mode, true,       \
           test_frame_method)

#define REQ_TEST_CACHE_SET(suffix, test_frame_method)                    \
  REQ_TEST_CACHE_SET_EX(ContextGlobalServer##suffix, CONTEXT_GLOBAL,     \
                        test_frame_method)                               \
  REQ_TEST_CACHE_SET_EX(ContextInMemoryServer##suffix, CONTEXT_INMEMORY, \
                        test_frame_method)                               \
  REQ_TEST_CACHE_SET_EX(ContextOnDiskServer##suffix, CONTEXT_ONDISK,     \
                        test_frame_method)

REQ_TEST_CACHE_SET(WithoutFrame, false)
REQ_TEST_CACHE_SET(WithFrame, true)

namespace {

class InvalidURLTestClient : public CefURLRequestClient {
 public:
  InvalidURLTestClient() {
    event_ = CefWaitableEvent::CreateWaitableEvent(true, false);
  }

  void RunTest() {
    CefPostTask(TID_UI,
                base::BindOnce(&InvalidURLTestClient::RunOnUIThread, this));

    // Wait for the test to complete.
    event_->Wait();
  }

  void OnRequestComplete(CefRefPtr<CefURLRequest> client) override {
    EXPECT_EQ(UR_FAILED, client->GetRequestStatus());
    EXPECT_EQ(ERR_UNKNOWN_URL_SCHEME, client->GetRequestError());

    // Let the call stack unwind before signaling completion.
    CefPostTask(TID_UI, base::BindOnce(
                            &InvalidURLTestClient::CompleteOnUIThread, this));
  }

  void OnUploadProgress(CefRefPtr<CefURLRequest> request,
                        int64_t current,
                        int64_t total) override {
    EXPECT_TRUE(false);  // Not reached.
  }

  void OnDownloadProgress(CefRefPtr<CefURLRequest> request,
                          int64_t current,
                          int64_t total) override {
    EXPECT_TRUE(false);  // Not reached.
  }

  void OnDownloadData(CefRefPtr<CefURLRequest> request,
                      const void* data,
                      size_t data_length) override {
    EXPECT_TRUE(false);  // Not reached.
  }

  bool GetAuthCredentials(bool isProxy,
                          const CefString& host,
                          int port,
                          const CefString& realm,
                          const CefString& scheme,
                          CefRefPtr<CefAuthCallback> callback) override {
    EXPECT_TRUE(false);  // Not reached.
    return false;
  }

 private:
  void RunOnUIThread() {
    EXPECT_UI_THREAD();
    CefRefPtr<CefRequest> request = CefRequest::Create();
    request->SetMethod("GET");
    request->SetURL("foo://invalidurl");

    CefURLRequest::Create(request, this, nullptr);
  }

  void CompleteOnUIThread() {
    EXPECT_UI_THREAD();
    // Signal that the test is complete.
    event_->Signal();
  }

  CefRefPtr<CefWaitableEvent> event_;

  IMPLEMENT_REFCOUNTING(InvalidURLTestClient);
};

}  // namespace

// Verify that failed requests do not leak references.
TEST(URLRequestTest, BrowserInvalidURL) {
  CefRefPtr<InvalidURLTestClient> client = new InvalidURLTestClient();
  client->RunTest();
}

// Entry point for creating URLRequest browser test objects.
// Called from client_app_delegates.cc.
void CreateURLRequestBrowserTests(
    client::ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new URLRequestBrowserTest);
}
