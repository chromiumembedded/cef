// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_bootstrap_helpers.h"

#include <limits>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/include/cef_api_hash.h"
#include "cef/libcef_dll/bootstrap/installer/installer_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_e2e_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_logger.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

std::wstring SlashSwitch(const char* name) {
  return L"/" + base::UTF8ToWide(name);
}

std::wstring DashSwitch(const char* name) {
  return L"--" + base::UTF8ToWide(name);
}

std::wstring SlashSwitchWithValue(const char* name, const std::wstring& value) {
  return L"/" + base::UTF8ToWide(name) + L"=" + value;
}

class BootstrapHelpersTest : public testing::Test {
 protected:
  void TearDown() override {
    internal::SetInstallerE2EConfigForTesting(std::nullopt);
    internal::ClearInstallDirectoryOverridesForTesting();
    internal::OverrideEnterprisePolicyForTesting(std::nullopt);
  }

  // Helper to create a command line from a string
  base::CommandLine MakeCommandLine(const std::wstring& args) {
    return base::CommandLine::FromString(L"test.exe " + args);
  }
};

// ============================================================================
// ParseInstallerCommand Tests
// ============================================================================

TEST_F(BootstrapHelpersTest, ParseUpdateCommand) {
  auto cmd_line = MakeCommandLine(SlashSwitch(kSwitchUpdate));
  auto command = ParseInstallerCommand(cmd_line);

  ASSERT_TRUE(command.has_value());
  EXPECT_EQ(Command::kUpdate, command.value());
}

TEST_F(BootstrapHelpersTest, ParseUpdateCommandDashDash) {
  auto cmd_line = MakeCommandLine(DashSwitch(kSwitchUpdate));
  auto command = ParseInstallerCommand(cmd_line);

  ASSERT_TRUE(command.has_value());
  EXPECT_EQ(Command::kUpdate, command.value());
}

TEST_F(BootstrapHelpersTest, ParseUninstallCommand) {
  auto cmd_line = MakeCommandLine(SlashSwitch(kSwitchUninstall));
  auto command = ParseInstallerCommand(cmd_line);

  ASSERT_TRUE(command.has_value());
  EXPECT_EQ(Command::kUninstall, command.value());
}

TEST_F(BootstrapHelpersTest, ParseUninstallCommandDashDash) {
  auto cmd_line = MakeCommandLine(DashSwitch(kSwitchUninstall));
  auto command = ParseInstallerCommand(cmd_line);

  ASSERT_TRUE(command.has_value());
  EXPECT_EQ(Command::kUninstall, command.value());
}

TEST_F(BootstrapHelpersTest, ParseNoFlags) {
  auto cmd_line = MakeCommandLine(L"");
  auto command = ParseInstallerCommand(cmd_line);

  EXPECT_FALSE(command.has_value());
}

TEST_F(BootstrapHelpersTest, ParseOtherFlags) {
  auto cmd_line = MakeCommandLine(L"/someother /flag");
  auto command = ParseInstallerCommand(cmd_line);

  EXPECT_FALSE(command.has_value());
}

TEST_F(BootstrapHelpersTest, ParseRetentionCommandsAndConflicts) {
  auto dry_run = MakeCommandLine(SlashSwitch(kSwitchRetentionDryRun));
  EXPECT_EQ(Command::kRetentionDryRun, ParseInstallerCommand(dry_run));
  EXPECT_FALSE(HasConflictingInstallerCommands(dry_run));

  auto apply = MakeCommandLine(SlashSwitch(kSwitchRetentionApply));
  EXPECT_EQ(Command::kRetentionApply, ParseInstallerCommand(apply));

  auto conflict = MakeCommandLine(SlashSwitch(kSwitchRetentionApply) + L" " +
                                  SlashSwitch(kSwitchUpdate));
  EXPECT_TRUE(HasConflictingInstallerCommands(conflict));
}

TEST_F(BootstrapHelpersTest, RetentionMaxAgeValidation) {
  int days = 0;
  auto defaults = MakeCommandLine(L"");
  EXPECT_TRUE(ParseRetentionMaxAgeDays(defaults, &days));
  EXPECT_EQ(180, days);

  auto minimum =
      MakeCommandLine(SlashSwitchWithValue(kSwitchRetentionMaxAgeDays, L"90"));
  EXPECT_TRUE(ParseRetentionMaxAgeDays(minimum, &days));
  EXPECT_EQ(90, days);

  for (const wchar_t* invalid : {L"", L"89", L"3651", L"not-a-number"}) {
    auto command = MakeCommandLine(
        SlashSwitchWithValue(kSwitchRetentionMaxAgeDays, invalid));
    EXPECT_FALSE(ParseRetentionMaxAgeDays(command, &days));
  }
}

TEST_F(BootstrapHelpersTest, RejectsRetentionOptionsOnOtherCommands) {
  base::CommandLine uninstall(base::CommandLine::NO_PROGRAM);
  uninstall.AppendSwitch(kSwitchUninstall);
  uninstall.AppendSwitchASCII(kSwitchRetentionMaxAgeDays, "90");
  EXPECT_TRUE(HasMisappliedRetentionOptions(uninstall,
                                            ParseInstallerCommand(uninstall)));

  base::CommandLine retention(base::CommandLine::NO_PROGRAM);
  retention.AppendSwitch(kSwitchRetentionApply);
  retention.AppendSwitchASCII(kSwitchRetentionMaxAgeDays, "90");
  EXPECT_FALSE(HasMisappliedRetentionOptions(retention,
                                             ParseInstallerCommand(retention)));
}

TEST_F(BootstrapHelpersTest, TrustedRelaunchConfigLoadUsesTypedE2EFailure) {
  internal::InstallerE2EConfig e2e;
  e2e.child_config_failure = true;
  internal::SetInstallerE2EConfigForTesting(e2e);

  Config config;
  const ConfigLoadResult result =
      TryLoadInstallerConfig(nullptr, &config, nullptr,
                             /*trusted_uninstall_relaunch=*/true);
  EXPECT_EQ(ConfigLoadStatus::kError, result.status);
  EXPECT_EQ(ConfigError::kJsonParseError, result.error);
  EXPECT_EQ(ConfigLoadSource::kNone, result.source);
  EXPECT_NE(std::string::npos, result.diagnostic.find("Forced child"));
}

// ============================================================================
// IsUninstallRelaunched Tests
// ============================================================================

TEST_F(BootstrapHelpersTest, IsUninstallRelaunchedTrue) {
  auto cmd_line = MakeCommandLine(SlashSwitch(kSwitchUninstallRelaunched));
  EXPECT_TRUE(IsUninstallRelaunched(cmd_line));
}

TEST_F(BootstrapHelpersTest, IsUninstallRelaunchedFalse) {
  auto cmd_line = MakeCommandLine(SlashSwitch(kSwitchUninstall));
  EXPECT_FALSE(IsUninstallRelaunched(cmd_line));
}

TEST_F(BootstrapHelpersTest, UninstallInvocationMatrix) {
  struct TestCase {
    std::optional<Command> command;
    bool marker;
    bool state;
    bool trusted;
    UninstallInvocationState expected;
  };
  const TestCase cases[] = {
      {Command::kUninstall, false, false, false,
       UninstallInvocationState::kOriginal},
      {Command::kUninstall, true, true, true,
       UninstallInvocationState::kRelaunched},
      {Command::kUninstall, true, false, false,
       UninstallInvocationState::kInvalid},
      {Command::kUninstall, false, true, false,
       UninstallInvocationState::kInvalid},
      {Command::kUninstall, true, true, false,
       UninstallInvocationState::kInvalid},
      {Command::kUpdate, true, true, true, UninstallInvocationState::kInvalid},
      {std::nullopt, true, false, false, UninstallInvocationState::kInvalid},
      {Command::kUpdate, false, false, false,
       UninstallInvocationState::kNotUninstall},
  };
  for (const auto& test : cases) {
    EXPECT_EQ(test.expected,
              internal::ClassifyUninstallInvocation(test.command, test.marker,
                                                    test.state, test.trusted));
  }
}

