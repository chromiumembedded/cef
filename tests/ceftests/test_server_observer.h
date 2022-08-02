// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_TEST_SERVER_OBSERVER_H_
#define CEF_TESTS_CEFTESTS_TEST_SERVER_OBSERVER_H_
#pragma once

#include <string>

#include "include/base/cef_callback.h"
#include "include/base/cef_weak_ptr.h"
#include "include/cef_registration.h"
#include "include/cef_request.h"
#include "tests/ceftests/test_server.h"

namespace test_server {

// Observer for server callbacks. Methods will be called on the UI thread.
class Observer {
 public:
  using ResponseCallback = test_server::ResponseCallback;

  // Called when this Observer is registered.
  virtual void OnRegistered() = 0;

  // Called when this Observer is unregistered.
  virtual void OnUnregistered() = 0;

  // Return true and execute |response_callback| either synchronously or
  // asynchronously if the request was handled. Do not execute
  // |response_callback| when returning false.
  virtual bool OnTestServerRequest(
      CefRefPtr<CefRequest> request,
      const ResponseCallback& response_callback) = 0;

 protected:
  virtual ~Observer() = default;
};

// Helper for managing Observer registration and callbacks. Only used on the UI
// thread.
class ObserverHelper : public Observer {
 public:
  ObserverHelper();
  ~ObserverHelper() override;

  // Initialize the registration. If |https_server| is true an HTTPS server will
  // be used, otherwise an HTTP server will be used. Results in a call to
  // OnInitialized().
  void Initialize(bool https_server);

  // Shut down the registration. Results in a call to OnShutdown().
  void Shutdown();

  // Implement this method to start sending server requests after Initialize().
  // |server_origin| will be value like "http://127.0.0.1:<port>".
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

#endif  // CEF_TESTS_CEFTESTS_TEST_SERVER_OBSERVER_H_
