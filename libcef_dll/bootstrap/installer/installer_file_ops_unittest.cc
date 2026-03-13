// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/file_path_reparse_point_win.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

TEST(InstallerVersionLeaseChildTest, HoldUntilKilled) {
  const base::CommandLine* command = base::CommandLine::ForCurrentProcess();
  base::FilePath trusted_root =
      command->GetSwitchValuePath("cef-lease-child-root");
  base::FilePath version_dir =
      command->GetSwitchValuePath("cef-lease-child-version");
  std::wstring event_name =
      command->GetSwitchValueNative("cef-lease-child-event");
  if (trusted_root.empty() || version_dir.empty() || event_name.empty()) {
    GTEST_SKIP();
  }
  std::unique_ptr<VersionLease> lease;
  ASSERT_EQ(VersionLeaseError::kSuccess,
            AcquireVersionLease(trusted_root, version_dir, &lease));
  HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, event_name.c_str());
  ASSERT_NE(nullptr, event);
  ASSERT_TRUE(SetEvent(event));
  CloseHandle(event);
  WaitForSingleObject(GetCurrentProcess(), INFINITE);
}

class InstallerFileOpsTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    install_dir_ = temp_dir_.GetPath().Append(kCefSubdirectory);
    ASSERT_TRUE(base::CreateDirectory(install_dir_));
  }

  void TearDown() override { SetFileOpsFaultForTesting(FileOpsFault::kNone); }

  // Create a mock source directory with the version metadata file
  base::FilePath CreateMockSourceDir(const std::string& version_str) {
    base::FilePath source_dir = temp_dir_.GetPath().Append(
        L"source_" + base::FilePath::FromUTF8Unsafe(version_str).value());
    base::CreateDirectory(source_dir);

    // Create the version metadata file
    base::FilePath metadata_file = source_dir.Append(kVersionMetadataFilename);
    base::WriteFile(metadata_file, R"({"version":")" + version_str + R"("})");

    return source_dir;
  }

  // Create a mock installed version
  void CreateInstalledVersion(const std::string& version_str) {
    Version version = Version::Parse(version_str);
    base::FilePath version_dir = GetVersionPath(install_dir_, version);
    base::CreateDirectory(version_dir);

    // Create the version metadata file
    base::FilePath metadata_file = version_dir.Append(kVersionMetadataFilename);
    base::WriteFile(metadata_file, R"({"version":")" + version_str + R"("})");
  }

  VersionMetadata CreateCompleteDistribution(
      const base::FilePath& path,
      const std::string& version_str,
      const std::string& abi_hash = "abc123def4567890") {
    EXPECT_TRUE(base::CreateDirectory(path));
    VersionMetadata metadata;
    metadata.version = Version::Parse(version_str);
    metadata.abi_hash = abi_hash;
    metadata.platform = GetCurrentPlatform();
    EXPECT_EQ(MetadataError::kSuccess, WriteVersionMetadata(path, metadata));
    base::FilePath release = path.Append(kReleaseSubdirectory);
    EXPECT_TRUE(base::CreateDirectory(release));
    EXPECT_TRUE(base::WriteFile(release.Append(kLibcefFilename), "libcef"));
    EXPECT_TRUE(base::WriteFile(path.Append(kCatalogFilename), "catalog"));
    return metadata;
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath install_dir_;
};

// =============================================================================
// InstallVersion Tests
// =============================================================================

TEST_F(InstallerFileOpsTest, InstallVersionSuccess) {
  base::FilePath source_dir = CreateMockSourceDir("137.3.5");
  Version version = Version::Parse("137.3.5");

  FileOpsError result = InstallVersion(source_dir, install_dir_, version);

  EXPECT_EQ(FileOpsError::kSuccess, result);

  // Verify the version was installed
  base::FilePath installed_path = GetVersionPath(install_dir_, version);
  EXPECT_TRUE(base::DirectoryExists(installed_path));
  EXPECT_TRUE(
      base::PathExists(installed_path.Append(kVersionMetadataFilename)));

  // Source should no longer exist (was renamed)
  EXPECT_FALSE(base::DirectoryExists(source_dir));
}

TEST_F(InstallerFileOpsTest, InstallVersionDestExists) {
  const std::string version_string = "137.3.5";
  Version version = Version::Parse(version_string);
  CreateInstalledVersion("137.3.5");
  base::FilePath destination = GetVersionPath(install_dir_, version);
  ASSERT_TRUE(
      base::WriteFile(destination.Append(L"old_marker.txt"), "old target"));

  base::FilePath source_dir = CreateMockSourceDir(version_string);
  ASSERT_TRUE(
      base::WriteFile(source_dir.Append(L"staging_marker.txt"), "staging"));

  FileOpsError result = InstallVersion(source_dir, install_dir_, version);

  EXPECT_EQ(FileOpsError::kSuccess, result);
  EXPECT_FALSE(base::PathExists(source_dir));
  EXPECT_FALSE(base::PathExists(destination.Append(L"old_marker.txt")));
  EXPECT_TRUE(base::PathExists(destination.Append(L"staging_marker.txt")));
}

TEST_F(InstallerFileOpsTest, InstallVersionSourceMissing) {
  base::FilePath nonexistent_dir = temp_dir_.GetPath().Append(L"nonexistent");
  Version version = Version::Parse("137.3.5");

  FileOpsError result = InstallVersion(nonexistent_dir, install_dir_, version);

  EXPECT_EQ(FileOpsError::kSourceNotFound, result);
}

