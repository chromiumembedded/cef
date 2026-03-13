// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_version.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

TEST(InstallerVersionTest, ParseValidThreeComponent) {
  Version v = Version::Parse("137.3.5");
  EXPECT_TRUE(v.IsValid());
  EXPECT_EQ("137.3.5", v.ToString());
}

TEST(InstallerVersionTest, ParseValidTwoComponent) {
  Version v = Version::Parse("137.0");
  EXPECT_TRUE(v.IsValid());
  EXPECT_EQ("137.0", v.ToString());
}

TEST(InstallerVersionTest, ParseValidOneComponent) {
  Version v = Version::Parse("137");
  EXPECT_TRUE(v.IsValid());
  EXPECT_EQ("137", v.ToString());
}

TEST(InstallerVersionTest, ParseInvalidEmpty) {
  Version v = Version::Parse("");
  EXPECT_FALSE(v.IsValid());
}

TEST(InstallerVersionTest, ParseInvalidAlpha) {
  Version v = Version::Parse("abc");
  EXPECT_FALSE(v.IsValid());
}

TEST(InstallerVersionTest, ParseInvalidTooManyComponents) {
  Version v = Version::Parse("1.2.3.4.5");
  EXPECT_FALSE(v.IsValid());
}

TEST(InstallerVersionTest, ParseInvalidNegative) {
  Version v = Version::Parse("-1.0");
  EXPECT_FALSE(v.IsValid());
}

TEST(InstallerVersionTest, CompareEqual) {
  Version v1 = Version::Parse("137.3.5");
  Version v2 = Version::Parse("137.3.5");
  EXPECT_TRUE(v1 == v2);
  EXPECT_FALSE(v1 != v2);
}

TEST(InstallerVersionTest, CompareLessThan) {
  Version v1 = Version::Parse("137.3.4");
  Version v2 = Version::Parse("137.3.5");
  EXPECT_TRUE(v1 < v2);
  EXPECT_TRUE(v1 <= v2);
  EXPECT_FALSE(v1 > v2);
  EXPECT_FALSE(v1 >= v2);
}

TEST(InstallerVersionTest, CompareGreaterThan) {
  Version v1 = Version::Parse("137.3.6");
  Version v2 = Version::Parse("137.3.5");
  EXPECT_TRUE(v1 > v2);
  EXPECT_TRUE(v1 >= v2);
  EXPECT_FALSE(v1 < v2);
  EXPECT_FALSE(v1 <= v2);
}

TEST(InstallerVersionTest, IsInRangeMatch) {
  Version v = Version::Parse("137.3.5");
  EXPECT_TRUE(v.IsInRange("137.1", "137.99"));
}

TEST(InstallerVersionTest, IsInRangeBelowMin) {
  Version v = Version::Parse("137.0");
  EXPECT_FALSE(v.IsInRange("137.1", "137.99"));
}

TEST(InstallerVersionTest, IsInRangeAboveMax) {
  Version v = Version::Parse("137.100");
  EXPECT_FALSE(v.IsInRange("137.1", "137.99"));
}

TEST(InstallerVersionTest, IsInRangeNoUpperBound) {
  Version v = Version::Parse("138.0");
  EXPECT_TRUE(v.IsInRange("137.1", ""));
}

TEST(InstallerVersionTest, GetMilestone) {
  Version v = Version::Parse("137.3.5");
  EXPECT_EQ(137, v.GetMilestone());
}

TEST(InstallerVersionTest, FromApiVersion) {
  Version v = Version::FromApiVersion(14601);
  EXPECT_TRUE(v.IsValid());
  EXPECT_EQ("146.1", v.ToString());
  EXPECT_EQ(146, v.GetMilestone());
}

TEST(InstallerVersionTest, FromApiVersionZeroMinor) {
  Version v = Version::FromApiVersion(13300);
  EXPECT_TRUE(v.IsValid());
  EXPECT_EQ("133.0", v.ToString());
  EXPECT_EQ(133, v.GetMilestone());
}

TEST(InstallerVersionTest, GetMilestoneInvalid) {
  Version v;  // Default-constructed, invalid
  EXPECT_EQ(0, v.GetMilestone());
}

TEST(InstallerVersionTest, ToStringInvalid) {
  Version v;  // Default-constructed, invalid
  EXPECT_TRUE(v.ToString().empty());
}

TEST(InstallerVersionTest, IsInRangeInvalidSelf) {
  Version v;  // Invalid version
  EXPECT_FALSE(v.IsInRange("137.0", "137.99"));
}

TEST(InstallerVersionTest, IsInRangeInvalidVmin) {
  Version v = Version::Parse("137.3.5");
  EXPECT_FALSE(v.IsInRange("not-a-version", "137.99"));
}

TEST(InstallerVersionTest, IsInRangeInvalidVmax) {
  Version v = Version::Parse("137.3.5");
  EXPECT_FALSE(v.IsInRange("137.0", "not-a-version"));
}

}  // namespace
}  // namespace cef_installer
