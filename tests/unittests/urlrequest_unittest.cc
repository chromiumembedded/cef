// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <map>
#include <sstream>

// Include this first to avoid type conflicts with CEF headers.
#include "tests/unittests/chromium_includes.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"

#include "include/base/cef_bind.h"
#include "include/cef_scheme.h"
#include "include/cef_task.h"
#include "include/cef_urlrequest.h"
#include "include/wrapper/cef_closure_task.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tests/cefclient/client_app.h"
#include "tests/unittests/test_handler.h"
#include "tests/unittests/test_util.h"

using client::ClientApp;

// How to add a new test:
// 1. Add a new value to the RequestTestMode enumeration.
// 2. Add methods to set up and run the test in RequestTestRunner.
// 3. Add a line for the test in the RequestTestRunner constructor.
// 4. Add lines for the test in the "Define the tests" section at the bottom of
//    the file.

namespace {

// Unique values for URLRequest tests.
const char* kRequestTestUrl = "http://tests/URLRequestTest.Test";
const char* kRequestTestMsg = "URLRequestTest.Test";
const char* kRequestScheme = "urcustom";
const char* kRequestHost = "test";
const char* kRequestOrigin = "urcustom://test";
const char* kRequestSendCookieName = "urcookie_send";
const char* kRequestSaveCookieName = "urcookie_save";

enum RequestTestMode {
  REQTEST_GET = 0,
  REQTEST_GET_NODATA,
  REQTEST_GET_ALLOWCOOKIES,
  REQTEST_GET_REDIRECT,
  REQTEST_POST,
  REQTEST_POST_FILE,
  REQTEST_POST_WITHPROGRESS,
  REQTEST_HEAD,
};

struct RequestRunSettings {
  RequestRunSettings()
    : expect_upload_progress(false),
      expect_download_progress(true),
      expect_download_data(true),
      expected_status(UR_SUCCESS),
      expected_error_code(ERR_NONE),
      expect_send_cookie(false),
      expect_save_cookie(false),
      expect_follow_redirect(true) {
  }

  // Request that will be sent.
  CefRefPtr<CefRequest> request;

  // Response that will be returned by the scheme handler.
  CefRefPtr<CefResponse> response;

  // Optional response data that will be returned by the scheme handler.
  std::string response_data;

  // If true upload progress notification will be expected.
  bool expect_upload_progress;

  // If true download progress notification will be expected.
  bool expect_download_progress;

  // If true download data will be expected.
  bool expect_download_data;

  // Expected status value.
  CefURLRequest::Status expected_status;

  // Expected error code value.
  CefURLRequest::ErrorCode expected_error_code;

  // If true the request cookie should be sent to the server.
  bool expect_send_cookie;

  // If true the response cookie should be save.
  bool expect_save_cookie;

  // If specified the test will begin with this redirect request and response.
  CefRefPtr<CefRequest> redirect_request;
  CefRefPtr<CefResponse> redirect_response;

