// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/osr/external_begin_frame_request_state.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(ExternalBeginFrameRequestState, RequestBeforeAcknowledgement) {
  CefExternalBeginFrameRequestState state;
  ASSERT_TRUE(state.Request());

  // OnPaint can precede acknowledgement of the frame that produced it.
  // The next request must survive instead of being dropped.
  EXPECT_FALSE(state.Request());
  EXPECT_TRUE(state.Acknowledge());
  ASSERT_TRUE(state.Request());
  EXPECT_FALSE(state.Acknowledge());
  EXPECT_FALSE(state.pending());
}

TEST(ExternalBeginFrameRequestState, CoalesceOverlappingRequests) {
  CefExternalBeginFrameRequestState state;
  ASSERT_TRUE(state.Request());
  for (int i = 0; i < 10; ++i) {
    EXPECT_FALSE(state.Request());
  }
  EXPECT_TRUE(state.Acknowledge());
  ASSERT_TRUE(state.Request());
  EXPECT_FALSE(state.Acknowledge());
  EXPECT_FALSE(state.pending());
}

TEST(ExternalBeginFrameRequestState, RequestAfterAcknowledgement) {
  CefExternalBeginFrameRequestState state;
  ASSERT_TRUE(state.Request());
  EXPECT_FALSE(state.Acknowledge());
  EXPECT_TRUE(state.Request());
  EXPECT_TRUE(state.pending());
  EXPECT_FALSE(state.Acknowledge());
}

TEST(ExternalBeginFrameRequestState, CancelWithoutCompositor) {
  CefExternalBeginFrameRequestState state;
  ASSERT_TRUE(state.Request());
  EXPECT_FALSE(state.Request());
  state.Cancel();
  EXPECT_FALSE(state.pending());
  EXPECT_TRUE(state.Request());
  EXPECT_FALSE(state.Acknowledge());
}