TEST_F(BootstrapHelpersTest, UninstallDecisionMatrix) {
  using internal::PathContainment;
  struct TestCase {
    UninstallInvocationState invocation;
    bool writable;
    bool background;
    PathContainment containment;
    UninstallExecution execution;
    int error_code;
  };
  const TestCase cases[] = {
      {UninstallInvocationState::kRelaunched, true, false,
       PathContainment::kOutside, UninstallExecution::kInProcess,
       kExitCodeSuccess},
      {UninstallInvocationState::kInvalid, false, false,
       PathContainment::kIndeterminate, UninstallExecution::kReject,
       kExitCodeConfigError},
      {UninstallInvocationState::kOriginal, false, false,
       PathContainment::kIndeterminate, UninstallExecution::kInProcess,
       kExitCodeSuccess},
      {UninstallInvocationState::kOriginal, true, false,
       PathContainment::kOutside, UninstallExecution::kInProcess,
       kExitCodeSuccess},
      {UninstallInvocationState::kOriginal, true, false,
       PathContainment::kContained, UninstallExecution::kRelaunch,
       kExitCodeSuccess},
      {UninstallInvocationState::kOriginal, true, true,
       PathContainment::kOutside, UninstallExecution::kRelaunch,
       kExitCodeSuccess},
      {UninstallInvocationState::kOriginal, true, false,
       PathContainment::kIndeterminate, UninstallExecution::kReject,
       kExitCodeInstallError},
  };
  for (const auto& test : cases) {
    const auto decision = internal::DecideUninstallExecution(
        test.invocation, test.writable, test.background, test.containment);
    EXPECT_EQ(test.execution, decision.execution);
    EXPECT_EQ(test.error_code, decision.error_code);
  }
  EXPECT_EQ(kExitCodeRelaunched,
            UninstallRelaunchExitCode(UninstallRelaunchStatus::kStarted));
  EXPECT_EQ(kExitCodeInstallError,
            UninstallRelaunchExitCode(UninstallRelaunchStatus::kFailed));
}

TEST_F(BootstrapHelpersTest, UninstallPreflightUsesWritableTarget) {
  base::ScopedTempDir target;
  base::ScopedTempDir outside;
  ASSERT_TRUE(target.CreateUniqueTempDir());
  ASSERT_TRUE(outside.CreateUniqueTempDir());
  const base::FilePath contained = target.GetPath().Append(L"bootstrap.exe");
  const base::FilePath external = outside.GetPath().Append(L"bootstrap.exe");
  ASSERT_TRUE(base::WriteFile(contained, "exe"));
  ASSERT_TRUE(base::WriteFile(external, "exe"));
  internal::OverrideEnterprisePolicyForTesting(PolicyLoadResult{});
  internal::OverrideInstallDirectoriesForTesting({target.GetPath()},
                                                 target.GetPath());

  Config config;
  ExtendedConfig extended;
  auto direct = PrepareUninstall(config, extended, external,
                                 UninstallInvocationState::kOriginal);
  ASSERT_TRUE(direct.prepared);
  EXPECT_EQ(UninstallExecution::kInProcess, direct.decision.execution);
  EXPECT_EQ(target.GetPath(), direct.prepared->directories->writable_dir);

  auto relaunch = PrepareUninstall(config, extended, contained,
                                   UninstallInvocationState::kOriginal);
  EXPECT_EQ(UninstallExecution::kRelaunch, relaunch.decision.execution);

  extended.background_mode = true;
  relaunch = PrepareUninstall(config, extended, external,
                              UninstallInvocationState::kOriginal);
  EXPECT_EQ(UninstallExecution::kRelaunch, relaunch.decision.execution);

  internal::ClearInstallDirectoryOverridesForTesting();
  internal::OverrideEnterprisePolicyForTesting(std::nullopt);
}

TEST_F(BootstrapHelpersTest, UninstallPreflightNoWritableRunsController) {
  base::ScopedTempDir outside;
  ASSERT_TRUE(outside.CreateUniqueTempDir());
  const base::FilePath external = outside.GetPath().Append(L"bootstrap.exe");
  ASSERT_TRUE(base::WriteFile(external, "exe"));
  internal::OverrideEnterprisePolicyForTesting(PolicyLoadResult{});
  internal::OverrideInstallDirectoriesForTesting({outside.GetPath()},
                                                 std::nullopt);

  ExtendedConfig extended;
  extended.background_mode = true;
  const auto preflight = PrepareUninstall(Config{}, extended, external,
                                          UninstallInvocationState::kOriginal);
  ASSERT_TRUE(preflight.prepared);
  EXPECT_EQ(UninstallExecution::kInProcess, preflight.decision.execution);
  ASSERT_TRUE(preflight.prepared->directories);
  EXPECT_TRUE(preflight.prepared->directories->writable_dir.empty());

  internal::ClearInstallDirectoryOverridesForTesting();
  internal::OverrideEnterprisePolicyForTesting(std::nullopt);
}

TEST_F(BootstrapHelpersTest,
       UninstallPreflightOwnsAdminMutationDeniedE2EOverride) {
  base::ScopedTempDir root;
  ASSERT_TRUE(root.CreateUniqueTempDir());
  const base::FilePath admin = root.GetPath().Append(L"Admin");
  const base::FilePath per_user = root.GetPath().Append(L"PerUser");
  ASSERT_TRUE(base::CreateDirectory(admin));
  ASSERT_TRUE(base::CreateDirectory(per_user));
  internal::OverrideEnterprisePolicyForTesting(PolicyLoadResult{});

  internal::InstallerE2EConfig e2e;
  e2e.directory_override =
      internal::InstallerE2EDirectoryOverride::kAdminMutationDenied;
  e2e.directory_root = root.GetPath();
  internal::SetInstallerE2EConfigForTesting(e2e);

  const auto preflight =
      PrepareUninstall(Config{}, ExtendedConfig{}, base::FilePath(),
                       UninstallInvocationState::kOriginal);
  ASSERT_TRUE(preflight.prepared);
  ASSERT_TRUE(preflight.prepared->directories);
  EXPECT_TRUE(preflight.prepared->resolution_context.is_elevated);
  EXPECT_FALSE(preflight.prepared->resolution_context.allow_admin_mutation);
  EXPECT_TRUE(preflight.prepared->directories->writable_dir.empty());
  ASSERT_EQ(1u, preflight.prepared->directories->readable_dirs.size());
  EXPECT_EQ(admin, preflight.prepared->directories->readable_dirs[0]);
  EXPECT_EQ(DirectoryRole::kHklmDefault,
            preflight.prepared->directories->readable_roles[0]);
  EXPECT_EQ(0u, internal::GetInstallDirectoryMutationProbeCountForTesting());
}