  // If true the redirect is expected to be followed.
  bool expect_follow_redirect;
};

void SetUploadData(CefRefPtr<CefRequest> request,
                   const std::string& data) {
  CefRefPtr<CefPostData> postData = CefPostData::Create();
  CefRefPtr<CefPostDataElement> element = CefPostDataElement::Create();
  element->SetToBytes(data.size(), data.c_str());
  postData->AddElement(element);
  request->SetPostData(postData);
}

void SetUploadFile(CefRefPtr<CefRequest> request,
                   const base::FilePath& file) {
  CefRefPtr<CefPostData> postData = CefPostData::Create();
  CefRefPtr<CefPostDataElement> element = CefPostDataElement::Create();
  element->SetToFile(file.value());
  postData->AddElement(element);
  request->SetPostData(postData);
}

void GetUploadData(CefRefPtr<CefRequest> request,
                   std::string& data) {
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

// Tests if the save cookie has been set. If set, it will be deleted at the same
// time.
void TestSaveCookie(base::WaitableEvent* event, bool* cookie_exists) {
  class Visitor : public CefCookieVisitor {
   public:
    Visitor(base::WaitableEvent* event, bool* cookie_exists)
      : event_(event),
        cookie_exists_(cookie_exists) {
    }
    ~Visitor() override {
      event_->Signal();
    }

    bool Visit(const CefCookie& cookie, int count, int total,
                bool& deleteCookie) override {
      std::string cookie_name = CefString(&cookie.name);
      if (cookie_name == kRequestSaveCookieName) {
        *cookie_exists_ = true;
        deleteCookie = true;
        return false;
      }
      return true;
    }

   private:
    base::WaitableEvent* event_;
    bool* cookie_exists_;

    IMPLEMENT_REFCOUNTING(Visitor);
  };

  CefRefPtr<CefCookieManager> cookie_manager =
      CefCookieManager::GetGlobalManager();
  EXPECT_TRUE(cookie_manager.get());
  cookie_manager->VisitUrlCookies(kRequestOrigin, true,
      new Visitor(event, cookie_exists));
  event->Wait();
}


// Serves request responses.
class RequestSchemeHandler : public CefResourceHandler {
 public:
  explicit RequestSchemeHandler(const RequestRunSettings& settings)
    : settings_(settings),
      offset_(0) {
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    // Shouldn't get here if we're not following redirects.
    EXPECT_TRUE(settings_.expect_follow_redirect);

    // Verify that the request was sent correctly.
    TestRequestEqual(settings_.request, request, true);

    // HEAD requests are identical to GET requests except no response data is
    // sent.
    if (request->GetMethod() == "HEAD")
      settings_.response_data.clear();

    CefRequest::HeaderMap headerMap;
    CefRequest::HeaderMap::iterator headerIter;
    request->GetHeaderMap(headerMap);

    // Check if the default headers were sent.
    headerIter = headerMap.find("User-Agent");
    EXPECT_TRUE(headerIter != headerMap.end() && !headerIter->second.empty());
    headerIter = headerMap.find("Accept-Language");
    EXPECT_TRUE(headerIter != headerMap.end() && !headerIter->second.empty());

    // Check if the request cookie was sent.
    bool has_send_cookie = false;
    headerIter = headerMap.find("Cookie");
    if (headerIter != headerMap.end()) {
      std::string cookie = headerIter->second;
      if (cookie.find(kRequestSendCookieName) != std::string::npos)
        has_send_cookie = true;
    }

    if (settings_.expect_send_cookie)
      EXPECT_TRUE(has_send_cookie);
    else
      EXPECT_FALSE(has_send_cookie);

    // Continue immediately.
    callback->Continue();
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    response->SetStatus(settings_.response->GetStatus());
    response->SetStatusText(settings_.response->GetStatusText());
    response->SetMimeType(settings_.response->GetMimeType());

    CefResponse::HeaderMap headerMap;
    settings_.response->GetHeaderMap(headerMap);

    if (settings_.expect_save_cookie) {
      std::string cookie = base::StringPrintf("%s=%s", kRequestSaveCookieName,
                                              "save-cookie-value");
      headerMap.insert(std::make_pair("Set-Cookie", cookie));
    }

    response->SetHeaderMap(headerMap);

    response_length = settings_.response_data.length();
  }

  bool ReadResponse(void* response_data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    bool has_data = false;
    bytes_read = 0;

    size_t size = settings_.response_data.length();
    if (offset_ < size) {
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(size - offset_));
      memcpy(response_data_out,
             settings_.response_data.c_str() + offset_,
             transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

  void Cancel() override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
  }

 private:
  RequestRunSettings settings_;
  size_t offset_;

  IMPLEMENT_REFCOUNTING(RequestSchemeHandler);
};

// Serves redirect request responses.
class RequestRedirectSchemeHandler : public CefResourceHandler {
 public:
  explicit RequestRedirectSchemeHandler(CefRefPtr<CefRequest> request,
                                        CefRefPtr<CefResponse> response)
    : request_(request),
      response_(response) {
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    // Verify that the request was sent correctly.
    TestRequestEqual(request_, request, true);

    // Continue immediately.
    callback->Continue();
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

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
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    NOTREACHED();
    return false;
  }

  void Cancel() override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
  }

 private:
  CefRefPtr<CefRequest> request_;
  CefRefPtr<CefResponse> response_;

  IMPLEMENT_REFCOUNTING(RequestRedirectSchemeHandler);
};


class RequestSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  RequestSchemeHandlerFactory() {
  }

  CefRefPtr<CefResourceHandler> Create(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const CefString& scheme_name,
      CefRefPtr<CefRequest> request) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    std::string url = request->GetURL();
    
    // Try to find a test match.
    {
      HandlerMap::const_iterator it = handler_map_.find(url);
      if (it != handler_map_.end())
        return new RequestSchemeHandler(it->second);
    }

    // Try to find a redirect match.
    {
      RedirectHandlerMap::const_iterator it = redirect_handler_map_.find(url);
      if (it != redirect_handler_map_.end()) {
        return new RequestRedirectSchemeHandler(it->second.first,
                                                it->second.second);
      }
    }

    // Unknown test.
    ADD_FAILURE();
    return NULL;
  }

