// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <list>
#include <map>
#include <memory>
#include <set>

#include "include/base/cef_callback.h"
#include "include/base/cef_ref_counted.h"
#include "include/cef_command_line.h"
#include "include/cef_task.h"
#include "include/cef_urlrequest.h"
#include "include/test/cef_test_server.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char kPlaceholderOrigin[] = "http://placeholder/";
const int kTestTimeout = 5000;

// Handles the test server. Used for both HTTP and HTTPS tests.
class TestServerHandler : public CefTestServerHandler {
 public:
  // HTTP test handler.
  // The methods of this class are always executed on the server thread.
  class HttpRequestHandler {
   public:
    virtual ~HttpRequestHandler() = default;
    virtual bool HandleRequest(
        CefRefPtr<CefTestServer> server,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefTestServerConnection> connection) = 0;
    virtual bool VerifyResults() = 0;
    virtual std::string ToString() = 0;
  };

  using StartCallback =
      base::OnceCallback<void(const std::string& server_origin)>;

  // |start_callback| will be executed on the UI thread after the server is
  // started.
  // |destroy_callback| will be executed on the UI thread after this handler
  // object is destroyed.
  TestServerHandler(StartCallback start_callback,
                    base::OnceClosure destroy_callback)
      : start_callback_(std::move(start_callback)),
        destroy_callback_(std::move(destroy_callback)) {
    EXPECT_FALSE(destroy_callback_.is_null());
  }

  ~TestServerHandler() override {
    EXPECT_UI_THREAD();
    std::move(destroy_callback_).Run();
  }

  // Must be called before CreateServer().
  void AddHttpRequestHandler(
      std::unique_ptr<HttpRequestHandler> request_handler) {
    EXPECT_FALSE(initialized_);
    EXPECT_TRUE(request_handler);
    http_request_handler_list_.push_back(std::move(request_handler));
  }

  // Must be called before CreateServer().
  void SetExpectedHttpRequestCount(int expected) {
    EXPECT_FALSE(initialized_);
    expected_http_request_ct_ = expected;
  }

  void CreateServer(bool https_server) {
    EXPECT_FALSE(initialized_);
    initialized_ = true;
    https_server_ = https_server;

    // Blocks until the server is created.
    server_ = CefTestServer::CreateAndStart(/*port=*/0, https_server,
                                            CEF_TEST_CERT_OK_DOMAIN, this);

    origin_ = server_->GetOrigin();
    EXPECT_TRUE(VerifyOrigin(origin_));

    RunStartCallback();
  }

  // Results in a call to VerifyResults() and eventual execution of the
  // |destroy_callback|.
  void ShutdownServer() {
    EXPECT_TRUE(server_);
    server_->Stop();
    server_ = nullptr;
    VerifyResults();
  }

  bool OnTestServerRequest(
      CefRefPtr<CefTestServer> server,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefTestServerConnection> connection) override {
    EXPECT_TRUE(server);
    EXPECT_STREQ(origin_.c_str(), server->GetOrigin().ToString().c_str());

    EXPECT_TRUE(request);
    EXPECT_TRUE(VerifyRequest(request));

    EXPECT_TRUE(connection);

    bool handled = false;
    for (const auto& handler : http_request_handler_list_) {
      handled = handler->HandleRequest(server, request, connection);
      if (handled) {
        break;
      }
    }
    EXPECT_TRUE(handled) << "missing HttpRequestHandler for "
                         << request->GetURL().ToString();

    actual_http_request_ct_++;

    return handled;
  }

 private:
  bool VerifyOrigin(const std::string& origin) const {
    V_DECLARE();
    V_EXPECT_TRUE(origin.find(https_server_ ? "https://" : "http://") == 0)
        << "origin " << origin_;
    V_RETURN();
  }

