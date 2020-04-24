// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <map>
#include <sstream>

#include "include/base/cef_bind.h"
#include "include/cef_parser.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_scheme.h"
#include "include/cef_server.h"
#include "include/cef_task.h"
#include "include/cef_urlrequest.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_suite.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/file_util.h"
#include "tests/shared/renderer/client_app_renderer.h"

using client::ClientAppRenderer;

// How to add a new test:
// 1. Add a new value to the RequestTestMode enumeration.
// 2. Add methods to set up and run the test in RequestTestRunner.
// 3. Add a line for the test in the RequestTestRunner constructor.
// 4. Add lines for the test in the "Define the tests" section at the bottom of
//    the file.

namespace {

// Unique values for URLRequest tests.
const char kRequestTestMsg[] = "URLRequestTest.Test";
const char kIncompleteRequestTestMsg[] = "URLRequestTest.IncompleteRequestTest";

// TEST DATA

// Custom scheme handler backend.
const char kRequestSchemeCustom[] = "urcustom";
const char kRequestHostCustom[] = "test";

// Server backend.
const char kRequestAddressServer[] = "127.0.0.1";
const uint16 kRequestPortServer = 8099;
const char kRequestSchemeServer[] = "http";

const char kRequestSendCookieName[] = "urcookie_send";
const char kRequestSaveCookieName[] = "urcookie_save";

const char kCacheControlHeader[] = "cache-control";

// Used with incomplete tests for data that should not be sent.
const char kIncompleteDoNotSendData[] = "DO NOT SEND";

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
  RequestRunSettings() {}

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

  typedef base::Callback<void(int /* next_send_count */,
                              const base::Closure& /* complete_callback */)>
      NextRequestCallback;

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

    Entry(Type entry_type) : type(entry_type), settings(nullptr) {}

    Type type;

    // Used with TYPE_NORMAL.
    // |settings| is not owned by this object.
    RequestRunSettings* settings;

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
  explicit TestCompletionCallback(const base::Closure& complete_callback)
      : complete_callback_(complete_callback) {
    EXPECT_FALSE(complete_callback_.is_null());
  }

  void OnComplete() override {
    complete_callback_.Run();
    complete_callback_.Reset();
  }

 private:
  base::Closure complete_callback_;

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
                   const base::Closure& callback) {
  class Callback : public CefSetCookieCallback {
   public:
    explicit Callback(const base::Closure& callback) : callback_(callback) {
      EXPECT_FALSE(callback_.is_null());
    }

    void OnComplete(bool success) override {
      EXPECT_TRUE(success);
      callback_.Run();
      callback_.Reset();
    }

   private:
    base::Closure callback_;

    IMPLEMENT_REFCOUNTING(Callback);
  };

  CefCookie cookie;
  CefString(&cookie.name) = kRequestSendCookieName;
  CefString(&cookie.value) = "send-cookie-value";
  CefString(&cookie.domain) = GetRequestHost(server_backend, false);
  CefString(&cookie.path) = "/";
  cookie.has_expires = false;
  EXPECT_TRUE(request_context->GetCookieManager(nullptr)->SetCookie(
      GetRequestOrigin(server_backend), cookie, new Callback(callback)));
}

typedef base::Callback<void(bool /* cookie exists */)> GetTestCookieCallback;

// Tests if the save cookie has been set. If set, it will be deleted at the same
// time.
void GetTestCookie(CefRefPtr<CefRequestContext> request_context,
                   bool server_backend,
                   const GetTestCookieCallback& callback) {
  class Visitor : public CefCookieVisitor {
   public:
    explicit Visitor(const GetTestCookieCallback& callback)
        : callback_(callback), cookie_exists_(false) {
      EXPECT_FALSE(callback_.is_null());
    }
    ~Visitor() override { callback_.Run(cookie_exists_); }

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
    bool cookie_exists_;

    IMPLEMENT_REFCOUNTING(Visitor);
  };

  CefRefPtr<CefCookieManager> cookie_manager =
      request_context->GetCookieManager(nullptr);
  cookie_manager->VisitUrlCookies(GetRequestOrigin(server_backend), true,
                                  new Visitor(callback));
}