  void AddSchemeHandler(const RequestRunSettings& settings) {
    // Verify that the scheme is correct.
    std::string url = settings.request->GetURL();
    EXPECT_EQ((unsigned long)0, url.find(kRequestScheme));

    handler_map_.insert(std::make_pair(url, settings));
  }

  void AddRedirectSchemeHandler(CefRefPtr<CefRequest> redirect_request,
                                CefRefPtr<CefResponse> redirect_response) {
    // Verify that the scheme is correct.
    std::string url = redirect_request->GetURL();
    EXPECT_EQ((unsigned long)0, url.find(kRequestScheme));

    redirect_handler_map_.insert(
        std::make_pair(url,
            std::make_pair(redirect_request, redirect_response)));
  }

 private:
  typedef std::map<std::string, RequestRunSettings> HandlerMap;
  HandlerMap handler_map_;

  typedef std::map<std::string,
                  std::pair<CefRefPtr<CefRequest>, CefRefPtr<CefResponse> > >
                  RedirectHandlerMap;
  RedirectHandlerMap redirect_handler_map_;

  IMPLEMENT_REFCOUNTING(RequestSchemeHandlerFactory);
};


// Implementation of CefURLRequestClient that stores response information.
class RequestClient : public CefURLRequestClient {
 public:
  class Delegate {
   public:
    // Used to notify the handler when the request has completed.
    virtual void OnRequestComplete(CefRefPtr<RequestClient> client) =0;
   protected:
    virtual ~Delegate() {}
  };

  static CefRefPtr<RequestClient> Create(Delegate* delegate,
                                         CefRefPtr<CefRequest> request) {
    CefRefPtr<RequestClient> client = new RequestClient(delegate);
    CefURLRequest::Create(request, client.get());
    return client;
  }

  void OnRequestComplete(CefRefPtr<CefURLRequest> request) override {
    request_complete_ct_++;

    request_ = request->GetRequest();
    EXPECT_TRUE(request_->IsReadOnly());
    status_ = request->GetRequestStatus();
    error_code_ = request->GetRequestError();
    response_ = request->GetResponse();
    EXPECT_TRUE(response_.get());
    EXPECT_TRUE(response_->IsReadOnly());

    delegate_->OnRequestComplete(this);
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
     return false;
   }

 private:
  explicit RequestClient(Delegate* delegate)
    : delegate_(delegate),
      request_complete_ct_(0),
      upload_progress_ct_(0),
      download_progress_ct_(0),
      download_data_ct_(0),
      upload_total_(0),
      download_total_(0) {
  }

  Delegate* delegate_;

 public:
  int request_complete_ct_;
  int upload_progress_ct_;
  int download_progress_ct_;
  int download_data_ct_;

  uint64 upload_total_;
  uint64 download_total_;
  std::string download_data_;
  CefRefPtr<CefRequest> request_;
  CefURLRequest::Status status_;
  CefURLRequest::ErrorCode error_code_;
  CefRefPtr<CefResponse> response_;

 private:
  IMPLEMENT_REFCOUNTING(RequestClient);
};


// Executes the tests.
class RequestTestRunner {
 public:
  typedef base::Callback<void(void)> TestCallback;

