// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/test_server_manager.h"

#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace test_server {

namespace {

Manager* g_http_manager = nullptr;
Manager* g_https_manager = nullptr;

}  // namespace

// May be created on any thread but will be destroyed on the UI thread.
class ObserverRegistration : public CefRegistration {
 public:
  ObserverRegistration(Observer* observer, bool https_server)
      : observer_(observer), https_server_(https_server) {
    EXPECT_TRUE(observer_);
  }

  ~ObserverRegistration() override {
    CEF_REQUIRE_UI_THREAD();

    if (auto manager = Manager::GetInstance(https_server_)) {
      manager->RemoveObserver(
          observer_,
          base::BindOnce([](Observer* observer) { observer->OnUnregistered(); },
                         base::Unretained(observer_)));
    }
  }

  void Initialize() {
    CEF_REQUIRE_UI_THREAD();
    Manager::GetOrCreateInstance(https_server_)->AddObserver(observer_);
    observer_->OnRegistered();
  }

  static void InitializeRegistration(
      CefRefPtr<ObserverRegistration> registration,
      Manager::DoneCallback callback) {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI, base::BindOnce(InitializeRegistration, registration,
                                         std::move(callback)));
      return;
    }

    registration->Initialize();
    if (!callback.is_null()) {
      std::move(callback).Run();
    }
  }

 private:
  Observer* const observer_;
  const bool https_server_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(ObserverRegistration);
  DISALLOW_COPY_AND_ASSIGN(ObserverRegistration);
};

Manager::Manager(bool https_server) : https_server_(https_server) {
  CEF_REQUIRE_UI_THREAD();

  if (https_server) {
    DCHECK(!g_https_manager);
    g_https_manager = this;
  } else {
    DCHECK(!g_http_manager);
    g_http_manager = this;
  }
}

Manager::~Manager() {
  CEF_REQUIRE_UI_THREAD();
  EXPECT_TRUE(observer_list_.empty());
  EXPECT_TRUE(start_callback_list_.empty());
  EXPECT_TRUE(stop_callback_.is_null());

  if (https_server_) {
    g_https_manager = nullptr;
  } else {
    g_http_manager = nullptr;
  }
}

// static
Manager* Manager::GetInstance(bool https_server) {
  return https_server ? g_https_manager : g_http_manager;
}

// static
Manager* Manager::GetOrCreateInstance(bool https_server) {
  if (auto manager = GetInstance(https_server)) {
    return manager;
  }

  new Manager(https_server);
  auto manager = GetInstance(https_server);
  DCHECK(manager);
  return manager;
}

// static
void Manager::Start(StartDoneCallback callback, bool https_server) {
  EXPECT_FALSE(callback.is_null());
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(Manager::Start, std::move(callback),
                                       https_server));
    return;
  }

  Manager::GetOrCreateInstance(https_server)->StartImpl(std::move(callback));
}

// static
void Manager::Stop(DoneCallback callback, bool https_server) {
  EXPECT_FALSE(callback.is_null());
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(Manager::Stop, std::move(callback),
                                       https_server));
    return;
  }

  if (auto manager = Manager::GetInstance(https_server)) {
    manager->StopImpl(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

// static
CefRefPtr<CefRegistration> Manager::AddObserver(Observer* observer,
                                                DoneCallback callback,
                                                bool https_server) {
  EXPECT_TRUE(observer);
  CefRefPtr<ObserverRegistration> registration =
      new ObserverRegistration(observer, https_server);
  ObserverRegistration::InitializeRegistration(registration,
                                               std::move(callback));
  return registration.get();
}

// static
CefRefPtr<CefRegistration> Manager::AddObserverAndStart(
    Observer* observer,
    StartDoneCallback callback,
    bool https_server) {
  return AddObserver(
      observer,
      base::BindOnce(Manager::Start, std::move(callback), https_server),
      https_server);
}

// static
std::string Manager::GetOrigin(bool https_server) {
  if (auto manager = Manager::GetInstance(https_server)) {
    return manager->origin_;
  }
  NOTREACHED();
  return std::string();
}

void Manager::StartImpl(StartDoneCallback callback) {
  CEF_REQUIRE_UI_THREAD();
  EXPECT_FALSE(stopping_);

  if (!origin_.empty()) {
    // The server is already running.
    std::move(callback).Run(origin_);
    return;
  }

  // If tests run in parallel, and the server is starting, then there may be
  // multiple pending callbacks.
  start_callback_list_.push_back(std::move(callback));

  // Only create the runner a single time.
  if (!runner_) {
    runner_ = Runner::Create(this, https_server_);
    runner_->StartServer();
  }
}

void Manager::StopImpl(DoneCallback callback) {
  CEF_REQUIRE_UI_THREAD();
  if (!runner_) {
    // The server is not currently running.
    std::move(callback).Run();
    return;
  }

  // Stop will be called one time on test framework shutdown.
  EXPECT_FALSE(stopping_);
  stopping_ = true;

  // Only 1 stop callback supported.
  EXPECT_TRUE(stop_callback_.is_null());
  stop_callback_ = std::move(callback);

  runner_->ShutdownServer();
}

void Manager::AddObserver(Observer* observer) {
  CEF_REQUIRE_UI_THREAD();
  EXPECT_FALSE(stopping_);
  observer_list_.push_back(observer);
}

void Manager::RemoveObserver(Observer* observer, DoneCallback callback) {
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

  if (observer_list_.empty() && https_server_ && !stopping_) {
    // Stop the HTTPS server when the last Observer is removed. We can't
    // currently reuse the HTTPS server between tests due to
    // https://crrev.com/dd2a57d753 causing cert registration issues.
    StopImpl(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void Manager::OnServerCreated(const std::string& server_origin) {
  CEF_REQUIRE_UI_THREAD();

  EXPECT_TRUE(origin_.empty());
  origin_ = server_origin;

  for (auto& callback : start_callback_list_) {
    std::move(callback).Run(origin_);
  }
  start_callback_list_.clear();
}

void Manager::OnServerDestroyed() {
  CEF_REQUIRE_UI_THREAD();

  origin_.clear();
  runner_.reset();
}

// All server-related objects have been torn down.
void Manager::OnServerHandlerDeleted() {
  CEF_REQUIRE_UI_THREAD();

  EXPECT_FALSE(stop_callback_.is_null());
  std::move(stop_callback_).Run();

  delete this;
}

void Manager::OnTestServerRequest(CefRefPtr<CefRequest> request,
                                  const ResponseCallback& response_callback) {
  CEF_REQUIRE_UI_THREAD();

  // TODO(chrome-runtime): Debug why favicon requests don't always have the
  // correct resource type.
  const std::string& url = request->GetURL();
  if (request->GetResourceType() == RT_FAVICON ||
      url.find("/favicon.ico") != std::string::npos) {
    // We don't currently handle favicon requests.
    response_callback.Run(Create404Response(), std::string());
    return;
  }

  EXPECT_FALSE(observer_list_.empty()) << url;

  // Use a copy in case |observer_list_| is modified during iteration.
  ObserverList list = observer_list_;

  bool handled = false;

  ObserverList::const_iterator it = list.begin();
  for (; it != list.end(); ++it) {
    if ((*it)->OnTestServerRequest(request, response_callback)) {
      handled = true;
      break;
    }
  }

  if (!handled) {
    LOG(WARNING) << "Unhandled request for: " << url;
    response_callback.Run(Create404Response(), std::string());
  }
}

}  // namespace test_server