std::string GetHeaderValue(const CefRequest::HeaderMap& header_map,
                           const std::string& header_name_lower) {
  CefRequest::HeaderMap::const_iterator it = header_map.begin();
  for (; it != header_map.end(); ++it) {
    std::string name = it->first;
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    if (name == header_name_lower)
      return it->second;
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
  EXPECT_STREQ("en-GB,en;q=0.9",
               GetHeaderValue(headerMap, "accept-language").c_str());

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
  if (!settings->response)
    return;

  response->SetStatus(settings->response->GetStatus());
  response->SetStatusText(settings->response->GetStatusText());
  response->SetMimeType(settings->response->GetMimeType());

  CefResponse::HeaderMap headerMap;
  settings->response->GetHeaderMap(headerMap);

  if (settings->expect_save_cookie) {
    std::stringstream ss;
    ss << kRequestSaveCookieName << "="
       << "save-cookie-value";
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
  if (authHeader.empty())
    return false;

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
                          const base::Closure& destroy_callback)
      : settings_(settings), destroy_callback_(destroy_callback) {}

  ~RequestSchemeHandlerOld() override {
    EXPECT_EQ(1, cancel_ct_);
    destroy_callback_.Run();
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();
    VerifyNormalRequest(settings_, request, false);

    // HEAD requests are identical to GET requests except no response data is
    // sent.
    if (request->GetMethod() != "HEAD")
      response_data_ = settings_->response_data;

    // Continue immediately.
    callback->Continue();
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
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
  base::Closure destroy_callback_;

  std::string response_data_;
  size_t offset_ = 0;

  int cancel_ct_ = 0;

  IMPLEMENT_REFCOUNTING(RequestSchemeHandlerOld);
  DISALLOW_COPY_AND_ASSIGN(RequestSchemeHandlerOld);
};

class RequestSchemeHandler : public CefResourceHandler {
 public:
  RequestSchemeHandler(RequestRunSettings* settings,
                       const base::Closure& destroy_callback)
      : settings_(settings), destroy_callback_(destroy_callback) {}

  ~RequestSchemeHandler() override {
    EXPECT_EQ(1, cancel_ct_);
    destroy_callback_.Run();
  }

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));
    VerifyNormalRequest(settings_, request, false);

    // HEAD requests are identical to GET requests except no response data is
    // sent.
    if (request->GetMethod() != "HEAD")
      response_data_ = settings_->response_data;

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
                          int64& response_length,
                          CefString& redirectUrl) override {
    EXPECT_IO_THREAD();
    GetNormalResponse(settings_, response);
    response_length = response_data_.length() - offset_;
  }

  bool Skip(int64 bytes_to_skip,
            int64& bytes_skipped,
            CefRefPtr<CefResourceSkipCallback> callback) override {
    size_t size = response_data_.length();
    if (offset_ < size) {
      bytes_skipped =
          std::min(bytes_to_skip, static_cast<int64>(size - offset_));
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
  base::Closure destroy_callback_;

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
                                  const base::Closure& destroy_callback)
      : request_(request),
        response_(response),
        destroy_callback_(destroy_callback) {}

  ~RequestRedirectSchemeHandlerOld() override {
    EXPECT_EQ(1, cancel_ct_);
    destroy_callback_.Run();
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
                          int64& response_length,
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
  base::Closure destroy_callback_;

  int cancel_ct_ = 0;

  IMPLEMENT_REFCOUNTING(RequestRedirectSchemeHandlerOld);
  DISALLOW_COPY_AND_ASSIGN(RequestRedirectSchemeHandlerOld);
};

class RequestRedirectSchemeHandler : public CefResourceHandler {
 public:
  RequestRedirectSchemeHandler(CefRefPtr<CefRequest> request,
                               CefRefPtr<CefResponse> response,
                               const base::Closure& destroy_callback)
      : request_(request),
        response_(response),
        destroy_callback_(destroy_callback) {}

  ~RequestRedirectSchemeHandler() override {
    EXPECT_EQ(1, cancel_ct_);
    destroy_callback_.Run();
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
                          int64& response_length,
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
  base::Closure destroy_callback_;

  int cancel_ct_ = 0;

  IMPLEMENT_REFCOUNTING(RequestRedirectSchemeHandler);
  DISALLOW_COPY_AND_ASSIGN(RequestRedirectSchemeHandler);
};

// Resource handler implementation that never completes. Used to test
// destruction handling behavior for in-progress requests.
class IncompleteSchemeHandlerOld : public CefResourceHandler {
 public:
  IncompleteSchemeHandlerOld(RequestRunSettings* settings,
                             const base::Closure& destroy_callback)
      : settings_(settings), destroy_callback_(destroy_callback) {
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

    destroy_callback_.Run();
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
                          int64& response_length,
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

    response_length = static_cast<int64>(settings_->response_data.size());
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
  const base::Closure destroy_callback_;

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
                          const base::Closure& destroy_callback)
      : settings_(settings), destroy_callback_(destroy_callback) {
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

    destroy_callback_.Run();
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
                          int64& response_length,
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

    response_length = static_cast<int64>(settings_->response_data.size());
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
  const base::Closure destroy_callback_;

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
  RequestSchemeHandlerFactory() {}

  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();

    handler_create_ct_++;
    const base::Closure destroy_callback =
        base::Bind(&RequestSchemeHandlerFactory::OnHandlerDestroyed, this);

    RequestDataMap::Entry entry = data_map_.Find(request->GetURL());
    if (entry.type == RequestDataMap::Entry::TYPE_NORMAL) {
      if (entry.settings->incomplete_type ==
          RequestRunSettings::INCOMPLETE_NONE) {
        if (TestOldResourceAPI()) {
          return new RequestSchemeHandlerOld(entry.settings, destroy_callback);
        }
        return new RequestSchemeHandler(entry.settings, destroy_callback);
      }

      if (TestOldResourceAPI()) {
        return new IncompleteSchemeHandlerOld(entry.settings, destroy_callback);
      }
      return new IncompleteSchemeHandler(entry.settings, destroy_callback);
    } else if (entry.type == RequestDataMap::Entry::TYPE_REDIRECT) {
      if (TestOldResourceAPI()) {
        return new RequestRedirectSchemeHandlerOld(
            entry.redirect_request, entry.redirect_response, destroy_callback);
      }
      return new RequestRedirectSchemeHandler(
          entry.redirect_request, entry.redirect_response, destroy_callback);
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
      CefPostTask(
          TID_IO,
          base::Bind(&RequestSchemeHandlerFactory::OnHandlerDestroyed, this));
      return;
    }

    handler_destroy_ct_++;

    MaybeShutdown();
  }

  void Shutdown(const base::Closure& complete_callback) {
    if (!CefCurrentlyOn(TID_IO)) {
      CefPostTask(TID_IO, base::Bind(&RequestSchemeHandlerFactory::Shutdown,
                                     this, complete_callback));
      return;
    }

    EXPECT_TRUE(shutdown_callback_.is_null());
    shutdown_callback_ = complete_callback;

    data_map_.SetOwnerTaskRunner(nullptr);

    MaybeShutdown();
  }

 private:
  void MaybeShutdown() {
    if (!shutdown_callback_.is_null() &&
        handler_create_ct_ == handler_destroy_ct_) {
      shutdown_callback_.Run();
      shutdown_callback_.Reset();
    }
  }

  RequestDataMap data_map_;

  int handler_create_ct_ = 0;
  int handler_destroy_ct_ = 0;
  base::Closure shutdown_callback_;

  IMPLEMENT_REFCOUNTING(RequestSchemeHandlerFactory);
  DISALLOW_COPY_AND_ASSIGN(RequestSchemeHandlerFactory);
};

// SERVER BACKEND

// HTTP server handler.
class RequestServerHandler : public CefServerHandler {
 public:
  RequestServerHandler()
      : initialized_(false),
        expected_connection_ct_(-1),
        actual_connection_ct_(0),
        expected_http_request_ct_(-1),
        actual_http_request_ct_(0) {}

  virtual ~RequestServerHandler() { RunCompleteCallback(false); }

  // Must be called before CreateServer().
  void AddSchemeHandler(RequestRunSettings* settings) {
    EXPECT_FALSE(initialized_);
    data_map_.AddSchemeHandler(settings);
  }

  // Must be called before CreateServer().
  void SetExpectedRequestCount(int count) {
    EXPECT_FALSE(initialized_);
    expected_connection_ct_ = expected_http_request_ct_ = count;
  }

  // |complete_callback| will be executed on the UI thread after the server is
  // started.
  void CreateServer(const base::Closure& complete_callback) {
    EXPECT_UI_THREAD();

    if (expected_connection_ct_ < 0) {
      // Default to the assumption of one request per registered URL.
      SetExpectedRequestCount(static_cast<int>(data_map_.size()));
    }

    EXPECT_FALSE(initialized_);
    initialized_ = true;

    EXPECT_TRUE(complete_callback_.is_null());
    complete_callback_ = complete_callback;

    CefServer::CreateServer(kRequestAddressServer, kRequestPortServer, 10,
                            this);
  }

  // Results in a call to VerifyResults() and eventual execution of the
  // |complete_callback| on the UI thread via RequestServerHandler destruction.
  void ShutdownServer(const base::Closure& complete_callback) {
    EXPECT_UI_THREAD();

    EXPECT_TRUE(complete_callback_.is_null());
    complete_callback_ = complete_callback;

    EXPECT_TRUE(server_);
    if (server_)
      server_->Shutdown();
  }

  void OnServerCreated(CefRefPtr<CefServer> server) override {
    EXPECT_TRUE(server);
    EXPECT_TRUE(server->IsRunning());
    EXPECT_FALSE(server->HasConnection());

    EXPECT_FALSE(got_server_created_);
    got_server_created_.yes();

    EXPECT_FALSE(server_);
    server_ = server;

    EXPECT_FALSE(server_runner_);
    server_runner_ = server_->GetTaskRunner();
    EXPECT_TRUE(server_runner_);
    EXPECT_TRUE(server_runner_->BelongsToCurrentThread());

    CefPostTask(TID_UI, base::Bind(&RequestServerHandler::RunCompleteCallback,
                                   this, true));
  }

  void OnServerDestroyed(CefRefPtr<CefServer> server) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_FALSE(server->IsRunning());
    EXPECT_FALSE(server->HasConnection());

    EXPECT_FALSE(got_server_destroyed_);
    got_server_destroyed_.yes();

    data_map_.SetOwnerTaskRunner(nullptr);
    server_ = nullptr;

    VerifyResults();
  }

  void OnClientConnected(CefRefPtr<CefServer> server,
                         int connection_id) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_TRUE(server->HasConnection());
    EXPECT_TRUE(server->IsValidConnection(connection_id));

    EXPECT_TRUE(connection_id_set_.find(connection_id) ==
                connection_id_set_.end());
    connection_id_set_.insert(connection_id);

    actual_connection_ct_++;
  }

  void OnClientDisconnected(CefRefPtr<CefServer> server,
                            int connection_id) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_FALSE(server->IsValidConnection(connection_id));

    ConnectionIdSet::iterator it = connection_id_set_.find(connection_id);
    EXPECT_TRUE(it != connection_id_set_.end());
    connection_id_set_.erase(it);
  }

  void OnHttpRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     const CefString& client_address,
                     CefRefPtr<CefRequest> request) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_TRUE(VerifyConnection(connection_id));
    EXPECT_FALSE(client_address.empty());

    // Log the requests for better error reporting.
    request_log_ += request->GetMethod().ToString() + " " +
                    request->GetURL().ToString() + "\n";

    HandleRequest(server, connection_id, request);

    actual_http_request_ct_++;
  }

  void OnWebSocketRequest(CefRefPtr<CefServer> server,
                          int connection_id,
                          const CefString& client_address,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefCallback> callback) override {
    NOTREACHED();
  }

  void OnWebSocketConnected(CefRefPtr<CefServer> server,
                            int connection_id) override {
    NOTREACHED();
  }

  void OnWebSocketMessage(CefRefPtr<CefServer> server,
                          int connection_id,
                          const void* data,
                          size_t data_size) override {
    NOTREACHED();
  }

 private:
  bool RunningOnServerThread() {
    return server_runner_ && server_runner_->BelongsToCurrentThread();
  }

  bool VerifyServer(CefRefPtr<CefServer> server) {
    V_DECLARE();
    V_EXPECT_TRUE(RunningOnServerThread());
    V_EXPECT_TRUE(server);
    V_EXPECT_TRUE(server_);
    V_EXPECT_TRUE(server->GetAddress().ToString() ==
                  server_->GetAddress().ToString());
    V_RETURN();
  }

  bool VerifyConnection(int connection_id) {
    return connection_id_set_.find(connection_id) != connection_id_set_.end();
  }

  void VerifyResults() {
    EXPECT_TRUE(RunningOnServerThread());

    EXPECT_TRUE(got_server_created_);
    EXPECT_TRUE(got_server_destroyed_);
    EXPECT_TRUE(connection_id_set_.empty());
    EXPECT_EQ(expected_connection_ct_, actual_connection_ct_) << request_log_;
    EXPECT_EQ(expected_http_request_ct_, actual_http_request_ct_)
        << request_log_;
  }

  void HandleRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     CefRefPtr<CefRequest> request) {
    RequestDataMap::Entry entry = data_map_.Find(request->GetURL());
    if (entry.type == RequestDataMap::Entry::TYPE_NORMAL) {
      const bool needs_auth = entry.settings->expect_authentication &&
                              !IsAuthorized(request, entry.settings->username,
                                            entry.settings->password);
      if (needs_auth) {
        HandleAuthRequest(server, connection_id, request);
        return;
      }

      HandleNormalRequest(server, connection_id, request, entry.settings);
    } else if (entry.type == RequestDataMap::Entry::TYPE_REDIRECT) {
      HandleRedirectRequest(server, connection_id, request,
                            entry.redirect_request, entry.redirect_response);
    } else {
      // Unknown test.
      ADD_FAILURE() << "url: " << request->GetURL().ToString();
      server->SendHttp500Response(connection_id, "Unknown test");
    }
  }

  void HandleAuthRequest(CefRefPtr<CefServer> server,
                         int connection_id,
                         CefRefPtr<CefRequest> request) {
    CefRefPtr<CefResponse> response = CefResponse::Create();
    GetAuthResponse(response);
    SendResponse(server, connection_id, response, std::string());
  }

  void HandleNormalRequest(CefRefPtr<CefServer> server,
                           int connection_id,
                           CefRefPtr<CefRequest> request,
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

    SendResponse(server, connection_id, response, response_data);
  }

  void HandleRedirectRequest(CefRefPtr<CefServer> server,
                             int connection_id,
                             CefRefPtr<CefRequest> request,
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

    SendResponse(server, connection_id, redirect_response, std::string());
  }

  void SendResponse(CefRefPtr<CefServer> server,
                    int connection_id,
                    CefRefPtr<CefResponse> response,
                    const std::string& response_data) {
    const int response_code = response->GetStatus();
    if (response_code <= 0) {
      // Intentionally not responding for incomplete request tests.
      return;
    }

    const CefString& content_type = response->GetMimeType();
    int64 content_length = static_cast<int64>(response_data.size());

    CefResponse::HeaderMap extra_headers;
    response->GetHeaderMap(extra_headers);

    server->SendHttpResponse(connection_id, response_code, content_type,
                             content_length, extra_headers);

    if (response_data == kIncompleteDoNotSendData) {
      // Intentionally not sending data for incomplete request tests.
      return;
    }

    if (content_length != 0) {
      server->SendRawData(connection_id, response_data.data(),
                          response_data.size());
      server->CloseConnection(connection_id);
    }

    // The connection should be closed.
    EXPECT_FALSE(server->IsValidConnection(connection_id));
  }

  void RunCompleteCallback(bool startup) {
    EXPECT_UI_THREAD();

    if (startup) {
      // Transfer DataMap ownership to the server thread.
      data_map_.SetOwnerTaskRunner(server_->GetTaskRunner());
    }

    EXPECT_FALSE(complete_callback_.is_null());
    complete_callback_.Run();
    complete_callback_.Reset();
  }

  RequestDataMap data_map_;

  CefRefPtr<CefServer> server_;
  CefRefPtr<CefTaskRunner> server_runner_;
  bool initialized_;

  // Only accessed on the UI thread.
  base::Closure complete_callback_;

  // After initialization the below members are only accessed on the server
  // thread.

  TrackCallback got_server_created_;
  TrackCallback got_server_destroyed_;

  typedef std::set<int> ConnectionIdSet;
  ConnectionIdSet connection_id_set_;

  int expected_connection_ct_;
  int actual_connection_ct_;
  int expected_http_request_ct_;
  int actual_http_request_ct_;

  std::string request_log_;

  IMPLEMENT_REFCOUNTING(RequestServerHandler);
  DISALLOW_COPY_AND_ASSIGN(RequestServerHandler);
};

