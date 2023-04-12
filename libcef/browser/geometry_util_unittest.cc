// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/geometry_util.h"
#include "tests/gtest/include/gtest/gtest.h"

#include "ui/gfx/geometry/rect.h"

namespace {

constexpr int kScreenWidth = 1024;
constexpr int kScreenHeight = 768;
const gfx::Rect kMainScreen(0, 0, kScreenWidth, kScreenHeight);
const gfx::Rect kLeftScreen(-1024, 0, kScreenWidth, kScreenHeight);

#define EXPECT_RECT_EQ(rect1, rect2)       \
  EXPECT_EQ(rect1.x(), rect2.x());         \
  EXPECT_EQ(rect1.y(), rect2.y());         \
  EXPECT_EQ(rect1.width(), rect2.width()); \
  EXPECT_EQ(rect1.height(), rect2.height())

}  // namespace

TEST(GeometryUtil, MakeVisibleOnScreenRect_RectSizeIsBiggerThanScreen) {
  const gfx::Rect rect{400, 500, 1500, 800};

  auto result = MakeVisibleOnScreenRect(rect, kMainScreen);

  EXPECT_EQ(result.x(), 0);
  EXPECT_EQ(result.width(), kMainScreen.width());
  EXPECT_EQ(result.y(), 0);
  EXPECT_EQ(result.height(), kMainScreen.height());
}

TEST(GeometryUtil, MakeVisibleOnScreenRect_RightBorderIsOutsideTheScreen) {
  const gfx::Rect rect{600, 400, 500, 300};

  auto result = MakeVisibleOnScreenRect(rect, kMainScreen);

  EXPECT_EQ(result.x(), 524);
  EXPECT_EQ(result.width(), rect.width());
  EXPECT_EQ(result.y(), rect.y());
  EXPECT_EQ(result.height(), rect.height());
}

TEST(GeometryUtil, MakeVisibleOnScreenRect_LeftBorderIsOutsideTheScreen) {
  const gfx::Rect rect{-400, 400, 500, 300};

  auto result = MakeVisibleOnScreenRect(rect, kMainScreen);

  EXPECT_EQ(result.x(), 0);
  EXPECT_EQ(result.width(), rect.width());
  EXPECT_EQ(result.y(), rect.y());
  EXPECT_EQ(result.height(), rect.height());
}

TEST(GeometryUtil, MakeVisibleOnScreenRect_BottomBorderIsOutsideTheScreen) {
  const gfx::Rect rect{600, 500, 300, 300};

  auto result = MakeVisibleOnScreenRect(rect, kMainScreen);

  EXPECT_EQ(result.x(), 600);
  EXPECT_EQ(result.width(), rect.width());
  EXPECT_EQ(result.y(), 468);
  EXPECT_EQ(result.height(), rect.height());
}

TEST(GeometryUtil, MakeVisibleOnScreenRect_RectIsVisibleOnTheLeftScreen) {
  const gfx::Rect rect{-500, 300, 300, 300};

  auto result = MakeVisibleOnScreenRect(rect, kLeftScreen);

  EXPECT_RECT_EQ(result, rect);
}

TEST(GeometryUtil, MakeVisibleOnScreenRect_RectSizeIsBiggerThanLeftScreen) {
  const gfx::Rect rect{-500, 300, 3000, 3000};

  auto result = MakeVisibleOnScreenRect(rect, kLeftScreen);

  EXPECT_RECT_EQ(result, kLeftScreen);
}

TEST(GeometryUtil, SubtractOverlayFromBoundingBox_Square_NoIntersect_NoInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{0, 0, 10, 10};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 10);

  EXPECT_RECT_EQ(bounds, result);
}

TEST(GeometryUtil, SubtractOverlayFromBoundingBox_Square_Contains_NoInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{5, 5, 85, 85};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 10);

  EXPECT_RECT_EQ(bounds, result);
}

