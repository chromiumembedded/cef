// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <sstream>

#include "include/base/cef_cxx17_backports.h"
#include "include/cef_task.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/ceftests/test_request.h"
#include "tests/ceftests/test_server.h"
#include "tests/ceftests/track_callback.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

struct TestState {
  TrackCallback got_initialized_;
  TrackCallback got_connected_;
  TrackCallback got_request_;
  TrackCallback got_response_;
  TrackCallback got_disconnected_;
  TrackCallback got_shutdown_;

  bool ExpectAll() {
    EXPECT_TRUE(got_initialized_);
    EXPECT_TRUE(got_connected_);
    EXPECT_TRUE(got_request_);
    EXPECT_TRUE(got_response_);
    EXPECT_TRUE(got_disconnected_);
    EXPECT_TRUE(got_shutdown_);
    return got_initialized_ && got_connected_ && got_request_ &&
           got_response_ && got_disconnected_ && got_shutdown_;
  }
};

const char kResponseData[] = "Test data";

class TestServerObserver : public test_server::ObserverHelper {
 public:
  TestServerObserver(TestState* state,
                     const std::string& path,
                     base::OnceClosure done_callback)
      : state_(state),
        path_(path),
        done_callback_(std::move(done_callback)),
        weak_ptr_factory_(this) {
    DCHECK(state);
    DCHECK(!path.empty());
    DCHECK(!done_callback_.is_null());
    Initialize();
  }

  ~TestServerObserver() override { std::move(done_callback_).Run(); }

  void OnInitialized(const std::string& server_origin) override {
    CEF_REQUIRE_UI_THREAD();
    EXPECT_FALSE(state_->got_initialized_);
    EXPECT_FALSE(state_->got_connected_);
    EXPECT_FALSE(state_->got_request_);
    EXPECT_FALSE(state_->got_response_);
    EXPECT_FALSE(state_->got_disconnected_);
    EXPECT_FALSE(state_->got_shutdown_);

    state_->got_initialized_.yes();

    url_ = server_origin + path_;

    // Send a request to the server.
    test_request::SendConfig config;
    config.request_ = CefRequest::Create();
    config.request_->SetURL(url_);
    test_request::Send(config,
                       base::BindOnce(&TestServerObserver::OnRequestResponse,
                                      weak_ptr_factory_.GetWeakPtr()));
  }

  bool OnClientConnected(CefRefPtr<CefServer> server,
                         int connection_id) override {
    CEF_REQUIRE_UI_THREAD();
    if (state_->got_connected_) {
      // We already got the callback once. Let the next observer get the
      // callback.
      return false;
    }

    EXPECT_TRUE(state_->got_initialized_);
    EXPECT_FALSE(state_->got_request_);
    EXPECT_FALSE(state_->got_response_);
    EXPECT_FALSE(state_->got_disconnected_);
    EXPECT_FALSE(state_->got_shutdown_);

    state_->got_connected_.yes();

    // We don't know if this connection is the one that we're going to
    // handle, so continue propagating the callback.
    return false;
  }

