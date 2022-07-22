// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/internal/cef_time_wrappers.h"
#include "tests/gtest/include/gtest/gtest.h"

TEST(TimeTest, Now) {
  // Sanity check, what it returns something.
  cef_basetime_t now = cef_basetime_now();
  EXPECT_NE(now.val, 0);
}

TEST(TimeTest, TimeToBaseTime) {
  // Choosing some date which should be always representable on any platform.
  cef_time_t date{};
  date.year = 2001;
  date.month = 2;
  date.day_of_week = 1;
  date.day_of_month = 5;
  date.hour = 6;
  date.minute = 7;
  date.second = 8;
  date.millisecond = 9;

  cef_basetime_t basetime{};

  // Null parameter handling.
  ASSERT_FALSE(cef_time_to_basetime(nullptr, nullptr));
  ASSERT_FALSE(cef_time_to_basetime(&date, nullptr));
  ASSERT_FALSE(cef_time_to_basetime(nullptr, &basetime));

  ASSERT_TRUE(cef_time_to_basetime(&date, &basetime));
  ASSERT_EQ(basetime.val, 12625826828009000);
}

TEST(TimeTest, BaseTimeToTime) {
  cef_basetime_t basetime{12625826828009000};

  // Choosing some date which should be always representable on any platform.
  cef_time_t date{};

  ASSERT_TRUE(cef_time_from_basetime(basetime, &date) != 0);

  EXPECT_EQ(date.year, 2001);
  EXPECT_EQ(date.month, 2);
  EXPECT_EQ(date.day_of_week, 1);
  EXPECT_EQ(date.day_of_month, 5);
  EXPECT_EQ(date.hour, 6);
  EXPECT_EQ(date.minute, 7);
  EXPECT_EQ(date.second, 8);
  EXPECT_EQ(date.millisecond, 9);
}

TEST(TimeTest, InvalidTimeToBaseTime) {
  // Some time which can't be represented
  cef_time_t date{};
  date.year = 90000;
  cef_basetime_t basetime{999999999};

  ASSERT_FALSE(cef_time_to_basetime(&date, &basetime) != 0);
  ASSERT_EQ(basetime.val, 0);  // Output should always be set.
}

// Only run on Windows because POSIX supports a wider range of dates.
#if defined(OS_WIN)
TEST(TimeTest, InvalidBaseTimeToTime) {
  // Unrepresentable.
  cef_basetime_t basetime{std::numeric_limits<int64_t>::max()};
  cef_time_t date{};
  date.year = 999999999;

  ASSERT_FALSE(cef_time_from_basetime(basetime, &date) != 0);

  // Output should always be set.
  EXPECT_EQ(date.year, 0);
  EXPECT_EQ(date.month, 0);
  EXPECT_EQ(date.day_of_week, 0);
  EXPECT_EQ(date.day_of_month, 0);
  EXPECT_EQ(date.hour, 0);
  EXPECT_EQ(date.minute, 0);
  EXPECT_EQ(date.second, 0);
  EXPECT_EQ(date.millisecond, 0);
}
#endif  // defined(OS_WIN)
