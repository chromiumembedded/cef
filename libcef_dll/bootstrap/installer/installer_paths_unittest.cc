// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"

#include <windows.h>

#include <algorithm>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/file_path_reparse_point_win.h"
#include "base/win/registry.h"
#include "base/win/security_descriptor.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace cef_installer {

using internal::ClearInstallDirectoryOverridesForTesting;
using internal::GetPhysicalPathContainment;
using internal::OverrideInstallDirectoriesForTesting;
using internal::OverrideInstallDirectoryCandidatesForTesting;
using internal::PathContainment;

namespace {

class InstallerPathsTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {
    ClearInstallDirectoryOverridesForTesting();
    // Clean up any registry keys we created
    base::win::RegKey key(HKEY_CURRENT_USER, L"SOFTWARE", KEY_WRITE);
    key.DeleteKey(L"CEF_Test");
  }

  base::ScopedTempDir temp_dir_;
};

// ============================================================================
// ResolveInstallDirectories — custom path (exclusive, fail-hard)
// ============================================================================

TEST_F(InstallerPathsTest, ResolveCustomPath_WritableDir) {
  base::FilePath custom_path = temp_dir_.GetPath().Append(L"CustomCEF");
  ASSERT_TRUE(base::CreateDirectory(custom_path));

  auto dirs = ResolveInstallDirectories(custom_path.AsUTF8Unsafe());

  EXPECT_EQ(PathError::kSuccess, dirs.write_error);
  EXPECT_EQ(custom_path, dirs.writable_dir);
  ASSERT_EQ(1u, dirs.readable_dirs.size());
  EXPECT_EQ(custom_path, dirs.readable_dirs[0]);
}

TEST_F(InstallerPathsTest, ResolveCustomPath_CreatesNewDir) {
  base::FilePath new_dir = temp_dir_.GetPath().Append(L"NewCEFDir");
  ASSERT_FALSE(base::DirectoryExists(new_dir));

  auto dirs = ResolveInstallDirectories(new_dir.AsUTF8Unsafe());

  EXPECT_EQ(PathError::kSuccess, dirs.write_error);
  EXPECT_EQ(new_dir, dirs.writable_dir);
  ASSERT_EQ(1u, dirs.readable_dirs.size());
  EXPECT_EQ(new_dir, dirs.readable_dirs[0]);
  EXPECT_TRUE(base::DirectoryExists(new_dir));
}

TEST_F(InstallerPathsTest, ResolveCustomPath_FileNotDir) {
  base::FilePath file_path = temp_dir_.GetPath().Append(L"not_a_dir");
  ASSERT_TRUE(base::WriteFile(file_path, "data"));

  auto dirs = ResolveInstallDirectories(file_path.AsUTF8Unsafe());

  EXPECT_EQ(PathError::kInvalidPath, dirs.write_error);
  EXPECT_TRUE(dirs.writable_dir.empty());
  // Custom path is exclusive — no readable dirs either (it's a file, not a dir)
  EXPECT_TRUE(dirs.readable_dirs.empty());
}

TEST_F(InstallerPathsTest, ResolveCustomPath_Exclusive) {
  // Custom path should not fall through to default search even if it fails.
  // Use a nonexistent path whose parent doesn't exist either, so creation
  // also fails.
  base::FilePath impossible = base::FilePath(L"Z:\\NonExistent\\Path\\CEF");

  auto dirs = ResolveInstallDirectories(impossible.AsUTF8Unsafe());

  EXPECT_NE(PathError::kSuccess, dirs.write_error);
  EXPECT_TRUE(dirs.writable_dir.empty());
  // Should NOT contain any default directories (ProgramFiles, etc.)
  EXPECT_TRUE(dirs.readable_dirs.empty());
}

// ============================================================================
// ResolveInstallDirectories — default search order
// ============================================================================

TEST_F(InstallerPathsTest, ResolveDefault_FindsSomeDirectory) {
  // Without a custom path and assuming no HKLM registry key,
  // it should find some valid location (ProgramFiles or LocalAppData).
  auto dirs = ResolveInstallDirectories("");

  // Should find at least one readable directory.
  EXPECT_FALSE(dirs.readable_dirs.empty());

  if (dirs.write_error == PathError::kSuccess) {
    EXPECT_FALSE(dirs.writable_dir.empty());
    // Writable dir should be the last readable dir (search stops there).
    EXPECT_EQ(dirs.writable_dir, dirs.readable_dirs.back());
  }
}

TEST_F(InstallerPathsTest, ResolveDefault_WritableDirIsLastReadable) {
  // The writable directory should always be the last entry in readable_dirs,
  // because the search stops as soon as a writable directory is found.
  auto dirs = ResolveInstallDirectories("");

  if (dirs.write_error == PathError::kSuccess) {
    ASSERT_FALSE(dirs.readable_dirs.empty());
    EXPECT_EQ(dirs.writable_dir, dirs.readable_dirs.back());
  }
}