  class Delegate {
   public:
    // Used to notify the handler when the test can be destroyed.
    virtual void DestroyTest(const RequestRunSettings& settings) =0;
   protected:
    virtual ~Delegate() {}
  };

  RequestTestRunner(Delegate* delegate, bool is_browser_process)
    : delegate_(delegate),
      is_browser_process_(is_browser_process) {
    // Helper macro for registering test callbacks.
    #define REGISTER_TEST(test_mode, setup_method, run_method) \
        RegisterTest(test_mode, \
                     base::Bind(&RequestTestRunner::setup_method, \
                                base::Unretained(this)), \
                     base::Bind(&RequestTestRunner::run_method, \
                                base::Unretained(this)));

    // Register the test callbacks.
    REGISTER_TEST(REQTEST_GET, SetupGetTest, GenericRunTest);
    REGISTER_TEST(REQTEST_GET_NODATA, SetupGetNoDataTest, GenericRunTest);
    REGISTER_TEST(REQTEST_GET_ALLOWCOOKIES, SetupGetAllowCookiesTest,
                  GenericRunTest);
    REGISTER_TEST(REQTEST_GET_REDIRECT, SetupGetRedirectTest, GenericRunTest);
    REGISTER_TEST(REQTEST_POST, SetupPostTest, GenericRunTest);
    REGISTER_TEST(REQTEST_POST_FILE, SetupPostFileTest, GenericRunTest);
    REGISTER_TEST(REQTEST_POST_WITHPROGRESS, SetupPostWithProgressTest,
                  GenericRunTest);
    REGISTER_TEST(REQTEST_HEAD, SetupHeadTest, GenericRunTest);
  }

  // Called in both the browser and render process to setup the test.
  void SetupTest(RequestTestMode test_mode) {
    TestMap::const_iterator it = test_map_.find(test_mode);
    if (it != test_map_.end()) {
      it->second.setup.Run();
      AddSchemeHandler();
    } else {
      // Unknown test.
      ADD_FAILURE();
    }
  }

  // Called in either the browser or render process to run the test.
  void RunTest(RequestTestMode test_mode) {
    TestMap::const_iterator it = test_map_.find(test_mode);
    if (it != test_map_.end()) {
      it->second.run.Run();
    } else {
      // Unknown test.
      ADD_FAILURE();
      DestroyTest();
    }
  }

 private:
  void SetupGetTest() {
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(MakeSchemeURL("GetTest.html"));
    settings_.request->SetMethod("GET");

    settings_.response = CefResponse::Create();
    settings_.response->SetMimeType("text/html");
    settings_.response->SetStatus(200);
    settings_.response->SetStatusText("OK");

    settings_.response_data = "GET TEST SUCCESS";
  }

  void SetupGetNoDataTest() {
    // Start with the normal get test.
    SetupGetTest();

    // Disable download data notifications.
    settings_.request->SetFlags(UR_FLAG_NO_DOWNLOAD_DATA);

    settings_.expect_download_data = false;
  }

  void SetupGetAllowCookiesTest() {
    // Start with the normal get test.
    SetupGetTest();

    // Send cookies.
    settings_.request->SetFlags(UR_FLAG_ALLOW_CACHED_CREDENTIALS);

    settings_.expect_save_cookie = true;
    settings_.expect_send_cookie = true;
  }

  void SetupGetRedirectTest() {
    // Start with the normal get test.
    SetupGetTest();

    // Add a redirect request.
    settings_.redirect_request = CefRequest::Create();
    settings_.redirect_request->SetURL(MakeSchemeURL("redirect.html"));
    settings_.redirect_request->SetMethod("GET");

    settings_.redirect_response = CefResponse::Create();
    settings_.redirect_response->SetMimeType("text/html");
    settings_.redirect_response->SetStatus(302);
    settings_.redirect_response->SetStatusText("Found");

    CefResponse::HeaderMap headerMap;
    headerMap.insert(std::make_pair("Location", settings_.request->GetURL()));
    settings_.redirect_response->SetHeaderMap(headerMap);
  }