  bool VerifyRequest(CefRefPtr<CefRequest> request) const {
    V_DECLARE();

    V_EXPECT_FALSE(request->GetMethod().empty());

    const std::string& url = request->GetURL();
    V_EXPECT_FALSE(url.empty());
    V_EXPECT_TRUE(VerifyOrigin(url)) << "url " << url;

    CefRefPtr<CefPostData> post_data = request->GetPostData();
    if (post_data) {
      CefPostData::ElementVector elements;
      post_data->GetElements(elements);
      V_EXPECT_TRUE(elements.size() == 1);
      V_EXPECT_TRUE(elements[0]->GetBytesCount() > 0U);
    }

    V_RETURN();
  }

  void VerifyResults() const {
    EXPECT_EQ(expected_http_request_ct_, actual_http_request_ct_);

    for (const auto& handler : http_request_handler_list_) {
      EXPECT_TRUE(handler->VerifyResults())
          << "HttpRequestHandler for " << handler->ToString();
    }
  }

 private:
  void RunStartCallback() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::BindOnce(&TestServerHandler::RunStartCallback, this));
      return;
    }

    EXPECT_FALSE(start_callback_.is_null());
    std::move(start_callback_).Run(server_->GetOrigin());
  }

  CefRefPtr<CefTestServer> server_;
  bool initialized_ = false;

  // After initialization only accessed on the UI thread.
  StartCallback start_callback_;
  base::OnceClosure destroy_callback_;

  bool https_server_ = false;
  std::string origin_;

  // After initialization the below members are only accessed on the server
  // thread.

  std::list<std::unique_ptr<HttpRequestHandler>> http_request_handler_list_;

  int expected_http_request_ct_ = 0;
  int actual_http_request_ct_ = 0;

  IMPLEMENT_REFCOUNTING(TestServerHandler);
  DISALLOW_COPY_AND_ASSIGN(TestServerHandler);
};

// HTTP TESTS

// Test runner for 1 or more HTTP requests/responses.
// Works similarly to TestHandler but without the CefClient dependencies.
class HttpTestRunner : public base::RefCountedThreadSafe<HttpTestRunner> {
 public:
  // The methods of this class are always executed on the UI thread.
  class RequestRunner {
   public:
    virtual ~RequestRunner() = default;

    // Create the server-side handler for the request.
    virtual std::unique_ptr<TestServerHandler::HttpRequestHandler>
    CreateHttpRequestHandler() = 0;

    // Run the request and execute |complete_callback| on completion.
    virtual void RunRequest(const std::string& server_origin,
                            base::OnceClosure complete_callback) = 0;

    virtual bool VerifyResults() = 0;
    virtual std::string ToString() = 0;
  };

  // If |parallel_requests| is true all requests will be run at the same time,
  // otherwise one request will be run at a time.
  HttpTestRunner(bool https_server, bool parallel_requests)
      : https_server_(https_server), parallel_requests_(parallel_requests) {}

  virtual ~HttpTestRunner() {
    if (destroy_event_) {
      destroy_event_->Signal();
    }
  }

  void AddRequestRunner(std::unique_ptr<RequestRunner> request_runner) {
    EXPECT_FALSE(initialized_);
    request_runner_map_.insert(
        std::make_pair(++next_request_id_, request_runner.release()));
  }