// URLREQUEST CLIENT

// Implementation of CefURLRequestClient that stores response information.
class RequestClient : public CefURLRequestClient {
 public:
  typedef base::Callback<void(CefRefPtr<RequestClient>)>
      RequestCompleteCallback;

  explicit RequestClient(const RequestCompleteCallback& complete_callback)
      : complete_callback_(complete_callback) {
    EXPECT_FALSE(complete_callback_.is_null());
  }

  void OnRequestComplete(CefRefPtr<CefURLRequest> request) override {
    request_complete_ct_++;

    request_ = request->GetRequest();
    EXPECT_TRUE(request_->IsReadOnly());
    status_ = request->GetRequestStatus();
    error_code_ = request->GetRequestError();
    response_was_cached_ = request->ResponseWasCached();
    response_ = request->GetResponse();
    if (response_) {
      EXPECT_TRUE(response_->IsReadOnly());
    }

    complete_callback_.Run(this);
  }

  void OnUploadProgress(CefRefPtr<CefURLRequest> request,
                        int64 current,
                        int64 total) override {
    upload_progress_ct_++;
    upload_total_ = total;
  }

  void OnDownloadProgress(CefRefPtr<CefURLRequest> request,
                          int64 current,
                          int64 total) override {
    response_ = request->GetResponse();
    EXPECT_TRUE(response_.get());
    EXPECT_TRUE(response_->IsReadOnly());
    download_progress_ct_++;
    download_total_ = total;
  }

  void OnDownloadData(CefRefPtr<CefURLRequest> request,
                      const void* data,
                      size_t data_length) override {
    response_ = request->GetResponse();
    EXPECT_TRUE(response_.get());
    EXPECT_TRUE(response_->IsReadOnly());
    download_data_ct_++;
    download_data_ += std::string(static_cast<const char*>(data), data_length);
  }

  bool GetAuthCredentials(bool isProxy,
                          const CefString& host,
                          int port,
                          const CefString& realm,
                          const CefString& scheme,
                          CefRefPtr<CefAuthCallback> callback) override {
    auth_credentials_ct_++;
    if (has_authentication_) {
      callback->Continue(username_, password_);
      return true;
    }
    return false;
  }

 private:
  const RequestCompleteCallback complete_callback_;

 public:
  bool has_authentication_ = false;
  std::string username_;
  std::string password_;

  int request_complete_ct_ = 0;
  int upload_progress_ct_ = 0;
  int download_progress_ct_ = 0;
  int download_data_ct_ = 0;
  int auth_credentials_ct_ = 0;

  int64 upload_total_ = 0;
  int64 download_total_ = 0;
  std::string download_data_;
  CefRefPtr<CefRequest> request_;
  CefURLRequest::Status status_ = UR_UNKNOWN;
  CefURLRequest::ErrorCode error_code_ = ERR_NONE;
  CefRefPtr<CefResponse> response_;
  bool response_was_cached_ = false;

 private:
  IMPLEMENT_REFCOUNTING(RequestClient);
};

// SHARED TEST RUNNER

// Executes the tests.
class RequestTestRunner : public base::RefCountedThreadSafe<RequestTestRunner> {
 public:
  typedef base::Callback<void(const base::Closure&)> TestCallback;

  RequestTestRunner(bool is_browser_process,
                    bool is_server_backend,
                    bool use_frame_method,
                    bool run_in_browser_process,
                    const base::Closure& incomplete_request_callback)
      : is_browser_process_(is_browser_process),
        is_server_backend_(is_server_backend),
        use_frame_method_(use_frame_method),
        run_in_browser_process_(run_in_browser_process),
        incomplete_request_callback_(incomplete_request_callback) {
    owner_task_runner_ = CefTaskRunner::GetForCurrentThread();
    EXPECT_TRUE(owner_task_runner_.get());
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());

// Helper macro for registering test callbacks.
#define REGISTER_TEST(test_mode, setup_method, run_method)                    \
  RegisterTest(test_mode, base::Bind(&RequestTestRunner::setup_method, this), \
               base::Bind(&RequestTestRunner::run_method, this));

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

  // Called in both the browser and render process to setup the test.
  void SetupTest(RequestTestMode test_mode,
                 const base::Closure& complete_callback) {
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());

    const base::Closure& safe_complete_callback = base::Bind(
        &RequestTestRunner::CompleteOnCorrectThread, this, complete_callback);