  void SetupPostTest() {
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(MakeSchemeURL("PostTest.html"));
    settings_.request->SetMethod("POST");
    SetUploadData(settings_.request, "the_post_data");

    settings_.response = CefResponse::Create();
    settings_.response->SetMimeType("text/html");
    settings_.response->SetStatus(200);
    settings_.response->SetStatusText("OK");

    settings_.response_data = "POST TEST SUCCESS";
  }

  void SetupPostFileTest() {
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(MakeSchemeURL("PostFileTest.html"));
    settings_.request->SetMethod("POST");

    EXPECT_TRUE(post_file_tmpdir_.CreateUniqueTempDir());
    const base::FilePath& path =
        post_file_tmpdir_.path().Append(FILE_PATH_LITERAL("example.txt"));
    const char content[] = "HELLO FRIEND!";
    int write_ct = base::WriteFile(path, content, sizeof(content) - 1);
    EXPECT_EQ(static_cast<int>(sizeof(content) - 1), write_ct);
    SetUploadFile(settings_.request, path);

    settings_.response = CefResponse::Create();
    settings_.response->SetMimeType("text/html");
    settings_.response->SetStatus(200);
    settings_.response->SetStatusText("OK");

    settings_.response_data = "POST TEST SUCCESS";
  }

  void SetupPostWithProgressTest() {
    // Start with the normal post test.
    SetupPostTest();

    // Enable upload progress notifications.
    settings_.request->SetFlags(UR_FLAG_REPORT_UPLOAD_PROGRESS);

    settings_.expect_upload_progress = true;
  }

  void SetupHeadTest() {
    settings_.request = CefRequest::Create();
    settings_.request->SetURL(MakeSchemeURL("HeadTest.html"));
    settings_.request->SetMethod("HEAD");

    settings_.response = CefResponse::Create();
    settings_.response->SetMimeType("text/html");
    settings_.response->SetStatus(200);
    settings_.response->SetStatusText("OK");

    // The scheme handler will disregard this value when it returns the result.
    settings_.response_data = "HEAD TEST SUCCESS";

    settings_.expect_download_progress = false;
    settings_.expect_download_data = false;
  }

  // Generic test runner.
  void GenericRunTest() {
    class Test : public RequestClient::Delegate {
     public:
      Test(RequestTestRunner* runner, const RequestRunSettings& settings)
        : runner_(runner),
          settings_(settings) {
      }

      void OnRequestComplete(CefRefPtr<RequestClient> client) override {
        CefRefPtr<CefRequest> expected_request;
        CefRefPtr<CefResponse> expected_response;

        if (runner_->settings_.redirect_request.get())
          expected_request = runner_->settings_.redirect_request;
        else
          expected_request = runner_->settings_.request;

        if (runner_->settings_.redirect_response.get() &&
            !runner_->settings_.expect_follow_redirect) {
          // A redirect response was sent but the redirect is not expected to be
          // followed.
          expected_response = runner_->settings_.redirect_response;
        } else {
          expected_response = runner_->settings_.response;
        }
        
        TestRequestEqual(expected_request, client->request_, false);

        EXPECT_EQ(settings_.expected_status, client->status_);
        EXPECT_EQ(settings_.expected_error_code, client->error_code_);
        TestResponseEqual(expected_response, client->response_, true);

        EXPECT_EQ(1, client->request_complete_ct_);

        if (settings_.expect_upload_progress) {
          EXPECT_LE(1, client->upload_progress_ct_);

          std::string upload_data;
          GetUploadData(expected_request, upload_data);
          EXPECT_EQ(upload_data.size(), client->upload_total_);
        } else {
          EXPECT_EQ(0, client->upload_progress_ct_);
          EXPECT_EQ((uint64)0, client->upload_total_);
        }

        if (settings_.expect_download_progress) {
          EXPECT_LE(1, client->download_progress_ct_);
          EXPECT_EQ(runner_->settings_.response_data.size(),
                    client->download_total_);
        } else {
          EXPECT_EQ(0, client->download_progress_ct_);
          EXPECT_EQ((uint64)0, client->download_total_);
        }

        if (settings_.expect_download_data) {
          EXPECT_LE(1, client->download_data_ct_);
          EXPECT_STREQ(runner_->settings_.response_data.c_str(),
                       client->download_data_.c_str());
        } else {
          EXPECT_EQ(0, client->download_data_ct_);
          EXPECT_TRUE(client->download_data_.empty());
        }

        runner_->DestroyTest();
      }

     private:
      RequestTestRunner* runner_;
      RequestRunSettings settings_;
    };

    CefRefPtr<CefRequest> request;
    if (settings_.redirect_request.get())
      request = settings_.redirect_request;
    else
      request = settings_.request;
    EXPECT_TRUE(request.get());

    RequestClient::Create(new Test(this, settings_), request);
  }