  // Blocks until the test has completed or timed out.
  void ExecuteTest() {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI));

    handler_ = new TestServerHandler(
        base::BindOnce(&HttpTestRunner::OnServerStarted, this),
        base::BindOnce(&HttpTestRunner::OnServerDestroyed, this));

    run_event_ = CefWaitableEvent::CreateWaitableEvent(false, false);

    CefPostTask(TID_UI, base::BindOnce(&HttpTestRunner::RunTest, this));

    // Block until test completion.
    run_event_->Wait();
  }

  // Event that will be signaled from the HttpTestRunner destructor.
  // Used by ReleaseAndWaitForDestructor.
  void SetDestroyEvent(CefRefPtr<CefWaitableEvent> event) {
    destroy_event_ = event;
  }

 private:
  void RunTest() {
    EXPECT_UI_THREAD();
    EXPECT_FALSE(initialized_);
    initialized_ = true;

    EXPECT_FALSE(request_runner_map_.empty());
    RequestRunnerMap::const_iterator it = request_runner_map_.begin();
    for (; it != request_runner_map_.end(); ++it) {
      handler_->AddHttpRequestHandler(it->second->CreateHttpRequestHandler());
    }

    handler_->SetExpectedHttpRequestCount(
        static_cast<int>(request_runner_map_.size()));

    handler_->CreateServer(https_server_);

    SetTestTimeout(kTestTimeout);
  }

  void OnServerStarted(const std::string& server_origin) {
    EXPECT_UI_THREAD();
    server_origin_ = server_origin;
    if (parallel_requests_) {
      RunAllRequests();
    } else {
      RunNextRequest();
    }
  }

  void OnServerDestroyed() {
    EXPECT_UI_THREAD();
    EXPECT_FALSE(got_server_destroyed_);
    got_server_destroyed_.yes();

    // Allow the call stack to unwind.
    CefPostTask(TID_UI, base::BindOnce(&HttpTestRunner::DestroyTest, this));
  }

  // Run all requests in parallel.
  void RunAllRequests() {
    RequestRunnerMap::const_iterator it = request_runner_map_.begin();
    for (; it != request_runner_map_.end(); ++it) {
      it->second->RunRequest(
          server_origin_,
          base::BindOnce(&HttpTestRunner::OnRequestComplete, this, it->first));
    }
  }

  // Run one request at a time.
  void RunNextRequest() {
    RequestRunnerMap::const_iterator it = request_runner_map_.begin();
    it->second->RunRequest(
        server_origin_,
        base::BindOnce(&HttpTestRunner::OnRequestComplete, this, it->first));
  }

  void OnRequestComplete(int request_id) {
    EXPECT_UI_THREAD()
    // Allow the call stack to unwind.
    CefPostTask(TID_UI,
                base::BindOnce(&HttpTestRunner::OnRequestCompleteContinue, this,
                               request_id));
  }

  void OnRequestCompleteContinue(int request_id) {
    RequestRunnerMap::iterator it = request_runner_map_.find(request_id);
    EXPECT_TRUE(it != request_runner_map_.end());

    // Verify the request results.
    EXPECT_TRUE(it->second->VerifyResults())
        << "request_id " << request_id << " RequestRunner for "
        << it->second->ToString();
    delete it->second;
    request_runner_map_.erase(it);

    if (request_runner_map_.empty()) {
      got_all_requests_.yes();

      // Will trigger TestServerHandler::HttpRequestHandler verification and a
      // call to OnServerDestroyed().
      handler_->ShutdownServer();
      handler_ = nullptr;
    } else if (!parallel_requests_) {
      RunNextRequest();
    }
  }

  void DestroyTest() {
    EXPECT_UI_THREAD();

    EXPECT_TRUE(got_all_requests_);
    EXPECT_TRUE(got_server_destroyed_);
    EXPECT_TRUE(request_runner_map_.empty());

    // Cancel the timeout, if any.
    if (ui_thread_helper_) {
      ui_thread_helper_.reset();
    }

    // Signal test completion.
    run_event_->Signal();
  }

  TestHandler::UIThreadHelper* GetUIThreadHelper() {
    EXPECT_UI_THREAD();
    if (!ui_thread_helper_) {
      ui_thread_helper_ = std::make_unique<TestHandler::UIThreadHelper>();
    }
    return ui_thread_helper_.get();
  }

  void SetTestTimeout(int timeout_ms) {
    EXPECT_UI_THREAD();
    const auto timeout = GetConfiguredTestTimeout(timeout_ms);
    if (!timeout) {
      return;
    }

    // Use a weak reference to |this| via UIThreadHelper so that the
    // test runner can be destroyed before the timeout expires.
    GetUIThreadHelper()->PostDelayedTask(
        base::BindOnce(&HttpTestRunner::OnTestTimeout, base::Unretained(this),
                       *timeout),
        *timeout);
  }

  void OnTestTimeout(int timeout_ms) {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(false) << "Test timed out after " << timeout_ms << "ms";
    DestroyTest();
  }

  const bool https_server_;
  const bool parallel_requests_;
  CefRefPtr<CefWaitableEvent> run_event_;
  CefRefPtr<CefWaitableEvent> destroy_event_;
  CefRefPtr<TestServerHandler> handler_;
  bool initialized_ = false;

  // After initialization the below members are only accessed on the UI thread.

  std::string server_origin_;
  int next_request_id_ = 0;

  // Map of request ID to RequestRunner.
  typedef std::map<int, RequestRunner*> RequestRunnerMap;
  RequestRunnerMap request_runner_map_;

  TrackCallback got_all_requests_;
  TrackCallback got_server_destroyed_;

  std::unique_ptr<TestHandler::UIThreadHelper> ui_thread_helper_;

  DISALLOW_COPY_AND_ASSIGN(HttpTestRunner);
};