TEST_F(BootstrapHelpersTest, UninstallDecisionHonorsPolicyAndAdminGates) {
  base::ScopedTempDir root;
  ASSERT_TRUE(root.CreateUniqueTempDir());
  const base::FilePath hklm = root.GetPath().Append(L"hklm");
  const base::FilePath program_files = root.GetPath().Append(L"program-files");
  const base::FilePath per_user = root.GetPath().Append(L"per-user");
  const base::FilePath external = root.GetPath().Append(L"external.exe");
  ASSERT_TRUE(base::CreateDirectory(hklm));
  ASSERT_TRUE(base::CreateDirectory(program_files));
  ASSERT_TRUE(base::CreateDirectory(per_user));
  ASSERT_TRUE(base::WriteFile(external, "exe"));
  const std::vector<internal::TestDirectoryCandidate> candidates = {
      {hklm, DirectoryRole::kHklmDefault, true, true},
      {program_files, DirectoryRole::kProgramFilesDefault, true, true},
      {per_user, DirectoryRole::kPerUserDefault, true, true},
  };
  Config config;
  ExtendedConfig extended;
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;

  policy.policy.allow_shared_user_store = false;
  internal::OverrideEnterprisePolicyForTesting(policy);
  internal::OverrideInstallDirectoryCandidatesForTesting(candidates);
  internal::OverrideProcessElevationForTesting(false);
  internal::OverrideAdminMutationAllowedForTesting(false);
  auto preflight = PrepareUninstall(config, extended, external,
                                    UninstallInvocationState::kOriginal);
  ASSERT_TRUE(preflight.prepared->directories);
  EXPECT_TRUE(preflight.prepared->directories->writable_dir.empty());
  EXPECT_EQ(UninstallExecution::kInProcess, preflight.decision.execution);

  policy.policy.allow_shared_user_store = true;
  internal::OverrideEnterprisePolicyForTesting(policy);
  internal::OverrideInstallDirectoryCandidatesForTesting(candidates);
  internal::OverrideProcessElevationForTesting(false);
  internal::OverrideAdminMutationAllowedForTesting(false);
  preflight = PrepareUninstall(config, extended, external,
                               UninstallInvocationState::kOriginal);
  ASSERT_EQ(per_user, preflight.prepared->directories->writable_dir);
  EXPECT_EQ(DirectoryRole::kPerUserDefault,
            preflight.prepared->directories->writable_role);

  internal::OverrideInstallDirectoryCandidatesForTesting(candidates);
  internal::OverrideProcessElevationForTesting(true);
  internal::OverrideAdminMutationAllowedForTesting(false);
  preflight = PrepareUninstall(config, extended, external,
                               UninstallInvocationState::kOriginal);
  EXPECT_TRUE(preflight.prepared->directories->writable_dir.empty());
  EXPECT_EQ(2u, preflight.prepared->directories->readable_dirs.size());

  internal::OverrideInstallDirectoryCandidatesForTesting(candidates);
  internal::OverrideProcessElevationForTesting(true);
  internal::OverrideAdminMutationAllowedForTesting(true);
  preflight = PrepareUninstall(config, extended, external,
                               UninstallInvocationState::kOriginal);
  EXPECT_EQ(hklm, preflight.prepared->directories->writable_dir);
  EXPECT_EQ(DirectoryRole::kHklmDefault,
            preflight.prepared->directories->writable_role);
}

TEST_F(BootstrapHelpersTest, UninstallDecisionPreservesExclusiveCustomErrors) {
  base::ScopedTempDir root;
  ASSERT_TRUE(root.CreateUniqueTempDir());
  const base::FilePath custom = root.GetPath().Append(L"custom");
  const base::FilePath external = root.GetPath().Append(L"external.exe");
  ASSERT_TRUE(base::CreateDirectory(custom));
  ASSERT_TRUE(base::WriteFile(external, "exe"));
  internal::OverrideEnterprisePolicyForTesting(PolicyLoadResult{});

  ExtendedConfig extended;
  extended.install_path = custom.AsUTF8Unsafe();
  internal::OverrideInstallDirectoriesForTesting({custom}, std::nullopt);
  auto preflight = PrepareUninstall(Config{}, extended, external,
                                    UninstallInvocationState::kOriginal);
  ASSERT_TRUE(preflight.prepared->directories);
  EXPECT_TRUE(preflight.prepared->directories->writable_dir.empty());
  ASSERT_EQ(1u, preflight.prepared->directories->readable_dirs.size());
  EXPECT_EQ(custom, preflight.prepared->directories->readable_dirs[0]);
  EXPECT_EQ(UninstallExecution::kInProcess, preflight.decision.execution);

  internal::ClearInstallDirectoryOverridesForTesting();
  const base::FilePath invalid = root.GetPath().Append(L"not-a-directory");
  ASSERT_TRUE(base::WriteFile(invalid, "file"));
  extended.install_path = invalid.AsUTF8Unsafe();
  preflight = PrepareUninstall(Config{}, extended, external,
                               UninstallInvocationState::kOriginal);
  EXPECT_EQ(PathError::kInvalidPath,
            preflight.prepared->directories->write_error);
  EXPECT_TRUE(preflight.prepared->directories->writable_dir.empty());
  EXPECT_EQ(UninstallExecution::kInProcess, preflight.decision.execution);
}

TEST_F(BootstrapHelpersTest,
       UninstallDecisionIndeterminateFailsUnlessBackground) {
  base::ScopedTempDir target;
  ASSERT_TRUE(target.CreateUniqueTempDir());
  internal::OverrideEnterprisePolicyForTesting(PolicyLoadResult{});
  internal::OverrideInstallDirectoriesForTesting({target.GetPath()},
                                                 target.GetPath());
  const base::FilePath missing_executable =
      target.GetPath().DirName().Append(L"missing.exe");
  ExtendedConfig extended;

  auto preflight = PrepareUninstall(Config{}, extended, missing_executable,
                                    UninstallInvocationState::kOriginal);
  EXPECT_EQ(UninstallExecution::kReject, preflight.decision.execution);
  EXPECT_EQ(kExitCodeInstallError, preflight.decision.error_code);

  extended.background_mode = true;
  preflight = PrepareUninstall(Config{}, extended, missing_executable,
                               UninstallInvocationState::kOriginal);
  EXPECT_EQ(UninstallExecution::kRelaunch, preflight.decision.execution);
}

// ============================================================================
// ParseExtendedConfigFromCommandLine Tests
// ============================================================================

TEST_F(BootstrapHelpersTest, ParseForcecheck) {
  auto cmd_line = MakeCommandLine(SlashSwitch(kSwitchUpdate) + L" " +
                                  SlashSwitch(kSwitchForceCheck));

  ExtendedConfig config;
  EXPECT_FALSE(config.force_check);  // Default is false

  ParseExtendedConfigFromCommandLine(cmd_line, &config);

  EXPECT_TRUE(config.force_check);
}

TEST_F(BootstrapHelpersTest, ParseBackground) {
  auto cmd_line = MakeCommandLine(SlashSwitch(kSwitchUpdate) + L" " +
                                  SlashSwitch(kSwitchBackground));

  ExtendedConfig config;
  EXPECT_TRUE(config.show_progress_ui);  // Default is true

  ParseExtendedConfigFromCommandLine(cmd_line, &config);

  // /cef-background disables progress UI (same as /cef-headless)
  EXPECT_FALSE(config.show_progress_ui);
  EXPECT_TRUE(config.background_mode);
}

TEST_F(BootstrapHelpersTest, ParseHeadless) {
  auto cmd_line = MakeCommandLine(SlashSwitch(kSwitchUpdate) + L" " +
                                  SlashSwitch(kSwitchHeadless));

  ExtendedConfig config;
  EXPECT_TRUE(config.show_progress_ui);  // Default is true

  ParseExtendedConfigFromCommandLine(cmd_line, &config);

  EXPECT_FALSE(config.show_progress_ui);
  EXPECT_FALSE(config.background_mode);
}