  // Register a test. Called in the constructor.
  void RegisterTest(RequestTestMode test_mode,
                    TestCallback setup,
                    TestCallback run) {
    TestEntry entry = {setup, run};
    test_map_.insert(std::make_pair(test_mode, entry));
  }

  // Destroy the current test. Called when the test is complete.
  void DestroyTest() {
    if (scheme_factory_.get()) {
      // Remove the factory registration.
      CefRegisterSchemeHandlerFactory(kRequestScheme, kRequestHost, NULL);
      scheme_factory_ = NULL;
    }

    if (post_file_tmpdir_.IsValid())
      EXPECT_TRUE(post_file_tmpdir_.Delete());

    delegate_->DestroyTest(settings_);
  }

  // Return an appropriate scheme URL for the specified |path|.
  std::string MakeSchemeURL(const std::string& path) {
    return base::StringPrintf("%s/%s", kRequestOrigin, path.c_str());
  }

  // Add a scheme handler for the current test. Called during test setup.
  void AddSchemeHandler() {
    // Scheme handlers are only registered in the browser process.
    if (!is_browser_process_)
      return;

    if (!scheme_factory_.get()) {
      // Add the factory registration.
      scheme_factory_ = new RequestSchemeHandlerFactory();
      CefRegisterSchemeHandlerFactory(kRequestScheme, kRequestHost,
                                      scheme_factory_.get());
    }

    EXPECT_TRUE(settings_.request.get());
    EXPECT_TRUE(settings_.response.get());

    scheme_factory_->AddSchemeHandler(settings_);

    if (settings_.redirect_request.get()) {
      scheme_factory_->AddRedirectSchemeHandler(settings_.redirect_request,
                                                settings_.redirect_response);
    }
  }

  Delegate* delegate_;
  bool is_browser_process_;

  struct TestEntry {
    TestCallback setup;
    TestCallback run;
  };
  typedef std::map<RequestTestMode, TestEntry> TestMap;
  TestMap test_map_;

  std::string scheme_name_;
  CefRefPtr<RequestSchemeHandlerFactory> scheme_factory_;

  base::ScopedTempDir post_file_tmpdir_;

 public:
  RequestRunSettings settings_;
};

// Renderer side.
class RequestRendererTest : public ClientApp::RenderDelegate,
                            public RequestTestRunner::Delegate {
 public:
  RequestRendererTest()
    : test_runner_(this, false) {
  }

  bool OnProcessMessageReceived(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) override {
    if (message->GetName() == kRequestTestMsg) {
      app_ = app;
      browser_ = browser;

      RequestTestMode test_mode =
          static_cast<RequestTestMode>(message->GetArgumentList()->GetInt(0));

      // Setup the test. This will create the objects that we test against but
      // not register any scheme handlers (because we're in the render process).
      test_runner_.SetupTest(test_mode);

      // Run the test.
      test_runner_.RunTest(test_mode);
      return true;
    }

    // Message not handled.
    return false;
  }

 protected:
  // Return from the test.
  void DestroyTest(const RequestRunSettings& settings) override {
    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kRequestTestMsg);
    EXPECT_TRUE(return_msg->GetArgumentList()->SetBool(0, result));
    EXPECT_TRUE(browser_->SendProcessMessage(PID_BROWSER, return_msg));

    app_ = NULL;
    browser_ = NULL;
  }

  CefRefPtr<ClientApp> app_;
  CefRefPtr<CefBrowser> browser_;

  RequestTestRunner test_runner_;

  IMPLEMENT_REFCOUNTING(RequestRendererTest);
};