// ============================================================================
// ResolveInstallDirectories — test override mechanism
// ============================================================================

TEST_F(InstallerPathsTest, ResolveOverride_WritableAndReadable) {
  base::ScopedTempDir readable_only;
  base::ScopedTempDir writable;
  ASSERT_TRUE(readable_only.CreateUniqueTempDir());
  ASSERT_TRUE(writable.CreateUniqueTempDir());

  OverrideInstallDirectoriesForTesting(
      {readable_only.GetPath(), writable.GetPath()}, writable.GetPath());

  auto dirs = ResolveInstallDirectories("");

  ClearInstallDirectoryOverridesForTesting();

  EXPECT_EQ(PathError::kSuccess, dirs.write_error);
  EXPECT_EQ(writable.GetPath(), dirs.writable_dir);
  ASSERT_EQ(2u, dirs.readable_dirs.size());
  EXPECT_EQ(readable_only.GetPath(), dirs.readable_dirs[0]);
  EXPECT_EQ(writable.GetPath(), dirs.readable_dirs[1]);
}

TEST_F(InstallerPathsTest, ResolveOverride_ReadableOnly) {
  base::ScopedTempDir readonly_dir;
  ASSERT_TRUE(readonly_dir.CreateUniqueTempDir());

  OverrideInstallDirectoriesForTesting({readonly_dir.GetPath()}, std::nullopt);

  auto dirs = ResolveInstallDirectories("");

  ClearInstallDirectoryOverridesForTesting();

  EXPECT_EQ(PathError::kNotFound, dirs.write_error);
  EXPECT_TRUE(dirs.writable_dir.empty());
  ASSERT_EQ(1u, dirs.readable_dirs.size());
  EXPECT_EQ(readonly_dir.GetPath(), dirs.readable_dirs[0]);
}

TEST_F(InstallerPathsTest, ResolveOverride_Empty) {
  OverrideInstallDirectoriesForTesting({}, std::nullopt);

  auto dirs = ResolveInstallDirectories("");

  ClearInstallDirectoryOverridesForTesting();

  EXPECT_EQ(PathError::kNotFound, dirs.write_error);
  EXPECT_TRUE(dirs.writable_dir.empty());
  EXPECT_TRUE(dirs.readable_dirs.empty());
}

TEST_F(InstallerPathsTest, RoleComesFromCandidateSource) {
  const base::FilePath path = temp_dir_.GetPath().Append(L"custom_spelling");
  OverrideInstallDirectoryCandidatesForTesting(
      {{path, DirectoryRole::kHklmDefault, true, true}});

  auto dirs = ResolveInstallDirectories("", {.is_elevated = true});
  ASSERT_EQ(PathError::kSuccess, dirs.write_error);
  ASSERT_TRUE(dirs.writable_role.has_value());
  EXPECT_EQ(DirectoryRole::kHklmDefault, *dirs.writable_role);
  ASSERT_EQ(1u, dirs.readable_roles.size());
  EXPECT_EQ(DirectoryRole::kHklmDefault, dirs.readable_roles[0]);
}

TEST_F(InstallerPathsTest, DuplicatePreservesHigherPriorityRole) {
  const base::FilePath path = temp_dir_.GetPath().Append(L"same");
  OverrideInstallDirectoryCandidatesForTesting(
      {{path, DirectoryRole::kHklmDefault, true, false},
       {path, DirectoryRole::kProgramFilesDefault, true, true}});

  auto dirs = ResolveInstallDirectories("", {.is_elevated = true});
  ASSERT_EQ(PathError::kSuccess, dirs.write_error);
  ASSERT_EQ(1u, dirs.readable_dirs.size());
  EXPECT_EQ(DirectoryRole::kHklmDefault, dirs.readable_roles[0]);
  EXPECT_EQ(DirectoryRole::kHklmDefault, dirs.writable_role);
}

TEST_F(InstallerPathsTest, DuplicateAdminRolePreventsPerUserMutation) {
  const base::FilePath path = temp_dir_.GetPath().Append(L"same");
  OverrideInstallDirectoryCandidatesForTesting(
      {{path, DirectoryRole::kHklmDefault, false, true},
       {path, DirectoryRole::kPerUserDefault, true, true}});

  const auto dirs =
      ResolveInstallDirectories("", {.mutation_capable = true,
                                     .allow_admin_mutation = false,
                                     .is_elevated = false});
  EXPECT_EQ(PathError::kNotFound, dirs.write_error);
  EXPECT_TRUE(dirs.writable_dir.empty());
  ASSERT_EQ(1u, dirs.readable_dirs.size());
  EXPECT_EQ(DirectoryRole::kHklmDefault, dirs.readable_roles[0]);
}

