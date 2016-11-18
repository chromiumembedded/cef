// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2011 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include <string>

#include "include/cef_file_util.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
#include "tests/gtest/include/gtest/gtest.h"

TEST(ScopedTempDir, FullPath) {
  CefString test_path;
  CefCreateNewTempDirectory("scoped_temp_dir", test_path);

  // Against an existing dir, it should get destroyed when leaving scope.
  EXPECT_TRUE(CefDirectoryExists(test_path));
  {
    CefScopedTempDir dir;
    EXPECT_TRUE(dir.Set(test_path));
    EXPECT_TRUE(dir.IsValid());
  }
  EXPECT_FALSE(CefDirectoryExists(test_path));

  {
    CefScopedTempDir dir;
    EXPECT_TRUE(dir.Set(test_path));
    // Now the dir doesn't exist, so ensure that it gets created.
    EXPECT_TRUE(CefDirectoryExists(test_path));
    // When we call Take(), it shouldn't get destroyed when leaving scope.
    CefString path = dir.Take();
    EXPECT_STREQ(path.ToString().c_str(), test_path.ToString().c_str());
    EXPECT_FALSE(dir.IsValid());
  }
  EXPECT_TRUE(CefDirectoryExists(test_path));

  // Clean up.
  {
    CefScopedTempDir dir;
    EXPECT_TRUE(dir.Set(test_path));
  }
  EXPECT_FALSE(CefDirectoryExists(test_path));
}

TEST(ScopedTempDir, TempDir) {
  // In this case, just verify that a directory was created and that it's a
  // child of TempDir.
  CefString test_path;
  {
    CefScopedTempDir dir;
    EXPECT_TRUE(dir.CreateUniqueTempDir());
    test_path = dir.GetPath();
    EXPECT_TRUE(CefDirectoryExists(test_path));
    CefString tmp_dir;
    EXPECT_TRUE(CefGetTempDirectory(tmp_dir));
    EXPECT_TRUE(test_path.ToString().find(tmp_dir.ToString()) !=
                std::string::npos);
  }
  EXPECT_FALSE(CefDirectoryExists(test_path));
}

TEST(ScopedTempDir, UniqueTempDirUnderPath) {
  // Create a path which will contain a unique temp path.
  CefString base_path;
  ASSERT_TRUE(CefCreateNewTempDirectory("base_dir", base_path));

  CefString test_path;
  {
    CefScopedTempDir dir;
    EXPECT_TRUE(dir.CreateUniqueTempDirUnderPath(base_path));
    test_path = dir.GetPath();
    EXPECT_TRUE(CefDirectoryExists(test_path));
    EXPECT_TRUE(test_path.ToString().find(base_path.ToString()) == 0);
  }
  EXPECT_FALSE(CefDirectoryExists(test_path));
  CefDeleteFile(base_path, true);
}

TEST(ScopedTempDir, MultipleInvocations) {
  CefScopedTempDir dir;
  EXPECT_TRUE(dir.CreateUniqueTempDir());
  EXPECT_FALSE(dir.CreateUniqueTempDir());
  EXPECT_TRUE(dir.Delete());
  EXPECT_TRUE(dir.CreateUniqueTempDir());
  EXPECT_FALSE(dir.CreateUniqueTempDir());
  CefScopedTempDir other_dir;
  EXPECT_TRUE(other_dir.Set(dir.Take()));
  EXPECT_TRUE(dir.CreateUniqueTempDir());
  EXPECT_FALSE(dir.CreateUniqueTempDir());
  EXPECT_FALSE(other_dir.CreateUniqueTempDir());
}