TEST(GeometryUtil, SubtractOverlayFromBoundingBox_Square_AllClose_TopInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{12, 12, 76, 76};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  // When all sides are close, inset from the top.
  const gfx::Rect expected_bounds{10, 88, 80, 2};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_Square_TopAndLeftClose_TopInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{12, 12, 30, 30};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  // When top and left sides are close, inset from the top.
  const gfx::Rect expected_bounds{10, 42, 80, 48};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_Square_TopAndRightClose_TopInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{58, 12, 30, 30};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  // When top and right sides are close, inset from the top.
  const gfx::Rect expected_bounds{10, 42, 80, 48};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_Square_BottomAndLeftClose_BottomInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{12, 58, 30, 30};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  // When bottom and left sides are close, inset from the botom.
  const gfx::Rect expected_bounds{10, 10, 80, 48};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_Square_BottomAndRightClose_BottomInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{58, 58, 30, 30};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  // When bottom and right sides are close, inset from the botom.
  const gfx::Rect expected_bounds{10, 10, 80, 48};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_WideRect_TopAndLeftExact_TopInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{10, 10, 10, 5};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 0);

  const gfx::Rect expected_bounds{10, 15, 80, 75};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_WideRect_TopIntersectLeftExact_TopInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{10, 7, 10, 5};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  const gfx::Rect expected_bounds{10, 12, 80, 78};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_WideRect_TopInsideLeftExact_TopInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{10, 12, 10, 5};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  const gfx::Rect expected_bounds{10, 17, 80, 73};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_WideRect_TopTooFarInsideLeftExact_NoInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{10, 16, 10, 5};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  EXPECT_RECT_EQ(bounds, result);
}

TEST(GeometryUtil, SubtractOverlayFromBoundingBox_WideRect_Oversized_TopInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{5, 5, 85, 10};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 10);

  const gfx::Rect expected_bounds{10, 15, 80, 75};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_WideRect_BottomAndLeftExact_BottomInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{10, 85, 10, 5};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 0);

  const gfx::Rect expected_bounds{10, 10, 80, 75};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(
    GeometryUtil,
    SubtractOverlayFromBoundingBox_WideRect_BottomInsideLeftExact_BottomInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{10, 83, 10, 5};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  const gfx::Rect expected_bounds{10, 10, 80, 73};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(
    GeometryUtil,
    SubtractOverlayFromBoundingBox_WideRect_BottomIntersectLeftExact_BottomInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{10, 87, 10, 5};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  const gfx::Rect expected_bounds{10, 10, 80, 77};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(
    GeometryUtil,
    SubtractOverlayFromBoundingBox_WideRect_BottomTooFarInsideLeftExact_NoInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{10, 77, 10, 5};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  EXPECT_RECT_EQ(bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_WideRect_Oversized_BottomInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{5, 85, 85, 10};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 10);

  const gfx::Rect expected_bounds{10, 10, 80, 75};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_TallRect_TopAndLeftExact_LeftInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{10, 10, 5, 10};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 0);

  const gfx::Rect expected_bounds{15, 10, 75, 80};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_TallRect_TopExactLeftIntersect_LeftInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{7, 10, 5, 10};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  const gfx::Rect expected_bounds{12, 10, 78, 80};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_TallRect_TopExactLeftInside_LeftInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{12, 10, 5, 10};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  const gfx::Rect expected_bounds{17, 10, 73, 80};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_TallRect_TopExactLeftTooFarInside_NoInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{16, 10, 5, 10};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  EXPECT_RECT_EQ(bounds, result);
}

TEST(GeometryUtil, SubtractOverlayFromBoundingBox_TallRect_Oversize_LeftInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{5, 5, 10, 85};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 10);

  const gfx::Rect expected_bounds{15, 10, 75, 80};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_TallRect_TopAndRightExact_RightInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{85, 10, 5, 10};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 0);

  const gfx::Rect expected_bounds{10, 10, 75, 80};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_TallRect_TopExactRightInside_RightInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{83, 10, 5, 10};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  const gfx::Rect expected_bounds{10, 10, 73, 80};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(
    GeometryUtil,
    SubtractOverlayFromBoundingBox_TallRect_TopExactRightIntersect_RightInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{87, 10, 5, 10};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  const gfx::Rect expected_bounds{10, 10, 77, 80};
  EXPECT_RECT_EQ(expected_bounds, result);
}

TEST(
    GeometryUtil,
    SubtractOverlayFromBoundingBox_TallRect_TopExactRightTooFarInside_NoInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{77, 10, 5, 10};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 5);

  EXPECT_RECT_EQ(bounds, result);
}

TEST(GeometryUtil,
     SubtractOverlayFromBoundingBox_TallRect_Oversize_RightInset) {
  const gfx::Rect bounds{10, 10, 80, 80};
  const gfx::Rect overlay{85, 5, 10, 85};
  auto result = SubtractOverlayFromBoundingBox(bounds, overlay, 10);

  const gfx::Rect expected_bounds{10, 10, 75, 80};
  EXPECT_RECT_EQ(expected_bounds, result);
}