TEST_F(InstallerPathsTest, AdminMutationGateUsesPerUserForStandardUser) {
  const base::FilePath hklm = temp_dir_.GetPath().Append(L"hklm");
  const base::FilePath program_files =
      temp_dir_.GetPath().Append(L"program_files");
  const base::FilePath per_user = temp_dir_.GetPath().Append(L"per_user");
  OverrideInstallDirectoryCandidatesForTesting(
      {{hklm, DirectoryRole::kHklmDefault, true, true},
       {program_files, DirectoryRole::kProgramFilesDefault, true, true},
       {per_user, DirectoryRole::kPerUserDefault, true, true}});
  const auto dirs =
      ResolveInstallDirectories("", {.mutation_capable = true,
                                     .allow_admin_mutation = false,
                                     .is_elevated = false});
  ASSERT_EQ(PathError::kSuccess, dirs.write_error);
  EXPECT_EQ(per_user, dirs.writable_dir);
  EXPECT_EQ(DirectoryRole::kPerUserDefault, dirs.writable_role);
  ASSERT_EQ(3u, dirs.readable_roles.size());
}

TEST_F(InstallerPathsTest, GatedElevatedStopsBeforePerUser) {
  const base::FilePath hklm = temp_dir_.GetPath().Append(L"hklm");
  const base::FilePath program_files =
      temp_dir_.GetPath().Append(L"program_files");
  const base::FilePath per_user = temp_dir_.GetPath().Append(L"per_user");
  OverrideInstallDirectoryCandidatesForTesting(
      {{hklm, DirectoryRole::kHklmDefault, true, true},
       {program_files, DirectoryRole::kProgramFilesDefault, true, true},
       {per_user, DirectoryRole::kPerUserDefault, true, true}});
  const auto dirs =
      ResolveInstallDirectories("", {.mutation_capable = true,
                                     .allow_admin_mutation = false,
                                     .is_elevated = true});
  EXPECT_EQ(PathError::kNotFound, dirs.write_error);
  EXPECT_TRUE(dirs.writable_dir.empty());
  ASSERT_EQ(2u, dirs.readable_dirs.size());
  EXPECT_EQ(hklm, dirs.readable_dirs[0]);
  EXPECT_EQ(program_files, dirs.readable_dirs[1]);
}

TEST_F(InstallerPathsTest, PolicyExcludesPerUserBeforeResolution) {
  const base::FilePath hklm = temp_dir_.GetPath().Append(L"hklm");
  const base::FilePath program_files =
      temp_dir_.GetPath().Append(L"program_files");
  const base::FilePath per_user = temp_dir_.GetPath().Append(L"per_user");
  OverrideInstallDirectoryCandidatesForTesting(
      {{hklm, DirectoryRole::kHklmDefault, true, false},
       {program_files, DirectoryRole::kProgramFilesDefault, true, false},
       {per_user, DirectoryRole::kPerUserDefault, true, true}});
  const auto dirs =
      ResolveInstallDirectories("", {.mutation_capable = true,
                                     .allow_admin_mutation = false,
                                     .is_elevated = false,
                                     .allow_shared_user_store = false});
  EXPECT_EQ(PathError::kNotFound, dirs.write_error);
  EXPECT_TRUE(dirs.writable_dir.empty());
  ASSERT_EQ(2u, dirs.readable_dirs.size());
  EXPECT_EQ(hklm, dirs.readable_dirs[0]);
  EXPECT_EQ(program_files, dirs.readable_dirs[1]);
}

TEST_F(InstallerPathsTest, PolicyPreservesDistinctCustomStore) {
  const base::FilePath standard = temp_dir_.GetPath().Append(L"standard");
  const base::FilePath custom = temp_dir_.GetPath().Append(L"custom");
  OverrideInstallDirectoryCandidatesForTesting(
      {{standard, DirectoryRole::kPerUserDefault, true, true}});
  const auto dirs = ResolveInstallDirectories(
      custom.AsUTF8Unsafe(), {.mutation_capable = true,
                              .allow_admin_mutation = false,
                              .is_elevated = false,
                              .allow_shared_user_store = false});
  EXPECT_EQ(PathError::kSuccess, dirs.write_error);
  EXPECT_EQ(custom, dirs.writable_dir);
  EXPECT_EQ(DirectoryRole::kCustom, dirs.writable_role);
}

TEST_F(InstallerPathsTest, PolicyRejectsCustomAliasOfEveryStandardRole) {
  const base::FilePath candidate = temp_dir_.GetPath().Append(L"candidate");
  ASSERT_TRUE(base::CreateDirectory(candidate));
  for (DirectoryRole role :
       {DirectoryRole::kHklmDefault, DirectoryRole::kProgramFilesDefault,
        DirectoryRole::kPerUserDefault}) {
    OverrideInstallDirectoryCandidatesForTesting(
        {{candidate, role, true, true}});
    for (bool allow_shared_user_store : {true, false}) {
      const auto dirs = ResolveInstallDirectories(
          candidate.AsUTF8Unsafe(),
          {.allow_shared_user_store = allow_shared_user_store});
      EXPECT_EQ(PathError::kInvalidPath, dirs.write_error);
      EXPECT_TRUE(dirs.writable_dir.empty());
      EXPECT_TRUE(dirs.readable_dirs.empty());
    }
  }
}

