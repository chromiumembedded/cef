// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/test_server.h"

#include <vector>

#include "include/cef_server.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace test_server {

// Must use a different port than server_unittest.cc.
const char kServerAddress[] = "127.0.0.1";
const uint16 kServerPort = 8098;
const char kServerScheme[] = "http";
const char kServerOrigin[] = "http://127.0.0.1:8098";
const char kIncompleteDoNotSendData[] = "DO NOT SEND";

namespace {

class ServerManager;
ServerManager* g_manager = nullptr;

// True if Stop() has been called.
bool g_stopping = false;

// Created on the UI thread and called on the dedicated server thread.
class ServerHandler : public CefServerHandler {
 public:
  ServerHandler() {
    CefServer::CreateServer(kServerAddress, kServerPort, 10, this);
  }

  ~ServerHandler() override {
    EXPECT_FALSE(server_);
    NotifyServerHandlerDeleted();
  }

  void Shutdown() { server_->Shutdown(); }

 protected:
  // CefServerHandler methods:
  void OnServerCreated(CefRefPtr<CefServer> server) override {
    server_ = server;
    NotifyServerCreated(kServerOrigin);
  }

  void OnServerDestroyed(CefRefPtr<CefServer> server) override {
    server_ = nullptr;
    NotifyServerDestroyed();
  }

  void OnClientConnected(CefRefPtr<CefServer> server,
                         int connection_id) override {
    EXPECT_TRUE(server->HasConnection());
    EXPECT_TRUE(server->IsValidConnection(connection_id));
  }

  void OnClientDisconnected(CefRefPtr<CefServer> server,
                            int connection_id) override {
    EXPECT_FALSE(server->IsValidConnection(connection_id));
  }

  void OnHttpRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     const CefString& client_address,
                     CefRefPtr<CefRequest> request) override {
    NotifyHttpRequest(server, connection_id, client_address, request);
  }

  void OnWebSocketRequest(CefRefPtr<CefServer> server,
                          int connection_id,
                          const CefString& client_address,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefCallback> callback) override {}
  void OnWebSocketConnected(CefRefPtr<CefServer> server,
                            int connection_id) override {}
  void OnWebSocketMessage(CefRefPtr<CefServer> server,
                          int connection_id,
                          const void* data,
                          size_t data_size) override {}

 private:
  static void NotifyServerCreated(const std::string& server_origin);
  static void NotifyServerDestroyed();
  static void NotifyServerHandlerDeleted();
  static void NotifyHttpRequest(CefRefPtr<CefServer> server,
                                int connection_id,
                                const CefString& client_address,
                                CefRefPtr<CefRequest> request);

  CefRefPtr<CefServer> server_;

  IMPLEMENT_REFCOUNTING(ServerHandler);
  DISALLOW_COPY_AND_ASSIGN(ServerHandler);
};

// Only accessed on the UI thread. Deletes itself after the server is stopped.
class ServerManager {
 public:
  ServerManager() {
    CEF_REQUIRE_UI_THREAD();
    EXPECT_FALSE(g_manager);
    g_manager = this;
  }

  ~ServerManager() {
    CEF_REQUIRE_UI_THREAD();
    EXPECT_TRUE(observer_list_.empty());
    EXPECT_TRUE(start_callback_list_.empty());
    EXPECT_TRUE(stop_callback_.is_null());

    g_manager = nullptr;
  }

  void Start(StartDoneCallback callback) {
    CEF_REQUIRE_UI_THREAD();
    if (!origin_.empty()) {
      // The server is already running.
      std::move(callback).Run(origin_);
      return;
    }

    // If tests run in parallel, and the server is starting, then there may be
    // multiple pending callbacks.
    start_callback_list_.push_back(std::move(callback));

    // Only create the handler a single time.
    if (!handler_) {
      handler_ = new ServerHandler();
    }
  }

  void Stop(DoneCallback callback) {
    CEF_REQUIRE_UI_THREAD();
    if (!handler_) {
      // The server is not currently running.
      std::move(callback).Run();
      return;
    }

    // Only 1 stop callback supported.
    EXPECT_TRUE(stop_callback_.is_null());
    stop_callback_ = std::move(callback);

    handler_->Shutdown();
  }

