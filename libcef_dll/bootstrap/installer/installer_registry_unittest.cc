// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_registry.h"

#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {

TEST(InstallerRegistryTest, SharedMachineViewPreservesRequestedAccess) {
  const REGSAM access = GetSharedMachineRegistryAccess(KEY_READ);
  EXPECT_EQ(static_cast<REGSAM>(KEY_READ),
            access & ~static_cast<REGSAM>(KEY_WOW64_64KEY));
  if (base::win::OSInfo::GetArchitecture() ==
      base::win::OSInfo::X86_ARCHITECTURE) {
    EXPECT_EQ(0u, access & KEY_WOW64_64KEY);
  } else {
    EXPECT_NE(0u, access & KEY_WOW64_64KEY);
  }
}

TEST(InstallerRegistryTest, MissingSharedMachineKeyIsReported) {
  base::win::RegKey key;
  EXPECT_EQ(
      ERROR_FILE_NOT_FOUND,
      OpenSharedMachineRegistryKey(
          L"SOFTWARE\\CEF\\CodexNonexistentPolicyFixture", KEY_READ, &key));
  EXPECT_FALSE(key.Valid());
}

TEST(InstallerRegistryTest, RejectsNullOutput) {
  EXPECT_EQ(ERROR_INVALID_PARAMETER,
            OpenSharedMachineRegistryKey(L"SOFTWARE\\CEF", KEY_READ, nullptr));
}

}  // namespace cef_installer
