// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_validation.h"

#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

TEST(InstallerValidationTest, IsValidAbiHashAcceptsHex) {
  EXPECT_TRUE(IsValidAbiHash("abc123"));
  EXPECT_TRUE(IsValidAbiHash("ABC123"));
  EXPECT_TRUE(IsValidAbiHash("0123456789abcdef"));
  EXPECT_TRUE(IsValidAbiHash("ABCDEF"));
  EXPECT_TRUE(IsValidAbiHash("a"));
  EXPECT_TRUE(IsValidAbiHash("0"));
}

TEST(InstallerValidationTest, IsValidAbiHashRejectsEmpty) {
  EXPECT_FALSE(IsValidAbiHash(""));
}

TEST(InstallerValidationTest, IsValidAbiHashRejectsNonHex) {
  EXPECT_FALSE(IsValidAbiHash("ghijkl"));
  EXPECT_FALSE(IsValidAbiHash("abc_123"));
  EXPECT_FALSE(IsValidAbiHash("abc 123"));
  EXPECT_FALSE(IsValidAbiHash("test_abi_hash"));
}

TEST(InstallerValidationTest, IsValidAbiHashRejectsUrlMetachars) {
  // Path traversal
  EXPECT_FALSE(IsValidAbiHash("../../etc/passwd"));
  EXPECT_FALSE(IsValidAbiHash("../secret"));
  // Query/fragment injection
  EXPECT_FALSE(IsValidAbiHash("abc?x=1"));
  EXPECT_FALSE(IsValidAbiHash("abc#frag"));
  // Slashes
  EXPECT_FALSE(IsValidAbiHash("abc/def"));
  EXPECT_FALSE(IsValidAbiHash("abc\\def"));
}

TEST(InstallerValidationTest, IsValidAbiHashRejectsOverlong) {
  std::string max_len(kMaxAbiHashLength, 'a');
  EXPECT_TRUE(IsValidAbiHash(max_len));

  std::string too_long(kMaxAbiHashLength + 1, 'a');
  EXPECT_FALSE(IsValidAbiHash(too_long));
}

}  // namespace
}  // namespace cef_installer