// Structure representing the data that can be sent via
// CefServer::SendHttp*Response().
struct HttpServerResponse {
  enum Type { TYPE_200, TYPE_404, TYPE_500, TYPE_CUSTOM };

  explicit HttpServerResponse(Type response_type) : type(response_type) {}

  Type type;

  // Used with 200 and CUSTOM response type.
  std::string content;
  std::string content_type;

  // Used with 500 response type.
  std::string error_message;

  // Used with CUSTOM response type.
  int response_code;
  CefRequest::HeaderMap extra_headers;
};

void SendHttpServerResponse(CefRefPtr<CefTestServerConnection> connection,
                            const HttpServerResponse& response) {
  switch (response.type) {
    case HttpServerResponse::TYPE_200:
      EXPECT_TRUE(!response.content_type.empty());
      connection->SendHttp200Response(response.content_type,
                                      response.content.data(),
                                      response.content.size());
      break;
    case HttpServerResponse::TYPE_404:
      connection->SendHttp404Response();
      break;
    case HttpServerResponse::TYPE_500:
      connection->SendHttp500Response(response.error_message);
      break;
    case HttpServerResponse::TYPE_CUSTOM:
      EXPECT_TRUE(!response.content_type.empty());
      connection->SendHttpResponse(
          response.response_code, response.content_type,
          response.content.data(), response.content.size(),
          response.extra_headers);
      break;
  }
}

std::string GetHeaderValue(const CefRequest::HeaderMap& header_map,
                           const std::string& header_name) {
  CefRequest::HeaderMap::const_iterator it = header_map.find(header_name);
  if (it != header_map.end()) {
    return it->second;
  }
  return std::string();
}

void VerifyHttpServerResponse(const HttpServerResponse& expected_response,
                              CefRefPtr<CefResponse> response,
                              const std::string& data) {
  CefRequest::HeaderMap header_map;
  response->GetHeaderMap(header_map);

  switch (expected_response.type) {
    case HttpServerResponse::TYPE_200:
      EXPECT_EQ(200, response->GetStatus());
      EXPECT_STREQ(expected_response.content_type.c_str(),
                   GetHeaderValue(header_map, "Content-Type").c_str());
      EXPECT_STREQ(expected_response.content.c_str(), data.c_str());
      break;
    case HttpServerResponse::TYPE_404:
      EXPECT_EQ(404, response->GetStatus());
      break;
    case HttpServerResponse::TYPE_500:
      EXPECT_EQ(500, response->GetStatus());
      break;
    case HttpServerResponse::TYPE_CUSTOM:
      EXPECT_EQ(expected_response.response_code, response->GetStatus());
      EXPECT_STREQ(expected_response.content_type.c_str(),
                   GetHeaderValue(header_map, "Content-Type").c_str());
      EXPECT_EQ(static_cast<int>(expected_response.content.size()),
                atoi(GetHeaderValue(header_map, "Content-Length").c_str()));
      EXPECT_STREQ(expected_response.content.c_str(), data.c_str());
      TestMapEqual(expected_response.extra_headers, header_map, true);
      break;
  }
}