  void AddObserver(Observer* observer) {
    CEF_REQUIRE_UI_THREAD();
    observer_list_.push_back(observer);
  }

  void RemoveObserver(Observer* observer) {
    CEF_REQUIRE_UI_THREAD();
    bool found = false;
    ObserverList::iterator it = observer_list_.begin();
    for (; it != observer_list_.end(); ++it) {
      if (*it == observer) {
        observer_list_.erase(it);
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }

  void NotifyServerCreated(const std::string& server_origin) {
    CEF_REQUIRE_UI_THREAD();

    EXPECT_TRUE(origin_.empty());
    origin_ = server_origin;

    for (auto& callback : start_callback_list_) {
      std::move(callback).Run(origin_);
    }
    start_callback_list_.clear();
  }

  void NotifyServerDestroyed() {
    CEF_REQUIRE_UI_THREAD();

    origin_.clear();
    handler_ = nullptr;
  }

  // All server-related objects have been torn down.
  void NotifyServerHandlerDeleted() {
    CEF_REQUIRE_UI_THREAD();

    EXPECT_FALSE(stop_callback_.is_null());
    std::move(stop_callback_).Run();

    delete this;
  }

  void NotifyHttpRequest(CefRefPtr<CefServer> server,
                         int connection_id,
                         const CefString& client_address,
                         CefRefPtr<CefRequest> request) {
    CEF_REQUIRE_UI_THREAD();

    // TODO(chrome-runtime): Debug why favicon requests don't always have the
    // correct resource type.
    const std::string& url = request->GetURL();
    if (request->GetResourceType() == RT_FAVICON ||
        url.find("/favicon.ico") != std::string::npos) {
      // We don't currently handle favicon requests.
      server->SendHttp404Response(connection_id);
      return;
    }

    EXPECT_FALSE(observer_list_.empty()) << url;

    // Use a copy in case |observer_list_| is modified during iteration.
    ObserverList list = observer_list_;

    bool handled = false;

    auto response_callback = base::BindRepeating(&ServerManager::SendResponse,
                                                 server, connection_id);

    ObserverList::const_iterator it = list.begin();
    for (; it != list.end(); ++it) {
      if ((*it)->OnHttpRequest(request, response_callback)) {
        handled = true;
        break;
      }
    }

    if (!handled) {
      server->SendHttp500Response(connection_id, "Unhandled request.");
    }
  }

 private:
  static void SendResponse(CefRefPtr<CefServer> server,
                           int connection_id,
                           CefRefPtr<CefResponse> response,
                           const std::string& response_data) {
    // Execute on the server thread because some methods require it.
    CefRefPtr<CefTaskRunner> task_runner = server->GetTaskRunner();
    if (!task_runner->BelongsToCurrentThread()) {
      task_runner->PostTask(CefCreateClosureTask(
          base::BindOnce(ServerManager::SendResponse, server, connection_id,
                         response, response_data)));
      return;
    }

    // No response should be sent yet.
    EXPECT_TRUE(server->IsValidConnection(connection_id));

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

  CefRefPtr<ServerHandler> handler_;
  std::string origin_;

  using StartDoneCallbackList = std::vector<StartDoneCallback>;
  StartDoneCallbackList start_callback_list_;

  DoneCallback stop_callback_;

  using ObserverList = std::vector<Observer*>;
  ObserverList observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ServerManager);
};

ServerManager* GetServerManager() {
  return g_manager;
}

ServerManager* GetOrCreateServerManager() {
  if (!g_manager) {
    new ServerManager();
    EXPECT_TRUE(g_manager);
  }
  return g_manager;
}

// static
void ServerHandler::NotifyServerCreated(const std::string& server_origin) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(ServerHandler::NotifyServerCreated,
                                       server_origin));
    return;
  }

  GetServerManager()->NotifyServerCreated(server_origin);
}

// static
void ServerHandler::NotifyServerDestroyed() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(ServerHandler::NotifyServerDestroyed));
    return;
  }

  GetServerManager()->NotifyServerDestroyed();
}

// static
void ServerHandler::NotifyServerHandlerDeleted() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(ServerHandler::NotifyServerHandlerDeleted));
    return;
  }

  GetServerManager()->NotifyServerHandlerDeleted();
}

