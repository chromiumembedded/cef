// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <list>
#include <map>
#include <memory>
#include <set>

#include "include/base/cef_callback.h"
#include "include/base/cef_ref_counted.h"
#include "include/cef_command_line.h"
#include "include/cef_server.h"
#include "include/cef_task.h"
#include "include/cef_urlrequest.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

// Must use a different port than test_server.cc.
const char kTestServerAddress[] = "127.0.0.1";
const uint16 kTestServerPort = 8099;
const int kTestTimeout = 5000;

std::string GetTestServerOrigin(bool is_websocket) {
  std::stringstream ss;
  ss << (is_websocket ? "ws://" : "http://") << kTestServerAddress << ":"
     << kTestServerPort;
  return ss.str();
}

// Handles the test server. Used for both HTTP and WebSocket tests.
class TestServerHandler : public CefServerHandler {
 public:
  // HTTP test handler.
  // The methods of this class are always executed on the server thread.
  class HttpRequestHandler {
   public:
    virtual ~HttpRequestHandler() {}
    virtual bool HandleRequest(CefRefPtr<CefServer> server,
                               int connection_id,
                               const CefString& client_address,
                               CefRefPtr<CefRequest> request) = 0;
    virtual bool VerifyResults() = 0;
    virtual std::string ToString() = 0;
  };

  // WebSocket test handler.
  // The methods of this class are always executed on the server thread.
  class WsRequestHandler {
   public:
    virtual ~WsRequestHandler() {}
    virtual bool HandleRequest(CefRefPtr<CefServer> server,
                               int connection_id,
                               const CefString& client_address,
                               CefRefPtr<CefRequest> request,
                               CefRefPtr<CefCallback> callback) = 0;
    virtual bool HandleConnected(CefRefPtr<CefServer> server,
                                 int connection_id) = 0;
    virtual bool HandleMessage(CefRefPtr<CefServer> server,
                               int connection_id,
                               const void* data,
                               size_t data_size) = 0;
    virtual bool VerifyResults() = 0;
    virtual std::string ToString() = 0;
  };

  // |start_callback| will be executed on the UI thread after the server is
  // started.
  // |destroy_callback| will be executed on the UI thread after this handler
  // object is destroyed.
  TestServerHandler(base::OnceClosure start_callback,
                    base::OnceClosure destroy_callback)
      : initialized_(false),
        start_callback_(std::move(start_callback)),
        destroy_callback_(std::move(destroy_callback)),
        expected_connection_ct_(0),
        actual_connection_ct_(0),
        expected_http_request_ct_(0),
        actual_http_request_ct_(0),
        expected_ws_request_ct_(0),
        actual_ws_request_ct_(0),
        expected_ws_connected_ct_(0),
        actual_ws_connected_ct_(0),
        expected_ws_message_ct_(0),
        actual_ws_message_ct_(0) {
    EXPECT_FALSE(destroy_callback_.is_null());
  }

  virtual ~TestServerHandler() {
    EXPECT_UI_THREAD();

    if (!http_request_handler_list_.empty()) {
      HttpRequestHandlerList::const_iterator it =
          http_request_handler_list_.begin();
      for (; it != http_request_handler_list_.end(); ++it)
        delete *it;
    }

    if (!ws_request_handler_list_.empty()) {
      WsRequestHandlerList::const_iterator it =
          ws_request_handler_list_.begin();
      for (; it != ws_request_handler_list_.end(); ++it)
        delete *it;
    }

    std::move(destroy_callback_).Run();
  }

  // Must be called before CreateServer().
  void SetExpectedConnectionCount(int expected) {
    EXPECT_FALSE(initialized_);
    expected_connection_ct_ = expected;
  }

  // Must be called before CreateServer().
  void AddHttpRequestHandler(
      std::unique_ptr<HttpRequestHandler> request_handler) {
    EXPECT_FALSE(initialized_);
    EXPECT_TRUE(request_handler);
    http_request_handler_list_.push_back(request_handler.release());
  }

  // Must be called before CreateServer().
  void SetExpectedHttpRequestCount(int expected) {
    EXPECT_FALSE(initialized_);
    expected_http_request_ct_ = expected;
  }