TEST_F(InstallerPathsTest, PolicyRejectsTextualStandardAlias) {
  const base::FilePath candidate = temp_dir_.GetPath().Append(L"candidate");
  OverrideInstallDirectoryCandidatesForTesting(
      {{candidate, DirectoryRole::kHklmDefault, false, false}});
  std::wstring alias = candidate.value();
  std::replace(alias.begin(), alias.end(), L'\\', L'/');
  alias.append(L"/");
  const auto dirs = ResolveInstallDirectories(base::WideToUTF8(alias));
  EXPECT_EQ(PathError::kInvalidPath, dirs.write_error);
  EXPECT_FALSE(base::PathExists(candidate));
}

TEST_F(InstallerPathsTest, PolicyRejectsStandardAliasThroughJunctionAncestor) {
  const base::FilePath real_parent = temp_dir_.GetPath().Append(L"real-parent");
  const base::FilePath standard = real_parent.Append(L"CEF");
  ASSERT_TRUE(base::CreateDirectory(standard));
  const base::FilePath junction = temp_dir_.GetPath().Append(L"alias-parent");
  ASSERT_TRUE(base::CreateDirectory(junction));
  std::optional<base::test::FilePathReparsePoint> reparse_point =
      base::test::FilePathReparsePoint::Create(junction, real_parent);
  ASSERT_TRUE(reparse_point.has_value());
  const base::FilePath alias = junction.Append(L"CEF");
  ASSERT_TRUE(base::DirectoryExists(alias));
  OverrideInstallDirectoryCandidatesForTesting(
      {{standard, DirectoryRole::kPerUserDefault, true, true}});

  const auto dirs = ResolveInstallDirectories(
      alias.AsUTF8Unsafe(), {.allow_shared_user_store = false});
  EXPECT_EQ(PathError::kInvalidPath, dirs.write_error);
  EXPECT_TRUE(dirs.readable_dirs.empty());
  EXPECT_TRUE(dirs.writable_dir.empty());
}

TEST_F(InstallerPathsTest, PolicyRejectsStandardAliasUsingShortName) {
  const base::FilePath standard = temp_dir_.GetPath()
                                      .Append(L"Long Standard Directory Name")
                                      .Append(L"CEF");
  ASSERT_TRUE(base::CreateDirectory(standard));
  std::wstring short_buffer(32768, L'\0');
  const DWORD length =
      ::GetShortPathNameW(standard.value().c_str(), short_buffer.data(),
                          static_cast<DWORD>(short_buffer.size()));
  if (length == 0 || length >= short_buffer.size()) {
    GTEST_SKIP() << "8.3 short names are unavailable on this volume";
  }
  short_buffer.resize(length);
  const base::FilePath alias(short_buffer);
  if (base::FilePath::CompareEqualIgnoreCase(alias.value(), standard.value())) {
    GTEST_SKIP() << "The filesystem returned no distinct short name";
  }
  OverrideInstallDirectoryCandidatesForTesting(
      {{standard, DirectoryRole::kHklmDefault, true, true}});

  const auto dirs = ResolveInstallDirectories(alias.AsUTF8Unsafe());
  EXPECT_EQ(PathError::kInvalidPath, dirs.write_error);
  EXPECT_TRUE(dirs.readable_dirs.empty());
  EXPECT_TRUE(dirs.writable_dir.empty());
}

TEST_F(InstallerPathsTest, ResolveCustomPath_QueryIsReadOnlyAndDoesNotCreate) {
  const base::FilePath existing = temp_dir_.GetPath().Append(L"existing");
  ASSERT_TRUE(base::CreateDirectory(existing));
  auto dirs = ResolveInstallDirectories(existing.AsUTF8Unsafe(),
                                        {.mutation_capable = false});
  EXPECT_EQ(PathError::kAccessDenied, dirs.write_error);
  EXPECT_TRUE(dirs.writable_dir.empty());
  ASSERT_EQ(1u, dirs.readable_dirs.size());
  EXPECT_EQ(DirectoryRole::kCustom, dirs.readable_roles[0]);

  const base::FilePath missing = temp_dir_.GetPath().Append(L"missing");
  dirs = ResolveInstallDirectories(missing.AsUTF8Unsafe(),
                                   {.mutation_capable = false});
  EXPECT_EQ(PathError::kNotFound, dirs.write_error);
  EXPECT_FALSE(base::PathExists(missing));
  EXPECT_TRUE(dirs.readable_dirs.empty());
}