TEST_F(InstallerFileOpsTest, InstallVersionInvalidVersion) {
  base::FilePath source_dir = CreateMockSourceDir("137.3.5");
  Version invalid_version;  // Default-constructed = invalid

  FileOpsError result =
      InstallVersion(source_dir, install_dir_, invalid_version);

  EXPECT_EQ(FileOpsError::kSourceNotFound, result);
}

TEST_F(InstallerFileOpsTest, InstallVersionCreatesVersionsDir) {
  // Use a fresh install dir without Versions subdirectory
  base::FilePath fresh_install_dir = temp_dir_.GetPath().Append(L"FreshCEF");
  ASSERT_TRUE(base::CreateDirectory(fresh_install_dir));

  base::FilePath source_dir = CreateMockSourceDir("137.3.5");
  Version version = Version::Parse("137.3.5");

  FileOpsError result = InstallVersion(source_dir, fresh_install_dir, version);

  EXPECT_EQ(FileOpsError::kSuccess, result);

  // Versions subdirectory should have been created
  base::FilePath versions_dir = fresh_install_dir.Append(kVersionsSubdirectory);
  EXPECT_TRUE(base::DirectoryExists(versions_dir));
}

TEST_F(InstallerFileOpsTest, ReplacesPartialDestination) {
  const std::string version_string = "137.3.5";
  Version version = Version::Parse(version_string);
  base::FilePath source = temp_dir_.GetPath().Append(L"replacement_source");
  VersionMetadata expected = CreateCompleteDistribution(source, version_string);
  ASSERT_TRUE(base::WriteFile(source.Append(L"staging_marker.txt"), "staging"));
  base::FilePath destination = GetVersionPath(install_dir_, version);
  ASSERT_TRUE(base::CreateDirectory(destination));
  ASSERT_TRUE(base::WriteFile(destination.Append(L"partial.txt"), "partial"));

  bool cleanup_deferred = false;
  EXPECT_EQ(FileOpsError::kSuccess,
            InstallVersion(source, install_dir_, version, &cleanup_deferred));
  EXPECT_EQ(DistributionValidation::kComplete,
            ValidateDistribution(install_dir_, destination, expected));
  EXPECT_FALSE(base::PathExists(destination.Append(L"partial.txt")));
  EXPECT_TRUE(base::PathExists(destination.Append(L"staging_marker.txt")));
  EXPECT_FALSE(cleanup_deferred);
}

TEST_F(InstallerFileOpsTest, ReplacesCompleteDestinationWithStaging) {
  const std::string version_string = "137.3.5";
  Version version = Version::Parse(version_string);
  base::FilePath source = temp_dir_.GetPath().Append(L"verified_source");
  VersionMetadata expected = CreateCompleteDistribution(source, version_string);
  ASSERT_TRUE(base::WriteFile(
      source.Append(kReleaseSubdirectory).Append(kLibcefFilename),
      "verified staging bytes"));
  ASSERT_TRUE(base::WriteFile(source.Append(L"staging_marker.txt"), "staging"));
  base::FilePath destination = GetVersionPath(install_dir_, version);
  CreateCompleteDistribution(destination, version_string);
  ASSERT_TRUE(base::WriteFile(
      destination.Append(kReleaseSubdirectory).Append(kLibcefFilename),
      "orphan bytes"));
  ASSERT_TRUE(
      base::WriteFile(destination.Append(L"old_marker.txt"), "old target"));

  bool cleanup_deferred = false;
  EXPECT_EQ(FileOpsError::kSuccess,
            InstallVersion(source, install_dir_, version, &cleanup_deferred));
  EXPECT_FALSE(base::PathExists(source));
  EXPECT_EQ(DistributionValidation::kComplete,
            ValidateDistribution(install_dir_, destination, expected));
  std::string installed_bytes;
  ASSERT_TRUE(base::ReadFileToString(
      destination.Append(kReleaseSubdirectory).Append(kLibcefFilename),
      &installed_bytes));
  EXPECT_EQ("verified staging bytes", installed_bytes);
  EXPECT_FALSE(base::PathExists(destination.Append(L"old_marker.txt")));
  EXPECT_TRUE(base::PathExists(destination.Append(L"staging_marker.txt")));
  EXPECT_FALSE(cleanup_deferred);
}

TEST_F(InstallerFileOpsTest, ReplacesEmptyDirectoryDestination) {
  const std::string version_string = "137.3.5";
  Version version = Version::Parse(version_string);
  base::FilePath source = temp_dir_.GetPath().Append(L"empty_target_source");
  CreateCompleteDistribution(source, version_string);
  ASSERT_TRUE(base::WriteFile(source.Append(L"staging_marker.txt"), "staging"));
  base::FilePath destination = GetVersionPath(install_dir_, version);
  ASSERT_TRUE(base::CreateDirectory(destination));

  EXPECT_EQ(FileOpsError::kSuccess,
            InstallVersion(source, install_dir_, version));
  EXPECT_TRUE(base::PathExists(destination.Append(L"staging_marker.txt")));
}

