// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/test_server_observer.h"

#include <sstream>

#include "include/cef_command_line.h"
#include "include/cef_task.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/ceftests/test_request.h"
#include "tests/ceftests/test_util.h"
#include "tests/ceftests/track_callback.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

struct TestState {
  bool https_server = false;

  TrackCallback got_initialized_;
  TrackCallback got_request_;
  TrackCallback got_response_;
  TrackCallback got_shutdown_;

  bool ExpectAll() {
    EXPECT_TRUE(got_initialized_);
    EXPECT_TRUE(got_request_);
    EXPECT_TRUE(got_response_);
    EXPECT_TRUE(got_shutdown_);
    return got_initialized_ && got_request_ && got_response_ && got_shutdown_;
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
    Initialize(state_->https_server);
  }

  ~TestServerObserver() override { std::move(done_callback_).Run(); }

  void OnInitialized(const std::string& server_origin) override {
    CEF_REQUIRE_UI_THREAD();
    EXPECT_FALSE(state_->got_initialized_);
    EXPECT_FALSE(state_->got_request_);
    EXPECT_FALSE(state_->got_response_);
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

  bool OnTestServerRequest(CefRefPtr<CefRequest> request,
                           const ResponseCallback& response_callback) override {
    CEF_REQUIRE_UI_THREAD();
    const std::string& url = request->GetURL();
    if (url != url_) {
      return false;
    }

    EXPECT_TRUE(state_->got_initialized_);
    EXPECT_FALSE(state_->got_request_);
    EXPECT_FALSE(state_->got_response_);
    EXPECT_FALSE(state_->got_shutdown_);

    state_->got_request_.yes();

    auto response = CefResponse::Create();
    response->SetStatus(200);
    response->SetMimeType("text/plain");

    response_callback.Run(response, kResponseData);

    // Stop propagating the callback.
    return true;
  }

  void OnRequestResponse(const test_request::State& state) {
    CEF_REQUIRE_UI_THREAD();
    // Don't test for disconnected, which may race response.
    EXPECT_TRUE(state_->got_initialized_);
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

  void OnShutdown() override {
    CEF_REQUIRE_UI_THREAD();
    EXPECT_TRUE(state_->got_initialized_);
    EXPECT_TRUE(state_->got_request_);
    EXPECT_TRUE(state_->got_response_);
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

void Wait(CefRefPtr<CefWaitableEvent> event) {
  const auto timeout = GetConfiguredTestTimeout(/*timeout_ms=*/2000);
  if (!timeout) {
    event->Wait();
  } else {
    event->TimedWait(*timeout);
  }
}

void RunHelperSingle(bool https_server) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  TestState state;
  state.https_server = https_server;
  CreateObserverOnUIThread(&state, "/TestServerTest.ObserverHelperSingle",
                           base::BindOnce(&CefWaitableEvent::Signal, event));

  Wait(event);

  EXPECT_TRUE(state.ExpectAll());
}

void RunHelperMultiple(bool https_server) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  TestState states[3];
  size_t count = 0;
  const size_t size = std::size(states);

  for (size_t i = 0; i < size; ++i) {
    std::stringstream ss;
    ss << "/TestServerTest.ObserverHelperMultiple" << i;
    auto done_callback =
        base::BindOnce(SignalIfDone, event, base::Unretained(&count), size);
    states[i].https_server = https_server;
    CreateObserverOnUIThread(&states[i], ss.str(), std::move(done_callback));
  }

  Wait(event);

  EXPECT_EQ(size, count);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_TRUE(states[i].ExpectAll()) << i;
  }
}

}  // namespace

TEST(TestServerObserverTest, HelperSingleHttp) {
  RunHelperSingle(/*https_server=*/false);
}

TEST(TestServerObserverTest, HelperMultipleHttp) {
  RunHelperMultiple(/*https_server=*/false);
}

TEST(TestServerObserverTest, HelperSingleHttps) {
  RunHelperSingle(/*https_server=*/true);
}

TEST(TestServerObserverTest, HelperMultipleHttps) {
  RunHelperMultiple(/*https_server=*/true);
}