// Browser side.
class RequestTestHandler : public TestHandler,
                           public RequestTestRunner::Delegate {
 public:
  // Don't hide the DestroyTest method.
  using TestHandler::DestroyTest;

  RequestTestHandler(RequestTestMode test_mode,
                     bool test_in_browser,
                     const char* test_url)
    : test_mode_(test_mode),
      test_in_browser_(test_in_browser),
      test_url_(test_url),
      test_runner_(this, true) {
  }

  void RunTest() override {
    EXPECT_TRUE(test_url_ != NULL);
    AddResource(test_url_, "<html><body>TEST</body></html>", "text/html");

    base::WaitableEvent event(false, false);
    SetTestCookie(&event);
    event.Wait();

    // Setup the test. This will create the objects that we test against and
    // register any scheme handlers.
    test_runner_.SetupTest(test_mode_);

    CreateBrowser(test_url_);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  CefRefPtr<CefProcessMessage> CreateTestMessage() {
    CefRefPtr<CefProcessMessage> msg =
        CefProcessMessage::Create(kRequestTestMsg);
    EXPECT_TRUE(msg->GetArgumentList()->SetInt(0, test_mode_));
    return msg;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (frame->IsMain()) {
      if (test_in_browser_) {
        // Run the test in the browser process.
        test_runner_.RunTest(test_mode_);
      } else {
        // Send a message to the renderer process to run the test.
        EXPECT_TRUE(browser->SendProcessMessage(PID_RENDERER,
            CreateTestMessage()));
      }
    }
  }

  bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) override {
    EXPECT_TRUE(browser.get());
    EXPECT_EQ(PID_RENDERER, source_process);
    EXPECT_TRUE(message.get());
    EXPECT_TRUE(message->IsReadOnly());

    got_message_.yes();

    if (message->GetArgumentList()->GetBool(0))
      got_success_.yes();

    // Test is complete.
    DestroyTest(test_runner_.settings_);

    return true;
  }

  void DestroyTest(const RequestRunSettings& settings) override {
    base::WaitableEvent event(false, false);

    bool has_save_cookie = false;
    TestSaveCookie(&event, &has_save_cookie);
    if (settings.expect_save_cookie)
      EXPECT_TRUE(has_save_cookie);
    else
      EXPECT_FALSE(has_save_cookie);

    ClearTestCookie(&event);
    event.Wait();

    TestHandler::DestroyTest();
  }

 protected:
  // Set a cookie so that we can test if it's sent with the request.
  void SetTestCookie(base::WaitableEvent* event) {
    if (CefCurrentlyOn(TID_IO)) {
      CefRefPtr<CefCookieManager> cookie_manager =
          CefCookieManager::GetGlobalManager();
      EXPECT_TRUE(cookie_manager.get());

      CefCookie cookie;
      CefString(&cookie.name) = kRequestSendCookieName;
      CefString(&cookie.value) = "send-cookie-value";
      CefString(&cookie.domain) = kRequestHost;
      CefString(&cookie.path) = "/";
      cookie.has_expires = false;
      EXPECT_TRUE(cookie_manager->SetCookie(kRequestOrigin, cookie));
      event->Signal();
    } else {
      // Execute on the IO thread.
      CefPostTask(TID_IO,
          base::Bind(&RequestTestHandler::SetTestCookie, this, event));
    }
  }

  // Remove the cookie that we set above.
  void ClearTestCookie(base::WaitableEvent* event) {
    if (CefCurrentlyOn(TID_IO)) {
      CefRefPtr<CefCookieManager> cookie_manager =
          CefCookieManager::GetGlobalManager();
      EXPECT_TRUE(cookie_manager.get());
      EXPECT_TRUE(cookie_manager->DeleteCookies(kRequestOrigin,
                                                kRequestSendCookieName));
      event->Signal();
    } else {
      // Execute on the IO thread.
      CefPostTask(TID_IO,
          base::Bind(&RequestTestHandler::ClearTestCookie, this, event));
    }
  }

  RequestTestMode test_mode_;
  bool test_in_browser_;
  const char* test_url_;

  RequestTestRunner test_runner_;

 public:
  // Only used when the test runs in the render process.
  TrackCallback got_message_;
  TrackCallback got_success_;
};

}  // namespace


