// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_TEST_SERVER_H_
#define CEF_TESTS_CEFTESTS_TEST_SERVER_H_
#pragma once

#include <string>

#include "include/base/cef_callback.h"
#include "include/cef_registration.h"
#include "include/cef_request.h"
#include "include/cef_response.h"
#include "include/cef_server.h"

namespace test_server {

extern const char kServerAddress[];
extern const uint16 kServerPort;
extern const char kServerScheme[];
extern const char kServerOrigin[];

using DoneCallback = base::OnceClosure;

using StartDoneCallback =
    base::OnceCallback<void(const std::string& server_origin)>;

// Starts the server if it is not currently running, and executes |callback| on
// the UI thread. This method should be called by each test case that relies on
// the server.
void Start(StartDoneCallback callback);

// Stops the server if it is currently running, and executes |callback| on the
// UI thread. This method will be called by the test framework on shutdown.
void Stop(DoneCallback callback);

// Observer for CefServerHandler callbacks. Methods will be called on the UI
// thread.
class Observer {
 public:
  // Called when this Observer is registered.
  virtual void OnRegistered() = 0;

  // Called when this Observer is unregistered.
  virtual void OnUnregistered() = 0;

  // See CefServerHandler documentation for usage. Return true if the callback
  // was handled.
  virtual bool OnClientConnected(CefRefPtr<CefServer> server,
                                 int connection_id) {
    return false;
  }
  virtual bool OnClientDisconnected(CefRefPtr<CefServer> server,
                                    int connection_id) {
    return false;
  }
  virtual bool OnHttpRequest(CefRefPtr<CefServer> server,
                             int connection_id,
                             const CefString& client_address,
                             CefRefPtr<CefRequest> request) = 0;

 protected:
  virtual ~Observer() {}
};

// Add an observer for CefServerHandler callbacks. Remains registered until the
// returned CefRegistration object is destroyed. Registered observers will be
// executed in the order of registration until one returns true to indicate that
// it handled the callback. |callback| will be executed on the UI thread after
// registration is complete.
CefRefPtr<CefRegistration> AddObserver(Observer* observer,
                                       DoneCallback callback);

// Combination of AddObserver() followed by Start().
CefRefPtr<CefRegistration> AddObserverAndStart(Observer* observer,
                                               StartDoneCallback callback);

// Helper for sending a fully qualified response.
void SendResponse(CefRefPtr<CefServer> server,
                  int connection_id,
                  CefRefPtr<CefResponse> response,
                  const std::string& response_data);

// Helper for managing Observer registration and callbacks. Only used on the UI
// thread.
class ObserverHelper : Observer {
 public:
  ObserverHelper();
  ~ObserverHelper() override;

  // Initialize the registration. Results in a call to OnInitialized().
  void Initialize();

  // Shut down the registration. Results in a call to OnShutdown().
  void Shutdown();

  // Implement this method to start sending server requests after Initialize().
  virtual void OnInitialized(const std::string& server_origin) = 0;

  // Implement this method to continue the test after Shutdown().
  virtual void OnShutdown() = 0;

 private:
  void OnStartDone(const std::string& server_origin);
  void OnRegistered() override;
  void OnUnregistered() override;

  CefRefPtr<CefRegistration> registration_;

  enum class State {
    NONE,
    INITIALIZING,
    INITIALIZED,
    SHUTTINGDOWN,
  } state_ = State::NONE;

  base::WeakPtrFactory<ObserverHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ObserverHelper);
};

}  // namespace test_server

#endif  // CEF_TESTS_CEFTESTS_TEST_SERVER_H_
