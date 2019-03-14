// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_api_hash.h"
#include "include/cef_version.h"
#include "tests/gtest/include/gtest/gtest.h"

TEST(VersionTest, VersionInfo) {
  EXPECT_EQ(CEF_VERSION_MAJOR, cef_version_info(0));
  EXPECT_EQ(CEF_VERSION_MINOR, cef_version_info(1));
  EXPECT_EQ(CEF_VERSION_PATCH, cef_version_info(2));
  EXPECT_EQ(CEF_COMMIT_NUMBER, cef_version_info(3));
  EXPECT_EQ(CHROME_VERSION_MAJOR, cef_version_info(4));
  EXPECT_EQ(CHROME_VERSION_MINOR, cef_version_info(5));
  EXPECT_EQ(CHROME_VERSION_BUILD, cef_version_info(6));
  EXPECT_EQ(CHROME_VERSION_PATCH, cef_version_info(7));
}

TEST(VersionTest, ApiHash) {
  EXPECT_STREQ(CEF_API_HASH_PLATFORM, cef_api_hash(0));
  EXPECT_STREQ(CEF_API_HASH_UNIVERSAL, cef_api_hash(1));
  EXPECT_STREQ(CEF_COMMIT_HASH, cef_api_hash(2));
}
