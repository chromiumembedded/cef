// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_TEST_SERVER_MANAGER_H_
#define CEF_TESTS_CEFTESTS_TEST_SERVER_MANAGER_H_
#pragma once

#include <string>
#include <vector>

#include "include/base/cef_callback.h"
#include "include/cef_registration.h"
#include "include/cef_request.h"
#include "include/cef_response.h"
#include "tests/ceftests/test_server_observer.h"
#include "tests/ceftests/test_server_runner.h"

namespace test_server {

class ObserverRegistration;

// Static methods are safe to call on any thread. Non-static methods are only
// called on the UI thread. Deletes itself after the server is stopped. Use
// ObserverHelper instead of calling these methods directly.
class Manager : public Runner::Delegate {
 public:
  using StartDoneCallback =
      base::OnceCallback<void(const std::string& server_origin)>;
  using DoneCallback = base::OnceClosure;

  // Starts the server if it is not currently running, and executes |callback|
  // on the UI thread.
  static void Start(StartDoneCallback callback, bool https_server);

  // Stops the server if it is currently running, and executes |callback| on the
  // UI thread. This method will be called by the test framework on shutdown.
  static void Stop(DoneCallback callback, bool https_server);

  // Add an observer for server callbacks. Remains registered until the returned
  // CefRegistration object is destroyed. Registered observers will be executed
  // in the order of registration until one returns true to indicate that it
  // handled the callback. |callback| will be executed on the UI thread after
  // registration is complete.
  static CefRefPtr<CefRegistration> AddObserver(Observer* observer,
                                                DoneCallback callback,
                                                bool https_server);

  // Combination of AddObserver() followed by Start().
  static CefRefPtr<CefRegistration> AddObserverAndStart(
      Observer* observer,
      StartDoneCallback callback,
      bool https_server);

  // Returns the origin for an existing server.
  static std::string GetOrigin(bool https_server);

 private:
  friend class ObserverRegistration;

  explicit Manager(bool https_server);
  ~Manager() override;

  static Manager* GetInstance(bool https_server);
  static Manager* GetOrCreateInstance(bool https_server);

  void StartImpl(StartDoneCallback callback);
  void StopImpl(DoneCallback callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer, DoneCallback callback);

  // Runner::Delegate methods:
  void OnServerCreated(const std::string& server_origin) override;
  void OnServerDestroyed() override;
  void OnServerHandlerDeleted() override;
  void OnTestServerRequest(CefRefPtr<CefRequest> request,
                           const ResponseCallback& response_callback) override;

 private:
  const bool https_server_;

  std::unique_ptr<Runner> runner_;
  std::string origin_;

  using StartDoneCallbackList = std::vector<StartDoneCallback>;
  StartDoneCallbackList start_callback_list_;

  DoneCallback stop_callback_;

  using ObserverList = std::vector<Observer*>;
  ObserverList observer_list_;

  bool stopping_ = false;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace test_server

#endif  // CEF_TESTS_CEFTESTS_TEST_SERVER_MANAGER_H_