TEST_F(InstallerPathsTest, CustomUnderProgramFilesSpellingRemainsCustomRole) {
  const base::FilePath custom =
      temp_dir_.GetPath().Append(L"Program Files").Append(L"CEF");
  ASSERT_TRUE(base::CreateDirectory(custom));
  const auto dirs = ResolveInstallDirectories(custom.AsUTF8Unsafe());
  EXPECT_EQ(DirectoryRole::kCustom, dirs.writable_role);
  ASSERT_EQ(1u, dirs.readable_roles.size());
  EXPECT_EQ(DirectoryRole::kCustom, dirs.readable_roles[0]);
}

TEST_F(InstallerPathsTest, UserRetentionEligibilityIsRoleBased) {
  EXPECT_FALSE(IsUserRetentionEligible(DirectoryRole::kHklmDefault));
  EXPECT_FALSE(IsUserRetentionEligible(DirectoryRole::kProgramFilesDefault));
  EXPECT_TRUE(IsUserRetentionEligible(DirectoryRole::kPerUserDefault));
  EXPECT_TRUE(IsUserRetentionEligible(DirectoryRole::kCustom));
}

TEST_F(InstallerPathsTest, ContainmentRecognizesDirectory) {
  const base::FilePath root = temp_dir_.GetPath().Append(L"Contain Root");
  ASSERT_TRUE(base::CreateDirectory(root));
  EXPECT_EQ(PathContainment::kContained,
            GetPhysicalPathContainment(root, root));
}

TEST_F(InstallerPathsTest, ContainmentRecognizesDirectAndDeepDescendants) {
  const base::FilePath root = temp_dir_.GetPath().Append(L"Contain Root");
  const base::FilePath direct = root.Append(L"bootstrap.exe");
  const base::FilePath deep = root.Append(L"one").Append(L"two.exe");
  ASSERT_TRUE(base::CreateDirectory(deep.DirName()));
  ASSERT_TRUE(base::WriteFile(direct, "direct"));
  ASSERT_TRUE(base::WriteFile(deep, "deep"));

  EXPECT_EQ(PathContainment::kContained,
            GetPhysicalPathContainment(root, direct));
  EXPECT_EQ(PathContainment::kContained,
            GetPhysicalPathContainment(root, deep));
}

TEST_F(InstallerPathsTest, ContainmentRejectsSiblingAndPrefixCollision) {
  const base::FilePath root = temp_dir_.GetPath().Append(L"CEF");
  const base::FilePath sibling = temp_dir_.GetPath().Append(L"Sibling");
  const base::FilePath prefix = temp_dir_.GetPath().Append(L"CEF-old");
  ASSERT_TRUE(base::CreateDirectory(root));
  ASSERT_TRUE(base::CreateDirectory(sibling));
  ASSERT_TRUE(base::CreateDirectory(prefix));

  EXPECT_EQ(PathContainment::kOutside,
            GetPhysicalPathContainment(root, sibling));
  EXPECT_EQ(PathContainment::kOutside,
            GetPhysicalPathContainment(root, prefix));
}

TEST_F(InstallerPathsTest, ContainmentRejectsCaseSensitiveSibling) {
  const base::FilePath parent =
      temp_dir_.GetPath().Append(L"case-sensitive-parent");
  ASSERT_TRUE(base::CreateDirectory(parent));
  base::File parent_handle(parent, base::File::FLAG_OPEN |
                                       base::File::FLAG_WRITE_ATTRIBUTES |
                                       base::File::FLAG_WIN_BACKUP_SEMANTICS);
  if (!parent_handle.IsValid()) {
    GTEST_SKIP() << "Could not open the case-sensitive directory fixture";
  }
  FILE_CASE_SENSITIVE_INFO case_sensitive = {
      .Flags = FILE_CS_FLAG_CASE_SENSITIVE_DIR};
  if (!::SetFileInformationByHandle(parent_handle.GetPlatformFile(),
                                    FileCaseSensitiveInfo, &case_sensitive,
                                    sizeof(case_sensitive))) {
    GTEST_SKIP() << "Per-directory case sensitivity is unavailable: "
                 << ::GetLastError();
  }
  parent_handle.Close();

  const base::FilePath upper = parent.Append(L"CEF");
  const base::FilePath lower = parent.Append(L"cef");
  ASSERT_TRUE(base::CreateDirectory(upper));
  ASSERT_TRUE(base::CreateDirectory(lower));
  const base::FilePath lower_child = lower.Append(L"bootstrap.exe");
  ASSERT_TRUE(base::WriteFile(lower_child, "lower"));

  EXPECT_EQ(PathContainment::kOutside,
            GetPhysicalPathContainment(upper, lower_child));
}

