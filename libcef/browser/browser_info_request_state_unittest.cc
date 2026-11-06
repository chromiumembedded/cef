// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/browser_info_request_state.h"

#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Stand-ins for a global frame token and browser info. The helper does not
// distinguish main frames from subframes or depend on live Chromium objects.
// These tests model lifecycle notifications, not the RFH call sites that
// generate those notifications.
using FrameToken = std::pair<int, int>;
using RequestState = CefBrowserInfoRequestState<FrameToken, int>;
constexpr FrameToken kFrame{1, 1};

struct Response {
  int calls = 0;
  int browser_info = 0;
  bool is_excluded = false;

  RequestState::Callback callback() {
    return base::BindOnce(
        [](Response* response, int browser_info, bool is_excluded) {
          ++response->calls;
          response->browser_info = browser_info;
          response->is_excluded = is_excluded;
        },
        base::Unretained(this));
  }
};

class BrowserInfoRequestStateTest : public testing::Test {
 protected:
  RequestState state_;
  scoped_refptr<base::TestSimpleTaskRunner> runner_ =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
};

TEST_F(BrowserInfoRequestStateTest, RequestBeforeFrameRegistration) {
  Response response;
  const auto timeout = state_.Queue(kFrame, response.callback(), runner_);
  EXPECT_EQ(0, response.calls);

  ASSERT_TRUE(state_.Complete(kFrame, 7, /*is_excluded=*/false));
  EXPECT_EQ(1, response.calls);
  EXPECT_EQ(7, response.browser_info);
  EXPECT_FALSE(response.is_excluded);
  EXPECT_FALSE(state_.Timeout(kFrame, timeout));
  EXPECT_EQ(1, response.calls);
}

TEST_F(BrowserInfoRequestStateTest, FrameDeletedWhileRequestPending) {
  Response response;
  const auto timeout = state_.Queue(kFrame, response.callback(), runner_);

  ASSERT_TRUE(state_.Cancel(kFrame));
  EXPECT_EQ(1, response.calls);
  EXPECT_EQ(0, response.browser_info);
  EXPECT_FALSE(response.is_excluded);
  EXPECT_FALSE(state_.Timeout(kFrame, timeout));
  EXPECT_FALSE(state_.Cancel(kFrame));
  EXPECT_FALSE(state_.Complete(kFrame, 7, /*is_excluded=*/false));
  EXPECT_EQ(1, response.calls);
}

TEST_F(BrowserInfoRequestStateTest, MissingFrameWithoutPendingRequest) {
  // Deletion may precede the IO-thread lookup. The later UI-thread check for
  // an absent RFH must be able to cancel the newly queued request.
  EXPECT_FALSE(state_.Cancel(kFrame));
  Response response;
  state_.Queue(kFrame, response.callback(), runner_);
  ASSERT_TRUE(state_.Cancel(kFrame));
  EXPECT_EQ(1, response.calls);
  EXPECT_EQ(0, response.browser_info);
}

TEST_F(BrowserInfoRequestStateTest, ExcludedContent) {
  Response response;
  state_.Queue(kFrame, response.callback(), runner_);
  ASSERT_TRUE(state_.Complete(kFrame, 7, /*is_excluded=*/true));
  EXPECT_EQ(1, response.calls);
  EXPECT_EQ(7, response.browser_info);
  EXPECT_TRUE(response.is_excluded);

  Response unowned;
  state_.Queue(kFrame, unowned.callback(), runner_);
  ASSERT_TRUE(state_.Complete(kFrame, 0, /*is_excluded=*/true));
  EXPECT_EQ(1, unowned.calls);
  EXPECT_EQ(0, unowned.browser_info);
  EXPECT_TRUE(unowned.is_excluded);
}

TEST_F(BrowserInfoRequestStateTest, TimeoutCancelsOnce) {
  Response response;
  const auto timeout = state_.Queue(kFrame, response.callback(), runner_);
  EXPECT_TRUE(state_.Timeout(kFrame, timeout));
  EXPECT_EQ(1, response.calls);
  EXPECT_EQ(0, response.browser_info);
  EXPECT_FALSE(state_.Timeout(kFrame, timeout));
  EXPECT_FALSE(state_.Complete(kFrame, 7, /*is_excluded=*/false));
  EXPECT_EQ(1, response.calls);
}

TEST_F(BrowserInfoRequestStateTest, StaleTimeoutDoesNotCancelNewRequest) {
  Response old_response;
  const auto old_timeout =
      state_.Queue(kFrame, old_response.callback(), runner_);
  ASSERT_TRUE(state_.Cancel(kFrame));

  Response new_response;
  const auto new_timeout =
      state_.Queue(kFrame, new_response.callback(), runner_);
  EXPECT_NE(old_timeout, new_timeout);
  EXPECT_FALSE(state_.Timeout(kFrame, old_timeout));
  EXPECT_EQ(0, new_response.calls);
  EXPECT_TRUE(state_.Complete(kFrame, 7, /*is_excluded=*/false));
  EXPECT_FALSE(state_.Timeout(kFrame, new_timeout));
  EXPECT_EQ(1, old_response.calls);
  EXPECT_EQ(1, new_response.calls);
  EXPECT_EQ(7, new_response.browser_info);
}

TEST_F(BrowserInfoRequestStateTest, ProcessDestroyedCancelsOnlyMatchingFrames) {
  Response first, second, other_process;
  state_.Queue(kFrame, first.callback(), runner_);
  state_.Queue({1, 2}, second.callback(), runner_);
  const auto other_timeout =
      state_.Queue({2, 1}, other_process.callback(), runner_);

  state_.CancelMatching(
      [](const FrameToken& token) { return token.first == 1; });
  EXPECT_EQ(1, first.calls);
  EXPECT_EQ(0, first.browser_info);
  EXPECT_EQ(1, second.calls);
  EXPECT_EQ(0, second.browser_info);
  EXPECT_EQ(0, other_process.calls);
  EXPECT_TRUE(state_.Timeout({2, 1}, other_timeout));
  EXPECT_EQ(1, other_process.calls);
}

TEST_F(BrowserInfoRequestStateTest, EraseBeforeDeliveringResponse) {
  Response response;
  state_.Queue(
      kFrame,
      base::BindLambdaForTesting([&](int browser_info, bool is_excluded) {
        EXPECT_FALSE(state_.Cancel(kFrame));
        state_.Queue(kFrame, response.callback(), runner_);
      }),
      runner_);
  EXPECT_TRUE(state_.Complete(kFrame, 7, /*is_excluded=*/false));
  EXPECT_EQ(0, response.calls);
  EXPECT_TRUE(state_.Cancel(kFrame));
  EXPECT_EQ(1, response.calls);
}

TEST_F(BrowserInfoRequestStateTest, RespondOnRequestSequence) {
  Response response;
  const auto timeout = state_.Queue(kFrame, response.callback(), runner_);
  base::Thread resolver("BrowserInfoResolver");
  ASSERT_TRUE(resolver.Start());
  base::WaitableEvent resolved;
  resolver.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        EXPECT_TRUE(state_.Complete(kFrame, 7, /*is_excluded=*/false));
        resolved.Signal();
      }));
  resolved.Wait();
  EXPECT_EQ(0, response.calls);
  EXPECT_FALSE(state_.Timeout(kFrame, timeout));
  runner_->RunUntilIdle();
  EXPECT_EQ(1, response.calls);
  EXPECT_EQ(7, response.browser_info);
}

}  // namespace