    TestMap::const_iterator it = test_map_.find(test_mode);
    if (it != test_map_.end()) {
      it->second.setup.Run(base::Bind(&RequestTestRunner::SetupContinue, this,
                                      safe_complete_callback));
    } else {
      // Unknown test.
      ADD_FAILURE();
      complete_callback.Run();
    }
  }

  // Called in either the browser or render process to run the test.
  void RunTest(RequestTestMode test_mode,
               CefRefPtr<CefFrame> frame,
               const base::Closure& complete_callback) {
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());

    frame_ = frame;

    const base::Closure& safe_complete_callback = base::Bind(
        &RequestTestRunner::CompleteOnCorrectThread, this, complete_callback);

    TestMap::const_iterator it = test_map_.find(test_mode);
    if (it != test_map_.end()) {
      it->second.run.Run(safe_complete_callback);
    } else {
      // Unknown test.
      ADD_FAILURE();
      complete_callback.Run();
    }
  }

  // Called in both the browser and render process to shut down the test.
  void ShutdownTest(const base::Closure& complete_callback) {
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());

    const base::Closure& safe_complete_callback = base::Bind(
        &RequestTestRunner::CompleteOnCorrectThread, this, complete_callback);

    if (!post_file_tmpdir_.IsEmpty()) {
      EXPECT_TRUE(is_browser_process_);
      CefPostTask(TID_FILE,
                  base::Bind(&RequestTestRunner::RunDeleteTempDirectory, this,
                             safe_complete_callback));
      return;
    }

    // Continue with test shutdown.
    RunShutdown(safe_complete_callback);
  }

 private:
  // Continued after |settings_| is populated for the test.
  void SetupContinue(const base::Closure& complete_callback) {
    if (!owner_task_runner_->BelongsToCurrentThread()) {
      owner_task_runner_->PostTask(CefCreateClosureTask(base::Bind(
          &RequestTestRunner::SetupContinue, this, complete_callback)));
      return;
    }

    if (is_browser_process_) {
      SetupTestBackend(complete_callback);
    } else {
      complete_callback.Run();
    }
  }

  std::string GetTestPath(const std::string& name) {
    return std::string(run_in_browser_process_ ? "/Browser" : "/Renderer") +
           name;
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

  void SetupGetTest(const base::Closure& complete_callback) {
    SetupGetTestShared();
    complete_callback.Run();
  }

  void SetupGetNoDataTest(const base::Closure& complete_callback) {
    // Start with the normal get test.
    SetupGetTestShared();

    // Disable download data notifications.
    settings_.request->SetFlags(UR_FLAG_NO_DOWNLOAD_DATA);

    settings_.expect_download_data = false;

    complete_callback.Run();
  }

  void SetupGetPartialContentTest(const base::Closure& complete_callback) {
    // Start with the normal get test.
    SetupGetTestShared();

    // Skip first 4 bytes of content and expect to receive the rest
    settings_.request->SetHeaderByName("Range", "bytes=4-", true);
    settings_.response->SetHeaderByName("Content-Range", "bytes 4-8/8", true);
    settings_.response->SetStatus(206);
    settings_.response->SetStatusText("Partial Content");
    settings_.expected_download_offset = 4;

    complete_callback.Run();
  }

  void SetupGetAllowCookiesTest(const base::Closure& complete_callback) {
    // Start with the normal get test.
    SetupGetTestShared();

    // Send cookies.
    settings_.request->SetFlags(UR_FLAG_ALLOW_STORED_CREDENTIALS);

    settings_.expect_save_cookie = true;
    settings_.expect_send_cookie = true;

    complete_callback.Run();
  }

  void SetupGetRedirectTest(const base::Closure& complete_callback) {
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

    complete_callback.Run();
  }

  void SetupGetRedirectStopTest(const base::Closure& complete_callback) {
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(GetTestURL("GetTest.html"));
    settings_.request->SetMethod("GET");

    // With the test server only the status is expected
    // on stop redirects.
    settings_.response = CefResponse::Create();
    settings_.response->SetStatus(302);
    if (is_browser_process_) {
      settings_.response->SetStatusText("Found");
    }

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

    complete_callback.Run();
  }

  void SetupGetRedirectLocationTest(const base::Closure& complete_callback) {
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

    complete_callback.Run();
  }

  void SetupGetReferrerTest(const base::Closure& complete_callback) {
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(GetTestURL("GetTest.html"));
    settings_.request->SetMethod("GET");

    // The referrer URL must be HTTP or HTTPS. This is enforced by
    // GURL::GetAsReferrer() called from URLRequest::SetReferrer().
    settings_.request->SetReferrer("http://tests.com/referrer.html",
                                   REFERRER_POLICY_DEFAULT);

    settings_.response = CefResponse::Create();
    settings_.response->SetMimeType("text/html");
    settings_.response->SetStatus(200);
    settings_.response->SetStatusText("OK");

    settings_.response_data = "GET TEST SUCCESS";

    complete_callback.Run();
  }

  void SetupGetAuthTest(const base::Closure& complete_callback) {
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

    complete_callback.Run();
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

  void SetupPostTest(const base::Closure& complete_callback) {
    SetupPostTestShared();
    complete_callback.Run();
  }

  void SetupPostFileTest(const base::Closure& complete_callback) {
    // This test is only supported in the browser process.
    EXPECT_TRUE(is_browser_process_);

    settings_.request = CefRequest::Create();
    settings_.request->SetURL(GetTestURL("PostFileTest.html"));
    settings_.request->SetMethod("POST");

    settings_.response = CefResponse::Create();
    settings_.response->SetMimeType("text/html");
    settings_.response->SetStatus(200);
    settings_.response->SetStatusText("OK");

    settings_.response_data = "POST TEST SUCCESS";

    CefPostTask(TID_FILE,
                base::Bind(&RequestTestRunner::SetupPostFileTestContinue, this,
                           complete_callback));
  }

  void SetupPostFileTestContinue(const base::Closure& complete_callback) {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE));

    EXPECT_TRUE(post_file_tmpdir_.CreateUniqueTempDir());
    const std::string& path =
        client::file_util::JoinPath(post_file_tmpdir_.GetPath(), "example.txt");
    const char content[] = "HELLO FRIEND!";
    int write_ct =
        client::file_util::WriteFile(path, content, sizeof(content) - 1);
    EXPECT_EQ(static_cast<int>(sizeof(content) - 1), write_ct);
    SetUploadFile(settings_.request, path);

    complete_callback.Run();
  }

  void SetupPostWithProgressTest(const base::Closure& complete_callback) {
    // Start with the normal post test.
    SetupPostTestShared();

    // Enable upload progress notifications.
    settings_.request->SetFlags(UR_FLAG_REPORT_UPLOAD_PROGRESS);

    settings_.expect_upload_progress = true;

    complete_callback.Run();
  }

  void SetupPostRedirectTest(const base::Closure& complete_callback) {
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

    complete_callback.Run();
  }

  void SetupPostRedirectToGetTest(const base::Closure& complete_callback) {
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

    complete_callback.Run();
  }

  void SetupHeadTest(const base::Closure& complete_callback) {
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

    complete_callback.Run();
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

  void SetupCacheWithControlTest(const base::Closure& complete_callback) {
    SetupCacheShared("CacheWithControlTest.html", true);

    // Send multiple requests. With the Cache-Control response header the 2nd+
    // should receive cached data.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 1;
    settings_.setup_next_request =
        base::Bind(&RequestTestRunner::SetupCacheWithControlTestNext, this);

    complete_callback.Run();
  }

  void SetupCacheWithControlTestNext(int next_send_count,
                                     const base::Closure& complete_callback) {
    // Only handle from the cache.
    settings_.expect_response_was_cached = true;

    // The following requests will use the same setup, so no more callbacks
    // are required.
    settings_.setup_next_request.Reset();

    complete_callback.Run();
  }

  void SetupCacheWithoutControlTest(const base::Closure& complete_callback) {
    SetupCacheShared("CacheWithoutControlTest.html", false);

    // Send multiple requests. Without the Cache-Control response header all
    // should be received.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 3;

    complete_callback.Run();
  }

  void SetupCacheSkipFlagTest(const base::Closure& complete_callback) {
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
        base::Bind(&RequestTestRunner::SetupCacheSkipFlagTestNext, this);

    complete_callback.Run();
  }

  void SetupCacheSkipFlagTestNext(int next_send_count,
                                  const base::Closure& complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheSkipFlagTest.html", true);

    // Expect a cached response.
    settings_.expect_response_was_cached = true;
    settings_.setup_next_request =
        base::Bind(&RequestTestRunner::SetupCacheSkipFlagTestLast, this);

    complete_callback.Run();
  }

  void SetupCacheSkipFlagTestLast(int next_send_count,
                                  const base::Closure& complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheSkipFlagTest.html", true);

    // Skip the cache despite the the Cache-Control response header.
    settings_.request->SetFlags(UR_FLAG_SKIP_CACHE);

    // Expect the cache to be skipped.
    settings_.expect_response_was_cached = false;
    settings_.setup_next_request.Reset();

    complete_callback.Run();
  }

  void SetupCacheSkipHeaderTest(const base::Closure& complete_callback) {
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
        base::Bind(&RequestTestRunner::SetupCacheSkipHeaderTestNext, this);

    complete_callback.Run();
  }

  void SetupCacheSkipHeaderTestNext(int next_send_count,
                                    const base::Closure& complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheSkipHeaderTest.html", true);

    // Expect a cached response.
    settings_.expect_response_was_cached = true;
    settings_.setup_next_request =
        base::Bind(&RequestTestRunner::SetupCacheSkipHeaderTestLast, this);

    complete_callback.Run();
  }

  void SetupCacheSkipHeaderTestLast(int next_send_count,
                                    const base::Closure& complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheSkipHeaderTest.html", true);

    // Skip the cache despite the the Cache-Control response header.
    settings_.request->SetFlags(UR_FLAG_SKIP_CACHE);

    // Expect the cache to be skipped.
    settings_.expect_response_was_cached = false;
    settings_.setup_next_request.Reset();

    complete_callback.Run();
  }

  void SetupCacheOnlyFailureFlagTest(const base::Closure& complete_callback) {
    SetupCacheShared("CacheOnlyFailureFlagTest.html", true);

    // Only handle from the cache.
    settings_.request->SetFlags(UR_FLAG_ONLY_FROM_CACHE);

    // Send multiple requests. All should fail because there's no entry in the
    // cache currently.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 0;

    // The request is expected to fail.
    settings_.SetRequestFailureExpected(ERR_CACHE_MISS);

    complete_callback.Run();
  }

  void SetupCacheOnlyFailureHeaderTest(const base::Closure& complete_callback) {
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

    complete_callback.Run();
  }

  void SetupCacheOnlySuccessFlagTest(const base::Closure& complete_callback) {
    SetupCacheShared("CacheOnlySuccessFlagTest.html", false);

    // Send multiple requests. The 1st request will be handled normally. The
    // 2nd+ requests will be configured by SetupCacheOnlySuccessFlagNext to
    // require cached data.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 1;
    settings_.setup_next_request =
        base::Bind(&RequestTestRunner::SetupCacheOnlySuccessFlagNext, this);

    complete_callback.Run();
  }

  void SetupCacheOnlySuccessFlagNext(int next_send_count,
                                     const base::Closure& complete_callback) {
    // Recreate the request object because the existing object will now be
    // read-only.
    EXPECT_TRUE(settings_.request->IsReadOnly());
    SetupCacheShared("CacheOnlySuccessFlagTest.html", false);

    // Only handle from the cache.
    settings_.request->SetFlags(UR_FLAG_ONLY_FROM_CACHE);
    settings_.expect_response_was_cached = true;

    // The following requests will use the same setup, so no more callbacks
    // are required.
    settings_.setup_next_request.Reset();

    complete_callback.Run();
  }

  void SetupCacheOnlySuccessHeaderTest(const base::Closure& complete_callback) {
    SetupCacheShared("CacheOnlySuccessHeaderTest.html", false);

    // Send multiple requests. The 1st request will be handled normally. The
    // 2nd+ requests will be configured by SetupCacheOnlySuccessHeaderNext to
    // require cached data.
    settings_.expected_send_count = 3;
    settings_.expected_receive_count = 1;
    settings_.setup_next_request =
        base::Bind(&RequestTestRunner::SetupCacheOnlySuccessHeaderNext, this);

    complete_callback.Run();
  }

  void SetupCacheOnlySuccessHeaderNext(int next_send_count,
                                       const base::Closure& complete_callback) {
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
    settings_.setup_next_request.Reset();

    complete_callback.Run();
  }

  void SetupCacheDisableFlagTest(const base::Closure& complete_callback) {
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
        base::Bind(&RequestTestRunner::SetupCacheDisableFlagTestNext, this);

    complete_callback.Run();
  }

  void SetupCacheDisableFlagTestNext(int next_send_count,
                                     const base::Closure& complete_callback) {
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
    settings_.setup_next_request.Reset();

    complete_callback.Run();
  }

  void SetupCacheDisableHeaderTest(const base::Closure& complete_callback) {
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
    settings_.setup_next_request =
        base::Bind(&RequestTestRunner::SetupCacheDisableHeaderTestNext, this);

    complete_callback.Run();
  }

  void SetupCacheDisableHeaderTestNext(int next_send_count,
                                       const base::Closure& complete_callback) {
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
    settings_.setup_next_request.Reset();

    complete_callback.Run();
  }

  void SetupIncompleteProcessRequestTest(
      const base::Closure& complete_callback) {
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

    complete_callback.Run();
  }

  void SetupIncompleteReadResponseTest(const base::Closure& complete_callback) {
    // Start with the normal get test.
    SetupGetTestShared();

    settings_.incomplete_type = RequestRunSettings::INCOMPLETE_READ_RESPONSE;

    // There will be a response but the request will be aborted without
    // receiving any data.
    settings_.response_data = kIncompleteDoNotSendData;
    settings_.expected_error_code = ERR_ABORTED;
    settings_.expected_status = UR_FAILED;
    // TODO(network): Download progress notifications are sent for incomplete
    // (with no data sent) requests in the browser process but not the renderer
    // process. Consider standardizing this behavior.
    settings_.expect_download_progress = is_browser_process_;
    settings_.expect_download_data = false;

    complete_callback.Run();
  }

  // Send a request. |complete_callback| will be executed on request completion.
  void SendRequest(
      const RequestClient::RequestCompleteCallback& complete_callback) {
    CefRefPtr<CefRequest> request;
    if (settings_.redirect_request)
      request = settings_.redirect_request;
    else
      request = settings_.request;
    EXPECT_TRUE(request.get());

    CefRefPtr<RequestClient> client = new RequestClient(complete_callback);

    // Not delegating to CefRequestHandler::GetAuthCredentials.
    if (!use_frame_method_ && settings_.expect_authentication) {
      client->has_authentication_ = true;
      client->username_ = settings_.username;
      client->password_ = settings_.password;
    }

    if (use_frame_method_) {
      EXPECT_TRUE(frame_);
      frame_->CreateURLRequest(request, client.get());
    } else {
      CefURLRequest::Create(request, client.get(), request_context_);
    }

    if (settings_.incomplete_type != RequestRunSettings::INCOMPLETE_NONE) {
      incomplete_request_callback_.Run();
      incomplete_request_callback_.Reset();
    }
  }

  // Verify a response.
  void VerifyResponse(CefRefPtr<RequestClient> client) {
    CefRefPtr<CefRequest> expected_request;
    CefRefPtr<CefResponse> expected_response;

    if (settings_.redirect_request)
      expected_request = settings_.redirect_request;
    else
      expected_request = settings_.request;

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
    if (expected_response && client->response_)
      TestResponseEqual(expected_response, client->response_, true);

    EXPECT_EQ(settings_.expect_response_was_cached,
              client->response_was_cached_);

    EXPECT_EQ(1, client->request_complete_ct_);

    if (settings_.expect_upload_progress) {
      EXPECT_LE(1, client->upload_progress_ct_);

      std::string upload_data;
      GetUploadData(expected_request, upload_data);
      EXPECT_EQ((int64)upload_data.size(), client->upload_total_);
    } else {
      EXPECT_EQ(0, client->upload_progress_ct_);
      EXPECT_EQ(0, client->upload_total_);
    }

    if (settings_.expect_download_progress) {
      EXPECT_LE(1, client->download_progress_ct_);
      EXPECT_EQ((int64)(settings_.response_data.size() -
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
  void SingleRunTest(const base::Closure& complete_callback) {
    SendRequest(base::Bind(&RequestTestRunner::SingleRunTestComplete, this,
                           complete_callback));
  }

  void SingleRunTestComplete(const base::Closure& complete_callback,
                             CefRefPtr<RequestClient> completed_client) {
    VerifyResponse(completed_client);
    complete_callback.Run();
  }

  // Run a test with multiple requests.
  void MultipleRunTest(const base::Closure& complete_callback) {
    EXPECT_GT(settings_.expected_send_count, 0);
    EXPECT_GE(settings_.expected_receive_count, 0);
    MultipleRunTestContinue(complete_callback, 1);
  }

  void MultipleRunTestContinue(const base::Closure& complete_callback,
                               int send_count) {
    // Send the next request.
    SendRequest(base::Bind(&RequestTestRunner::MultipleRunTestNext, this,
                           complete_callback, send_count));
  }

  void MultipleRunTestNext(const base::Closure& complete_callback,
                           int send_count,
                           CefRefPtr<RequestClient> completed_client) {
    // Verify the completed request.
    VerifyResponse(completed_client);

    if (send_count == settings_.expected_send_count) {
      // All requests complete.
      complete_callback.Run();
      return;
    }

    const int next_send_count = send_count + 1;
    const base::Closure& continue_callback =
        base::Bind(&RequestTestRunner::MultipleRunTestContinue, this,
                   complete_callback, next_send_count);

    if (!settings_.setup_next_request.is_null()) {
      // Provide an opportunity to modify expectations before the next request.
      // Copy the callback object in case |settings_.setup_next_request| is
      // modified as a result of the call.
      RequestRunSettings::NextRequestCallback next_callback =
          settings_.setup_next_request;
      next_callback.Run(next_send_count, continue_callback);
    } else {
      continue_callback.Run();
    }
  }

  // Register a test. Called in the constructor.
  void RegisterTest(RequestTestMode test_mode,
                    TestCallback setup,
                    TestCallback run) {
    TestEntry entry = {setup, run};
    test_map_.insert(std::make_pair(test_mode, entry));
  }

  void CompleteOnCorrectThread(const base::Closure& complete_callback) {
    if (!owner_task_runner_->BelongsToCurrentThread()) {
      owner_task_runner_->PostTask(CefCreateClosureTask(
          base::Bind(&RequestTestRunner::CompleteOnCorrectThread, this,
                     complete_callback)));
      return;
    }

    complete_callback.Run();
  }

  void RunDeleteTempDirectory(const base::Closure& complete_callback) {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE));

    EXPECT_TRUE(post_file_tmpdir_.Delete());
    EXPECT_TRUE(post_file_tmpdir_.IsEmpty());

    // Continue with test shutdown.
    RunShutdown(complete_callback);
  }

  void RunShutdown(const base::Closure& complete_callback) {
    if (!owner_task_runner_->BelongsToCurrentThread()) {
      owner_task_runner_->PostTask(CefCreateClosureTask(base::Bind(
          &RequestTestRunner::RunShutdown, this, complete_callback)));
      return;
    }

    if (is_browser_process_) {
      ShutdownTestBackend(complete_callback);
    } else {
      complete_callback.Run();
    }
  }

  // Create the backend for the current test. Called during test setup.
  void SetupTestBackend(const base::Closure& complete_callback) {
    // Backends are only created in the browser process.
    EXPECT_TRUE(is_browser_process_);

    EXPECT_TRUE(settings_.request.get());
    EXPECT_TRUE(settings_.response.get() ||
                settings_.expected_status == UR_FAILED);

    if (is_server_backend_)
      StartServer(complete_callback);
    else
      AddSchemeHandler(complete_callback);
  }

  void StartServer(const base::Closure& complete_callback) {
    EXPECT_FALSE(server_handler_);

    server_handler_ = new RequestServerHandler();

    server_handler_->AddSchemeHandler(&settings_);
    if (settings_.expected_receive_count >= 0) {
      server_handler_->SetExpectedRequestCount(
          settings_.expected_receive_count);
    }

    server_handler_->CreateServer(complete_callback);
  }

  void AddSchemeHandler(const base::Closure& complete_callback) {
    EXPECT_FALSE(scheme_factory_);

    // Add the factory registration.
    scheme_factory_ = new RequestSchemeHandlerFactory();
    request_context_->RegisterSchemeHandlerFactory(GetRequestScheme(false),
                                                   GetRequestHost(false, false),
                                                   scheme_factory_.get());

    scheme_factory_->AddSchemeHandler(&settings_);

    // Any further calls will come from the IO thread.
    scheme_factory_->SetOwnerTaskRunner(CefTaskRunner::GetForThread(TID_IO));

    complete_callback.Run();
  }

  // Shutdown the backend for the current test. Called during test shutdown.
  void ShutdownTestBackend(const base::Closure& complete_callback) {
    // Backends are only created in the browser process.
    EXPECT_TRUE(is_browser_process_);
    if (is_server_backend_)
      ShutdownServer(complete_callback);
    else
      RemoveSchemeHandler(complete_callback);
  }

  void ShutdownServer(const base::Closure& complete_callback) {
    EXPECT_TRUE(server_handler_);

    server_handler_->ShutdownServer(complete_callback);
    server_handler_ = nullptr;
  }

  void RemoveSchemeHandler(const base::Closure& complete_callback) {
    EXPECT_TRUE(scheme_factory_);

    // Remove the factory registration.
    request_context_->RegisterSchemeHandlerFactory(
        GetRequestScheme(false), GetRequestHost(false, false), nullptr);
    scheme_factory_->Shutdown(complete_callback);
    scheme_factory_ = nullptr;
  }

  const bool is_browser_process_;
  const bool is_server_backend_;
  const bool use_frame_method_;
  const bool run_in_browser_process_;

  // Used with incomplete request tests.
  base::Closure incomplete_request_callback_;

  // Primary thread runner for the object that owns us. In the browser process
  // this will be the UI thread and in the renderer process this will be the
  // RENDERER thread.
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
  CefRefPtr<RequestServerHandler> server_handler_;

  // Scheme handler backend.
  std::string scheme_name_;
  CefRefPtr<RequestSchemeHandlerFactory> scheme_factory_;

  CefScopedTempDir post_file_tmpdir_;

 public:
  RequestRunSettings settings_;
};

// RENDERER-SIDE TEST HARNESS

class RequestRendererTest : public ClientAppRenderer::Delegate {
 public:
  RequestRendererTest() {}

  bool OnProcessMessageReceived(CefRefPtr<ClientAppRenderer> app,
                                CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    if (message->GetName() == kRequestTestMsg) {
      EXPECT_TRUE(CefCurrentlyOn(TID_RENDERER));
      EXPECT_TRUE(frame->IsMain());

      app_ = app;
      browser_ = browser;
      frame_ = nullptr;

      CefRefPtr<CefListValue> args = message->GetArgumentList();

      const bool use_frame_method = args->GetBool(2);
      if (use_frame_method)
        frame_ = frame;

      test_mode_ = static_cast<RequestTestMode>(args->GetInt(0));
      test_runner_ = new RequestTestRunner(
          false, args->GetBool(1), use_frame_method, false,
          base::Bind(&RequestRendererTest::OnIncompleteRequest, this));

      // Setup the test. This will create the objects that we test against but
      // not register any backend (because we're in the render process).
      test_runner_->SetupTest(
          test_mode_, base::Bind(&RequestRendererTest::OnSetupComplete, this));

      return true;
    }

    // Message not handled.
    return false;
  }

 private:
  void OnSetupComplete() {
    EXPECT_TRUE(CefCurrentlyOn(TID_RENDERER));

    // Run the test.
    test_runner_->RunTest(
        test_mode_, frame_,
        base::Bind(&RequestRendererTest::OnRunComplete, this));
  }

  void OnRunComplete() {
    EXPECT_TRUE(CefCurrentlyOn(TID_RENDERER));

    // Shutdown the test.
    test_runner_->ShutdownTest(
        base::Bind(&RequestRendererTest::OnShutdownComplete, this));
  }

  void OnIncompleteRequest() {
    EXPECT_TRUE(CefCurrentlyOn(TID_RENDERER));

    // This method will only be called for incomplete requests.
    EXPECT_NE(test_runner_->settings_.incomplete_type,
              RequestRunSettings::INCOMPLETE_NONE);

    // Check if the test has failed.
    bool result = !TestFailed();

    // The browser will be closed to abort in-progress requests.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kIncompleteRequestTestMsg);
    EXPECT_TRUE(return_msg->GetArgumentList()->SetBool(0, result));
    browser_->GetMainFrame()->SendProcessMessage(PID_BROWSER, return_msg);
  }

  void OnShutdownComplete() {
    EXPECT_TRUE(CefCurrentlyOn(TID_RENDERER));

    if (test_runner_->settings_.incomplete_type !=
        RequestRunSettings::INCOMPLETE_NONE) {
      // For incomplete tests there's a race between process destruction due to
      // the browser closing, and the test possibly completing due to request
      // cancellation. We therefore ignore test completion in this case.
      return;
    }

    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kRequestTestMsg);
    EXPECT_TRUE(return_msg->GetArgumentList()->SetBool(0, result));
    browser_->GetMainFrame()->SendProcessMessage(PID_BROWSER, return_msg);

    app_ = nullptr;
    browser_ = nullptr;
  }

  CefRefPtr<ClientAppRenderer> app_;
  CefRefPtr<CefBrowser> browser_;
  CefRefPtr<CefFrame> frame_;
  RequestTestMode test_mode_;

  scoped_refptr<RequestTestRunner> test_runner_;

  IMPLEMENT_REFCOUNTING(RequestRendererTest);
};

// BROWSER-SIDE TEST HARNESS

class RequestTestHandler : public TestHandler {
 public:
  RequestTestHandler(RequestTestMode test_mode,
                     ContextTestMode context_mode,
                     bool test_in_browser,
                     bool test_server_backend,
                     bool test_frame_method)
      : test_mode_(test_mode),
        context_mode_(context_mode),
        test_in_browser_(test_in_browser),
        test_server_backend_(test_server_backend),
        test_frame_method_(test_frame_method),
        // Must use the request origin to avoid failures in
        // CorsURLLoaderFactory::IsSane for requests originating from the
        // renderer process.
        test_url_(GetRequestOrigin(test_server_backend) +
                  "/URLRequestTest.Test") {}

  void RunTest() override {
    // Time out the test after a reasonable period of time.
    SetTestTimeout();

    // Start pre-setup actions.
    PreSetupStart();
  }

  void PreSetupStart() {
    CefPostTask(TID_FILE,
                base::Bind(&RequestTestHandler::PreSetupFileTasks, this));
  }

  void PreSetupFileTasks() {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE));

    if (context_mode_ == CONTEXT_ONDISK) {
      EXPECT_TRUE(context_tmpdir_.CreateUniqueTempDir());
      context_tmpdir_path_ = context_tmpdir_.GetPath();
      EXPECT_FALSE(context_tmpdir_path_.empty());
    }

    CefPostTask(TID_UI,
                base::Bind(&RequestTestHandler::PreSetupContinue, this));
  }

  void PreSetupContinue() {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));

    test_runner_ = new RequestTestRunner(
        true, test_server_backend_, test_frame_method_, test_in_browser_,
        base::Bind(&RequestTestHandler::OnIncompleteRequest, this));

    // Get or create the request context.
    if (context_mode_ == CONTEXT_GLOBAL) {
      CefRefPtr<CefRequestContext> request_context =
          CefRequestContext::GetGlobalContext();
      EXPECT_TRUE(request_context.get());
      test_runner_->SetRequestContext(request_context);

      PreSetupComplete();
    } else {
      // Don't end the test until the temporary request context has been
      // destroyed.
      SetSignalCompletionWhenAllBrowsersClose(false);

      CefRequestContextSettings settings;

      if (context_mode_ == CONTEXT_ONDISK) {
        EXPECT_FALSE(context_tmpdir_.IsEmpty());
        CefString(&settings.cache_path) = context_tmpdir_path_;
      }

      // Create a new temporary request context.
      CefRefPtr<CefRequestContext> request_context =
          CefRequestContext::CreateContext(settings,
                                           new RequestContextHandler(this));
      EXPECT_TRUE(request_context.get());
      test_runner_->SetRequestContext(request_context);

      if (!test_server_backend_) {
        // Set the schemes that are allowed to store cookies.
        std::vector<CefString> supported_schemes;
        supported_schemes.push_back(GetRequestScheme(false));

        // Continue the test once supported schemes has been set.
        request_context->GetCookieManager(nullptr)->SetSupportedSchemes(
            supported_schemes, true,
            new TestCompletionCallback(
                base::Bind(&RequestTestHandler::PreSetupComplete, this)));
      } else {
        PreSetupComplete();
      }
    }
  }

  void PreSetupComplete() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::Bind(&RequestTestHandler::PreSetupComplete, this));
      return;
    }

    // Setup the test. This will create the objects that we test against and
    // register the backend.
    test_runner_->SetupTest(
        test_mode_, base::Bind(&RequestTestHandler::OnSetupComplete, this));
  }

  // Browser process setup is complete.
  void OnSetupComplete() {
    // Start post-setup actions.
    SetTestCookie(test_runner_->GetRequestContext(), test_server_backend_,
                  base::Bind(&RequestTestHandler::PostSetupComplete, this));
  }

  void PostSetupComplete() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::Bind(&RequestTestHandler::PostSetupComplete, this));
      return;
    }

    if (test_in_browser_) {
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
            base::Bind(&RequestTestHandler::OnRunComplete, this));
      }
    } else {
      AddResource(test_url_, "<html><body>TEST</body></html>", "text/html");

      // Create a browser to run the test in the renderer process.
      CreateBrowser(test_url_, test_runner_->GetRequestContext());
    }
  }

  ReturnValue OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefRequestCallback> callback) override {
    if (test_running_ && test_frame_method_) {
      EXPECT_TRUE(frame);
      EXPECT_EQ(test_frame_->GetIdentifier(), frame->GetIdentifier());
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
    EXPECT_TRUE(test_in_browser_);
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
    if (test_in_browser_ && test_frame_method_) {
      // Run the test now.
      test_frame_ = frame;
      test_running_ = true;
      test_runner_->RunTest(
          test_mode_, frame,
          base::Bind(&RequestTestHandler::OnRunComplete, this));
      return;
    }

    EXPECT_FALSE(test_in_browser_);
    if (frame->IsMain()) {
      CefRefPtr<CefProcessMessage> test_message =
          CefProcessMessage::Create(kRequestTestMsg);
      CefRefPtr<CefListValue> args = test_message->GetArgumentList();
      EXPECT_TRUE(args->SetInt(0, test_mode_));
      EXPECT_TRUE(args->SetBool(1, test_server_backend_));
      EXPECT_TRUE(args->SetBool(2, test_frame_method_));

      if (test_frame_method_)
        test_frame_ = frame;
      test_running_ = true;

      // Send a message to the renderer process to run the test.
      frame->SendProcessMessage(PID_RENDERER, test_message);
    }
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    EXPECT_TRUE(browser.get());
    EXPECT_TRUE(frame.get());
    EXPECT_EQ(PID_RENDERER, source_process);
    EXPECT_TRUE(message.get());
    EXPECT_TRUE(message->IsReadOnly());
    EXPECT_FALSE(test_in_browser_);

    EXPECT_FALSE(got_message_);
    got_message_.yes();

    if (message->GetArgumentList()->GetBool(0))
      got_success_.yes();

    const std::string& message_name = message->GetName();
    if (message_name == kRequestTestMsg) {
      // Renderer process test is complete.
      OnRunComplete();
    } else if (message_name == kIncompleteRequestTestMsg) {
      // Incomplete renderer tests will not complete normally. Instead, trigger
      // browser close and then signal completion from OnBeforeClose.
      OnIncompleteRequest();
    }

    return true;
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    if (!test_in_browser_ && test_runner_->settings_.incomplete_type !=
                                 RequestRunSettings::INCOMPLETE_NONE) {
      // Incomplete tests running in the renderer process will never recieve the
      // test complete process message, so call the method here.
      OnRunComplete();
    }

    TestHandler::OnBeforeClose(browser);
  }

  // Incomplete tests will not complete normally. Instead, we trigger a browser
  // close to abort in-progress requests.
  void OnIncompleteRequest() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::Bind(&RequestTestHandler::OnIncompleteRequest, this));
      return;
    }

    EXPECT_TRUE(test_frame_method_);
    EXPECT_NE(RequestRunSettings::INCOMPLETE_NONE,
              test_runner_->settings_.incomplete_type);

    // TestComplete will eventually be called from DestroyTest instead of being
    // triggered by browser destruction.
    SetSignalCompletionWhenAllBrowsersClose(false);
    CefPostDelayedTask(
        TID_UI, base::Bind(&TestHandler::CloseBrowser, GetBrowser(), false),
        1000);
  }

  // Test run is complete. It ran in either the browser or render process.
  void OnRunComplete() {
    GetTestCookie(test_runner_->GetRequestContext(), test_server_backend_,
                  base::Bind(&RequestTestHandler::PostRunComplete, this));
  }

  void PostRunComplete(bool has_save_cookie) {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI, base::Bind(&RequestTestHandler::PostRunComplete, this,
                                     has_save_cookie));
      return;
    }

    EXPECT_EQ(test_runner_->settings_.expect_save_cookie, has_save_cookie);

    // Shut down the browser side of the test.
    test_runner_->ShutdownTest(
        base::Bind(&RequestTestHandler::MaybeClearAuthCredentials, this));
  }

  void MaybeClearAuthCredentials() {
    if (test_runner_->settings_.expect_authentication &&
        context_mode_ == CONTEXT_GLOBAL) {
      // Clear the HTTP authentication cache to avoid leaking state between
      // test runs when using the global request context.
      test_runner_->GetRequestContext()->ClearHttpAuthCredentials(
          new TestCompletionCallback(
              base::Bind(&RequestTestHandler::DestroyTest, this)));
      return;
    }

    DestroyTest();
  }

  void DestroyTest() override {
    if (!test_in_browser_) {
      EXPECT_TRUE(got_message_);
      EXPECT_TRUE(got_success_);
    }

    if (test_frame_method_) {
      // Expect at least 1 call to OnBeforeResourceLoad for every test.
      // Redirect tests may get multiple calls.
      EXPECT_LE(1, test_frame_resource_load_ct_);
    }

    // CefRequestHandler::GetAuthCredentials should be called after
    // CefURLRequestClient::GetAuthCredentials when the request has an
    // associated frame.
    if (test_in_browser_ && test_frame_method_ &&
        test_runner_->settings_.expect_authentication) {
      EXPECT_EQ(1, auth_credentials_ct_);
    } else {
      EXPECT_EQ(0, auth_credentials_ct_);
    }

    TestHandler::DestroyTest();

    // For non-global contexts OnTestComplete() will be called when the
    // RequestContextHandler is destroyed.
    bool call_test_complete = false;
    if (context_mode_ == CONTEXT_GLOBAL) {
      if (test_in_browser_ && !test_frame_method_) {
        // These tests don't create a browser that would signal implicitly.
        call_test_complete = true;
      } else if (!SignalCompletionWhenAllBrowsersClose()) {
        // These tests close the browser to terminate in-progress requests
        // before test completion.
        call_test_complete = true;
      }
    }

    // Release references to the context and handler.
    test_runner_->Destroy();

    if (call_test_complete)
      OnTestComplete();
  }

  void OnTestComplete() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::Bind(&RequestTestHandler::OnTestComplete, this));
      return;
    }

    EXPECT_FALSE(got_on_test_complete_);
    got_on_test_complete_.yes();

    if (!context_tmpdir_.IsEmpty()) {
      // Wait a bit for cache file handles to close after browser or request
      // context destruction.
      CefPostDelayedTask(
          TID_FILE,
          base::Bind(&RequestTestHandler::PostTestCompleteFileTasks, this),
          100);
    } else {
      TestComplete();
    }
  }

  void PostTestCompleteFileTasks() {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE));

    EXPECT_TRUE(context_tmpdir_.Delete());
    EXPECT_TRUE(context_tmpdir_.IsEmpty());

    CefPostTask(TID_UI, base::Bind(&RequestTestHandler::TestComplete, this));
  }

 private:
  // Used with temporary request contexts to signal test completion once the
  // temporary context has been destroyed.
  class RequestContextHandler : public CefRequestContextHandler {
   public:
    explicit RequestContextHandler(CefRefPtr<RequestTestHandler> test_handler)
        : test_handler_(test_handler) {}
    ~RequestContextHandler() override { test_handler_->OnTestComplete(); }

   private:
    CefRefPtr<RequestTestHandler> test_handler_;

    IMPLEMENT_REFCOUNTING(RequestContextHandler);
  };

  const RequestTestMode test_mode_;
  const ContextTestMode context_mode_;
  const bool test_in_browser_;
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
  // Only used when the test runs in the render process.
  TrackCallback got_message_;
  TrackCallback got_success_;

  int auth_credentials_ct_ = 0;
  TrackCallback got_on_test_complete_;

  IMPLEMENT_REFCOUNTING(RequestTestHandler);
};

