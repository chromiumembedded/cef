// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SERVER_IMPL_H_
#define CEF_LIBCEF_BROWSER_SERVER_IMPL_H_
#pragma once

#include <map>
#include <memory>

#include "include/cef_server.h"

#include "base/task/single_thread_task_runner.h"
#include "net/server/http_server.h"

namespace base {
class Thread;
}

class CefServerImpl : public CefServer, public net::HttpServer::Delegate {
 public:
  explicit CefServerImpl(CefRefPtr<CefServerHandler> handler);

  CefServerImpl(const CefServerImpl&) = delete;
  CefServerImpl& operator=(const CefServerImpl&) = delete;

  void Start(const std::string& address, uint16_t port, int backlog);

  // CefServer methods:
  CefRefPtr<CefTaskRunner> GetTaskRunner() override;
  void Shutdown() override;
  bool IsRunning() override;
  CefString GetAddress() override;
  bool HasConnection() override;
  bool IsValidConnection(int connection_id) override;
  void SendHttp200Response(int connection_id,
                           const CefString& content_type,
                           const void* data,
                           size_t data_size) override;
  void SendHttp404Response(int connection_id) override;
  void SendHttp500Response(int connection_id,
                           const CefString& error_message) override;
  void SendHttpResponse(int connection_id,
                        int response_code,
                        const CefString& content_type,
                        int64_t content_length,
                        const HeaderMap& extra_headers) override;
  void SendRawData(int connection_id,
                   const void* data,
                   size_t data_size) override;
  void CloseConnection(int connection_id) override;
  void SendWebSocketMessage(int connection_id,
                            const void* data,
                            size_t data_size) override;

  void ContinueWebSocketRequest(int connection_id,
                                const net::HttpServerRequestInfo& request_info,
                                bool allow);

 private:
  void SendHttp200ResponseInternal(int connection_id,
                                   const CefString& content_type,
                                   std::unique_ptr<std::string> data);
  void SendRawDataInternal(int connection_id,
                           std::unique_ptr<std::string> data);
  void SendWebSocketMessageInternal(int connection_id,
                                    std::unique_ptr<std::string> data);

  // HttpServer::Delegate methods:
  void OnConnect(int connection_id) override;
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& request_info) override;
  void OnWebSocketRequest(
      int connection_id,
      const net::HttpServerRequestInfo& request_info) override;
  void OnWebSocketMessage(int connection_id, std::string data) override;
  void OnClose(int connection_id) override;

  void StartOnUIThread(const std::string& address, uint16_t port, int backlog);
  void StartOnHandlerThread(const std::string& address,
                            uint16_t port,
                            int backlog);

  void ShutdownOnHandlerThread();
  void ShutdownOnUIThread();

  bool ValidateServer() const;

  struct ConnectionInfo;
  ConnectionInfo* CreateConnectionInfo(int connection_id);
  ConnectionInfo* GetConnectionInfo(int connection_id) const;
  void RemoveConnectionInfo(int connection_id);

  bool CurrentlyOnHandlerThread() const;

  // Safe to access from any thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::string address_;

  // Only accessed on the UI thread.
  std::unique_ptr<base::Thread> thread_;

  // Only accessed on the server thread.
  CefRefPtr<CefServerHandler> handler_;
  std::unique_ptr<net::HttpServer> server_;

  // Map of connection_id to ConnectionInfo.
  using ConnectionInfoMap = std::map<int, std::unique_ptr<ConnectionInfo>>;
  ConnectionInfoMap connection_info_map_;

  IMPLEMENT_REFCOUNTING(CefServerImpl);
};

#endif  // CEF_LIBCEF_BROWSER_SERVER_IMPL_H_