TEST_F(InstallerFileOpsTest, ReplacesRegularFileDestination) {
  const std::string version_string = "137.3.5";
  Version version = Version::Parse(version_string);
  base::FilePath source = temp_dir_.GetPath().Append(L"file_target_source");
  CreateCompleteDistribution(source, version_string);
  ASSERT_TRUE(base::WriteFile(source.Append(L"staging_marker.txt"), "staging"));
  base::FilePath destination = GetVersionPath(install_dir_, version);
  ASSERT_TRUE(base::CreateDirectory(destination.DirName()));
  ASSERT_TRUE(base::WriteFile(destination, "old regular file"));

  EXPECT_EQ(FileOpsError::kSuccess,
            InstallVersion(source, install_dir_, version));
  EXPECT_TRUE(base::DirectoryExists(destination));
  EXPECT_TRUE(base::PathExists(destination.Append(L"staging_marker.txt")));
}

TEST_F(InstallerFileOpsTest, RejectsReparsePointDestination) {
  const std::string version_string = "137.3.5";
  Version version = Version::Parse(version_string);
  base::FilePath source = temp_dir_.GetPath().Append(L"reparse_source");
  CreateCompleteDistribution(source, version_string);
  base::FilePath external = temp_dir_.GetPath().Append(L"external_target");
  ASSERT_TRUE(base::CreateDirectory(external));
  ASSERT_TRUE(base::WriteFile(external.Append(L"sentinel.txt"), "sentinel"));
  base::FilePath destination = GetVersionPath(install_dir_, version);
  ASSERT_TRUE(base::CreateDirectory(destination));
  auto reparse_point =
      base::test::FilePathReparsePoint::Create(destination, external);
  ASSERT_TRUE(reparse_point.has_value());

  EXPECT_EQ(FileOpsError::kQuarantineFailed,
            InstallVersion(source, install_dir_, version));
  EXPECT_TRUE(base::PathExists(source));
  EXPECT_TRUE(base::PathExists(destination));
  EXPECT_TRUE(base::PathExists(external.Append(L"sentinel.txt")));
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kTrashSubdirectory)));
}