TEST_F(InstallerPathsTest, ContainmentNormalizesCaseSeparatorsAndDotDot) {
  const base::FilePath root = temp_dir_.GetPath().Append(L"CaseRoot");
  const base::FilePath child = root.Append(L"Subdir").Append(L"child.exe");
  ASSERT_TRUE(base::CreateDirectory(child.DirName()));
  ASSERT_TRUE(base::WriteFile(child, "child"));

  std::wstring root_variant = root.value();
  std::transform(root_variant.begin(), root_variant.end(), root_variant.begin(),
                 ::towupper);
  std::replace(root_variant.begin(), root_variant.end(),
               static_cast<wchar_t>(92), L'/');
  const base::FilePath dot_dot =
      child.DirName().Append(L"unused").Append(L"..").Append(child.BaseName());

  EXPECT_EQ(PathContainment::kContained,
            GetPhysicalPathContainment(base::FilePath(root_variant), child));
  EXPECT_EQ(PathContainment::kContained,
            GetPhysicalPathContainment(root, dot_dot));
}

TEST_F(InstallerPathsTest, ContainmentIsIndeterminateForUnprovableInputs) {
  const base::FilePath root = temp_dir_.GetPath().Append(L"root");
  const base::FilePath file = root.Append(L"locked.exe");
  ASSERT_TRUE(base::CreateDirectory(root));
  ASSERT_TRUE(base::WriteFile(file, "locked"));

  EXPECT_EQ(PathContainment::kIndeterminate,
            GetPhysicalPathContainment({}, file));
  EXPECT_EQ(PathContainment::kIndeterminate,
            GetPhysicalPathContainment(base::FilePath(L"relative"), file));
  EXPECT_EQ(PathContainment::kIndeterminate,
            GetPhysicalPathContainment(root, root.Append(L"missing.exe")));
}

TEST_F(InstallerPathsTest, ContainmentIsIndeterminateForAclInaccessibleInput) {
  const base::FilePath root = temp_dir_.GetPath().Append(L"acl-root");
  const base::FilePath file = root.Append(L"inaccessible.exe");
  ASSERT_TRUE(base::CreateDirectory(root));
  ASSERT_TRUE(base::WriteFile(file, "inaccessible"));

  std::optional<base::win::SecurityDescriptor> original =
      base::win::SecurityDescriptor::FromFile(file, DACL_SECURITY_INFORMATION);
  if (!original) {
    GTEST_SKIP() << "The filesystem does not expose a writable file DACL";
  }
  base::win::SecurityDescriptor denied;
  denied.set_dacl({});
  denied.set_dacl_protected(true);
  if (!denied.WriteToFile(file, DACL_SECURITY_INFORMATION)) {
    GTEST_SKIP() << "The filesystem does not permit a scoped empty DACL";
  }
  absl::Cleanup restore_dacl = [&] {
    EXPECT_TRUE(original->WriteToFile(file, DACL_SECURITY_INFORMATION));
  };

  EXPECT_EQ(PathContainment::kIndeterminate,
            GetPhysicalPathContainment(root, file));
}

TEST_F(InstallerPathsTest, ContainmentResolvesJunctionAncestor) {
  const base::FilePath real_root = temp_dir_.GetPath().Append(L"real-root");
  const base::FilePath child = real_root.Append(L"child.exe");
  ASSERT_TRUE(base::CreateDirectory(real_root));
  ASSERT_TRUE(base::WriteFile(child, "child"));
  const base::FilePath alias = temp_dir_.GetPath().Append(L"alias-root");
  ASSERT_TRUE(base::CreateDirectory(alias));
  std::optional<base::test::FilePathReparsePoint> reparse_point =
      base::test::FilePathReparsePoint::Create(alias, real_root);
  ASSERT_TRUE(reparse_point.has_value());

  EXPECT_EQ(PathContainment::kContained,
            GetPhysicalPathContainment(alias, child));
  EXPECT_EQ(PathContainment::kContained,
            GetPhysicalPathContainment(real_root, alias.Append(L"child.exe")));
}

TEST_F(InstallerPathsTest, ContainmentResolvesShortName) {
  const base::FilePath root =
      temp_dir_.GetPath().Append(L"Long Containment Directory Name");
  const base::FilePath child = root.Append(L"child.exe");
  ASSERT_TRUE(base::CreateDirectory(root));
  ASSERT_TRUE(base::WriteFile(child, "child"));
  std::wstring short_buffer(32768, wchar_t{});
  const DWORD length =
      ::GetShortPathNameW(child.value().c_str(), short_buffer.data(),
                          static_cast<DWORD>(short_buffer.size()));
  if (length == 0 || length >= short_buffer.size()) {
    GTEST_SKIP() << "8.3 short names are unavailable on this volume";
  }
  short_buffer.resize(length);
  const base::FilePath alias(short_buffer);
  if (base::FilePath::CompareEqualIgnoreCase(alias.value(), child.value())) {
    GTEST_SKIP() << "The filesystem returned no distinct short name";
  }

  EXPECT_EQ(PathContainment::kContained,
            GetPhysicalPathContainment(root, alias));
}

// ============================================================================
// Other Tests
// ============================================================================

TEST_F(InstallerPathsTest, GetDatabasePath) {
  base::FilePath install_dir = temp_dir_.GetPath();
  base::FilePath db_path = GetDatabasePath(install_dir);

  EXPECT_EQ(install_dir.Append(L"installer.json"), db_path);
}