  // Must be called before CreateServer().
  void AddWsRequestHandler(std::unique_ptr<WsRequestHandler> request_handler) {
    EXPECT_FALSE(initialized_);
    EXPECT_TRUE(request_handler);
    ws_request_handler_list_.push_back(request_handler.release());
  }

  // Must be called before CreateServer().
  void SetExpectedWsRequestCount(int expected) {
    EXPECT_FALSE(initialized_);
    expected_ws_request_ct_ = expected;
  }

  // Must be called before CreateServer().
  void SetExpectedWsConnectedCount(int expected) {
    EXPECT_FALSE(initialized_);
    expected_ws_connected_ct_ = expected;
  }

  // Must be called before CreateServer().
  void SetExpectedWsMessageCount(int expected) {
    EXPECT_FALSE(initialized_);
    expected_ws_message_ct_ = expected;
  }

  void CreateServer() {
    EXPECT_FALSE(initialized_);
    initialized_ = true;
    CefServer::CreateServer(kTestServerAddress, kTestServerPort, 10, this);
  }

  // Results in a call to VerifyResults() and eventual execution of the
  // |destroy_callback|.
  void ShutdownServer() {
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

    RunStartCallback();
  }

  void OnServerDestroyed(CefRefPtr<CefServer> server) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_FALSE(server->IsRunning());
    EXPECT_FALSE(server->HasConnection());

    EXPECT_FALSE(got_server_destroyed_);
    got_server_destroyed_.yes();

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

    ConnectionIdSet::iterator it2 = ws_connection_id_set_.find(connection_id);
    if (it2 != ws_connection_id_set_.end())
      ws_connection_id_set_.erase(it2);