CefRefPtr<CefRequest> CreateTestServerRequest(
    const std::string& path,
    const std::string& method,
    const std::string& data = std::string(),
    const std::string& content_type = std::string(),
    const CefRequest::HeaderMap& extra_headers = CefRequest::HeaderMap()) {
  CefRefPtr<CefRequest> request = CefRequest::Create();

  request->SetURL(kPlaceholderOrigin + path);
  request->SetMethod(method);

  CefRequest::HeaderMap header_map;

  if (!data.empty()) {
    CefRefPtr<CefPostData> post_data = CefPostData::Create();
    CefRefPtr<CefPostDataElement> post_element = CefPostDataElement::Create();
    post_element->SetToBytes(data.size(), data.data());
    post_data->AddElement(post_element);
    request->SetPostData(post_data);

    EXPECT_FALSE(content_type.empty());
    header_map.insert(std::make_pair("content-type", content_type));
  }

  if (!extra_headers.empty()) {
    header_map.insert(extra_headers.begin(), extra_headers.end());
  }
  request->SetHeaderMap(header_map);

  return request;
}

// RequestHandler that returns a static response for 1 or more requests.
class StaticHttpServerRequestHandler
    : public TestServerHandler::HttpRequestHandler {
 public:
  StaticHttpServerRequestHandler(CefRefPtr<CefRequest> expected_request,
                                 int expected_request_ct,
                                 const HttpServerResponse& response)
      : expected_request_(expected_request),
        expected_request_ct_(expected_request_ct),

        response_(response) {}

  bool HandleRequest(CefRefPtr<CefTestServer> server,
                     CefRefPtr<CefRequest> request,
                     CefRefPtr<CefTestServerConnection> connection) override {
    if (request->GetURL() == expected_request_->GetURL() &&
        request->GetMethod() == expected_request_->GetMethod()) {
      TestRequestEqual(expected_request_, request, true);
      actual_request_ct_++;

      SendHttpServerResponse(connection, response_);
      return true;
    }

    return false;
  }

  bool VerifyResults() override {
    EXPECT_EQ(expected_request_ct_, actual_request_ct_);
    return expected_request_ct_ == actual_request_ct_;
  }

  std::string ToString() override { return expected_request_->GetURL(); }

 private:
  CefRefPtr<CefRequest> expected_request_;
  int expected_request_ct_;
  int actual_request_ct_ = 0;
  HttpServerResponse response_;

  DISALLOW_COPY_AND_ASSIGN(StaticHttpServerRequestHandler);
};

// URLRequestClient that runs a single request and executes a callback with the
// response.
class StaticHttpURLRequestClient : public CefURLRequestClient {
 public:
  using ResponseCallback =
      base::OnceCallback<void(cef_errorcode_t /* error */,
                              CefRefPtr<CefResponse> /* response */,
                              const std::string& /* data */)>;

  // |response_callback| will be executed on the UI thread when the response
  // is complete.
  StaticHttpURLRequestClient(CefRefPtr<CefRequest> request,
                             ResponseCallback response_callback)
      : request_(request), response_callback_(std::move(response_callback)) {
    EXPECT_TRUE(request_);
    EXPECT_FALSE(response_callback_.is_null());
  }

  void RunRequest() {
    EXPECT_UI_THREAD();
    CefURLRequest::Create(request_, this, nullptr);
  }

  void OnRequestComplete(CefRefPtr<CefURLRequest> request) override {
    EXPECT_FALSE(response_callback_.is_null());
    std::move(response_callback_)
        .Run(request->GetRequestError(), request->GetResponse(), data_);
  }

  void OnUploadProgress(CefRefPtr<CefURLRequest> request,
                        int64_t current,
                        int64_t total) override {}

  void OnDownloadProgress(CefRefPtr<CefURLRequest> request,
                          int64_t current,
                          int64_t total) override {}

  void OnDownloadData(CefRefPtr<CefURLRequest> request,
                      const void* data,
                      size_t data_length) override {
    data_.append(static_cast<const char*>(data), data_length);
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
  CefRefPtr<CefRequest> request_;
  ResponseCallback response_callback_;
  std::string data_;

  IMPLEMENT_REFCOUNTING(StaticHttpURLRequestClient);
  DISALLOW_COPY_AND_ASSIGN(StaticHttpURLRequestClient);
};

// RequestRunner that will manage a single static HTTP request/response.
class StaticHttpRequestRunner : public HttpTestRunner::RequestRunner {
 public:
  StaticHttpRequestRunner(CefRefPtr<CefRequest> request,
                          const HttpServerResponse& response)
      : request_(request), response_(response) {}

