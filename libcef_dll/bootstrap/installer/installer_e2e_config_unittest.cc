// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_e2e_config.h"

#include "base/files/scoped_temp_dir.h"
#include "base/scoped_environment_variable_override.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer::internal {
namespace {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST(InstallerE2EConfigTest, ParsesTypedEnvironmentConfiguration) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath barrier = temp_dir.GetPath().Append(L"barrier");
  base::ScopedEnvironmentVariableOverride directory_scenario(
      "CEF_INSTALLER_TEST_DIRECTORY_SCENARIO", "admin_mutation_denied");
  base::ScopedEnvironmentVariableOverride directory_root(
      "CEF_INSTALLER_TEST_DIRECTORY_ROOT", temp_dir.GetPath().AsUTF8Unsafe());
  base::ScopedEnvironmentVariableOverride file_ops(
      "CEF_INSTALLER_TEST_FILE_OPS_FAULT", "trash_reclaim");
  base::ScopedEnvironmentVariableOverride version_index(
      "CEF_INSTALLER_TEST_INDEX_FAULT", "validation");
  base::ScopedEnvironmentVariableOverride database(
      "CEF_INSTALLER_TEST_DATABASE_SAVE_FAILURE", "1");
  base::ScopedEnvironmentVariableOverride relaunch(
      "CEF_INSTALLER_TEST_RELAUNCH_FAILURE", "1");
  base::ScopedEnvironmentVariableOverride child_config(
      "CEF_INSTALLER_TEST_CHILD_CONFIG_FAILURE", "1");
  base::ScopedEnvironmentVariableOverride child_barrier(
      "CEF_INSTALLER_TEST_CHILD_STATE_BARRIER", barrier.AsUTF8Unsafe());

  const InstallerE2EConfig config =
      LoadInstallerE2EConfigFromEnvironmentForTesting();
  EXPECT_EQ(InstallerE2EDirectoryOverride::kAdminMutationDenied,
            config.directory_override);
  EXPECT_EQ(temp_dir.GetPath(), config.directory_root);
  EXPECT_EQ(InstallerE2EFileOpsFault::kTrashReclaim, config.file_ops_fault);
  EXPECT_EQ(InstallerE2EVersionIndexFault::kValidation,
            config.version_index_fault);
  EXPECT_TRUE(config.database_save_failure);
  EXPECT_TRUE(config.relaunch_failure);
  EXPECT_TRUE(config.child_config_failure);
  EXPECT_EQ(barrier, config.child_state_barrier);
}

TEST(InstallerE2EConfigTest, RejectsIncompleteDirectoryConfiguration) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::ScopedEnvironmentVariableOverride directory_root(
      "CEF_INSTALLER_TEST_DIRECTORY_ROOT", temp_dir.GetPath().AsUTF8Unsafe());

  EXPECT_EQ(
      InstallerE2EDirectoryOverride::kInvalid,
      LoadInstallerE2EConfigFromEnvironmentForTesting().directory_override);
}

TEST(InstallerE2EConfigTest, ParsesEveryFileOpsFault) {
  struct TestCase {
    const char* value;
    InstallerE2EFileOpsFault expected;
  };
  const TestCase cases[] = {
      {"quarantine_move", InstallerE2EFileOpsFault::kQuarantineMove},
      {"repair_move", InstallerE2EFileOpsFault::kRepairMove},
      {"trash_move", InstallerE2EFileOpsFault::kTrashMove},
      {"trash_reclaim", InstallerE2EFileOpsFault::kTrashReclaim},
      {"unknown", InstallerE2EFileOpsFault::kNone},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.value);
    base::ScopedEnvironmentVariableOverride fault(
        "CEF_INSTALLER_TEST_FILE_OPS_FAULT", test.value);
    EXPECT_EQ(test.expected,
              LoadInstallerE2EConfigFromEnvironmentForTesting().file_ops_fault);
  }
}

TEST(InstallerE2EConfigTest, ParsesEveryVersionIndexFault) {
  struct TestCase {
    const char* value;
    InstallerE2EVersionIndexFault expected;
  };
  const TestCase cases[] = {
      {"write", InstallerE2EVersionIndexFault::kWrite},
      {"replace", InstallerE2EVersionIndexFault::kReplace},
      {"reread", InstallerE2EVersionIndexFault::kReread},
      {"validation", InstallerE2EVersionIndexFault::kValidation},
      {"unknown", InstallerE2EVersionIndexFault::kNone},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.value);
    base::ScopedEnvironmentVariableOverride fault(
        "CEF_INSTALLER_TEST_INDEX_FAULT", test.value);
    EXPECT_EQ(
        test.expected,
        LoadInstallerE2EConfigFromEnvironmentForTesting().version_index_fault);
  }
}
#endif

}  // namespace
}  // namespace cef_installer::internal
