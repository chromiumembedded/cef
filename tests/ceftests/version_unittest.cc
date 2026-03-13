// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_api_hash.h"
#include "include/cef_version_info.h"
#include "tests/gtest/include/gtest/gtest.h"

#if CEF_API_ADDED(CEF_NEXT)
static_assert(CEF_VERSION_INFO_SIZE_WITH_INSTALLER_ERROR ==
              offsetof(cef_version_info_t, installer_error_message) +
                  sizeof(((cef_version_info_t*)0)->installer_error_message));
#endif

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
  EXPECT_STREQ(CEF_API_HASH_PLATFORM, cef_api_hash(CEF_API_VERSION, 0));
  EXPECT_STREQ(CEF_API_HASH_PLATFORM, cef_api_hash(CEF_API_VERSION, 1));
  EXPECT_STREQ(CEF_COMMIT_HASH, cef_api_hash(CEF_API_VERSION, 2));
}

#if CEF_API_ADDED(CEF_NEXT)
TEST(VersionTest, VersionInfoBootstrapFieldsSizeGated) {
  cef_version_info_t info = {};
  info.size = CEF_VERSION_INFO_SIZE_WITH_SANDBOX_HASH;
  info.libcef_path = L"bootstrap";
  info.libcef_is_bundled = 1;
  info.libcef_version_full = "full";
  info.installer_error_code = 123;
  info.installer_error_message = "unchanged";

  cef_version_info_all(&info);
  EXPECT_STREQ(L"bootstrap", info.libcef_path);
  EXPECT_EQ(1, info.libcef_is_bundled);
  EXPECT_STREQ("full", info.libcef_version_full);
  EXPECT_EQ(123, info.installer_error_code);
  EXPECT_STREQ("unchanged", info.installer_error_message);

  info.size = CEF_VERSION_INFO_SIZE_WITH_INSTALLER_ERROR;
  cef_version_info_all(&info);
  EXPECT_EQ(nullptr, info.libcef_path);
  EXPECT_EQ(0, info.libcef_is_bundled);
  EXPECT_EQ(nullptr, info.libcef_version_full);
  EXPECT_EQ(0, info.installer_error_code);
  EXPECT_EQ(nullptr, info.installer_error_message);
}

TEST(VersionTest, PopulateVersionInfoClearsBootstrapFields) {
  cef_version_info_t info;
  CEF_POPULATE_VERSION_INFO(&info);
  EXPECT_EQ(0, info.installer_error_code);
  EXPECT_EQ(nullptr, info.installer_error_message);
}
#endif