// Entry point for creating URLRequest renderer test objects.
// Called from client_app_delegates.cc.
void CreateURLRequestRendererTests(ClientApp::RenderDelegateSet& delegates) {
  delegates.insert(new RequestRendererTest);
}

// Entry point for registering custom schemes.
// Called from client_app_delegates.cc.
void RegisterURLRequestCustomSchemes(
      CefRefPtr<CefSchemeRegistrar> registrar,
      std::vector<CefString>& cookiable_schemes) {
  registrar->AddCustomScheme(kRequestScheme, true, false, false);
  cookiable_schemes.push_back(kRequestScheme);
}


// Helpers for defining URLRequest tests.
#define REQ_TEST_EX(name, test_mode, test_in_browser, test_url) \
  TEST(URLRequestTest, name) { \
    CefRefPtr<RequestTestHandler> handler = \
        new RequestTestHandler(test_mode, test_in_browser, test_url); \
    handler->ExecuteTest(); \
    if (!test_in_browser) { \
      EXPECT_TRUE(handler->got_message_); \
      EXPECT_TRUE(handler->got_success_); \
    } \
    ReleaseAndWaitForDestructor(handler); \
  }

#define REQ_TEST(name, test_mode, test_in_browser) \
  REQ_TEST_EX(name, test_mode, test_in_browser, kRequestTestUrl)


// Define the tests.
REQ_TEST(BrowserGET, REQTEST_GET, true);
REQ_TEST(BrowserGETNoData, REQTEST_GET_NODATA, true);
REQ_TEST(BrowserGETAllowCookies, REQTEST_GET_ALLOWCOOKIES, true);
REQ_TEST(BrowserGETRedirect, REQTEST_GET_REDIRECT, true);
REQ_TEST(BrowserPOST, REQTEST_POST, true);
REQ_TEST(BrowserPOSTFile, REQTEST_POST_FILE, true);
REQ_TEST(BrowserPOSTWithProgress, REQTEST_POST_WITHPROGRESS, true);
REQ_TEST(BrowserHEAD, REQTEST_HEAD, true);

REQ_TEST(RendererGET, REQTEST_GET, false);
REQ_TEST(RendererGETNoData, REQTEST_GET_NODATA, false);
REQ_TEST(RendererGETAllowCookies, REQTEST_GET_ALLOWCOOKIES, false);
REQ_TEST(RendererGETRedirect, REQTEST_GET_REDIRECT, false);
REQ_TEST(RendererPOST, REQTEST_POST, false);
REQ_TEST(RendererPOSTWithProgress, REQTEST_POST_WITHPROGRESS, false);
REQ_TEST(RendererHEAD, REQTEST_HEAD, false);


namespace {

class InvalidURLTestClient : public CefURLRequestClient {
 public:
  InvalidURLTestClient()
      : event_(false, false) {
  }

  void RunTest() {
    CefPostTask(TID_UI, base::Bind(&InvalidURLTestClient::RunOnUIThread, this));

    // Wait for the test to complete.
    event_.Wait();
  }

  void OnRequestComplete(CefRefPtr<CefURLRequest> client) override {
    EXPECT_EQ(UR_FAILED, client->GetRequestStatus());

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

  bool GetAuthCredentials(
      bool isProxy,
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

    CefURLRequest::Create(request, this);
  }

  void CompleteOnUIThread() {
    EXPECT_UI_THREAD();
    // Signal that the test is complete.
    event_.Signal();
  }

  base::WaitableEvent event_;

  IMPLEMENT_REFCOUNTING(InvalidURLTestClient);
};

}  // namespace

// Verify that failed requests do not leak references.
TEST(URLRequestTest, BrowserInvalidURL) { 
  CefRefPtr<InvalidURLTestClient> client = new InvalidURLTestClient();
  client->RunTest();

  // Verify that there's only one reference to the client.
  EXPECT_TRUE(client->HasOneRef());
}
