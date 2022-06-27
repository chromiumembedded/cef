// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/screen_util.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

constexpr int kScreenWidth = 1024;
constexpr int kScreenHeight = 768;
const CefRect kMainScreen(0, 0, kScreenWidth, kScreenHeight);
const CefRect kLeftScreen(-1024, 0, kScreenWidth, kScreenHeight);

}  // namespace

TEST(MakeVisibleOnScreenRect, RectSizeIsBiggerThanScreen) {
  const CefRect rect{400, 500, 1500, 800};

  auto result = MakeVisibleOnScreenRect(rect, kMainScreen);

  EXPECT_EQ(result.x, 0);
  EXPECT_EQ(result.width, kMainScreen.width);
  EXPECT_EQ(result.y, 0);
  EXPECT_EQ(result.height, kMainScreen.height);
}

TEST(MakeVisibleOnScreenRect, RightBorderIsOutsideTheScreen) {
  const CefRect rect{600, 400, 500, 300};

  auto result = MakeVisibleOnScreenRect(rect, kMainScreen);

  EXPECT_EQ(result.x, 524);
  EXPECT_EQ(result.width, rect.width);
  EXPECT_EQ(result.y, rect.y);
  EXPECT_EQ(result.height, rect.height);
}

TEST(MakeVisibleOnScreenRect, LeftBorderIsOutsideTheScreen) {
  const CefRect rect{-400, 400, 500, 300};

  auto result = MakeVisibleOnScreenRect(rect, kMainScreen);

  EXPECT_EQ(result.x, 0);
  EXPECT_EQ(result.width, rect.width);
  EXPECT_EQ(result.y, rect.y);
  EXPECT_EQ(result.height, rect.height);
}

TEST(MakeVisibleOnScreenRect, BottomBorderIsOutsideTheScreen) {
  const CefRect rect{600, 500, 300, 300};

  auto result = MakeVisibleOnScreenRect(rect, kMainScreen);

  EXPECT_EQ(result.x, 600);
  EXPECT_EQ(result.width, rect.width);
  EXPECT_EQ(result.y, 468);
  EXPECT_EQ(result.height, rect.height);
}

TEST(MakeVisibleOnScreenRect, RectIsVisibleOnTheLeftScreen) {
  const CefRect rect{-500, 300, 300, 300};

  auto result = MakeVisibleOnScreenRect(rect, kLeftScreen);

  EXPECT_EQ(result.x, rect.x);
  EXPECT_EQ(result.width, rect.width);
  EXPECT_EQ(result.y, rect.y);
  EXPECT_EQ(result.height, rect.height);
}

TEST(MakeVisibleOnScreenRect, RectSizeIsBiggerThanLeftScreen) {
  const CefRect rect{-500, 300, 3000, 3000};

  auto result = MakeVisibleOnScreenRect(rect, kLeftScreen);

  EXPECT_EQ(result.x, kLeftScreen.x);
  EXPECT_EQ(result.width, kLeftScreen.width);
  EXPECT_EQ(result.y, kLeftScreen.y);
  EXPECT_EQ(result.height, kLeftScreen.height);
}