bool IsTestSupported(RequestTestMode test_mode,
                     ContextTestMode context_mode,
                     bool test_in_browser,
                     bool test_server_backend,
                     bool test_frame_method) {
  if (!test_in_browser && !test_server_backend && !test_frame_method) {
    // Requests from the render process can only reach non-server backends when
    // using the CefFrame::CreateURLRequest method.
    return false;
  }

  return true;
}

}  // namespace

// Entry point for creating URLRequest renderer test objects.
// Called from client_app_delegates.cc.
void CreateURLRequestRendererTests(ClientAppRenderer::DelegateSet& delegates) {
  delegates.insert(new RequestRendererTest);
}

// Entry point for registering custom schemes.
// Called from client_app_delegates.cc.
void RegisterURLRequestCustomSchemes(
    CefRawPtr<CefSchemeRegistrar> registrar,
    std::vector<CefString>& cookiable_schemes) {
  const std::string& scheme = GetRequestScheme(false);
  registrar->AddCustomScheme(
      scheme, CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_CORS_ENABLED);
  cookiable_schemes.push_back(scheme);
}

// Helpers for defining URLRequest tests.
#define REQ_TEST_EX(name, test_mode, context_mode, test_in_browser,      \
                    test_server_backend, test_frame_method)              \
  TEST(URLRequestTest, name) {                                           \
    if (!IsTestSupported(test_mode, context_mode, test_in_browser,       \
                         test_server_backend, test_frame_method)) {      \
      return;                                                            \
    }                                                                    \
    CefRefPtr<RequestTestHandler> handler =                              \
        new RequestTestHandler(test_mode, context_mode, test_in_browser, \
                               test_server_backend, test_frame_method);  \
    handler->ExecuteTest();                                              \
    ReleaseAndWaitForDestructor(handler);                                \
  }