TEST_F(InstallerPathsTest, GetVersionPath) {
  base::FilePath install_dir = temp_dir_.GetPath();
  Version version = Version::Parse("137.3.5");

  base::FilePath version_path = GetVersionPath(install_dir, version);

  // Path includes platform: Versions/<version>/<platform>
  std::string platform = GetCurrentPlatform();
  EXPECT_EQ(install_dir.Append(kVersionsSubdirectory)
                .Append(L"137.3.5")
                .Append(base::FilePath::FromUTF8Unsafe(platform)),
            version_path);
}

TEST_F(InstallerPathsTest, ScanInstalledVersions) {
  // Create valid version directories using GetVersionPath
  ASSERT_TRUE(base::CreateDirectory(
      GetVersionPath(temp_dir_.GetPath(), Version::Parse("137.3.5"))));
  ASSERT_TRUE(base::CreateDirectory(
      GetVersionPath(temp_dir_.GetPath(), Version::Parse("136.0.0"))));
  ASSERT_TRUE(base::CreateDirectory(
      GetVersionPath(temp_dir_.GetPath(), Version::Parse("138.1.0"))));

  // Create invalid entries that should be skipped (manually constructed)
  base::FilePath versions_dir =
      temp_dir_.GetPath().Append(kVersionsSubdirectory);
  ASSERT_TRUE(base::CreateDirectory(versions_dir.Append(L"not-a-version")));
  ASSERT_TRUE(base::CreateDirectory(versions_dir.Append(L"135.0.0")));

  std::vector<Version> versions = ScanInstalledVersions(temp_dir_.GetPath());

  ASSERT_EQ(3u, versions.size());
  // Should be sorted newest-first
  EXPECT_EQ("138.1.0", versions[0].ToString());
  EXPECT_EQ("137.3.5", versions[1].ToString());
  EXPECT_EQ("136.0.0", versions[2].ToString());
}

TEST_F(InstallerPathsTest, ScanInstalledVersionsSorted) {
  // Create versions in random order
  ASSERT_TRUE(base::CreateDirectory(
      GetVersionPath(temp_dir_.GetPath(), Version::Parse("100.0.0"))));
  ASSERT_TRUE(base::CreateDirectory(
      GetVersionPath(temp_dir_.GetPath(), Version::Parse("200.0.0"))));
  ASSERT_TRUE(base::CreateDirectory(
      GetVersionPath(temp_dir_.GetPath(), Version::Parse("150.5.3"))));
  ASSERT_TRUE(base::CreateDirectory(
      GetVersionPath(temp_dir_.GetPath(), Version::Parse("150.10.0"))));

  std::vector<Version> versions = ScanInstalledVersions(temp_dir_.GetPath());

  ASSERT_EQ(4u, versions.size());
  // Verify descending order
  EXPECT_EQ("200.0.0", versions[0].ToString());
  EXPECT_EQ("150.10.0", versions[1].ToString());
  EXPECT_EQ("150.5.3", versions[2].ToString());
  EXPECT_EQ("100.0.0", versions[3].ToString());
}

TEST_F(InstallerPathsTest, GetCurrentPlatform) {
  std::string platform = GetCurrentPlatform();

  // Should be one of the valid Windows platforms
  EXPECT_TRUE(platform == "windows64" || platform == "windows32" ||
              platform == "windowsarm64");
}

TEST_F(InstallerPathsTest, IsRunningFromTempDirectory) {
  const base::FilePath temp_dir = GetTempDirectoryPath();
  ASSERT_FALSE(temp_dir.empty());
  base::ScopedTempDir child;
  ASSERT_TRUE(child.CreateUniqueTempDirUnderPath(temp_dir));
  const base::FilePath exe_in_temp = child.GetPath().Append(L"test.exe");
  const base::FilePath exe_not_in_temp(L"C:\\Program Files\\test.exe");
  ASSERT_TRUE(base::WriteFile(exe_in_temp, "test"));

  EXPECT_TRUE(IsRunningFromTempDirectory(exe_in_temp));
  EXPECT_FALSE(IsRunningFromTempDirectory(exe_not_in_temp));
}

TEST_F(InstallerPathsTest, PathErrorToString) {
  EXPECT_STREQ("Success", PathErrorToString(PathError::kSuccess));
  EXPECT_STREQ("No valid install directory found",
               PathErrorToString(PathError::kNotFound));
  EXPECT_STREQ("Directory exists but not writable",
               PathErrorToString(PathError::kAccessDenied));
  EXPECT_STREQ("Path exists but is not a directory",
               PathErrorToString(PathError::kInvalidPath));
}

// ============================================================================
// ScanInstalledVersions — additional coverage
// ============================================================================