  bool OnHttpRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     const CefString& client_address,
                     CefRefPtr<CefRequest> request) override {
    CEF_REQUIRE_UI_THREAD();
    const std::string& url = request->GetURL();
    if (url != url_)
      return false;

    EXPECT_TRUE(state_->got_initialized_);
    EXPECT_TRUE(state_->got_connected_);
    EXPECT_FALSE(state_->got_request_);
    EXPECT_FALSE(state_->got_response_);
    EXPECT_FALSE(state_->got_disconnected_);
    EXPECT_FALSE(state_->got_shutdown_);

    state_->got_request_.yes();
    connection_id_ = connection_id;

    server->SendHttp200Response(connection_id, "text/plain", kResponseData,
                                sizeof(kResponseData) - 1);

    // Stop propagating the callback.
    return true;
  }

  void OnRequestResponse(const test_request::State& state) {
    CEF_REQUIRE_UI_THREAD();
    // Don't test for disconnected, which may race response.
    EXPECT_TRUE(state_->got_initialized_);
    EXPECT_TRUE(state_->got_connected_);
    EXPECT_TRUE(state_->got_request_);
    EXPECT_FALSE(state_->got_response_);
    EXPECT_FALSE(state_->got_shutdown_);

    state_->got_response_.yes();

    EXPECT_STREQ(url_.c_str(), state.request_->GetURL().ToString().c_str());
    EXPECT_EQ(UR_SUCCESS, state.status_);
    EXPECT_EQ(ERR_NONE, state.error_code_);
    EXPECT_EQ(200, state.response_->GetStatus());
    EXPECT_STREQ(kResponseData, state.download_data_.c_str());

    // Trigger shutdown asynchronously.
    CefPostTask(TID_UI, base::BindOnce(&TestServerObserver::Shutdown,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

  bool OnClientDisconnected(CefRefPtr<CefServer> server,
                            int connection_id) override {
    CEF_REQUIRE_UI_THREAD();
    if (connection_id != connection_id_) {
      // Not the connection that we handled. Let the next observer get the
      // callback.
      return false;
    }

    // Don't test for response, which may race disconnected.
    EXPECT_TRUE(state_->got_initialized_);
    EXPECT_TRUE(state_->got_connected_);
    EXPECT_TRUE(state_->got_request_);
    EXPECT_FALSE(state_->got_disconnected_);
    EXPECT_FALSE(state_->got_shutdown_);

    state_->got_disconnected_.yes();

    // Stop propagating the callback.
    return true;
  }

  void OnShutdown() override {
    CEF_REQUIRE_UI_THREAD();
    EXPECT_TRUE(state_->got_initialized_);
    EXPECT_TRUE(state_->got_connected_);
    EXPECT_TRUE(state_->got_request_);
    EXPECT_TRUE(state_->got_response_);
    EXPECT_TRUE(state_->got_disconnected_);
    EXPECT_FALSE(state_->got_shutdown_);

    state_->got_shutdown_.yes();

    // End the test.
    delete this;
  }

 private:
  TestState* const state_;
  const std::string path_;
  base::OnceClosure done_callback_;

  std::string url_;
  int connection_id_ = -1;

  base::WeakPtrFactory<TestServerObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestServerObserver);
};

void CreateObserverOnUIThread(TestState* state,
                              const std::string& path,
                              base::OnceClosure done_callback) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(CreateObserverOnUIThread,
                                       base::Unretained(state), path,
                                       std::move(done_callback)));
    return;
  }

  new TestServerObserver(state, path, std::move(done_callback));
}

void SignalIfDone(CefRefPtr<CefWaitableEvent> event,
                  size_t* count,
                  size_t total) {
  if (++(*count) == total) {
    event->Signal();
  }
}

}  // namespace

TEST(TestServerTest, ObserverHelperSingle) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  TestState state;
  CreateObserverOnUIThread(&state, "/TestServerTest.ObserverHelperSingle",
                           base::BindOnce(&CefWaitableEvent::Signal, event));
  event->TimedWait(2000);

  EXPECT_TRUE(state.ExpectAll());
}

TEST(TestServerTest, ObserverHelperMultiple) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  TestState states[3];
  size_t count = 0;
  const size_t size = base::size(states);

  for (size_t i = 0; i < size; ++i) {
    std::stringstream ss;
    ss << "/TestServerTest.ObserverHelperMultiple" << i;
    auto done_callback =
        base::BindOnce(SignalIfDone, event, base::Unretained(&count), size);
    CreateObserverOnUIThread(&states[i], ss.str(), std::move(done_callback));
  }

  event->TimedWait(2000);

  EXPECT_EQ(size, count);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_TRUE(states[i].ExpectAll()) << i;
  }
}