TEST_F(BootstrapHelpersTest, ParseParentWindow) {
  auto cmd_line =
      MakeCommandLine(SlashSwitch(kSwitchUpdate) + L" " +
                      SlashSwitchWithValue(kSwitchParentWindow, L"12345"));

  ExtendedConfig config;
  EXPECT_EQ(nullptr, config.parent_window);  // Default is nullptr

  ParseExtendedConfigFromCommandLine(cmd_line, &config);

  EXPECT_EQ(reinterpret_cast<HWND>(12345), config.parent_window);
}

TEST_F(BootstrapHelpersTest, ParseMultipleFlags) {
  auto cmd_line = MakeCommandLine(SlashSwitch(kSwitchUpdate) + L" " +
                                  SlashSwitch(kSwitchForceCheck) + L" " +
                                  SlashSwitch(kSwitchBackground) + L" " +
                                  SlashSwitch(kSwitchHeadless));

  ExtendedConfig config;
  ParseExtendedConfigFromCommandLine(cmd_line, &config);

  EXPECT_TRUE(config.force_check);
  EXPECT_FALSE(config.show_progress_ui);
  EXPECT_TRUE(config.background_mode);
}

TEST_F(BootstrapHelpersTest, ParseBackgroundStateClearsOnReuse) {
  ExtendedConfig config;
  ParseExtendedConfigFromCommandLine(
      MakeCommandLine(SlashSwitch(kSwitchBackground)), &config);
  ASSERT_TRUE(config.background_mode);

  ParseExtendedConfigFromCommandLine(
      MakeCommandLine(SlashSwitch(kSwitchHeadless)), &config);
  EXPECT_FALSE(config.background_mode);
  EXPECT_FALSE(config.show_progress_ui);
}

TEST_F(BootstrapHelpersTest, ParseNullConfig) {
  auto cmd_line = MakeCommandLine(SlashSwitch(kSwitchUpdate) + L" " +
                                  SlashSwitch(kSwitchForceCheck));
  // Should not crash with nullptr
  ParseExtendedConfigFromCommandLine(cmd_line, nullptr);
}

// ============================================================================
// CommandToString / CommandFromString Tests
// ============================================================================

TEST_F(BootstrapHelpersTest, CommandToStringInstall) {
  EXPECT_STREQ("install", CommandToString(Command::kInstall));
}

TEST_F(BootstrapHelpersTest, CommandToStringUpdate) {
  EXPECT_STREQ("update", CommandToString(Command::kUpdate));
}

TEST_F(BootstrapHelpersTest, CommandToStringUninstall) {
  EXPECT_STREQ("uninstall", CommandToString(Command::kUninstall));
}

TEST_F(BootstrapHelpersTest, CommandToStringQuery) {
  EXPECT_STREQ("query", CommandToString(Command::kQuery));
}

TEST_F(BootstrapHelpersTest, CommandFromStringInstall) {
  auto cmd = CommandFromString("install");
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(Command::kInstall, cmd.value());
}

TEST_F(BootstrapHelpersTest, CommandFromStringUpdate) {
  auto cmd = CommandFromString("update");
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(Command::kUpdate, cmd.value());
}

TEST_F(BootstrapHelpersTest, CommandFromStringUninstall) {
  auto cmd = CommandFromString("uninstall");
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(Command::kUninstall, cmd.value());
}

TEST_F(BootstrapHelpersTest, CommandFromStringQuery) {
  auto cmd = CommandFromString("query");
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(Command::kQuery, cmd.value());
}

TEST_F(BootstrapHelpersTest, CommandFromStringCaseInsensitive) {
  EXPECT_EQ(Command::kInstall, CommandFromString("INSTALL").value());
  EXPECT_EQ(Command::kUpdate, CommandFromString("Update").value());
  EXPECT_EQ(Command::kUninstall, CommandFromString("UNINSTALL").value());
}

TEST_F(BootstrapHelpersTest, CommandFromStringInvalid) {
  EXPECT_FALSE(CommandFromString("invalid").has_value());
  EXPECT_FALSE(CommandFromString("").has_value());
  EXPECT_FALSE(CommandFromString("remove").has_value());
}

TEST_F(BootstrapHelpersTest, ParseExtendedConfigParentWindowInvalid) {
  auto cmd_line = MakeCommandLine(
      SlashSwitchWithValue(kSwitchParentWindow, L"not-a-number"));
  ExtendedConfig config;
  ParseExtendedConfigFromCommandLine(cmd_line, &config);

  // Invalid value should leave parent_window as nullptr
  EXPECT_EQ(nullptr, config.parent_window);
}

TEST_F(BootstrapHelpersTest, ParseParentWindowUintptrMax) {
  const std::wstring value =
      base::UTF8ToWide(std::to_string(std::numeric_limits<uintptr_t>::max()));
  auto cmd_line =
      MakeCommandLine(SlashSwitchWithValue(kSwitchParentWindow, value));
  ExtendedConfig config;
  ParseExtendedConfigFromCommandLine(cmd_line, &config);
  EXPECT_EQ(reinterpret_cast<HWND>(std::numeric_limits<uintptr_t>::max()),
            config.parent_window);
}

TEST_F(BootstrapHelpersTest, ParseParentWindowOverflow) {
  auto cmd_line = MakeCommandLine(
      SlashSwitchWithValue(kSwitchParentWindow, L"18446744073709551616"));
  ExtendedConfig config;
  ParseExtendedConfigFromCommandLine(cmd_line, &config);
  EXPECT_EQ(nullptr, config.parent_window);
}

