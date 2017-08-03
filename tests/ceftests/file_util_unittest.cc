// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2011 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include <string>

#include "include/wrapper/cef_scoped_temp_dir.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/file_util.h"

TEST(FileUtil, JoinPath) {
  // Should return whichever path component is non-empty.
  EXPECT_STREQ("", client::file_util::JoinPath("", "").c_str());
  EXPECT_STREQ("path1", client::file_util::JoinPath("path1", "").c_str());
  EXPECT_STREQ("path2", client::file_util::JoinPath("", "path2").c_str());

  const std::string& expected =
      std::string("path1") + client::file_util::kPathSep + std::string("path2");

  // Should always be 1 kPathSep character between paths.
  EXPECT_STREQ(expected.c_str(),
               client::file_util::JoinPath("path1", "path2").c_str());
  EXPECT_STREQ(expected.c_str(),
               client::file_util::JoinPath(
                   std::string("path1") + client::file_util::kPathSep, "path2")
                   .c_str());
  EXPECT_STREQ(expected.c_str(),
               client::file_util::JoinPath(
                   "path1", client::file_util::kPathSep + std::string("path2"))
                   .c_str());
  EXPECT_STREQ(expected.c_str(),
               client::file_util::JoinPath(
                   std::string("path1") + client::file_util::kPathSep,
                   client::file_util::kPathSep + std::string("path2"))
                   .c_str());
}

TEST(FileUtil, WriteAndReadFile) {
  CefScopedTempDir dir;
  EXPECT_TRUE(dir.CreateUniqueTempDir());

  const std::string& data = "Test contents to read/write";
  const std::string& path =
      client::file_util::JoinPath(dir.GetPath(), "test.txt");

  EXPECT_EQ(static_cast<int>(data.size()),
            client::file_util::WriteFile(path.c_str(), data.data(),
                                         static_cast<int>(data.size())));

  std::string read;
  EXPECT_TRUE(client::file_util::ReadFileToString(path.c_str(), &read));
  EXPECT_STREQ(data.c_str(), read.c_str());
}

TEST(FileUtil, GetFileExtension) {
  EXPECT_TRUE(client::file_util::GetFileExtension(std::string()).empty());
  EXPECT_TRUE(client::file_util::GetFileExtension("/path/to/foo").empty());
  EXPECT_STREQ("ext",
               client::file_util::GetFileExtension("/path/to/foo.ext").c_str());
}