TEST_F(InstallerFileOpsTest, ReplacementFaultBoundaries) {
  const std::string version_string = "137.3.5";
  Version version = Version::Parse(version_string);

  for (FileOpsFault fault :
       {FileOpsFault::kQuarantineMove, FileOpsFault::kRepairMove}) {
    base::FilePath source = temp_dir_.GetPath().Append(
        fault == FileOpsFault::kQuarantineMove ? L"quarantine_source"
                                               : L"repair_move_source");
    CreateCompleteDistribution(source, version_string);
    ASSERT_TRUE(
        base::WriteFile(source.Append(L"staging_marker.txt"), "staging"));
    base::FilePath destination = GetVersionPath(install_dir_, version);
    ASSERT_TRUE(base::CreateDirectory(destination));
    ASSERT_TRUE(
        base::WriteFile(destination.Append(L"old_marker.txt"), "old target"));

    SetFileOpsFaultForTesting(fault);
    FileOpsError error = InstallVersion(source, install_dir_, version);
    SetFileOpsFaultForTesting(FileOpsFault::kNone);
    EXPECT_EQ(fault == FileOpsFault::kQuarantineMove
                  ? FileOpsError::kQuarantineFailed
                  : FileOpsError::kRepairFailed,
              error);

    EXPECT_TRUE(base::PathExists(source));
    base::FilePath trash = install_dir_.Append(kTrashSubdirectory);
    base::FileEnumerator entries(
        trash, false,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    base::FilePath quarantined = entries.Next();
    if (fault == FileOpsFault::kQuarantineMove) {
      EXPECT_TRUE(base::PathExists(destination.Append(L"old_marker.txt")));
      EXPECT_TRUE(quarantined.empty());
    } else {
      EXPECT_FALSE(base::PathExists(destination));
      ASSERT_FALSE(quarantined.empty());
      EXPECT_TRUE(base::PathExists(quarantined.Append(L"old_marker.txt")));
    }

    base::DeletePathRecursively(destination);
    base::DeletePathRecursively(source);
    base::DeletePathRecursively(trash);
  }
}

TEST_F(InstallerFileOpsTest, ReplacementCleanupDeferredAndRetryable) {
  const std::string version_string = "137.3.5";
  Version version = Version::Parse(version_string);
  base::FilePath source = temp_dir_.GetPath().Append(L"deferred_source");
  CreateCompleteDistribution(source, version_string);
  ASSERT_TRUE(base::WriteFile(source.Append(L"staging_marker.txt"), "staging"));
  base::FilePath destination = GetVersionPath(install_dir_, version);
  ASSERT_TRUE(base::CreateDirectory(destination));
  ASSERT_TRUE(
      base::WriteFile(destination.Append(L"old_marker.txt"), "old target"));

  bool cleanup_deferred = false;
  SetFileOpsFaultForTesting(FileOpsFault::kTrashReclaim);
  EXPECT_EQ(FileOpsError::kSuccess,
            InstallVersion(source, install_dir_, version, &cleanup_deferred));
  SetFileOpsFaultForTesting(FileOpsFault::kNone);

  EXPECT_TRUE(cleanup_deferred);
  EXPECT_TRUE(base::PathExists(destination.Append(L"staging_marker.txt")));
  EXPECT_FALSE(base::PathExists(destination.Append(L"old_marker.txt")));
  EXPECT_EQ(1, RetryPendingDeletions(install_dir_));
}

TEST_F(InstallerFileOpsTest, TrashFaultBoundaries) {
  const std::string version_string = "137.3.5";
  Version version = Version::Parse(version_string);

  CreateInstalledVersion(version_string);
  SetFileOpsFaultForTesting(FileOpsFault::kTrashMove);
  EXPECT_EQ(FileOpsError::kRenameFailed,
            UninstallVersion(install_dir_, version));
  SetFileOpsFaultForTesting(FileOpsFault::kNone);
  EXPECT_TRUE(base::PathExists(GetVersionPath(install_dir_, version)));

  SetFileOpsFaultForTesting(FileOpsFault::kTrashReclaim);
  EXPECT_EQ(FileOpsError::kInUse, UninstallVersion(install_dir_, version));
  SetFileOpsFaultForTesting(FileOpsFault::kNone);
  EXPECT_FALSE(base::PathExists(GetVersionPath(install_dir_, version)));
  base::FilePath trash = install_dir_.Append(kTrashSubdirectory);
  base::FileEnumerator entries(trash, false, base::FileEnumerator::DIRECTORIES);
  EXPECT_FALSE(entries.Next().empty());
}

// =============================================================================
// UninstallVersion Tests
// =============================================================================

TEST_F(InstallerFileOpsTest, UninstallVersionSuccess) {
  CreateInstalledVersion("137.3.5");
  Version version = Version::Parse("137.3.5");

  // Verify it exists before uninstall
  base::FilePath version_path = GetVersionPath(install_dir_, version);
  ASSERT_TRUE(base::DirectoryExists(version_path));

  FileOpsError result = UninstallVersion(install_dir_, version);

  EXPECT_EQ(FileOpsError::kSuccess, result);

  // Version directory should be gone
  EXPECT_FALSE(base::DirectoryExists(version_path));
}

TEST_F(InstallerFileOpsTest, UninstallVersionNotFound) {
  Version version = Version::Parse("999.0.0");

  FileOpsError result = UninstallVersion(install_dir_, version);

  EXPECT_EQ(FileOpsError::kDestinationNotFound, result);
}

TEST_F(InstallerFileOpsTest, UninstallVersionInUse) {
  CreateInstalledVersion("137.3.5");
  Version version = Version::Parse("137.3.5");
  base::FilePath version_path = GetVersionPath(install_dir_, version);

  // Lock a file in the directory to simulate "in use"
  base::FilePath locked_file = version_path.Append(L"locked.dll");
  base::WriteFile(locked_file, "test");

  // Open with exclusive access to lock it
  HANDLE handle =
      ::CreateFileW(locked_file.value().c_str(), GENERIC_READ | GENERIC_WRITE,
                    0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE(INVALID_HANDLE_VALUE, handle);

  FileOpsError result = UninstallVersion(install_dir_, version);

  // When a file is locked, different things can happen:
  // - kInUse: MoveFileEx detects sharing violation
  // - kAccessDenied: MoveFileEx returns access denied for locked directory
  // - kSuccess: Rename succeeded but delete failed (kInUse semantics)
  // All are acceptable outcomes for a locked file scenario
  EXPECT_TRUE(result == FileOpsError::kInUse ||
              result == FileOpsError::kAccessDenied ||
              result == FileOpsError::kSuccess)
      << "Unexpected error: " << FileOpsErrorToString(result);

  ::CloseHandle(handle);

  // Clean up - retry deletions
  RetryPendingDeletions(install_dir_);
}

TEST_F(InstallerFileOpsTest, UninstallVersionCleansEmptyParent) {
  CreateInstalledVersion("137.3.5");
  Version version = Version::Parse("137.3.5");

  // Verify parent version dir exists: Versions/137.3.5/
  base::FilePath version_parent =
      install_dir_.Append(kVersionsSubdirectory).Append(L"137.3.5");
  ASSERT_TRUE(base::DirectoryExists(version_parent));

  // Verify platform dir exists: Versions/137.3.5/<platform>/
  base::FilePath version_path = GetVersionPath(install_dir_, version);
  ASSERT_TRUE(base::DirectoryExists(version_path));

  FileOpsError result = UninstallVersion(install_dir_, version);
  EXPECT_EQ(FileOpsError::kSuccess, result);

  // Platform directory should be gone
  EXPECT_FALSE(base::DirectoryExists(version_path));

  // Parent version directory should also be cleaned up since it's now empty
  EXPECT_FALSE(base::DirectoryExists(version_parent));
}

TEST_F(InstallerFileOpsTest, VersionLeaseBlocksWholeDirectoryMove) {
  const Version version = Version::Parse("137.3.5");
  const base::FilePath version_path = GetVersionPath(install_dir_, version);
  CreateCompleteDistribution(version_path, version.ToString());
  std::unique_ptr<VersionLease> lease;
  ASSERT_EQ(VersionLeaseError::kSuccess,
            AcquireVersionLease(install_dir_, version_path, &lease));
  ASSERT_TRUE(lease);
  EXPECT_TRUE(lease->IsValid());

  const base::FilePath moved = temp_dir_.GetPath().Append(L"leased_moved");
  EXPECT_FALSE(MoveFileExW(version_path.value().c_str(), moved.value().c_str(),
                           MOVEFILE_WRITE_THROUGH));
  EXPECT_TRUE(base::DirectoryExists(version_path));

  lease.reset();
  EXPECT_TRUE(MoveFileExW(version_path.value().c_str(), moved.value().c_str(),
                          MOVEFILE_WRITE_THROUGH));
}

TEST_F(InstallerFileOpsTest, MultipleVersionLeaseReaders) {
  const Version version = Version::Parse("137.3.5");
  const base::FilePath version_path = GetVersionPath(install_dir_, version);
  CreateCompleteDistribution(version_path, version.ToString());
  std::unique_ptr<VersionLease> first;
  std::unique_ptr<VersionLease> second;
  EXPECT_EQ(VersionLeaseError::kSuccess,
            AcquireVersionLease(install_dir_, version_path, &first));
  EXPECT_EQ(VersionLeaseError::kSuccess,
            AcquireVersionLease(install_dir_, version_path, &second));
  EXPECT_TRUE(first);
  EXPECT_TRUE(second);
}

TEST_F(InstallerFileOpsTest, VersionLeaseDetectsLostRemovalRace) {
  const Version version = Version::Parse("137.3.5");
  const base::FilePath version_path = GetVersionPath(install_dir_, version);
  std::unique_ptr<VersionLease> lease;
  EXPECT_EQ(VersionLeaseError::kLostRace,
            AcquireVersionLease(install_dir_, version_path, &lease));
  EXPECT_FALSE(lease);
}

TEST_F(InstallerFileOpsTest, ProcessTerminationReleasesVersionLease) {
  const Version version = Version::Parse("137.3.5");
  const base::FilePath version_path = GetVersionPath(install_dir_, version);
  CreateCompleteDistribution(version_path, version.ToString());
  std::wstring event_name = std::wstring(L"Local") + static_cast<wchar_t>(92) +
                            L"CEF_Installer_Lease_Test_" +
                            std::to_wstring(GetCurrentProcessId()) + L"_" +
                            std::to_wstring(GetTickCount64());
  HANDLE event = CreateEventW(nullptr, TRUE, FALSE, event_name.c_str());
  ASSERT_NE(nullptr, event);

  wchar_t executable[MAX_PATH] = {};
  ASSERT_GT(GetModuleFileNameW(nullptr, executable, MAX_PATH), 0u);
  const wchar_t quote = static_cast<wchar_t>(34);
  std::wstring command_line(1, quote);
  command_line += executable;
  command_line += quote;
  command_line +=
      L" --single-process-tests "
      L"--gtest_filter=InstallerVersionLeaseChildTest.HoldUntilKilled "
      L"--cef-lease-child-root=";
  command_line += quote;
  command_line += install_dir_.value();
  command_line += quote;
  command_line += L" --cef-lease-child-version=";
  command_line += quote;
  command_line += version_path.value();
  command_line += quote;
  command_line += L" --cef-lease-child-event=" + event_name;
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process = {};
  ASSERT_TRUE(CreateProcessW(nullptr, command_line.data(), nullptr, nullptr,
                             FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                             &startup, &process));
  CloseHandle(process.hThread);
  ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(event, 10000));
  CloseHandle(event);

  const base::FilePath moved =
      temp_dir_.GetPath().Append(L"process_lease_moved");
  EXPECT_FALSE(MoveFileExW(version_path.value().c_str(), moved.value().c_str(),
                           MOVEFILE_WRITE_THROUGH));
  ASSERT_TRUE(TerminateProcess(process.hProcess, 1));
  ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(process.hProcess, 10000));
  CloseHandle(process.hProcess);
  EXPECT_TRUE(MoveFileExW(version_path.value().c_str(), moved.value().c_str(),
                          MOVEFILE_WRITE_THROUGH));
}