#define REQ_TEST(name, test_mode, context_mode, test_in_browser, \
                 test_server_backend, test_frame_method)         \
  REQ_TEST_EX(name, test_mode, context_mode, test_in_browser,    \
              test_server_backend, test_frame_method)

// Define the tests.
#define REQ_TEST_SET_EX(suffix, context_mode, test_server_backend,             \
                        test_frame_method)                                     \
  REQ_TEST(BrowserGET##suffix, REQTEST_GET, context_mode, true,                \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(BrowserGETNoData##suffix, REQTEST_GET_NODATA, context_mode, true,   \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(BrowserGETPartialContent##suffix, REQTEST_GET_PARTIAL_CONTENT,      \
           context_mode, true, test_server_backend, test_frame_method)         \
  REQ_TEST(BrowserGETAllowCookies##suffix, REQTEST_GET_ALLOWCOOKIES,           \
           context_mode, true, test_server_backend, test_frame_method)         \
  REQ_TEST(BrowserGETRedirect##suffix, REQTEST_GET_REDIRECT, context_mode,     \
           true, test_server_backend, test_frame_method)                       \
  REQ_TEST(BrowserGETRedirectStop##suffix, REQTEST_GET_REDIRECT_STOP,          \
           context_mode, true, test_server_backend, test_frame_method)         \
  REQ_TEST(BrowserGETRedirectLocation##suffix, REQTEST_GET_REDIRECT_LOCATION,  \
           context_mode, true, test_server_backend, test_frame_method)         \
  REQ_TEST(BrowserGETReferrer##suffix, REQTEST_GET_REFERRER, context_mode,     \
           true, test_server_backend, test_frame_method)                       \
  REQ_TEST(BrowserPOST##suffix, REQTEST_POST, context_mode, true,              \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(BrowserPOSTFile##suffix, REQTEST_POST_FILE, context_mode, true,     \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(BrowserPOSTWithProgress##suffix, REQTEST_POST_WITHPROGRESS,         \
           context_mode, true, test_server_backend, test_frame_method)         \
  REQ_TEST(BrowserPOSTRedirect##suffix, REQTEST_POST_REDIRECT, context_mode,   \
           true, test_server_backend, test_frame_method)                       \
  REQ_TEST(BrowserPOSTRedirectToGET##suffix, REQTEST_POST_REDIRECT_TOGET,      \
           context_mode, true, test_server_backend, test_frame_method)         \
  REQ_TEST(BrowserHEAD##suffix, REQTEST_HEAD, context_mode, true,              \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(RendererGET##suffix, REQTEST_GET, context_mode, false,              \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(RendererGETNoData##suffix, REQTEST_GET_NODATA, context_mode, false, \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(RendererGETAllowCookies##suffix, REQTEST_GET_ALLOWCOOKIES,          \
           context_mode, false, test_server_backend, test_frame_method)        \
  REQ_TEST(RendererGETRedirect##suffix, REQTEST_GET_REDIRECT, context_mode,    \
           false, test_server_backend, test_frame_method)                      \
  REQ_TEST(RendererGETRedirectStop##suffix, REQTEST_GET_REDIRECT_STOP,         \
           context_mode, false, test_server_backend, test_frame_method)        \
  REQ_TEST(RendererGETRedirectLocation##suffix, REQTEST_GET_REDIRECT_LOCATION, \
           context_mode, false, test_server_backend, test_frame_method)        \
  REQ_TEST(RendererGETReferrer##suffix, REQTEST_GET_REFERRER, context_mode,    \
           false, test_server_backend, test_frame_method)                      \
  REQ_TEST(RendererPOST##suffix, REQTEST_POST, context_mode, false,            \
           test_server_backend, test_frame_method)                             \
  REQ_TEST(RendererPOSTWithProgress##suffix, REQTEST_POST_WITHPROGRESS,        \
           context_mode, false, test_server_backend, test_frame_method)        \
  REQ_TEST(RendererPOSTRedirect##suffix, REQTEST_POST_REDIRECT, context_mode,  \
           false, test_server_backend, test_frame_method)                      \
  REQ_TEST(RendererPOSTRedirectToGET##suffix, REQTEST_POST_REDIRECT_TOGET,     \
           context_mode, false, test_server_backend, test_frame_method)        \
  REQ_TEST(RendererHEAD##suffix, REQTEST_HEAD, context_mode, false,            \
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
           REQTEST_INCOMPLETE_PROCESS_REQUEST, context_mode, true,       \
           test_server_backend, true)                                    \
  REQ_TEST(BrowserIncompleteReadResponse##suffix,                        \
           REQTEST_INCOMPLETE_READ_RESPONSE, context_mode, true,         \
           test_server_backend, true)                                    \
  REQ_TEST(RendererIncompleteProcessRequest##suffix,                     \
           REQTEST_INCOMPLETE_PROCESS_REQUEST, context_mode, false,      \
           test_server_backend, true)                                    \
  REQ_TEST(RendererIncompleteReadResponse##suffix,                       \
           REQTEST_INCOMPLETE_READ_RESPONSE, context_mode, false,        \
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
           context_mode, true, true, test_frame_method)                        \
  REQ_TEST(BrowserGETCacheWithoutControl##suffix,                              \
           REQTEST_CACHE_WITHOUT_CONTROL, context_mode, true, true,            \
           test_frame_method)                                                  \
  REQ_TEST(BrowserGETCacheSkipFlag##suffix, REQTEST_CACHE_SKIP_FLAG,           \
           context_mode, true, true, test_frame_method)                        \
  REQ_TEST(BrowserGETCacheSkipHeader##suffix, REQTEST_CACHE_SKIP_HEADER,       \
           context_mode, true, true, test_frame_method)                        \
  REQ_TEST(BrowserGETCacheOnlyFailureFlag##suffix,                             \
           REQTEST_CACHE_ONLY_FAILURE_FLAG, context_mode, true, true,          \
           test_frame_method)                                                  \
  REQ_TEST(BrowserGETCacheOnlyFailureHeader##suffix,                           \
           REQTEST_CACHE_ONLY_FAILURE_HEADER, context_mode, true, true,        \
           test_frame_method)                                                  \
  REQ_TEST(BrowserGETCacheOnlySuccessFlag##suffix,                             \
           REQTEST_CACHE_ONLY_SUCCESS_FLAG, context_mode, true, true,          \
           test_frame_method)                                                  \
  REQ_TEST(BrowserGETCacheOnlySuccessHeader##suffix,                           \
           REQTEST_CACHE_ONLY_SUCCESS_HEADER, context_mode, true, true,        \
           test_frame_method)                                                  \
  REQ_TEST(BrowserGETCacheDisableFlag##suffix, REQTEST_CACHE_DISABLE_FLAG,     \
           context_mode, true, true, test_frame_method)                        \
  REQ_TEST(BrowserGETCacheDisableHeader##suffix, REQTEST_CACHE_DISABLE_HEADER, \
           context_mode, true, true, test_frame_method)                        \
  REQ_TEST(RendererGETCacheWithControl##suffix, REQTEST_CACHE_WITH_CONTROL,    \
           context_mode, false, true, test_frame_method)                       \
  REQ_TEST(RendererGETCacheWithoutControl##suffix,                             \
           REQTEST_CACHE_WITHOUT_CONTROL, context_mode, false, true,           \
           test_frame_method)                                                  \
  REQ_TEST(BrowserGETAuth##suffix, REQTEST_GET_AUTH, context_mode, true, true, \
           test_frame_method)                                                  \
  REQ_TEST(RendererGETCacheSkipFlag##suffix, REQTEST_CACHE_SKIP_FLAG,          \
           context_mode, false, true, test_frame_method)                       \
  REQ_TEST(RendererGETCacheSkipHeader##suffix, REQTEST_CACHE_SKIP_HEADER,      \
           context_mode, false, true, test_frame_method)                       \
  REQ_TEST(RendererGETCacheOnlyFailureFlag##suffix,                            \
           REQTEST_CACHE_ONLY_FAILURE_FLAG, context_mode, false, true,         \
           test_frame_method)                                                  \
  REQ_TEST(RendererGETCacheOnlyFailureHeader##suffix,                          \
           REQTEST_CACHE_ONLY_FAILURE_HEADER, context_mode, false, true,       \
           test_frame_method)                                                  \
  REQ_TEST(RendererGETCacheOnlySuccessFlag##suffix,                            \
           REQTEST_CACHE_ONLY_SUCCESS_FLAG, context_mode, false, true,         \
           test_frame_method)                                                  \
  REQ_TEST(RendererGETCacheOnlySuccessHeader##suffix,                          \
           REQTEST_CACHE_ONLY_SUCCESS_HEADER, context_mode, false, true,       \
           test_frame_method)                                                  \
  REQ_TEST(RendererGETCacheDisableFlag##suffix, REQTEST_CACHE_DISABLE_FLAG,    \
           context_mode, false, true, test_frame_method)                       \
  REQ_TEST(RendererGETCacheDisableHeader##suffix,                              \
           REQTEST_CACHE_DISABLE_HEADER, context_mode, false, true,            \
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
    CefPostTask(TID_UI, base::Bind(&InvalidURLTestClient::RunOnUIThread, this));

    // Wait for the test to complete.
    event_->Wait();
  }

  void OnRequestComplete(CefRefPtr<CefURLRequest> client) override {
    EXPECT_EQ(UR_FAILED, client->GetRequestStatus());
    EXPECT_EQ(ERR_UNKNOWN_URL_SCHEME, client->GetRequestError());

    // Let the call stack unwind before signaling completion.
    CefPostTask(TID_UI,
                base::Bind(&InvalidURLTestClient::CompleteOnUIThread, this));
  }

  void OnUploadProgress(CefRefPtr<CefURLRequest> request,
                        int64 current,
                        int64 total) override {
    EXPECT_TRUE(false);  // Not reached.
  }

  void OnDownloadProgress(CefRefPtr<CefURLRequest> request,
                          int64 current,
                          int64 total) override {
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