  static std::unique_ptr<HttpTestRunner::RequestRunner> Create200(
      const std::string& path,
      bool with_content = true) {
    CefRefPtr<CefRequest> request = CreateTestServerRequest(path, "GET");
    HttpServerResponse response(HttpServerResponse::TYPE_200);
    response.content_type = "text/html";
    if (with_content) {
      response.content = "<html>200 response content</html>";
    }
    return std::make_unique<StaticHttpRequestRunner>(request, response);
  }

  static std::unique_ptr<HttpTestRunner::RequestRunner> Create404(
      const std::string& path) {
    CefRefPtr<CefRequest> request = CreateTestServerRequest(path, "GET");
    HttpServerResponse response(HttpServerResponse::TYPE_404);
    return std::make_unique<StaticHttpRequestRunner>(request, response);
  }

  static std::unique_ptr<HttpTestRunner::RequestRunner> Create500(
      const std::string& path) {
    CefRefPtr<CefRequest> request = CreateTestServerRequest(path, "GET");
    // Don't retry the request.
    request->SetFlags(UR_FLAG_NO_RETRY_ON_5XX);
    HttpServerResponse response(HttpServerResponse::TYPE_500);
    response.error_message = "Something went wrong!";
    return std::make_unique<StaticHttpRequestRunner>(request, response);
  }

  static std::unique_ptr<HttpTestRunner::RequestRunner> CreateCustom(
      const std::string& path,
      bool with_content = true) {
    CefRequest::HeaderMap request_headers;
    request_headers.insert(std::make_pair("x-request-custom1", "My Value A"));
    request_headers.insert(std::make_pair("x-request-custom2", "My Value B"));
    CefRefPtr<CefRequest> request = CreateTestServerRequest(
        path, "POST", "foo=bar&choo=too", "application/x-www-form-urlencoded",
        request_headers);
    request->SetReferrer("http://tests/referer.html", REFERRER_POLICY_DEFAULT);

    HttpServerResponse response(HttpServerResponse::TYPE_CUSTOM);
    response.response_code = 202;
    if (with_content) {
      response.content = "BlahBlahBlah";
    }
    response.content_type = "application/x-blah-blah";
    response.extra_headers.insert(
        std::make_pair("x-response-custom1", "My Value 1"));
    response.extra_headers.insert(
        std::make_pair("x-response-custom2", "My Value 2"));

    return std::make_unique<StaticHttpRequestRunner>(request, response);
  }

  std::unique_ptr<TestServerHandler::HttpRequestHandler>
  CreateHttpRequestHandler() override {
    EXPECT_FALSE(got_create_handler_);
    got_create_handler_.yes();
    return std::make_unique<StaticHttpServerRequestHandler>(request_, 1,
                                                            response_);
  }

  void RunRequest(const std::string& server_origin,
                  base::OnceClosure complete_callback) override {
    EXPECT_UI_THREAD();

    EXPECT_FALSE(got_run_request_);
    got_run_request_.yes();

    complete_callback_ = std::move(complete_callback);

    // Replace the placeholder with the actual server origin.
    const std::string& url = request_->GetURL();
    EXPECT_TRUE(url.find(kPlaceholderOrigin) == 0);
    const std::string& real_url =
        server_origin + "/" + url.substr(sizeof(kPlaceholderOrigin) - 1);
    request_->SetURL(real_url);

    request_client_ = new StaticHttpURLRequestClient(
        request_, base::BindOnce(&StaticHttpRequestRunner::OnResponseComplete,
                                 base::Unretained(this)));
    request_client_->RunRequest();
  }

  bool VerifyResults() override {
    V_DECLARE();
    V_EXPECT_TRUE(got_create_handler_);
    V_EXPECT_TRUE(got_run_request_);
    V_EXPECT_TRUE(got_response_complete_);
    V_RETURN();
  }

  std::string ToString() override { return request_->GetURL(); }