TEST_F(BootstrapHelpersTest, ParseExtendedConfigNullptr) {
  auto cmd_line = MakeCommandLine(SlashSwitch(kSwitchForceCheck));
  // Should not crash when passed nullptr
  ParseExtendedConfigFromCommandLine(cmd_line, nullptr);
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(BootstrapHelpersTest, ParseInstallPath) {
  auto cmd_line = MakeCommandLine(
      SlashSwitchWithValue(kSwitchInstallPath, L"C:\\temp\\test_install"));

  ExtendedConfig config;
  EXPECT_TRUE(config.install_path.empty());

  ParseExtendedConfigFromCommandLine(cmd_line, &config);

  EXPECT_EQ("C:\\temp\\test_install", config.install_path);
}

TEST_F(BootstrapHelpersTest, ParseInstallPathEmpty) {
  auto cmd_line =
      MakeCommandLine(SlashSwitchWithValue(kSwitchInstallPath, L""));

  ExtendedConfig config;
  ParseExtendedConfigFromCommandLine(cmd_line, &config);

  EXPECT_TRUE(config.install_path.empty());
}
#endif

TEST_F(BootstrapHelpersTest, ParseLogLevel) {
  auto cmd_line =
      MakeCommandLine(SlashSwitchWithValue(kSwitchLogLevel, L"info"));

  ExtendedConfig config;
  EXPECT_EQ(LogLevel::kWarning, config.log_level);  // Default

  ParseExtendedConfigFromCommandLine(cmd_line, &config);

  EXPECT_EQ(LogLevel::kInfo, config.log_level);
}

TEST_F(BootstrapHelpersTest, ParseLogLevelError) {
  auto cmd_line =
      MakeCommandLine(SlashSwitchWithValue(kSwitchLogLevel, L"error"));

  ExtendedConfig config;
  ParseExtendedConfigFromCommandLine(cmd_line, &config);

  EXPECT_EQ(LogLevel::kError, config.log_level);
}

TEST_F(BootstrapHelpersTest, ParseLogLevelInvalid) {
  auto cmd_line =
      MakeCommandLine(SlashSwitchWithValue(kSwitchLogLevel, L"debug"));

  ExtendedConfig config;
  ParseExtendedConfigFromCommandLine(cmd_line, &config);

  // Invalid value should leave default unchanged
  EXPECT_EQ(LogLevel::kWarning, config.log_level);
}

TEST(InstallerStartupResultTest, MapsConfiguredSuccessAndFailure) {
  Config config;
  config.appid = "12345678-1234-1234-1234-123456789abc";
  config.vmin = "151.0";
  config.vmax = "152.0";
  ConfigLoadResult loaded{.status = ConfigLoadStatus::kLoaded};
  InstallerStartupState state = MakeInstallerStartupState(loaded, config);
  EXPECT_TRUE(state.configured);
  EXPECT_EQ(0, state.error_code);
  EXPECT_EQ("151.0", state.effective_vmin);

  ExtendedConfig extended;
  Result success =
      Result::Success(base::FilePath(L"C:\\CEF\\libcef.dll"), "151.0",
                      "151.0+gabc+chromium-151.0.0.0", true);
  success.liveness_path = success.libcef_path.DirName().Append(L"liveness");
  ApplyInstallerResultToStartupState(success, config, extended, true, &state);
  EXPECT_EQ(success.libcef_path, state.libcef_path);
  EXPECT_TRUE(state.is_bundled);
  EXPECT_EQ(0, state.error_code);
  EXPECT_TRUE(state.error_message.empty());
  EXPECT_EQ(success.liveness_path, state.liveness_path);

  Result failure = Result::Error(kExitCodeCancelled, "cancelled");
  ApplyInstallerResultToStartupState(failure, config, extended, true, &state);
  EXPECT_TRUE(state.libcef_path.empty());
  EXPECT_EQ(kExitCodeCancelled, state.error_code);
  EXPECT_EQ("cancelled", state.error_message);
  EXPECT_TRUE(state.liveness_path.empty());
}

TEST(InstallerStartupResultTest, LaunchHealthRequiresResolvedInstall) {
  Config config;
  config.appid = "12345678-1234-1234-1234-123456789abc";
  ExtendedConfig extended;
  Result success =
      Result::Success(base::FilePath(L"C:\\CEF\\libcef.dll"), "151.0",
                      "151.0+gabc+chromium-151.0.0.0", false);
  success.launch_state_path = base::FilePath(L"C:\\CEF\\launch.json");
  success.launch_consecutive_failures = 2;
  success.launch_version = "151.0";
  success.launch_platform = "windows64";
  success.launch_cleanup_paths.emplace_back(L"C:\\CEF\\cleanup");

  InstallerStartupState state;
  ApplyInstallerResultToStartupState(success, config, extended, false, &state);
  EXPECT_TRUE(state.launch_state_path.empty());
  EXPECT_TRUE(state.launch_cleanup_paths.empty());
  EXPECT_TRUE(state.appid.empty());

  Result empty_path_success = success;
  empty_path_success.libcef_path.clear();
  ApplyInstallerResultToStartupState(empty_path_success, config, extended, true,
                                     &state);
  EXPECT_TRUE(state.launch_state_path.empty());
  EXPECT_TRUE(state.launch_cleanup_paths.empty());
  EXPECT_TRUE(state.appid.empty());

  ApplyInstallerResultToStartupState(success, config, extended, true, &state);
  EXPECT_EQ(success.launch_state_path, state.launch_state_path);
  EXPECT_EQ(success.launch_cleanup_paths, state.launch_cleanup_paths);
  EXPECT_EQ(config.appid, state.appid);
}

TEST(InstallerStartupResultTest, MapsNotConfiguredAndConfigError) {
  Config config;
  InstallerStartupState missing = MakeInstallerStartupState(
      {.status = ConfigLoadStatus::kNotFound}, config);
  EXPECT_FALSE(missing.configured);
  EXPECT_TRUE(missing.libcef_path.empty());
  EXPECT_EQ(0, missing.error_code);
  EXPECT_TRUE(missing.error_message.empty());

  InstallerStartupState error =
      MakeInstallerStartupState({.status = ConfigLoadStatus::kError,
                                 .error = ConfigError::kJsonParseError,
                                 .diagnostic = "malformed trusted config"},
                                config);
  EXPECT_FALSE(error.configured);
  EXPECT_EQ(kExitCodeConfigError, error.error_code);
  EXPECT_EQ("malformed trusted config", error.error_message);
}

#if CEF_API_ADDED(15101)
TEST(InstallerStartupResultTest, VersionInfoSizeGating) {
  InstallerStartupState state;
  state.libcef_path = base::FilePath(L"C:\\CEF\\libcef.dll");
  state.is_bundled = true;
  state.version_full = "151.0+gabc+chromium-151.0.0.0";
  state.error_code = kExitCodeNetworkError;
  state.error_message = "network failed";

  cef_version_info_t info = {};
  info.size = CEF_VERSION_INFO_SIZE_WITH_SANDBOX_HASH;
  info.libcef_path = L"unchanged";
  info.libcef_is_bundled = 0;
  info.libcef_version_full = "unchanged";
  info.installer_error_code = 777;
  info.installer_error_message = "unchanged";
  PopulateInstallerVersionInfo(state, &info);
  EXPECT_STREQ(L"unchanged", info.libcef_path);
  EXPECT_EQ(0, info.libcef_is_bundled);
  EXPECT_STREQ("unchanged", info.libcef_version_full);
  EXPECT_EQ(777, info.installer_error_code);
  EXPECT_STREQ("unchanged", info.installer_error_message);

  info.size = CEF_VERSION_INFO_SIZE_WITH_INSTALLER_ERROR;
  PopulateInstallerVersionInfo(state, &info);
  EXPECT_EQ(state.libcef_path.value(), info.libcef_path);
  EXPECT_EQ(1, info.libcef_is_bundled);
  EXPECT_STREQ(state.version_full.c_str(), info.libcef_version_full);
  EXPECT_EQ(kExitCodeNetworkError, info.installer_error_code);
  EXPECT_STREQ("network failed", info.installer_error_message);
}
#endif

TEST(InstallerStartupResultTest, CrashAnnotationValues) {
  InstallerStartupState state;
  state.error_code = kExitCodeNetworkError;
  state.error_message = "network failed";
  state.effective_vmin = "151.0";
  auto values = BuildInstallerCrashAnnotationValues(state);
  EXPECT_TRUE(values.has_failure);
  EXPECT_EQ("101", values.error);
  EXPECT_EQ("network failed", values.message);
  EXPECT_EQ("151.0", values.vmin);
  EXPECT_EQ("unbounded", values.vmax);
  EXPECT_TRUE(values.policy_denied.empty());
  EXPECT_TRUE(values.cancelled.empty());

  state.error_code = kExitCodeCancelled;
  EXPECT_EQ("1", BuildInstallerCrashAnnotationValues(state).cancelled);
  state.error_code = kExitCodePolicyDenied;
  EXPECT_EQ("1", BuildInstallerCrashAnnotationValues(state).policy_denied);
  state.error_code = kExitCodePolicyError;
  EXPECT_TRUE(BuildInstallerCrashAnnotationValues(state).policy_denied.empty());
  EXPECT_EQ("119", BuildInstallerCrashAnnotationValues(state).error);
  state.error_code = kExitCodeSuccess;
  EXPECT_FALSE(BuildInstallerCrashAnnotationValues(state).has_failure);
}

TEST(InstallerStartupResultTest, CrashAnnotationOversizedMessage) {
  InstallerStartupState state;
  state.error_code = kExitCodeInstallError;
  state.error_message.assign(4096, 'x');
  auto values = BuildInstallerCrashAnnotationValues(state);
  EXPECT_EQ(4096U, values.message.size());
}

// ============================================================================
// TryLoadInstallerConfig Tests
// ============================================================================

class ConfigPrecedenceTest : public testing::Test {
 protected:
  HMODULE LoadTestDll(const wchar_t* name,
                      DWORD flags = LOAD_LIBRARY_AS_DATAFILE |
                                    LOAD_LIBRARY_AS_IMAGE_RESOURCE) {
    base::FilePath exe_dir;
    base::PathService::Get(base::DIR_EXE, &exe_dir);
    base::FilePath dll_path = exe_dir.Append(name);
    HMODULE module = ::LoadLibraryExW(dll_path.value().c_str(), nullptr, flags);
    if (module) {
      loaded_modules_.push_back(module);
    }
    return module;
  }

  void TearDown() override {
    for (HMODULE m : loaded_modules_) {
      ::FreeLibrary(m);
    }
    loaded_modules_.clear();
  }

  std::vector<HMODULE> loaded_modules_;
};

TEST_F(ConfigPrecedenceTest, ClientDllResourceWinsOverBootstrapResource) {
  HMODULE dll_a =
      LoadTestDll(L"cef_config_test_appid_a.dll", DONT_RESOLVE_DLL_REFERENCES);
  HMODULE dll_b = LoadTestDll(L"cef_config_test_appid_b.dll");
  ASSERT_NE(nullptr, dll_a) << "cef_config_test_appid_a.dll not found";
  ASSERT_NE(nullptr, dll_b) << "cef_config_test_appid_b.dll not found";

  Config config;
  ConfigLoadResult loaded = TryLoadInstallerConfig(dll_a, &config, dll_b);
  ASSERT_EQ(ConfigLoadStatus::kLoaded, loaded.status);
  EXPECT_EQ(ConfigLoadSource::kClientResource, loaded.source);
  EXPECT_EQ("aaaaaaaa-1111-1111-1111-111111111111", config.appid);
  EXPECT_EQ("relative_store", config.install_path);
  EXPECT_EQ((std::vector<std::string>{"https://client.example/prefix/",
                                      "https://client.example/prefix/"}),
            config.cdn_urls);
}

TEST_F(ConfigPrecedenceTest, ClientInstallPathResolvesRelativeToDll) {
  HMODULE dll_a =
      LoadTestDll(L"cef_config_test_appid_a.dll", DONT_RESOLVE_DLL_REFERENCES);
  HMODULE dll_b = LoadTestDll(L"cef_config_test_appid_b.dll");
  ASSERT_NE(nullptr, dll_a);
  ASSERT_NE(nullptr, dll_b);
  Config config;
  ASSERT_EQ(ConfigLoadStatus::kLoaded,
            TryLoadInstallerConfig(dll_a, &config, dll_b).status);

  wchar_t module_path[MAX_PATH];
  const DWORD len = ::GetModuleFileNameW(dll_a, module_path, MAX_PATH);
  ASSERT_GT(len, 0u);
  ASSERT_LT(len, static_cast<DWORD>(MAX_PATH));
  const base::FilePath expected = base::MakeAbsoluteFilePath(
      base::FilePath(module_path).DirName().Append(L"relative_store"));
  ASSERT_TRUE(ResolveClientInstallPath(dll_a, &config));
  EXPECT_EQ(expected.AsUTF8Unsafe(), config.install_path);
}

TEST_F(ConfigPrecedenceTest, BootstrapResourceInstallPathIsIgnored) {
  HMODULE dll_a = LoadTestDll(L"cef_config_test_appid_a.dll");
  ASSERT_NE(nullptr, dll_a);
  Config config;
  ASSERT_EQ(ConfigLoadStatus::kLoaded,
            TryLoadInstallerConfig(nullptr, &config, dll_a).status);
  EXPECT_EQ("aaaaaaaa-1111-1111-1111-111111111111", config.appid);
  EXPECT_TRUE(config.install_path.empty());
}

TEST_F(ConfigPrecedenceTest, BootstrapResourceUsedAsFallback) {
  HMODULE no_config_dll = LoadTestDll(L"cef_config_test_no_config.dll");
  HMODULE dll_b = LoadTestDll(L"cef_config_test_appid_b.dll");
  ASSERT_NE(nullptr, no_config_dll)
      << "cef_config_test_no_config.dll not found";
  ASSERT_NE(nullptr, dll_b) << "cef_config_test_appid_b.dll not found";

  Config config;
  ConfigLoadResult loaded =
      TryLoadInstallerConfig(no_config_dll, &config, dll_b);
  ASSERT_EQ(ConfigLoadStatus::kLoaded, loaded.status);
  EXPECT_EQ(ConfigLoadSource::kBootstrapResource, loaded.source);
  EXPECT_EQ("bbbbbbbb-2222-2222-2222-222222222222", config.appid);
  EXPECT_EQ((std::vector<std::string>{"https://bootstrap.example/"}),
            config.cdn_urls);
}

TEST_F(ConfigPrecedenceTest, SelectedClientWithoutUrlsDoesNotMergeBootstrap) {
  HMODULE client = LoadTestDll(L"cef_config_test_explicit_modes.dll");
  HMODULE bootstrap = LoadTestDll(L"cef_config_test_appid_b.dll");
  ASSERT_NE(nullptr, client);
  ASSERT_NE(nullptr, bootstrap);

  Config config;
  ASSERT_EQ(ConfigLoadStatus::kLoaded,
            TryLoadInstallerConfig(client, &config, bootstrap).status);
  EXPECT_EQ("eeeeeeee-5555-5555-5555-555555555555", config.appid);
  EXPECT_TRUE(config.cdn_urls.empty());
}

TEST_F(ConfigPrecedenceTest, MalformedClientResourceRejected) {
  HMODULE malformed = LoadTestDll(L"cef_config_test_malformed.dll");
  HMODULE valid = LoadTestDll(L"cef_config_test_appid_b.dll");
  ASSERT_NE(nullptr, malformed);
  ASSERT_NE(nullptr, valid);
  Config config;
  ConfigLoadResult result = TryLoadInstallerConfig(malformed, &config, valid);
  EXPECT_EQ(ConfigLoadStatus::kError, result.status);
  EXPECT_EQ(ConfigLoadSource::kNone, result.source);
  EXPECT_EQ(ConfigError::kJsonParseError, result.error);
  EXPECT_NE(std::string::npos, result.diagnostic.find("Client DLL resource"));
}

TEST_F(ConfigPrecedenceTest, InvalidClientResourceRejected) {
  HMODULE invalid = LoadTestDll(L"cef_config_test_invalid.dll");
  HMODULE valid = LoadTestDll(L"cef_config_test_appid_b.dll");
  ASSERT_NE(nullptr, invalid);
  ASSERT_NE(nullptr, valid);
  Config config;
  ConfigLoadResult result = TryLoadInstallerConfig(invalid, &config, valid);
  EXPECT_EQ(ConfigLoadStatus::kError, result.status);
  EXPECT_EQ(ConfigLoadSource::kNone, result.source);
  EXPECT_EQ(ConfigError::kMissingRequiredField, result.error);
}

TEST_F(ConfigPrecedenceTest, InvalidClientCdnUrlsRejectWithoutFallback) {
  HMODULE invalid = LoadTestDll(L"cef_config_test_invalid_cdn.dll");
  HMODULE valid = LoadTestDll(L"cef_config_test_appid_b.dll");
  ASSERT_NE(nullptr, invalid);
  ASSERT_NE(nullptr, valid);
  Config config;
  ConfigLoadResult result = TryLoadInstallerConfig(invalid, &config, valid);
  EXPECT_EQ(ConfigLoadStatus::kError, result.status);
  EXPECT_EQ(ConfigLoadSource::kNone, result.source);
  EXPECT_EQ(ConfigError::kInvalidFieldValue, result.error);
  EXPECT_NE(std::string::npos, result.diagnostic.find("Client DLL resource"));
}

TEST_F(ConfigPrecedenceTest, MalformedBootstrapResourceRejected) {
  HMODULE no_config = LoadTestDll(L"cef_config_test_no_config.dll");
  HMODULE malformed = LoadTestDll(L"cef_config_test_malformed.dll");
  ASSERT_NE(nullptr, no_config);
  ASSERT_NE(nullptr, malformed);
  Config config;
  ConfigLoadResult result =
      TryLoadInstallerConfig(no_config, &config, malformed);
  EXPECT_EQ(ConfigLoadStatus::kError, result.status);
  EXPECT_EQ(ConfigLoadSource::kNone, result.source);
  EXPECT_EQ(ConfigError::kJsonParseError, result.error);
  EXPECT_NE(std::string::npos, result.diagnostic.find("Bootstrap resource"));
}

TEST_F(ConfigPrecedenceTest, InvalidBootstrapCdnUrlsAreRejected) {
  HMODULE invalid = LoadTestDll(L"cef_config_test_invalid_cdn.dll");
  ASSERT_NE(nullptr, invalid);
  Config config;
  ConfigLoadResult result = TryLoadInstallerConfig(
      /*client_dll_module=*/nullptr, &config, invalid);
  EXPECT_EQ(ConfigLoadStatus::kError, result.status);
  EXPECT_EQ(ConfigLoadSource::kNone, result.source);
  EXPECT_EQ(ConfigError::kInvalidFieldValue, result.error);
  EXPECT_NE(std::string::npos, result.diagnostic.find("Bootstrap resource"));
}

TEST_F(ConfigPrecedenceTest, CdnResourceShapeAndBoundsRejectEverySource) {
  HMODULE valid = LoadTestDll(L"cef_config_test_appid_b.dll");
  ASSERT_NE(nullptr, valid);
  for (const wchar_t* name :
       {L"cef_config_test_cdn_wrong_shape.dll",
        L"cef_config_test_cdn_empty.dll", L"cef_config_test_cdn_overcount.dll",
        L"cef_config_test_cdn_overlength.dll"}) {
    HMODULE invalid = LoadTestDll(name);
    ASSERT_NE(nullptr, invalid) << name;

    Config client_config;
    ConfigLoadResult client_result =
        TryLoadInstallerConfig(invalid, &client_config, valid);
    EXPECT_EQ(ConfigLoadStatus::kError, client_result.status) << name;
    EXPECT_EQ(ConfigError::kInvalidFieldValue, client_result.error) << name;
    EXPECT_EQ(ConfigLoadSource::kNone, client_result.source) << name;
    EXPECT_NE(std::string::npos,
              client_result.diagnostic.find("Client DLL resource"))
        << name;

    Config bootstrap_config;
    ConfigLoadResult bootstrap_result = TryLoadInstallerConfig(
        /*client_dll_module=*/nullptr, &bootstrap_config, invalid);
    EXPECT_EQ(ConfigLoadStatus::kError, bootstrap_result.status) << name;
    EXPECT_EQ(ConfigError::kInvalidFieldValue, bootstrap_result.error) << name;
    EXPECT_EQ(ConfigLoadSource::kNone, bootstrap_result.source) << name;
    EXPECT_NE(std::string::npos,
              bootstrap_result.diagnostic.find("Bootstrap resource"))
        << name;
  }
}

TEST_F(ConfigPrecedenceTest, StandaloneBootstrapResourceLoads) {
  HMODULE valid = LoadTestDll(L"cef_config_test_appid_b.dll");
  ASSERT_NE(nullptr, valid);
  Config config;
  ConfigLoadResult result =
      TryLoadInstallerConfig(/*client_dll_module=*/nullptr, &config, valid);
  EXPECT_EQ(ConfigLoadStatus::kLoaded, result.status);
  EXPECT_EQ(ConfigLoadSource::kBootstrapResource, result.source);
  EXPECT_EQ("bbbbbbbb-2222-2222-2222-222222222222", config.appid);
}

TEST_F(ConfigPrecedenceTest, StandaloneResourceNotFound) {
  HMODULE no_config = LoadTestDll(L"cef_config_test_no_config.dll");
  ASSERT_NE(nullptr, no_config);
  Config config;
  const ConfigLoadResult result =
      TryLoadInstallerConfig(/*client_dll_module=*/nullptr, &config, no_config);
  EXPECT_EQ(ConfigLoadStatus::kNotFound, result.status);
  EXPECT_EQ(ConfigError::kResourceNotFound, result.error);
  EXPECT_EQ(ConfigLoadSource::kNone, result.source);
}

TEST_F(ConfigPrecedenceTest, StandaloneMalformedResourceRejected) {
  HMODULE malformed = LoadTestDll(L"cef_config_test_malformed.dll");
  ASSERT_NE(nullptr, malformed);
  Config config;
  const ConfigLoadResult result =
      TryLoadInstallerConfig(/*client_dll_module=*/nullptr, &config, malformed);
  EXPECT_EQ(ConfigLoadStatus::kError, result.status);
  EXPECT_EQ(ConfigError::kJsonParseError, result.error);
  EXPECT_EQ(ConfigLoadSource::kNone, result.source);
  EXPECT_NE(std::string::npos, result.diagnostic.find("Bootstrap resource"));
}

TEST(ConfigLoaderApiTest, AcceptsOnlyModuleInputs) {
  using LoaderFunction = ConfigLoadResult (*)(HMODULE, Config*, HMODULE, bool);
  LoaderFunction loader = &TryLoadInstallerConfig;
  EXPECT_NE(nullptr, loader);
}

TEST(ConfigLoaderApiTest, NullOutputHasNoSource) {
  const ConfigLoadResult result =
      TryLoadInstallerConfig(nullptr, nullptr, nullptr);
  EXPECT_EQ(ConfigLoadStatus::kError, result.status);
  EXPECT_EQ(ConfigLoadSource::kNone, result.source);
}
TEST_F(ConfigPrecedenceTest, ExplicitModesFromBootstrapExeWithClientDll) {
  // When a client DLL provides the main config, enable_explicit_modes should
  // still be read from the bootstrap exe's resource.
  HMODULE dll_a = LoadTestDll(L"cef_config_test_appid_a.dll");
  HMODULE explicit_dll = LoadTestDll(L"cef_config_test_explicit_modes.dll");
  ASSERT_NE(nullptr, dll_a) << "cef_config_test_appid_a.dll not found";
  ASSERT_NE(nullptr, explicit_dll)
      << "cef_config_test_explicit_modes.dll not found";

  Config config;
  ConfigLoadResult loaded =
      TryLoadInstallerConfig(dll_a, &config, explicit_dll);
  ASSERT_EQ(ConfigLoadStatus::kLoaded, loaded.status);
  EXPECT_EQ("aaaaaaaa-1111-1111-1111-111111111111", config.appid);
  EXPECT_TRUE(config.enable_explicit_modes);
  EXPECT_EQ("relative_store", config.install_path);
}

TEST_F(ConfigPrecedenceTest, ExplicitModesDefaultsFalseWithClientDll) {
  // When the bootstrap exe has no enable_explicit_modes, it should default
  // to false even when the client DLL provides a valid config.
  HMODULE dll_a = LoadTestDll(L"cef_config_test_appid_a.dll");
  HMODULE dll_b = LoadTestDll(L"cef_config_test_appid_b.dll");
  ASSERT_NE(nullptr, dll_a) << "cef_config_test_appid_a.dll not found";
  ASSERT_NE(nullptr, dll_b) << "cef_config_test_appid_b.dll not found";

  Config config;
  ConfigLoadResult loaded = TryLoadInstallerConfig(dll_a, &config, dll_b);
  ASSERT_EQ(ConfigLoadStatus::kLoaded, loaded.status);
  EXPECT_EQ("aaaaaaaa-1111-1111-1111-111111111111", config.appid);
  EXPECT_FALSE(config.enable_explicit_modes);
}

TEST(RetentionOutputTest, FormatsDeterministicText) {
  Result result = Result::Success({}, "");
  result.retention_max_age_days = 180;
  RetentionPlan plan;
  RetentionRegistrationReport registration;
  registration.entry = {"app", "windows64", "100", "200", "abi"};
  registration.evidence = {RetentionEvidenceKind::kLiveness, 123};
  registration.age_days = 180;
  registration.decision = RetentionRegistrationDecision::kReclaim;
  registration.reason = RetentionReason::kStaleEvidence;
  plan.registrations.push_back(registration);
  RetentionVersionReport version;
  version.version = Version::Parse("100.1");
  version.platform = "windows64";
  version.required_before = true;
  version.expected_removal = true;
  version.decision = RetentionVersionDecision::kNewlyReclaimable;
  version.reason = RetentionReason::kNewlyUnreferenced;
  plan.versions.push_back(version);
  result.retention_plan = std::move(plan);

  EXPECT_EQ(
      "retention outcome=committed max_age_days=180 "
      "registrations_committed=false versions_pruned=false "
      "retry_required=false\r\n"
      "registration appid=app platform=windows64 vmin=100 vmax=200 "
      "abi_hash=abi evidence_kind=liveness evidence_time=123 age_days=180 "
      "decision=reclaim reason=stale_evidence\r\n"
      "version value=100.1 platform=windows64 required_before=true "
      "required_after=false expected_removal=true cleanup_deferred=false "
      "decision=newly_reclaimable reason=newly_unreferenced\r\n",
      FormatRetentionOutput(result, /*json_output=*/false));
}

TEST(RetentionOutputTest, FormatsJsonWithTerminatingCrlf) {
  Result result = Result::Success({}, "");
  result.retention_max_age_days = 180;
  result.retention_plan = RetentionPlan{};

  std::string output = FormatRetentionOutput(result, /*json_output=*/true);

  ASSERT_TRUE(output.ends_with("\r\n"));
  output.resize(output.size() - 2);
  std::optional<Result> parsed = Result::FromJson(output);
  ASSERT_TRUE(parsed);
  EXPECT_TRUE(parsed->retention_plan);
  EXPECT_EQ(180, parsed->retention_max_age_days);
}

TEST(RetentionOutputTest, FormatsErrorsAndWarningsWithPlan) {
  Result result = Result::Error(kExitCodeInstallError, "commit failed");
  result.retention_plan = RetentionPlan{};
  result.warnings = {"version cleanup deferred", "evidence cleanup deferred"};

  const std::string output =
      FormatRetentionOutput(result, /*json_output=*/false);

  EXPECT_NE(std::string::npos, output.find("error=commit failed\r\n"));
  EXPECT_NE(std::string::npos,
            output.find("warning=version cleanup deferred\r\n"));
  EXPECT_NE(std::string::npos,
            output.find("warning=evidence cleanup deferred\r\n"));
  EXPECT_LT(output.find("warning=evidence cleanup deferred\r\n"),
            output.find("warning=version cleanup deferred\r\n"));
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST(RetentionOutputTest, WriteAllHandlesCompleteWrite) {
  std::string received;
  int calls = 0;
  auto write = base::BindRepeating(
      [](std::string* received, int* calls, HANDLE, const char* data,
         DWORD size, DWORD* written) {
        ++*calls;
        received->append(data, size);
        *written = size;
        return true;
      },
      base::Unretained(&received), base::Unretained(&calls));

  EXPECT_TRUE(internal::WriteAllToHandleForTesting(reinterpret_cast<HANDLE>(1),
                                                   "abcdef", 100, write));
  EXPECT_EQ("abcdef", received);
  EXPECT_EQ(1, calls);
}

TEST(RetentionOutputTest, WriteAllHandlesPartialAndChunkedWrites) {
  std::string received;
  std::vector<DWORD> requests;
  auto write = base::BindRepeating(
      [](std::string* received, std::vector<DWORD>* requests, HANDLE,
         const char* data, DWORD size, DWORD* written) {
        requests->push_back(size);
        *written = std::min<DWORD>(2, size);
        received->append(data, *written);
        return true;
      },
      base::Unretained(&received), base::Unretained(&requests));

  EXPECT_TRUE(internal::WriteAllToHandleForTesting(reinterpret_cast<HANDLE>(1),
                                                   "abcdefg", 3, write));
  EXPECT_EQ("abcdefg", received);
  EXPECT_EQ((std::vector<DWORD>{3, 3, 3, 1}), requests);
}

TEST(RetentionOutputTest, WriteAllRejectsZeroProgress) {
  auto write =
      base::BindRepeating([](HANDLE, const char*, DWORD, DWORD* written) {
        *written = 0;
        return true;
      });

  EXPECT_FALSE(internal::WriteAllToHandleForTesting(reinterpret_cast<HANDLE>(1),
                                                    "data", 100, write));
  EXPECT_EQ(static_cast<DWORD>(ERROR_WRITE_FAULT), ::GetLastError());
}

TEST(RetentionOutputTest, WriteAllPreservesWriteFailure) {
  auto write = base::BindRepeating([](HANDLE, const char*, DWORD, DWORD*) {
    ::SetLastError(ERROR_ACCESS_DENIED);
    return false;
  });

  EXPECT_FALSE(internal::WriteAllToHandleForTesting(reinterpret_cast<HANDLE>(1),
                                                    "data", 100, write));
  EXPECT_EQ(static_cast<DWORD>(ERROR_ACCESS_DENIED), ::GetLastError());
}

TEST(RetentionOutputTest, WriteAllRejectsInvalidInputsWithoutWriting) {
  int calls = 0;
  auto write = base::BindRepeating(
      [](int* calls, HANDLE, const char*, DWORD size, DWORD* written) {
        ++*calls;
        *written = size;
        return true;
      },
      base::Unretained(&calls));

  EXPECT_FALSE(
      internal::WriteAllToHandleForTesting(nullptr, "data", 100, write));
  EXPECT_EQ(static_cast<DWORD>(ERROR_INVALID_HANDLE), ::GetLastError());
  EXPECT_FALSE(internal::WriteAllToHandleForTesting(reinterpret_cast<HANDLE>(1),
                                                    "data", 0, write));
  EXPECT_EQ(static_cast<DWORD>(ERROR_INVALID_PARAMETER), ::GetLastError());
  EXPECT_EQ(0, calls);
}

TEST(RetentionOutputTest, WriteAllRejectsOversizedProgress) {
  auto write =
      base::BindRepeating([](HANDLE, const char*, DWORD size, DWORD* written) {
        *written = size + 1;
        return true;
      });

  EXPECT_FALSE(internal::WriteAllToHandleForTesting(reinterpret_cast<HANDLE>(1),
                                                    "data", 100, write));
  EXPECT_EQ(static_cast<DWORD>(ERROR_WRITE_FAULT), ::GetLastError());
}
#endif

}  // namespace
}  // namespace cef_installer