TEST_F(InstallerPathsTest, ScanInstalledVersionsEmpty) {
  // Create the Versions directory but leave it empty.
  ASSERT_TRUE(
      base::CreateDirectory(temp_dir_.GetPath().Append(kVersionsSubdirectory)));

  std::vector<Version> versions = ScanInstalledVersions(temp_dir_.GetPath());
  EXPECT_TRUE(versions.empty());
}

TEST_F(InstallerPathsTest, ScanInstalledVersionsNoVersionsDir) {
  // No Versions subdirectory at all.
  std::vector<Version> versions = ScanInstalledVersions(temp_dir_.GetPath());
  EXPECT_TRUE(versions.empty());
}

TEST_F(InstallerPathsTest, ScanInstalledVersionsSkipsNonVersionDirs) {
  base::FilePath versions_dir =
      temp_dir_.GetPath().Append(kVersionsSubdirectory);
  ASSERT_TRUE(base::CreateDirectory(versions_dir));

  // Non-version directory names should be ignored.
  ASSERT_TRUE(base::CreateDirectory(versions_dir.Append(L"not-a-version")));
  ASSERT_TRUE(base::CreateDirectory(versions_dir.Append(L"abc.def.ghi")));

  // Valid version but no platform subdirectory — should also be skipped.
  ASSERT_TRUE(base::CreateDirectory(versions_dir.Append(L"140.0.0")));

  std::vector<Version> versions = ScanInstalledVersions(temp_dir_.GetPath());
  EXPECT_TRUE(versions.empty());
}

TEST_F(InstallerPathsTest, ScanInstalledVersionsOnlyMatchesCurrentPlatform) {
  // Create a version with a foreign platform subdirectory.
  base::FilePath versions_dir =
      temp_dir_.GetPath().Append(kVersionsSubdirectory);
  base::FilePath ver_dir = versions_dir.Append(L"140.0.0");
  ASSERT_TRUE(base::CreateDirectory(ver_dir));

  // Use a platform that definitely isn't the current one.
  std::string current = GetCurrentPlatform();
  std::string foreign = (current == "windows64") ? "windows32" : "windows64";
  ASSERT_TRUE(base::CreateDirectory(
      ver_dir.Append(base::FilePath::FromUTF8Unsafe(foreign))));

  std::vector<Version> versions = ScanInstalledVersions(temp_dir_.GetPath());
  EXPECT_TRUE(versions.empty());
}

// ============================================================================
// IsRunningFromTempDirectory — additional coverage
// ============================================================================

TEST_F(InstallerPathsTest, IsRunningFromTempDirectoryEmptyPath) {
  EXPECT_FALSE(IsRunningFromTempDirectory(base::FilePath()));
}

TEST_F(InstallerPathsTest, IsRunningFromTempDirectoryShortPath) {
  // A path shorter than the temp dir prefix should return false.
  EXPECT_FALSE(IsRunningFromTempDirectory(base::FilePath(L"C:\\")));
}

// ============================================================================
// Security: Reparse Point Validation (H3)
// ============================================================================

// Note: Actually creating junctions/symlinks requires admin privileges on
// Windows, so we cannot directly test reparse point rejection in unit tests.
// The security behavior is verified by code review:
// - TryUseDirectory() calls IsReparsePoint() before accepting a directory
// - If the path is a reparse point (junction/symlink), it returns false
// This prevents attackers from redirecting the install location to
// sensitive directories by creating a junction at the expected path.

// ============================================================================
// ResolvePathRelativeTo Tests
// ============================================================================

TEST_F(InstallerPathsTest, ResolvePathRelativeTo_Absolute) {
  base::FilePath base_dir = temp_dir_.GetPath();
  base::FilePath resolved =
      ResolvePathRelativeTo(base_dir.AsUTF8Unsafe(), base_dir);
  EXPECT_EQ(base_dir, resolved);
}

TEST_F(InstallerPathsTest, ResolvePathRelativeTo_RelativeDot) {
  base::FilePath base_dir = temp_dir_.GetPath();
  base::FilePath resolved = ResolvePathRelativeTo(".", base_dir);
  EXPECT_EQ(base_dir, resolved);
}

TEST_F(InstallerPathsTest, ResolvePathRelativeTo_RelativeSubdir) {
  base::FilePath base_dir = temp_dir_.GetPath();
  base::FilePath sub_dir = base_dir.Append(L"subdir");
  ASSERT_TRUE(base::CreateDirectory(sub_dir));

  base::FilePath resolved = ResolvePathRelativeTo("subdir", base_dir);
  EXPECT_EQ(sub_dir, resolved);
}

TEST_F(InstallerPathsTest, ResolvePathRelativeTo_DotDot) {
  base::FilePath base_dir = temp_dir_.GetPath();
  base::FilePath sub_dir = base_dir.Append(L"subdir");
  ASSERT_TRUE(base::CreateDirectory(sub_dir));

  base::FilePath resolved = ResolvePathRelativeTo("..", sub_dir);
  EXPECT_EQ(base_dir, resolved);
}

}  // namespace
}  // namespace cef_installer