 private:
  void OnResponseComplete(cef_errorcode_t error,
                          CefRefPtr<CefResponse> response,
                          const std::string& data) {
    EXPECT_UI_THREAD();

    EXPECT_FALSE(got_response_complete_);
    got_response_complete_.yes();

    EXPECT_EQ(error, ERR_NONE)
        << "OnResponseComplete for " << request_->GetURL().ToString();
    if (error == ERR_NONE) {
      VerifyHttpServerResponse(response_, response, data);
    }

    std::move(complete_callback_).Run();
  }

  CefRefPtr<CefRequest> request_;
  HttpServerResponse response_;

  CefRefPtr<StaticHttpURLRequestClient> request_client_;
  base::OnceClosure complete_callback_;

  TrackCallback got_run_request_;
  TrackCallback got_create_handler_;
  TrackCallback got_response_complete_;

  DISALLOW_COPY_AND_ASSIGN(StaticHttpRequestRunner);
};

}  // namespace

// Verify handling of a single HTTP 200 request.
TEST(TestServerTest, HttpSingle200) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/false, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTPS 200 request.
TEST(TestServerTest, HttpsSingle200) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/true, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP 200 request with no content.
TEST(TestServerTest, HttpSingle200NoContent) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/false, /*parallel_requests=*/false);
  runner->AddRequestRunner(
      StaticHttpRequestRunner::Create200("200.html", false));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTPS 200 request with no content.
TEST(TestServerTest, HttpsSingle200NoContent) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/true, /*parallel_requests=*/false);
  runner->AddRequestRunner(
      StaticHttpRequestRunner::Create200("200.html", false));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP 404 request.
TEST(TestServerTest, HttpSingle404) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/false, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create404("404.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTPS 404 request.
TEST(TestServerTest, HttpsSingle404) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/true, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create404("404.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP 500 request.
TEST(TestServerTest, HttpSingle500) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/false, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create500("500.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTPS 500 request.
TEST(TestServerTest, HttpsSingle500) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/true, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create500("500.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP custom request.
TEST(TestServerTest, HttpSingleCustom) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/false, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::CreateCustom("202.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP custom request.
TEST(TestServerTest, HttpsSingleCustom) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/true, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::CreateCustom("202.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP custom request with no content.
TEST(TestServerTest, HttpSingleCustomNoContent) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/false, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::CreateCustom(
      "202.html", /*with_content=*/false));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTPS custom request with no content.
TEST(TestServerTest, HttpsSingleCustomNoContent) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/true, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::CreateCustom(
      "202.html", /*with_content=*/false));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of multiple HTTP requests in parallel.
TEST(TestServerTest, HttpMultipleParallel200) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/false, /*parallel_requests=*/true);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200a.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200b.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200c.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of multiple HTTPS requests in parallel.
TEST(TestServerTest, HttpsMultipleParallel200) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/true, /*parallel_requests=*/true);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200a.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200b.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200c.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of multiple HTTP requests in serial.
TEST(TestServerTest, HttpMultipleSerial200) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/false, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200a.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200b.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200c.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of multiple HTTPS requests in serial.
TEST(TestServerTest, HttpsMultipleSerial200) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/true, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200a.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200b.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200c.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of multiple HTTP requests in parallel.
TEST(TestServerTest, HttpMultipleParallelMixed) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/false, /*parallel_requests=*/true);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create404("404.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create500("500.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::CreateCustom("202.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of multiple HTTPS requests in parallel.
TEST(TestServerTest, HttpsMultipleParallelMixed) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/true, /*parallel_requests=*/true);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create404("404.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create500("500.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::CreateCustom("202.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of multiple HTTP requests in serial.
TEST(TestServerTest, HttpMultipleSerialMixed) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/false, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create404("404.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create500("500.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::CreateCustom("202.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of multiple HTTPS requests in serial.
TEST(TestServerTest, HttpsMultipleSerialMixed) {
  CefRefPtr<HttpTestRunner> runner =
      new HttpTestRunner(/*https_server=*/true, /*parallel_requests=*/false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create404("404.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create500("500.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::CreateCustom("202.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}