// =============================================================================
// RetryPendingDeletions Tests
// =============================================================================

TEST_F(InstallerFileOpsTest, RetryPendingDeletions) {
  // Create .trash/ with entries
  base::FilePath trash_root = install_dir_.Append(L".trash");
  ASSERT_TRUE(base::CreateDirectory(trash_root));

  base::FilePath trash1 = trash_root.Append(L"137.3.5_ABCDEF1234567890");
  base::FilePath trash2 = trash_root.Append(L"138.0.0_1234567890ABCDEF");
  ASSERT_TRUE(base::CreateDirectory(trash1));
  ASSERT_TRUE(base::CreateDirectory(trash2));

  // Add some files to make them non-empty
  base::WriteFile(trash1.Append(L"file.txt"), "test");
  base::WriteFile(trash2.Append(L"file.txt"), "test");

  int deleted = RetryPendingDeletions(install_dir_);

  EXPECT_EQ(2, deleted);
  EXPECT_FALSE(base::DirectoryExists(trash1));
  EXPECT_FALSE(base::DirectoryExists(trash2));
}

TEST_F(InstallerFileOpsTest, RetryPendingDeletionsPartialSuccess) {
  // Create .trash/ with two entries
  base::FilePath trash_root = install_dir_.Append(L".trash");
  ASSERT_TRUE(base::CreateDirectory(trash_root));

  base::FilePath trash1 = trash_root.Append(L"137.3.5_ABCDEF1234567890");
  base::FilePath trash2 = trash_root.Append(L"138.0.0_1234567890ABCDEF");
  ASSERT_TRUE(base::CreateDirectory(trash1));
  ASSERT_TRUE(base::CreateDirectory(trash2));

  // Lock a file in trash1
  base::FilePath locked_file = trash1.Append(L"locked.dll");
  base::WriteFile(locked_file, "test");
  HANDLE handle =
      ::CreateFileW(locked_file.value().c_str(), GENERIC_READ | GENERIC_WRITE,
                    0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE(INVALID_HANDLE_VALUE, handle);

  int deleted = RetryPendingDeletions(install_dir_);

  // trash2 should be deleted, trash1 should remain (locked)
  EXPECT_EQ(1, deleted);
  EXPECT_TRUE(base::DirectoryExists(trash1));
  EXPECT_FALSE(base::DirectoryExists(trash2));

  ::CloseHandle(handle);

  // Now retry - should delete the remaining one
  deleted = RetryPendingDeletions(install_dir_);
  EXPECT_EQ(1, deleted);
  EXPECT_FALSE(base::DirectoryExists(trash1));
}

TEST_F(InstallerFileOpsTest, RetryPendingDeletionsRegularFileTrash) {
  base::FilePath trash_root = install_dir_.Append(kTrashSubdirectory);
  ASSERT_TRUE(base::CreateDirectory(trash_root));
  base::FilePath trash_file = trash_root.Append(L"137.3.5_file");
  ASSERT_TRUE(base::WriteFile(trash_file, "old target"));

  EXPECT_EQ(1, RetryPendingDeletions(install_dir_));
  EXPECT_FALSE(base::PathExists(trash_file));
}

TEST_F(InstallerFileOpsTest,
       RetryPendingDeletionsTrashDoesNotTraverseReparsePoint) {
  base::FilePath external = temp_dir_.GetPath().Append(L"external_reclaim");
  ASSERT_TRUE(base::CreateDirectory(external));
  base::FilePath sentinel = external.Append(L"sentinel.txt");
  ASSERT_TRUE(base::WriteFile(sentinel, "preserve"));

  base::FilePath trash_root = install_dir_.Append(kTrashSubdirectory);
  base::FilePath trash_entry = trash_root.Append(L"137.3.5_reparse");
  base::FilePath nested = trash_entry.Append(L"nested");
  ASSERT_TRUE(base::CreateDirectory(nested));
  auto reparse_point =
      base::test::FilePathReparsePoint::Create(nested, external);
  ASSERT_TRUE(reparse_point.has_value());
  ASSERT_TRUE(base::PathExists(nested.Append(L"sentinel.txt")));

  EXPECT_EQ(1, RetryPendingDeletions(install_dir_));
  EXPECT_FALSE(base::PathExists(trash_entry));
  EXPECT_TRUE(base::PathExists(sentinel));
}

// =============================================================================
// FileOpsErrorToString Tests
// =============================================================================

TEST_F(InstallerFileOpsTest, FileOpsErrorToString) {
  EXPECT_STREQ("Success", FileOpsErrorToString(FileOpsError::kSuccess));
  EXPECT_STREQ("Source directory not found",
               FileOpsErrorToString(FileOpsError::kSourceNotFound));
  EXPECT_STREQ("Destination already exists",
               FileOpsErrorToString(FileOpsError::kDestinationExists));
  EXPECT_STREQ("Destination not found",
               FileOpsErrorToString(FileOpsError::kDestinationNotFound));
  EXPECT_STREQ("Access denied",
               FileOpsErrorToString(FileOpsError::kAccessDenied));
  EXPECT_STREQ("Disk full", FileOpsErrorToString(FileOpsError::kDiskFull));
  EXPECT_STREQ("Files are in use", FileOpsErrorToString(FileOpsError::kInUse));
  EXPECT_STREQ("Rename operation failed",
               FileOpsErrorToString(FileOpsError::kRenameFailed));
  EXPECT_STREQ("Delete operation failed",
               FileOpsErrorToString(FileOpsError::kDeleteFailed));
}

TEST_F(InstallerFileOpsTest, IsReparsePointRegularDir) {
  // A regular directory should not be a reparse point
  EXPECT_FALSE(IsReparsePoint(temp_dir_.GetPath()));
  EXPECT_FALSE(IsReparsePoint(install_dir_));
}

TEST_F(InstallerFileOpsTest, IsReparsePointNonExistent) {
  // Non-existent path should return false
  base::FilePath nonexistent = temp_dir_.GetPath().Append(L"does_not_exist");
  EXPECT_FALSE(IsReparsePoint(nonexistent));
}

// =============================================================================
// VerifySafeFilePath Tests
// =============================================================================

TEST_F(InstallerFileOpsTest, VerifySafeFilePathRegularFile) {
  base::FilePath file = temp_dir_.GetPath().Append(L"safe.txt");
  base::WriteFile(file, "content");
  EXPECT_TRUE(VerifySafeFilePath(file));
  // File should still exist when delete_if_exists is false.
  EXPECT_TRUE(base::PathExists(file));
}

TEST_F(InstallerFileOpsTest, VerifySafeFilePathNonExistent) {
  base::FilePath missing = temp_dir_.GetPath().Append(L"missing.txt");
  EXPECT_TRUE(VerifySafeFilePath(missing));
}

TEST_F(InstallerFileOpsTest, VerifySafeFilePathDeleteIfExists) {
  base::FilePath file = temp_dir_.GetPath().Append(L"to_delete.txt");
  base::WriteFile(file, "content");
  EXPECT_TRUE(VerifySafeFilePath(file, /*delete_if_exists=*/true));
  EXPECT_FALSE(base::PathExists(file));
}

TEST_F(InstallerFileOpsTest, VerifySafeFilePathDeleteNonExistent) {
  base::FilePath missing = temp_dir_.GetPath().Append(L"missing.txt");
  // Should succeed even if file doesn't exist.
  EXPECT_TRUE(VerifySafeFilePath(missing, /*delete_if_exists=*/true));
}

// =============================================================================
// VerifySafeDirectoryPath Tests
// =============================================================================

TEST_F(InstallerFileOpsTest, VerifySafeDirectoryPathRegularDir) {
  EXPECT_TRUE(VerifySafeDirectoryPath(install_dir_));
  // Directory should still exist when delete_if_exists is false.
  EXPECT_TRUE(base::DirectoryExists(install_dir_));
}

TEST_F(InstallerFileOpsTest, VerifySafeDirectoryPathNonExistent) {
  base::FilePath missing = temp_dir_.GetPath().Append(L"missing_dir");
  EXPECT_TRUE(VerifySafeDirectoryPath(missing));
}

TEST_F(InstallerFileOpsTest, VerifySafeDirectoryPathDeleteIfExists) {
  base::FilePath dir = temp_dir_.GetPath().Append(L"dir_to_delete");
  ASSERT_TRUE(base::CreateDirectory(dir));
  base::WriteFile(dir.Append(L"child.txt"), "data");

  EXPECT_TRUE(VerifySafeDirectoryPath(dir, /*delete_if_exists=*/true));
  EXPECT_FALSE(base::DirectoryExists(dir));
}

TEST_F(InstallerFileOpsTest, VerifySafeDirectoryPathDeleteNonExistent) {
  base::FilePath missing = temp_dir_.GetPath().Append(L"missing_dir");
  EXPECT_TRUE(VerifySafeDirectoryPath(missing, /*delete_if_exists=*/true));
}

// =============================================================================
// IsReadableDirectory Tests
// =============================================================================

TEST_F(InstallerFileOpsTest, IsReadableDirectorySuccess) {
  EXPECT_TRUE(IsReadableDirectory(install_dir_));
}

TEST_F(InstallerFileOpsTest, IsReadableDirectoryNonExistent) {
  base::FilePath nonexistent = temp_dir_.GetPath().Append(L"does_not_exist");
  EXPECT_FALSE(IsReadableDirectory(nonexistent));
}

TEST_F(InstallerFileOpsTest, IsReadableDirectoryEmptyPath) {
  EXPECT_FALSE(IsReadableDirectory(base::FilePath()));
}

TEST_F(InstallerFileOpsTest, IsReadableDirectoryFile) {
  base::FilePath file_path = temp_dir_.GetPath().Append(L"afile.txt");
  base::WriteFile(file_path, "content");
  EXPECT_FALSE(IsReadableDirectory(file_path));
}

// =============================================================================
// IsPathSafeForLoading Tests
// =============================================================================

TEST_F(InstallerFileOpsTest, IsPathSafeForLoadingNormalHierarchy) {
  // Create root/a/b/file.dll
  base::FilePath root = temp_dir_.GetPath().Append(L"root");
  base::FilePath a = root.Append(L"a");
  base::FilePath b = a.Append(L"b");
  ASSERT_TRUE(base::CreateDirectory(b));
  base::FilePath file = b.Append(L"file.dll");
  ASSERT_TRUE(base::WriteFile(file, "content"));

  EXPECT_TRUE(IsPathSafeForLoading(root, file));
}

TEST_F(InstallerFileOpsTest, IsPathSafeForLoadingNonexistentComponent) {
  base::FilePath root = temp_dir_.GetPath().Append(L"root");
  ASSERT_TRUE(base::CreateDirectory(root));
  base::FilePath missing = root.Append(L"missing").Append(L"file.dll");

  EXPECT_FALSE(IsPathSafeForLoading(root, missing));
}

TEST_F(InstallerFileOpsTest, IsPathSafeForLoadingPathNotUnderRoot) {
  base::FilePath root = temp_dir_.GetPath().Append(L"root");
  base::FilePath other = temp_dir_.GetPath().Append(L"other");
  ASSERT_TRUE(base::CreateDirectory(root));
  ASSERT_TRUE(base::CreateDirectory(other));
  base::FilePath file = other.Append(L"file.dll");
  ASSERT_TRUE(base::WriteFile(file, "content"));

  EXPECT_FALSE(IsPathSafeForLoading(root, file));
}

TEST_F(InstallerFileOpsTest, IsPathSafeForLoadingEmptyRoot) {
  EXPECT_FALSE(IsPathSafeForLoading(base::FilePath(), install_dir_));
}

TEST_F(InstallerFileOpsTest, IsPathSafeForLoadingEmptyPath) {
  EXPECT_FALSE(IsPathSafeForLoading(install_dir_, base::FilePath()));
}

TEST_F(InstallerFileOpsTest, IsPathSafeForLoadingRootEqualsPath) {
  // Path is not under root (IsParent returns false when they're equal)
  EXPECT_FALSE(IsPathSafeForLoading(install_dir_, install_dir_));
}

TEST_F(InstallerFileOpsTest, IsPathSafeForLoadingRelativePaths) {
  EXPECT_FALSE(
      IsPathSafeForLoading(base::FilePath(L"relative\\root"),
                           base::FilePath(L"relative\\root\\file.dll")));
}

TEST_F(InstallerFileOpsTest, IsPathSafeForLoadingHonorsSoftDeadline) {
  bool time_limit_reached = false;
  EXPECT_FALSE(
      IsPathSafeForLoading(install_dir_, install_dir_.Append(L"file.dll"),
                           base::TimeTicks::Now(), &time_limit_reached));
  EXPECT_TRUE(time_limit_reached);
}

// =============================================================================
// IsSameDirectory Tests
// =============================================================================

TEST_F(InstallerFileOpsTest, IsSameDirectorySamePath) {
  EXPECT_TRUE(IsSameDirectory(install_dir_, install_dir_));
}

TEST_F(InstallerFileOpsTest, IsSameDirectoryDifferentCase) {
  // On Windows, paths differing only in case should be the same.
  base::FilePath upper(L"C:\\WINDOWS\\TEMP");
  base::FilePath lower(L"C:\\windows\\temp");
  EXPECT_TRUE(IsSameDirectory(upper, lower));
}

TEST_F(InstallerFileOpsTest, IsSameDirectoryDifferentPaths) {
  base::FilePath dir_a = temp_dir_.GetPath().Append(L"DirA");
  base::FilePath dir_b = temp_dir_.GetPath().Append(L"DirB");
  ASSERT_TRUE(base::CreateDirectory(dir_a));
  ASSERT_TRUE(base::CreateDirectory(dir_b));
  EXPECT_FALSE(IsSameDirectory(dir_a, dir_b));
}

// =============================================================================
// ScopedFileDeleter Tests
// =============================================================================

TEST_F(InstallerFileOpsTest, ScopedFileDeleterDeletesOnDestruction) {
  base::FilePath file = temp_dir_.GetPath().Append(L"to_delete.txt");
  base::WriteFile(file, "content");
  ASSERT_TRUE(base::PathExists(file));

  {
    ScopedFileDeleter deleter(file);
  }

  EXPECT_FALSE(base::PathExists(file));
}

TEST_F(InstallerFileOpsTest, ScopedFileDeleterReleasePreventsDeletion) {
  base::FilePath file = temp_dir_.GetPath().Append(L"keep.txt");
  base::WriteFile(file, "content");
  ASSERT_TRUE(base::PathExists(file));

  {
    ScopedFileDeleter deleter(file);
    deleter.Release();
  }

  EXPECT_TRUE(base::PathExists(file));
}

TEST_F(InstallerFileOpsTest, ScopedFileDeleterEmptyPathIsNoOp) {
  // Should not crash when constructed with an empty path.
  {
    ScopedFileDeleter deleter{base::FilePath()};
  }
}

TEST_F(InstallerFileOpsTest, ScopedFileDeleterNonexistentPathIsNoOp) {
  base::FilePath missing = temp_dir_.GetPath().Append(L"no_such_file.txt");
  ASSERT_FALSE(base::PathExists(missing));

  // Should not crash when the file doesn't exist.
  {
    ScopedFileDeleter deleter(missing);
  }
}

// =============================================================================
// ScopedDirectoryDeleter Tests
// =============================================================================

TEST_F(InstallerFileOpsTest, ScopedDirectoryDeleterDeletesOnDestruction) {
  base::FilePath dir = temp_dir_.GetPath().Append(L"dir_to_delete");
  ASSERT_TRUE(base::CreateDirectory(dir));
  base::WriteFile(dir.Append(L"child.txt"), "data");
  ASSERT_TRUE(base::DirectoryExists(dir));

  {
    ScopedDirectoryDeleter deleter(dir);
  }

  EXPECT_FALSE(base::DirectoryExists(dir));
}

TEST_F(InstallerFileOpsTest, ScopedDirectoryDeleterReleasePreventsDeletion) {
  base::FilePath dir = temp_dir_.GetPath().Append(L"dir_to_keep");
  ASSERT_TRUE(base::CreateDirectory(dir));
  base::WriteFile(dir.Append(L"child.txt"), "data");

  {
    ScopedDirectoryDeleter deleter(dir);
    deleter.Release();
  }

  EXPECT_TRUE(base::DirectoryExists(dir));
  EXPECT_TRUE(base::PathExists(dir.Append(L"child.txt")));
}

TEST_F(InstallerFileOpsTest, ScopedDirectoryDeleterDeletesNestedContents) {
  base::FilePath dir = temp_dir_.GetPath().Append(L"nested_dir");
  base::FilePath sub = dir.Append(L"sub");
  ASSERT_TRUE(base::CreateDirectory(sub));
  base::WriteFile(sub.Append(L"deep.txt"), "deep");

  {
    ScopedDirectoryDeleter deleter(dir);
  }

  EXPECT_FALSE(base::DirectoryExists(dir));
}

TEST_F(InstallerFileOpsTest, ScopedDirectoryDeleterEmptyPathIsNoOp) {
  {
    ScopedDirectoryDeleter deleter{base::FilePath()};
  }
}

TEST_F(InstallerFileOpsTest, ScopedDirectoryDeleterNonexistentPathIsNoOp) {
  base::FilePath missing = temp_dir_.GetPath().Append(L"no_such_dir");
  ASSERT_FALSE(base::DirectoryExists(missing));

  {
    ScopedDirectoryDeleter deleter(missing);
  }
}

}  // namespace
}  // namespace cef_installer