// static
void ServerHandler::NotifyHttpRequest(CefRefPtr<CefServer> server,
                                      int connection_id,
                                      const CefString& client_address,
                                      CefRefPtr<CefRequest> request) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(ServerHandler::NotifyHttpRequest, server,
                                       connection_id, client_address, request));
    return;
  }

  GetServerManager()->NotifyHttpRequest(server, connection_id, client_address,
                                        request);
}

// May be created on any thread but will be destroyed on the UI thread.
class ObserverRegistration : public CefRegistration {
 public:
  explicit ObserverRegistration(Observer* const observer)
      : observer_(observer) {
    EXPECT_TRUE(observer_);
  }

  ~ObserverRegistration() override {
    CEF_REQUIRE_UI_THREAD();

    ServerManager* manager = GetServerManager();
    if (manager) {
      manager->RemoveObserver(observer_);
      observer_->OnUnregistered();
    }
  }

  void Initialize() {
    CEF_REQUIRE_UI_THREAD();
    GetOrCreateServerManager()->AddObserver(observer_);
    observer_->OnRegistered();
  }

 private:
  Observer* const observer_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(ObserverRegistration);
  DISALLOW_COPY_AND_ASSIGN(ObserverRegistration);
};

void InitializeRegistration(CefRefPtr<ObserverRegistration> registration,
                            DoneCallback callback) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(InitializeRegistration, registration,
                                       std::move(callback)));
    return;
  }

  EXPECT_FALSE(g_stopping);

  registration->Initialize();
  if (!callback.is_null())
    std::move(callback).Run();
}

}  // namespace

void Start(StartDoneCallback callback) {
  EXPECT_FALSE(callback.is_null());
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(Start, std::move(callback)));
    return;
  }

  EXPECT_FALSE(g_stopping);

  GetOrCreateServerManager()->Start(std::move(callback));
}

void Stop(DoneCallback callback) {
  EXPECT_FALSE(callback.is_null());
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(Stop, std::move(callback)));
    return;
  }

  // Stop will be called one time on test framework shutdown.
  EXPECT_FALSE(g_stopping);
  g_stopping = true;

  ServerManager* manager = GetServerManager();
  if (manager) {
    manager->Stop(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

CefRefPtr<CefRegistration> AddObserver(Observer* observer,
                                       DoneCallback callback) {
  EXPECT_TRUE(observer);
  CefRefPtr<ObserverRegistration> registration =
      new ObserverRegistration(observer);
  InitializeRegistration(registration, std::move(callback));
  return registration.get();
}

CefRefPtr<CefRegistration> AddObserverAndStart(Observer* observer,
                                               StartDoneCallback callback) {
  return AddObserver(observer, base::BindOnce(Start, std::move(callback)));
}

// ObserverHelper

ObserverHelper::ObserverHelper() : weak_ptr_factory_(this) {
  CEF_REQUIRE_UI_THREAD();
}

ObserverHelper::~ObserverHelper() {
  EXPECT_TRUE(state_ == State::NONE);
}

void ObserverHelper::Initialize() {
  CEF_REQUIRE_UI_THREAD();
  EXPECT_TRUE(state_ == State::NONE);
  state_ = State::INITIALIZING;
  registration_ =
      AddObserverAndStart(this, base::BindOnce(&ObserverHelper::OnStartDone,
                                               weak_ptr_factory_.GetWeakPtr()));
}

void ObserverHelper::Shutdown() {
  CEF_REQUIRE_UI_THREAD();
  EXPECT_TRUE(state_ == State::INITIALIZED);
  state_ = State::SHUTTINGDOWN;
  registration_ = nullptr;
}

void ObserverHelper::OnStartDone(const std::string& server_origin) {
  EXPECT_TRUE(state_ == State::INITIALIZING);
  state_ = State::INITIALIZED;
  OnInitialized(server_origin);
}

void ObserverHelper::OnRegistered() {
  EXPECT_TRUE(state_ == State::INITIALIZING);
}

void ObserverHelper::OnUnregistered() {
  EXPECT_TRUE(state_ == State::SHUTTINGDOWN);
  state_ = State::NONE;
  OnShutdown();
}

}  // namespace test_server