    if (connection_id_set_.empty()) {
      EXPECT_TRUE(ws_connection_id_set_.empty());
      EXPECT_FALSE(server->HasConnection());
    }
  }

  void OnHttpRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     const CefString& client_address,
                     CefRefPtr<CefRequest> request) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_TRUE(VerifyConnection(connection_id));
    EXPECT_FALSE(client_address.empty());
    EXPECT_TRUE(VerifyRequest(request, false));

    bool handled = false;
    HttpRequestHandlerList::const_iterator it =
        http_request_handler_list_.begin();
    for (; it != http_request_handler_list_.end(); ++it) {
      handled =
          (*it)->HandleRequest(server, connection_id, client_address, request);
      if (handled)
        break;
    }
    EXPECT_TRUE(handled) << "missing HttpRequestHandler for "
                         << request->GetURL().ToString();

    actual_http_request_ct_++;
  }

  void OnWebSocketRequest(CefRefPtr<CefServer> server,
                          int connection_id,
                          const CefString& client_address,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_TRUE(VerifyConnection(connection_id));
    EXPECT_FALSE(client_address.empty());
    EXPECT_TRUE(VerifyRequest(request, true));

    EXPECT_TRUE(ws_connection_id_set_.find(connection_id) ==
                ws_connection_id_set_.end());
    ws_connection_id_set_.insert(connection_id);

    bool handled = false;
    WsRequestHandlerList::const_iterator it = ws_request_handler_list_.begin();
    for (; it != ws_request_handler_list_.end(); ++it) {
      handled = (*it)->HandleRequest(server, connection_id, client_address,
                                     request, callback);
      if (handled)
        break;
    }
    EXPECT_TRUE(handled) << "missing WsRequestHandler for "
                         << request->GetURL().ToString();

    actual_ws_request_ct_++;
  }

  void OnWebSocketConnected(CefRefPtr<CefServer> server,
                            int connection_id) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_TRUE(VerifyConnection(connection_id));

    EXPECT_TRUE(ws_connection_id_set_.find(connection_id) !=
                ws_connection_id_set_.end());

    bool handled = false;
    WsRequestHandlerList::const_iterator it = ws_request_handler_list_.begin();
    for (; it != ws_request_handler_list_.end(); ++it) {
      handled = (*it)->HandleConnected(server, connection_id);
      if (handled)
        break;
    }
    EXPECT_TRUE(handled) << "missing WsRequestHandler for " << connection_id;

    actual_ws_connected_ct_++;
  }

  void OnWebSocketMessage(CefRefPtr<CefServer> server,
                          int connection_id,
                          const void* data,
                          size_t data_size) override {
    EXPECT_TRUE(VerifyServer(server));
    EXPECT_TRUE(VerifyConnection(connection_id));
    EXPECT_TRUE(data);
    EXPECT_GT(data_size, 0U);

    EXPECT_TRUE(ws_connection_id_set_.find(connection_id) !=
                ws_connection_id_set_.end());

    bool handled = false;
    WsRequestHandlerList::const_iterator it = ws_request_handler_list_.begin();
    for (; it != ws_request_handler_list_.end(); ++it) {
      handled = (*it)->HandleMessage(server, connection_id, data, data_size);
      if (handled)
        break;
    }
    EXPECT_TRUE(handled) << "missing WsRequestHandler for " << connection_id;

    actual_ws_message_ct_++;
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

  bool VerifyRequest(CefRefPtr<CefRequest> request, bool is_websocket) {
    V_DECLARE();

    V_EXPECT_FALSE(request->GetMethod().empty());

    const std::string& url = request->GetURL();
    V_EXPECT_FALSE(url.empty());
    const std::string& address = server_->GetAddress();
    V_EXPECT_TRUE(url.find((is_websocket ? "ws://" : "http://") + address) == 0)
        << "url " << url << " address " << address;

    CefRefPtr<CefPostData> post_data = request->GetPostData();
    if (post_data) {
      CefPostData::ElementVector elements;
      post_data->GetElements(elements);
      V_EXPECT_TRUE(elements.size() == 1);
      V_EXPECT_TRUE(elements[0]->GetBytesCount() > 0U);
    }

    V_RETURN();
  }

  void VerifyResults() {
    EXPECT_TRUE(RunningOnServerThread());

    EXPECT_TRUE(got_server_created_);
    EXPECT_TRUE(got_server_destroyed_);
    EXPECT_TRUE(connection_id_set_.empty());
    EXPECT_EQ(expected_connection_ct_, actual_connection_ct_);

    // HTTP

    EXPECT_EQ(expected_http_request_ct_, actual_http_request_ct_);

    if (!http_request_handler_list_.empty()) {
      HttpRequestHandlerList::const_iterator it =
          http_request_handler_list_.begin();
      for (; it != http_request_handler_list_.end(); ++it) {
        EXPECT_TRUE((*it)->VerifyResults())
            << "HttpRequestHandler for " << (*it)->ToString();
      }
    }

    // WebSocket

    EXPECT_EQ(expected_ws_request_ct_, actual_ws_request_ct_);
    EXPECT_EQ(expected_ws_connected_ct_, actual_ws_connected_ct_);
    EXPECT_EQ(expected_ws_message_ct_, actual_ws_message_ct_);

    if (!ws_request_handler_list_.empty()) {
      WsRequestHandlerList::const_iterator it =
          ws_request_handler_list_.begin();
      for (; it != ws_request_handler_list_.end(); ++it) {
        EXPECT_TRUE((*it)->VerifyResults())
            << "WsRequestHandler for " << (*it)->ToString();
      }
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
    std::move(start_callback_).Run();
  }

  CefRefPtr<CefServer> server_;
  CefRefPtr<CefTaskRunner> server_runner_;
  bool initialized_;

  // After initialization only accessed on the UI thread.
  base::OnceClosure start_callback_;
  base::OnceClosure destroy_callback_;

  // After initialization the below members are only accessed on the server
  // thread.

  TrackCallback got_server_created_;
  TrackCallback got_server_destroyed_;

  typedef std::set<int> ConnectionIdSet;
  ConnectionIdSet connection_id_set_;

  int expected_connection_ct_;
  int actual_connection_ct_;

  // HTTP

  typedef std::list<HttpRequestHandler*> HttpRequestHandlerList;
  HttpRequestHandlerList http_request_handler_list_;

  int expected_http_request_ct_;
  int actual_http_request_ct_;

  // WebSocket

  typedef std::list<WsRequestHandler*> WsRequestHandlerList;
  WsRequestHandlerList ws_request_handler_list_;

  ConnectionIdSet ws_connection_id_set_;

  int expected_ws_request_ct_;
  int actual_ws_request_ct_;

  int expected_ws_connected_ct_;
  int actual_ws_connected_ct_;

  int expected_ws_message_ct_;
  int actual_ws_message_ct_;

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
    virtual ~RequestRunner() {}

    // Create the server-side handler for the request.
    virtual std::unique_ptr<TestServerHandler::HttpRequestHandler>
    CreateHttpRequestHandler() = 0;

    // Run the request and execute |complete_callback| on completion.
    virtual void RunRequest(base::OnceClosure complete_callback) = 0;

    virtual bool VerifyResults() = 0;
    virtual std::string ToString() = 0;
  };

  // If |parallel_requests| is true all requests will be run at the same time,
  // otherwise one request will be run at a time.
  HttpTestRunner(bool parallel_requests)
      : parallel_requests_(parallel_requests),
        initialized_(false),
        next_request_id_(0) {}

  virtual ~HttpTestRunner() {
    if (destroy_event_)
      destroy_event_->Signal();
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

    handler_->SetExpectedConnectionCount(
        static_cast<int>(request_runner_map_.size()));
    handler_->SetExpectedHttpRequestCount(
        static_cast<int>(request_runner_map_.size()));

    handler_->CreateServer();

    SetTestTimeout(kTestTimeout);
  }

  void OnServerStarted() {
    EXPECT_UI_THREAD();
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
          base::BindOnce(&HttpTestRunner::OnRequestComplete, this, it->first));
    }
  }

  // Run one request at a time.
  void RunNextRequest() {
    RequestRunnerMap::const_iterator it = request_runner_map_.begin();
    it->second->RunRequest(
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
    if (ui_thread_helper_)
      ui_thread_helper_.reset();

    // Signal test completion.
    run_event_->Signal();
  }

  TestHandler::UIThreadHelper* GetUIThreadHelper() {
    EXPECT_UI_THREAD();
    if (!ui_thread_helper_)
      ui_thread_helper_.reset(new TestHandler::UIThreadHelper());
    return ui_thread_helper_.get();
  }

  void SetTestTimeout(int timeout_ms) {
    EXPECT_UI_THREAD();
    if (CefCommandLine::GetGlobalCommandLine()->HasSwitch(
            "disable-test-timeout")) {
      return;
    }

    // Use a weak reference to |this| via UIThreadHelper so that the
    // test runner can be destroyed before the timeout expires.
    GetUIThreadHelper()->PostDelayedTask(
        base::BindOnce(&HttpTestRunner::OnTestTimeout, base::Unretained(this),
                       timeout_ms),
        timeout_ms);
  }

  void OnTestTimeout(int timeout_ms) {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(false) << "Test timed out after " << timeout_ms << "ms";
    DestroyTest();
  }

  bool parallel_requests_;
  CefRefPtr<CefWaitableEvent> run_event_;
  CefRefPtr<CefWaitableEvent> destroy_event_;
  CefRefPtr<TestServerHandler> handler_;
  bool initialized_;

  // After initialization the below members are only accessed on the UI thread.

  int next_request_id_;

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

  explicit HttpServerResponse(Type response_type)
      : type(response_type), no_content_length(false) {}

  Type type;

  // Used with 200 and CUSTOM response type.
  std::string content;
  std::string content_type;

  // Used with 500 response type.
  std::string error_message;

  // Used with CUSTOM response type.
  int response_code;
  CefServer::HeaderMap extra_headers;
  bool no_content_length;
};

void SendHttpServerResponse(CefRefPtr<CefServer> server,
                            int connection_id,
                            const HttpServerResponse& response) {
  EXPECT_TRUE(server->GetTaskRunner()->BelongsToCurrentThread());
  EXPECT_TRUE(server->IsValidConnection(connection_id));

  switch (response.type) {
    case HttpServerResponse::TYPE_200:
      EXPECT_TRUE(!response.content_type.empty());
      server->SendHttp200Response(connection_id, response.content_type,
                                  response.content.data(),
                                  response.content.size());
      break;
    case HttpServerResponse::TYPE_404:
      server->SendHttp404Response(connection_id);
      break;
    case HttpServerResponse::TYPE_500:
      server->SendHttp500Response(connection_id, response.error_message);
      break;
    case HttpServerResponse::TYPE_CUSTOM:
      EXPECT_TRUE(!response.content_type.empty());
      server->SendHttpResponse(
          connection_id, response.response_code, response.content_type,
          response.no_content_length
              ? -1
              : static_cast<int64>(response.content.size()),
          response.extra_headers);
      if (!response.content.empty()) {
        server->SendRawData(connection_id, response.content.data(),
                            response.content.size());
      }
      if (!response.content.empty() ||
          (response.content.empty() && response.no_content_length)) {
        server->CloseConnection(connection_id);
      }
      break;
  }

  // All of the above responses should close the connection.
  EXPECT_FALSE(server->IsValidConnection(connection_id));
}

std::string GetHeaderValue(const CefServer::HeaderMap& header_map,
                           const std::string& header_name) {
  CefServer::HeaderMap::const_iterator it = header_map.find(header_name);
  if (it != header_map.end())
    return it->second;
  return std::string();
}

void VerifyHttpServerResponse(const HttpServerResponse& expected_response,
                              CefRefPtr<CefResponse> response,
                              const std::string& data) {
  CefServer::HeaderMap header_map;
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
      if (expected_response.no_content_length) {
        EXPECT_TRUE(GetHeaderValue(header_map, "Content-Length").empty());
      } else {
        EXPECT_FALSE(GetHeaderValue(header_map, "Content-Length").empty());
      }
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
  request->SetURL(GetTestServerOrigin(false) + "/" + path);
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

  if (!extra_headers.empty())
    header_map.insert(extra_headers.begin(), extra_headers.end());
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
        actual_request_ct_(0),
        response_(response) {}

  bool HandleRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     const CefString& client_address,
                     CefRefPtr<CefRequest> request) override {
    if (request->GetURL() == expected_request_->GetURL() &&
        request->GetMethod() == expected_request_->GetMethod()) {
      TestRequestEqual(expected_request_, request, true);
      actual_request_ct_++;

      SendHttpServerResponse(server, connection_id, response_);
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
  int actual_request_ct_;
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
                        int64 current,
                        int64 total) override {}

  void OnDownloadProgress(CefRefPtr<CefURLRequest> request,
                          int64 current,
                          int64 total) override {}

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
    if (with_content)
      response.content = "<html>200 response content</html>";
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
      bool with_content = true,
      bool with_content_length = true) {
    CefRequest::HeaderMap request_headers;
    request_headers.insert(std::make_pair("x-request-custom1", "My Value A"));
    request_headers.insert(std::make_pair("x-request-custom2", "My Value B"));
    CefRefPtr<CefRequest> request = CreateTestServerRequest(
        path, "POST", "foo=bar&choo=too", "application/x-www-form-urlencoded",
        request_headers);
    request->SetReferrer("http://tests/referer.html", REFERRER_POLICY_DEFAULT);

    HttpServerResponse response(HttpServerResponse::TYPE_CUSTOM);
    response.response_code = 202;
    if (with_content)
      response.content = "BlahBlahBlah";
    if (!with_content_length)
      response.no_content_length = true;
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

  void RunRequest(base::OnceClosure complete_callback) override {
    EXPECT_UI_THREAD();

    EXPECT_FALSE(got_run_request_);
    got_run_request_.yes();

    complete_callback_ = std::move(complete_callback);

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
    if (error == ERR_NONE)
      VerifyHttpServerResponse(response_, response, data);

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
TEST(ServerTest, HttpSingle200) {
  CefRefPtr<HttpTestRunner> runner = new HttpTestRunner(false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP 200 request with no content.
TEST(ServerTest, HttpSingle200NoContent) {
  CefRefPtr<HttpTestRunner> runner = new HttpTestRunner(false);
  runner->AddRequestRunner(
      StaticHttpRequestRunner::Create200("200.html", false));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP 404 request.
TEST(ServerTest, HttpSingle404) {
  CefRefPtr<HttpTestRunner> runner = new HttpTestRunner(false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create404("404.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP 500 request.
TEST(ServerTest, HttpSingle500) {
  CefRefPtr<HttpTestRunner> runner = new HttpTestRunner(false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create500("500.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP custom request.
TEST(ServerTest, HttpSingleCustom) {
  CefRefPtr<HttpTestRunner> runner = new HttpTestRunner(false);
  runner->AddRequestRunner(StaticHttpRequestRunner::CreateCustom("202.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP custom request with no content.
TEST(ServerTest, HttpSingleCustomNoContent) {
  CefRefPtr<HttpTestRunner> runner = new HttpTestRunner(false);
  runner->AddRequestRunner(
      StaticHttpRequestRunner::CreateCustom("202.html", false));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP custom request with no Content-Length
// header.
TEST(ServerTest, HttpSingleCustomNoContentLength) {
  CefRefPtr<HttpTestRunner> runner = new HttpTestRunner(false);
  runner->AddRequestRunner(
      StaticHttpRequestRunner::CreateCustom("202.html", true, false));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of a single HTTP custom request with no content and no
// Content-Length header.
TEST(ServerTest, HttpSingleCustomNoContentAndNoLength) {
  CefRefPtr<HttpTestRunner> runner = new HttpTestRunner(false);
  runner->AddRequestRunner(
      StaticHttpRequestRunner::CreateCustom("202.html", false, false));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of multiple HTTP requests in parallel.
TEST(ServerTest, HttpMultipleParallel200) {
  CefRefPtr<HttpTestRunner> runner = new HttpTestRunner(true);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200a.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200b.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200c.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of multiple HTTP requests in serial.
TEST(ServerTest, HttpMultipleSerial200) {
  CefRefPtr<HttpTestRunner> runner = new HttpTestRunner(false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200a.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200b.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200c.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of multiple HTTP requests in parallel.
TEST(ServerTest, HttpMultipleParallelMixed) {
  CefRefPtr<HttpTestRunner> runner = new HttpTestRunner(true);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create404("404.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create500("500.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::CreateCustom("202.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

// Verify handling of multiple HTTP requests in serial.
TEST(ServerTest, HttpMultipleSerialMixed) {
  CefRefPtr<HttpTestRunner> runner = new HttpTestRunner(false);
  runner->AddRequestRunner(StaticHttpRequestRunner::Create200("200.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create404("404.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::Create500("500.html"));
  runner->AddRequestRunner(StaticHttpRequestRunner::CreateCustom("202.html"));
  runner->ExecuteTest();
  ReleaseAndWaitForDestructor(runner);
}

namespace {

// WEBSOCKET TESTS

const char kWebSocketUrl[] = "http://tests-display/websocket.html";
const char kDoneMsgPrefix[] = "done:";

class WebSocketTestHandler : public RoutingTestHandler {
 public:
  WebSocketTestHandler() {}

  void RunTest() override {
    handler_ = new TestServerHandler(
        base::BindOnce(&WebSocketTestHandler::OnServerStarted, this),
        base::BindOnce(&WebSocketTestHandler::OnServerDestroyed, this));
    OnHandlerCreated(handler_);

    handler_->CreateServer();

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    const std::string& request_str = request.ToString();
    if (request_str.find(kDoneMsgPrefix) == 0) {
      EXPECT_FALSE(got_done_message_);
      got_done_message_.yes();
      OnDoneMessage(request_str.substr(strlen(kDoneMsgPrefix)));
      DestroyTestIfDone();
      return true;
    }
    return false;
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_server_started_);
    EXPECT_TRUE(got_done_message_);
    EXPECT_TRUE(got_server_destroyed_);

    TestHandler::DestroyTest();
  }

 protected:
  // Returns the HTML/JS for the client.
  virtual std::string GetClientHtml() = 0;

  // Called after the server handler is created to set test expectations.
  virtual void OnHandlerCreated(CefRefPtr<TestServerHandler> handler) = 0;

  // Returns the JS to execute when the test is done.
  std::string GetDoneJS(const std::string& result) {
    return "window.testQuery({request:'" + std::string(kDoneMsgPrefix) +
           "' + " + result + "});";
  }

  // Called with the result from the done message.
  virtual void OnDoneMessage(const std::string& result) = 0;

  void ShutdownServer() {
    EXPECT_TRUE(handler_);
    handler_->ShutdownServer();
    handler_ = nullptr;
  }

 private:
  void OnServerStarted() {
    EXPECT_UI_THREAD();
    EXPECT_FALSE(got_server_started_);
    got_server_started_.yes();

    // Add the WebSocket client code.
    AddResource(kWebSocketUrl, GetClientHtml(), "text/html");

    // Create the browser.
    CreateBrowser(kWebSocketUrl);
  }

  void OnServerDestroyed() {
    EXPECT_UI_THREAD();
    EXPECT_FALSE(got_server_destroyed_);
    got_server_destroyed_.yes();
    DestroyTestIfDone();
  }

  void DestroyTestIfDone() {
    if (got_server_destroyed_ && got_done_message_) {
      // Allow the call stack to unwind.
      CefPostTask(TID_UI,
                  base::BindOnce(&WebSocketTestHandler::DestroyTest, this));
    }
  }

  CefRefPtr<TestServerHandler> handler_;

  TrackCallback got_server_started_;
  TrackCallback got_done_message_;
  TrackCallback got_server_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketTestHandler);
};

// WebSocket request handler that echoes each message sent.
class EchoWebSocketRequestHandler : public TestServerHandler::WsRequestHandler {
 public:
  explicit EchoWebSocketRequestHandler(int expected_message_ct)
      : expected_message_ct_(expected_message_ct), actual_message_ct_(0) {}

  std::string GetWebSocketUrl() { return GetTestServerOrigin(true) + "/echo"; }

  bool HandleRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     const CefString& client_address,
                     CefRefPtr<CefRequest> request,
                     CefRefPtr<CefCallback> callback) override {
    EXPECT_STREQ(GetWebSocketUrl().c_str(),
                 request->GetURL().ToString().c_str());

    callback->Continue();
    return true;
  }

  bool HandleConnected(CefRefPtr<CefServer> server,
                       int connection_id) override {
    return true;
  }

  bool HandleMessage(CefRefPtr<CefServer> server,
                     int connection_id,
                     const void* data,
                     size_t data_size) override {
    actual_message_ct_++;

    // Echo the message back to the sender.
    server->SendWebSocketMessage(connection_id, data, data_size);

    return true;
  }

  bool VerifyResults() override {
    EXPECT_EQ(expected_message_ct_, actual_message_ct_);
    return expected_message_ct_ == actual_message_ct_;
  }

  std::string ToString() override { return "EchoRequestHandler"; }

 private:
  int expected_message_ct_;
  int actual_message_ct_;

  DISALLOW_COPY_AND_ASSIGN(EchoWebSocketRequestHandler);
};

class EchoWebSocketTestHandler : public WebSocketTestHandler {
 public:
  // Create |connection_ct| connections and send |message_ct| messages to each
  // connection. If |in_parallel| is true the connections will be created in
  // parallel.
  EchoWebSocketTestHandler(int connection_ct, int message_ct, bool in_parallel)
      : connection_ct_(connection_ct),
        message_ct_(message_ct),
        in_parallel_(in_parallel) {}

  std::string GetClientHtml() override {
    std::stringstream ss;
    ss << connection_ct_;
    std::string cct_str = ss.str();
    ss.str("");
    ss << message_ct_;
    std::string mct_str = ss.str();

    // clang-format off
    return
        "<html><body><script>\n"

        // Input variables.
        "var url = '" + ws_url_ +"';\n"
        "var expected_connection_ct = " + cct_str + ";\n"
        "var expected_message_ct = " + mct_str + ";\n"
        "var in_parallel = " + (in_parallel_ ? "true" : "false") + ";\n"
        "var complete_callback = function() { " +
            GetDoneJS("complete_message_ct") + " }\n"

        // Result variables.
        "var complete_connection_ct = 0;\n"
        "var complete_message_ct = 0;\n"

        // Send the next message on the connection asynchronously, or close the
        // connection if all messages have been sent.
        "function sendNextMessage(ws, connection_id, message_id) {\n"
        "  if (message_id < expected_message_ct) {\n"
        "    setTimeout(function() {\n"
        "      ws.send('message:' + connection_id + ':' + message_id);\n"
        "    }, 1);\n"
        "  } else {\n"
        "    ws.close();\n"
        "  }\n"
        "}\n"

        // Handle a received message.
        "function onMessage(ws, connection_id, data) {\n"
        "  var parts = data.split(':');\n"
        "  if (parts.length == 3 && parts[0] == 'message') {\n"
        "    var cid = parseInt(parts[1]);\n"
        "    var mid = parseInt(parts[2]);\n"
        "    if (cid == connection_id) {\n"
        "      complete_message_ct++;\n"
        "      sendNextMessage(ws, connection_id, mid + 1);\n"
        "    } else {\n"
        "      console.log('Connection id mismatch; expected ' +\n"
        "                  connection_id + ', actual ' + cid);\n"
        "    }\n"
        "  } else {\n"
        "    console.log('Unexpected message format: ' + data);\n"
        "  }\n"
        "}\n"

        // Handle socket closed. If all messages have been sent on all
        // connections then complete the test.
        "function onClose(ws) {\n"
        "  if (++complete_connection_ct == expected_connection_ct) {\n"
        "    complete_callback();\n"
        "  } else if (!in_parallel) {\n"
        "    startConnection(complete_connection_ct);\n"
        "  }\n"
        "}\n"

        // Start a new WebSocket connection.
        "function startConnection(connection_id) {\n"
        "  var ws = new WebSocket(url);\n"
        "  ws.onopen = function() {\n"
        "    sendNextMessage(ws, connection_id, 0);\n"
        "  };\n"
        "  ws.onmessage = function(event) {\n"
        "    onMessage(ws, connection_id, event.data);\n"
        "  };\n"
        "  ws.onclose = function() { onClose(ws); };\n"
        "}\n"

        // JS entry point.
        "if (in_parallel) {\n"
        "  for (var i = 0; i < expected_connection_ct; ++i) {\n"
        "    startConnection(i);\n"
        "  }\n"
        "} else {\n"
        "  startConnection(0);\n"
        "}\n"

        "</script>WebSocket Test</body></html>";
    // clang-format on
  }

  void OnHandlerCreated(CefRefPtr<TestServerHandler> handler) override {
    handler->SetExpectedConnectionCount(connection_ct_);
    handler->SetExpectedWsRequestCount(connection_ct_);
    handler->SetExpectedWsConnectedCount(connection_ct_);
    handler->SetExpectedWsMessageCount(connection_ct_ * message_ct_);

    auto echo_handler = std::make_unique<EchoWebSocketRequestHandler>(
        connection_ct_ * message_ct_);
    ws_url_ = echo_handler->GetWebSocketUrl();
    handler->AddWsRequestHandler(std::move(echo_handler));
  }

  void OnDoneMessage(const std::string& result) override {
    const int complete_message_ct = atoi(result.c_str());
    EXPECT_EQ(connection_ct_ * message_ct_, complete_message_ct);
    ShutdownServer();
  }

 private:
  int connection_ct_;
  int message_ct_;
  bool in_parallel_;
  std::string ws_url_;

  IMPLEMENT_REFCOUNTING(EchoWebSocketTestHandler);
  DISALLOW_COPY_AND_ASSIGN(EchoWebSocketTestHandler);
};

}  // namespace

// Test handling of a single connection with a single message.
TEST(ServerTest, WebSocketSingleConnectionSingleMessage) {
  CefRefPtr<EchoWebSocketTestHandler> handler =
      new EchoWebSocketTestHandler(1, 1, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test handling of a single connection with multiple messages.
TEST(ServerTest, WebSocketSingleConnectionMultipleMessages) {
  CefRefPtr<EchoWebSocketTestHandler> handler =
      new EchoWebSocketTestHandler(1, 5, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test handling of multiple connections and multiple messages in parallel.
TEST(ServerTest, WebSocketMultipleConnectionsMultipleMessagesInParallel) {
  CefRefPtr<EchoWebSocketTestHandler> handler =
      new EchoWebSocketTestHandler(4, 6, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test handling of multiple connections and multiple messages in serial.
TEST(ServerTest, WebSocketMultipleConnectionsMultipleMessagesInSerial) {
  CefRefPtr<EchoWebSocketTestHandler> handler =
      new EchoWebSocketTestHandler(4, 6, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
