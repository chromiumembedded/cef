// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_launch_state.h"

#include <windows.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "cef/libcef_dll/bootstrap/installer/installer_lock.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {

class LaunchStateTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }
  void TearDown() override {
    SetRetentionCanonicalObservationHookForTesting({});
    SetLaunchStateGcTimeForTesting(std::nullopt);
  }

  base::FilePath install_dir() { return temp_dir_.GetPath(); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(LaunchStateTest, GetAppidHash_Deterministic) {
  std::string appid = "A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6";
  std::wstring hash1 = GetAppidHash(appid);
  std::wstring hash2 = GetAppidHash(appid);
  EXPECT_EQ(hash1, hash2);
  EXPECT_EQ(16u, hash1.size());
}

TEST_F(LaunchStateTest, GetAppidHash_DifferentAppids) {
  std::wstring hash1 = GetAppidHash("A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6");
  std::wstring hash2 = GetAppidHash("B4C0D5E6-F7A8-5B9C-0D1E-F2A3B4C5D6E7");
  EXPECT_NE(hash1, hash2);
}

TEST_F(LaunchStateTest, IsProcessAlive_CurrentProcess) {
  uint32_t pid = GetCurrentProcessId();
  uint64_t start_time = GetCurrentPidStartTime();
  EXPECT_TRUE(IsProcessAlive(pid, start_time));
}

TEST_F(LaunchStateTest, IsProcessAlive_DeadProcess) {
  EXPECT_FALSE(IsProcessAlive(99999, 1));
}

TEST_F(LaunchStateTest, IsProcessAlive_WrongStartTime) {
  uint32_t pid = GetCurrentProcessId();
  EXPECT_FALSE(IsProcessAlive(pid, 1));
}

TEST_F(LaunchStateTest, AssessLaunchStateModeAndBootMatrix) {
  LaunchState state;
  state.running = true;
  state.pid_start_time = 200;
  state.consecutive_failures = 2;

  EXPECT_EQ(
      LaunchStateStatus::kIndeterminate,
      AssessLaunchState(LaunchHealthMode::kOff, state, 100, false).status);
  EXPECT_EQ(LaunchStateStatus::kIndeterminate,
            AssessLaunchState(LaunchHealthMode::kExplicit, state, std::nullopt,
                              std::nullopt)
                .status);
  EXPECT_EQ(
      LaunchStateStatus::kIndeterminate,
      AssessLaunchState(LaunchHealthMode::kExitCode, state, 201, std::nullopt)
          .status);
  EXPECT_EQ(
      LaunchStateStatus::kRunning,
      AssessLaunchState(LaunchHealthMode::kExplicit, state, 200, true).status);
  const LaunchStateAssessment failure =
      AssessLaunchState(LaunchHealthMode::kExplicit, state, 200, false);
  EXPECT_EQ(LaunchStateStatus::kFailure, failure.status);
  EXPECT_EQ(3, failure.consecutive_failures);
}

TEST_F(LaunchStateTest, AssessLaunchStateConfirmedAndNeutral) {
  LaunchState state;
  state.running = false;
  state.consecutive_failures = 0;
  state.confirmed = true;
  EXPECT_EQ(LaunchStateStatus::kConfirmed,
            AssessLaunchState(LaunchHealthMode::kExplicit, state, std::nullopt,
                              std::nullopt)
                .status);
  state.consecutive_failures = 2;
  EXPECT_EQ(LaunchStateStatus::kNeutral,
            AssessLaunchState(LaunchHealthMode::kExitCode, state, std::nullopt,
                              std::nullopt)
                .status);
}

TEST_F(LaunchStateTest, ApplyLaunchExitModeMatrix) {
  LaunchState state;
  state.running = true;
  state.consecutive_failures = 2;
  EXPECT_FALSE(ApplyLaunchExit(LaunchHealthMode::kOff, true, false, &state));
  EXPECT_TRUE(state.running);

  EXPECT_TRUE(
      ApplyLaunchExit(LaunchHealthMode::kExplicit, false, false, &state));
  EXPECT_FALSE(state.running);
  EXPECT_EQ(2, state.consecutive_failures);

  state.running = true;
  EXPECT_TRUE(
      ApplyLaunchExit(LaunchHealthMode::kExitCode, true, false, &state));
  EXPECT_FALSE(state.running);
  EXPECT_EQ(0, state.consecutive_failures);

  state.running = true;
  state.consecutive_failures = 2;
  EXPECT_TRUE(
      ApplyLaunchExit(LaunchHealthMode::kExitCode, false, true, &state));
  EXPECT_FALSE(state.running);
  EXPECT_EQ(2, state.consecutive_failures);

  state.running = true;
  EXPECT_FALSE(
      ApplyLaunchExit(LaunchHealthMode::kExitCode, false, false, &state));
  EXPECT_TRUE(state.running);
}

TEST_F(LaunchStateTest,
       EvaluateAppLaunchHealthOffSkipsAndExplicitDisqualifies) {
  const std::string appid = "550e8400-e29b-41d4-a716-446655440000";
  const Version version = Version::Parse("150.1.0");
  LaunchState state;
  state.appid = appid;
  state.pid = 99999;
  state.pid_start_time = GetCurrentPidStartTime();
  state.consecutive_failures = 2;
  state.running = true;
  state.confirmed = false;
  state.version = version.ToString();
  state.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir(), GetAppidHash(appid), version,
                                   GetCurrentPlatform()),
      state));

  AppLaunchHealthEvaluation off =
      EvaluateAppLaunchHealth(install_dir(), appid, LaunchHealthMode::kOff, 3);
  EXPECT_TRUE(off.states.empty());
  EXPECT_TRUE(off.disqualified_versions.empty());

  AppLaunchHealthEvaluation explicit_mode = EvaluateAppLaunchHealth(
      install_dir(), appid, LaunchHealthMode::kExplicit, 3);
  EXPECT_EQ(1u, explicit_mode.states.size());
  EXPECT_EQ(1u, explicit_mode.disqualified_versions.count(version));
}

TEST_F(LaunchStateTest, MissingConfirmedFieldIsRejected) {
  base::FilePath path =
      GetLaunchStateDir(install_dir()).AppendASCII("legacy_state");
  ASSERT_TRUE(base::CreateDirectory(path.DirName()));
  ASSERT_TRUE(
      base::WriteFile(path, R"({"appid":"legacy","pid":1,"pid_start_time":"1",)"
                            R"("consecutive_failures":0,"running":false,)"
                            R"("version":"150.1.0","platform":"windows64"})"));

  std::optional<LaunchState> state = ReadLaunchStatePath(path);
  EXPECT_FALSE(state);
}

// ============================================================================
// Process-global active launch state path
// ============================================================================

TEST_F(LaunchStateTest, ActiveLaunchStatePath_DefaultsEmpty) {
  // Defaults to empty before any set. RoundTrip restores empty so this holds
  // regardless of test execution order.
  EXPECT_TRUE(GetActiveLaunchStatePath().empty());
}

TEST_F(LaunchStateTest, ActiveLaunchStatePath_RoundTrip) {
  base::FilePath path =
      install_dir().Append(L".launch").Append(L"abc_150.1.0_windows64");
  SetActiveLaunchStatePath(path);
  EXPECT_EQ(path, GetActiveLaunchStatePath());

  // Restore the empty default so other tests (and DefaultsEmpty) see a clean
  // process-global.
  SetActiveLaunchStatePath(base::FilePath());
  EXPECT_TRUE(GetActiveLaunchStatePath().empty());
}

// ============================================================================
// Install-dir-level API tests (.launch/ directory)
// ============================================================================

TEST_F(LaunchStateTest, GetLaunchStateDir_ReturnsCorrectPath) {
  base::FilePath dir = GetLaunchStateDir(install_dir());
  EXPECT_EQ(dir, install_dir().Append(L".launch"));
}

TEST_F(LaunchStateTest, GetInstallDirLaunchStatePath_Format) {
  Version ver = Version::Parse("150.1.0");
  base::FilePath path = GetInstallDirLaunchStatePath(
      install_dir(), L"abc123def4567890", ver, "windows64");
  EXPECT_EQ(path, install_dir()
                      .Append(L".launch")
                      .Append(L"abc123def4567890_150.1.0_windows64"));
}

TEST_F(LaunchStateTest, ReadWriteLaunchStatePath_RoundTrip) {
  base::FilePath launch_dir = install_dir().Append(L".launch");
  base::FilePath path =
      launch_dir.Append(L"abc123def4567890_150.1.0_windows64");

  LaunchState state;
  state.appid = "test-app-uuid";
  state.pid = 42;
  state.pid_start_time = 133612345678901234ULL;
  state.consecutive_failures = 1;
  state.running = true;
  state.version = "150.1.0";
  state.platform = "windows64";

  constexpr uint64_t kPublicationTime = 123456789;
  SetLaunchStateGcTimeForTesting(kPublicationTime);
  state.last_update = 1;
  ASSERT_TRUE(WriteLaunchStatePath(path, state));

  auto read = ReadLaunchStatePath(path);
  ASSERT_TRUE(read.has_value());
  EXPECT_EQ(state.appid, read->appid);
  EXPECT_EQ(state.pid, read->pid);
  EXPECT_EQ(state.pid_start_time, read->pid_start_time);
  EXPECT_EQ(state.consecutive_failures, read->consecutive_failures);
  EXPECT_EQ(state.running, read->running);
  EXPECT_EQ(state.version, read->version);
  EXPECT_EQ(state.platform, read->platform);
  EXPECT_EQ(kPublicationTime, read->last_update);
}

TEST_F(LaunchStateTest, HealthWriterOwnsAndRefreshesLastUpdate) {
  base::FilePath path = GetInstallDirLaunchStatePath(
      install_dir(), L"abc123def4567890", Version::Parse("150.1.0"),
      GetCurrentPlatform());
  LaunchState state;
  state.appid = "test-app";
  state.pid_start_time = 1;
  state.version = "150.1.0";
  state.platform = GetCurrentPlatform();
  state.last_update = 7;

  SetLaunchStateGcTimeForTesting(100);
  ASSERT_TRUE(WriteLaunchStatePath(path, state));
  ASSERT_EQ(100u, ReadLaunchStatePath(path)->last_update);
  SetLaunchStateGcTimeForTesting(200);
  ASSERT_TRUE(WriteLaunchStatePath(path, state));
  EXPECT_EQ(200u, ReadLaunchStatePath(path)->last_update);
}

TEST_F(LaunchStateTest, ZeroPublicationClockPreservesPriorHealthState) {
  base::FilePath path = GetInstallDirLaunchStatePath(
      install_dir(), L"abc123def4567890", Version::Parse("150.1.0"),
      GetCurrentPlatform());
  LaunchState state;
  state.appid = "test-app";
  state.pid_start_time = 1;
  state.version = "150.1.0";
  state.platform = GetCurrentPlatform();
  SetLaunchStateGcTimeForTesting(100);
  ASSERT_TRUE(WriteLaunchStatePath(path, state));
  std::string before;
  ASSERT_TRUE(base::ReadFileToString(path, &before));

  SetLaunchStateGcTimeForTesting(0);
  EXPECT_FALSE(WriteLaunchStatePath(path, state));
  std::string after;
  ASSERT_TRUE(base::ReadFileToString(path, &after));
  EXPECT_EQ(before, after);
}

TEST_F(LaunchStateTest, LaunchStateGcAgeBoundaries) {
  constexpr uint64_t kNow = kLaunchStateGcMaxAge + 100;
  SetLaunchStateGcTimeForTesting(kNow);
  EXPECT_EQ(LaunchStateGcAge::kInvalid, ClassifyLaunchStateGcAge(0));
  EXPECT_EQ(LaunchStateGcAge::kFresh,
            ClassifyLaunchStateGcAge(kNow - kLaunchStateGcMaxAge + 1));
  EXPECT_EQ(LaunchStateGcAge::kExpired,
            ClassifyLaunchStateGcAge(kNow - kLaunchStateGcMaxAge));
  EXPECT_EQ(LaunchStateGcAge::kFresh, ClassifyLaunchStateGcAge(kNow + 1));
  SetLaunchStateGcTimeForTesting(0);
  EXPECT_EQ(LaunchStateGcAge::kUnavailable, ClassifyLaunchStateGcAge(kNow));
}

TEST_F(LaunchStateTest, OldSchemaAndFooterlessHealthAreRejectedAndPreserved) {
  base::FilePath dir = GetLaunchStateDir(install_dir());
  ASSERT_TRUE(base::CreateDirectory(dir));
  const std::string old_schema =
      R"({"appid":"test-app","pid":1,"pid_start_time":"1",)"
      R"("consecutive_failures":0,"running":false,"confirmed":false,)"
      R"("version":"150.1.0","platform":"windows64"})";
  base::FilePath old_path = dir.Append(L"old-schema");
  ASSERT_TRUE(WriteFileWithIntegrity(old_path, old_schema));
  EXPECT_FALSE(ReadLaunchStatePath(old_path));
  EXPECT_TRUE(base::PathExists(old_path));

  const std::string current_schema =
      R"({"appid":"test-app","pid":1,"pid_start_time":"1",)"
      R"("consecutive_failures":0,"running":false,"confirmed":false,)"
      R"("version":"150.1.0","platform":"windows64",)"
      R"("last_update":"1"})";
  base::FilePath footerless_path = dir.Append(L"footerless");
  ASSERT_TRUE(base::WriteFile(footerless_path, current_schema));
  EXPECT_FALSE(ReadLaunchStatePath(footerless_path));
  EXPECT_TRUE(base::PathExists(footerless_path));
}

TEST_F(LaunchStateTest, InvalidHealthTimestampsAreRejected) {
  base::FilePath dir = GetLaunchStateDir(install_dir());
  ASSERT_TRUE(base::CreateDirectory(dir));
  const char quote = 34;
  const std::vector<std::string> values = {
      std::string() + quote + "0" + quote, "-1",
      std::string() + quote + "-1" + quote,
      std::string() + quote + "18446744073709551616" + quote,
      std::string() + quote + "not-a-time" + quote};
  for (size_t i = 0; i < values.size(); ++i) {
    const std::string json =
        R"({"appid":"test-app","pid":1,"pid_start_time":"1",)"
        R"("consecutive_failures":0,"running":false,"confirmed":false,)"
        R"("version":"150.1.0","platform":"windows64","last_update":)" +
        values[i] + "}";
    base::FilePath path =
        dir.AppendASCII("invalid-time-" + base::NumberToString(i));
    ASSERT_TRUE(WriteFileWithIntegrity(path, json));
    EXPECT_FALSE(ReadLaunchStatePath(path)) << values[i];
    EXPECT_TRUE(base::PathExists(path));
  }
}

TEST_F(LaunchStateTest, WriteLaunchStatePath_RejectsEmptyAppid) {
  base::FilePath path =
      install_dir().Append(L".launch").Append(L"abc123def4567890_1.0.0_win");
  LaunchState state;
  state.pid = 1;
  state.pid_start_time = 1;
  state.version = "1.0.0";
  state.platform = "win";
  EXPECT_FALSE(WriteLaunchStatePath(path, state));
}

TEST_F(LaunchStateTest, WriteLaunchStatePath_RejectsEmptyVersion) {
  base::FilePath path =
      install_dir().Append(L".launch").Append(L"abc123def4567890_1.0.0_win");
  LaunchState state;
  state.appid = "test";
  state.pid = 1;
  state.pid_start_time = 1;
  state.platform = "win";
  EXPECT_FALSE(WriteLaunchStatePath(path, state));
}

TEST_F(LaunchStateTest, WriteLaunchStatePath_RejectsEmptyPlatform) {
  base::FilePath path =
      install_dir().Append(L".launch").Append(L"abc123def4567890_1.0.0_win");
  LaunchState state;
  state.appid = "test";
  state.pid = 1;
  state.pid_start_time = 1;
  state.version = "1.0.0";
  EXPECT_FALSE(WriteLaunchStatePath(path, state));
}

TEST_F(LaunchStateTest, ReadLaunchStatePath_MissingFile) {
  base::FilePath path =
      install_dir().Append(L".launch").Append(L"abc123def4567890_1.0_win");
  auto read = ReadLaunchStatePath(path);
  EXPECT_FALSE(read.has_value());
}

TEST_F(LaunchStateTest, ReadLaunchStatePath_CorruptFile) {
  base::FilePath launch_dir = install_dir().Append(L".launch");
  ASSERT_TRUE(base::CreateDirectory(launch_dir));
  base::FilePath path = launch_dir.Append(L"abc123def4567890_1.0_win");
  ASSERT_TRUE(base::WriteFile(path, "corrupt garbage data!!"));

  auto read = ReadLaunchStatePath(path);
  EXPECT_FALSE(read.has_value());
}

TEST_F(LaunchStateTest, ScanLaunchStates_MultipleVersions) {
  base::FilePath install = install_dir();
  std::wstring hash = L"abc123def4567890";

  for (const char* ver : {"150.1.0", "150.2.0", "150.3.0"}) {
    LaunchState ls;
    ls.appid = "test-app";
    ls.pid = 1;
    ls.pid_start_time = 1;
    ls.consecutive_failures = 0;
    ls.running = false;
    ls.version = ver;
    ls.platform = GetCurrentPlatform();
    base::FilePath path = GetInstallDirLaunchStatePath(
        install, hash, Version::Parse(ver), GetCurrentPlatform());
    ASSERT_TRUE(WriteLaunchStatePath(path, ls));
  }

  auto states = ScanLaunchStates(install, hash);
  ASSERT_EQ(3u, states.size());
  EXPECT_TRUE(states.count(Version::Parse("150.1.0")));
  EXPECT_TRUE(states.count(Version::Parse("150.2.0")));
  EXPECT_TRUE(states.count(Version::Parse("150.3.0")));
  EXPECT_EQ("test-app", states.at(Version::Parse("150.1.0")).appid);
  EXPECT_EQ("150.2.0", states.at(Version::Parse("150.2.0")).version);
}

TEST_F(LaunchStateTest, ScanLaunchStates_FiltersAppidHash) {
  base::FilePath install = install_dir();
  std::wstring hash1 = L"aaaa000000000000";
  std::wstring hash2 = L"bbbb000000000000";
  std::string platform = GetCurrentPlatform();

  LaunchState ls;
  ls.appid = "app1";
  ls.pid = 1;
  ls.pid_start_time = 1;
  ls.version = "150.1.0";
  ls.platform = platform;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install, hash1, Version::Parse("150.1.0"),
                                   platform),
      ls));

  ls.appid = "app2";
  ls.version = "150.2.0";
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install, hash2, Version::Parse("150.2.0"),
                                   platform),
      ls));

  auto states = ScanLaunchStates(install, hash1);
  ASSERT_EQ(1u, states.size());
  EXPECT_TRUE(states.count(Version::Parse("150.1.0")));
  EXPECT_EQ("app1", states.at(Version::Parse("150.1.0")).appid);
}

TEST_F(LaunchStateTest, ScanLaunchStates_SkipsMalformed) {
  base::FilePath install = install_dir();
  std::wstring hash = L"abc123def4567890";
  std::string platform = GetCurrentPlatform();

  LaunchState ls;
  ls.appid = "test-app";
  ls.pid = 1;
  ls.pid_start_time = 1;
  ls.version = "150.1.0";
  ls.platform = platform;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install, hash, Version::Parse("150.1.0"),
                                   platform),
      ls));

  // Write a garbage file with the right prefix.
  base::FilePath launch_dir = GetLaunchStateDir(install);
  base::FilePath garbage = launch_dir.Append(hash + L"_garbage");
  ASSERT_TRUE(base::WriteFile(garbage, "not a valid file"));

  auto states = ScanLaunchStates(install, hash);
  ASSERT_EQ(1u, states.size());
  EXPECT_TRUE(states.count(Version::Parse("150.1.0")));
}

TEST_F(LaunchStateTest, ScanLaunchStates_EmptyDir) {
  base::FilePath install = install_dir();
  ASSERT_TRUE(base::CreateDirectory(GetLaunchStateDir(install)));

  auto states = ScanLaunchStates(install, L"abc123def4567890");
  EXPECT_TRUE(states.empty());
}

TEST_F(LaunchStateTest, ScanLaunchStates_NoDir) {
  auto states = ScanLaunchStates(install_dir(), L"abc123def4567890");
  EXPECT_TRUE(states.empty());
}

TEST_F(LaunchStateTest, LivenessFilenameCannotCollideWithHealthSentinel) {
  std::wstring hash = L"abc123def4567890";
  std::string platform = GetCurrentPlatform();
  base::FilePath liveness =
      GetInstallDirLivenessPath(install_dir(), hash, platform);
  base::FilePath health = GetInstallDirLaunchStatePath(
      install_dir(), hash, Version::Parse("150.1.0"), platform);
  EXPECT_NE(liveness, health);
  EXPECT_EQ(hash + L"_" + base::UTF8ToWide(platform),
            liveness.BaseName().value());
}

TEST_F(LaunchStateTest, LivenessRoundTrip) {
  base::FilePath path = GetInstallDirLivenessPath(
      install_dir(), L"abc123def4567890", GetCurrentPlatform());
  LivenessRecord record{"test-app", GetCurrentPlatform(), 123456};
  ASSERT_TRUE(WriteLivenessPath(path, record));
  std::optional<LivenessRecord> read = ReadLivenessPath(path);
  ASSERT_TRUE(read);
  EXPECT_EQ(record.appid, read->appid);
  EXPECT_EQ(record.platform, read->platform);
  EXPECT_EQ(record.last_launch, read->last_launch);
  EXPECT_FALSE(ReadLaunchStatePath(path));
}

TEST_F(LaunchStateTest, LivenessRejectsZeroAndFooterlessRecords) {
  base::FilePath path = GetInstallDirLivenessPath(
      install_dir(), L"abc123def4567890", GetCurrentPlatform());
  EXPECT_FALSE(WriteLivenessPath(path, {"test-app", GetCurrentPlatform(), 0}));
  EXPECT_FALSE(RefreshLivenessPath(path, "test-app", GetCurrentPlatform(), 0));
  ASSERT_TRUE(base::CreateDirectory(path.DirName()));
  ASSERT_TRUE(base::WriteFile(
      path,
      R"({"appid":"test-app","platform":"windows64","last_launch":"1"})"));
  EXPECT_FALSE(ReadLivenessPath(path));
  EXPECT_TRUE(base::PathExists(path));
}

TEST_F(LaunchStateTest, InvalidLivenessTimestampsAreRejectedAndPreserved) {
  base::FilePath path = GetInstallDirLivenessPath(
      install_dir(), L"abc123def4567890", GetCurrentPlatform());
  ASSERT_TRUE(base::CreateDirectory(path.DirName()));
  const std::vector<std::string> invalid_records = {
      R"({"appid":"test-app","platform":"windows64"})",
      R"({"appid":"test-app","platform":"windows64","last_launch":1})",
      R"({"appid":"test-app","platform":"windows64","last_launch":"-1"})",
      R"({"appid":"test-app","platform":"windows64","last_launch":"18446744073709551616"})",
      R"({"appid":"test-app","platform":"windows64","last_launch":"invalid"})",
      R"({"appid":"test-app","platform":"windows64","last_launch":"0"})",
  };
  for (const auto& json : invalid_records) {
    ASSERT_TRUE(WriteFileWithIntegrity(path, json));
    EXPECT_FALSE(ReadLivenessPath(path)) << json;
    EXPECT_TRUE(base::PathExists(path)) << json;
  }
}

TEST_F(LaunchStateTest, LaunchRecordSnapshotBindsExactPublication) {
  const std::string appid = "test-app";
  const std::string platform = GetCurrentPlatform();
  base::FilePath path =
      GetInstallDirLivenessPath(install_dir(), GetAppidHash(appid), platform);
  ASSERT_TRUE(WriteLivenessPath(path, {appid, platform, 100}));
  std::optional<base::FilePath> resolved =
      GetSafeDirectoryResolvedPath(path.DirName());
  ASSERT_TRUE(resolved);
  std::optional<LaunchRecordSnapshot> snapshot =
      ReadLaunchRecordSnapshot(path, *resolved);
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(LaunchRecordKind::kLiveness, snapshot->kind);
  EXPECT_EQ(100u, snapshot->liveness.last_launch);

  ASSERT_TRUE(WriteLivenessPath(path, {appid, platform, 200}));
  EXPECT_EQ(
      ConditionalDeleteResult::kChanged,
      DeleteFileWithIntegrityIfMatching(snapshot->path, snapshot->content,
                                        /*expected_integrity_protected=*/true,
                                        snapshot->resolved_parent_path));
  EXPECT_EQ(200u, ReadLivenessPath(path)->last_launch);
}

TEST_F(LaunchStateTest, LaunchRecordSnapshotClassifiesInvalidPublications) {
  const std::string platform = GetCurrentPlatform();
  base::FilePath path =
      GetInstallDirLivenessPath(install_dir(), L"abc123def4567890", platform);
  ASSERT_TRUE(base::CreateDirectory(path.DirName()));
  std::optional<base::FilePath> resolved =
      GetSafeDirectoryResolvedPath(path.DirName());
  ASSERT_TRUE(resolved);

  ASSERT_TRUE(WriteFileWithIntegrity(path, "{}"));
  std::optional<LaunchRecordSnapshot> malformed =
      ReadLaunchRecordSnapshot(path, *resolved);
  ASSERT_TRUE(malformed);
  EXPECT_EQ(LaunchRecordKind::kInvalid, malformed->kind);
  EXPECT_FALSE(malformed->raw_content.empty());
  EXPECT_FALSE(malformed->oversized);

  const std::string footerless =
      R"({"appid":"test-app","platform":"windows64","last_launch":"1"})";
  ASSERT_TRUE(base::WriteFile(path, footerless));
  std::optional<LaunchRecordSnapshot> no_footer =
      ReadLaunchRecordSnapshot(path, *resolved);
  ASSERT_TRUE(no_footer);
  EXPECT_EQ(LaunchRecordKind::kInvalid, no_footer->kind);
  EXPECT_EQ(footerless, no_footer->raw_content);

  ASSERT_TRUE(WriteLivenessPath(path, {"test-app", platform, 1}));
  std::string corrupt;
  ASSERT_TRUE(base::ReadFileToString(path, &corrupt));
  ASSERT_FALSE(corrupt.empty());
  corrupt[0] ^= 1;
  ASSERT_TRUE(base::WriteFile(path, corrupt));
  std::optional<LaunchRecordSnapshot> crc_mismatch =
      ReadLaunchRecordSnapshot(path, *resolved);
  ASSERT_TRUE(crc_mismatch);
  EXPECT_EQ(LaunchRecordKind::kInvalid, crc_mismatch->kind);
  EXPECT_EQ(corrupt, crc_mismatch->raw_content);

  ASSERT_TRUE(
      base::WriteFile(path, std::string(kMaxLaunchStateFileSize + 17, 'x')));
  std::optional<LaunchRecordSnapshot> oversized =
      ReadLaunchRecordSnapshot(path, *resolved);
  ASSERT_TRUE(oversized);
  EXPECT_EQ(LaunchRecordKind::kInvalid, oversized->kind);
  EXPECT_TRUE(oversized->oversized);

  std::optional<base::FilePath> wrong_parent =
      GetSafeDirectoryResolvedPath(install_dir());
  ASSERT_TRUE(wrong_parent);
  EXPECT_FALSE(ReadLaunchRecordSnapshot(path, *wrong_parent));

  ASSERT_TRUE(base::DeleteFile(path));
  base::FilePath target = install_dir().Append(L"reparse-target");
  ASSERT_TRUE(WriteFileWithIntegrity(target, "target"));
  constexpr DWORD kAllowUnprivilegedCreate = 0x2;
  if (!::CreateSymbolicLinkW(path.value().c_str(), target.value().c_str(),
                             kAllowUnprivilegedCreate)) {
    GTEST_SKIP() << "Symbolic-link creation is unavailable";
  }
  std::optional<LaunchRecordSnapshot> reparse =
      ReadLaunchRecordSnapshot(path, *resolved);
  ASSERT_TRUE(reparse);
  EXPECT_EQ(LaunchRecordKind::kInvalid, reparse->kind);
  EXPECT_TRUE(reparse->reparse_point);
}

TEST_F(LaunchStateTest, LivenessRefreshUsesContentTimestamp) {
  base::FilePath path = GetInstallDirLivenessPath(
      install_dir(), L"abc123def4567890", GetCurrentPlatform());
  constexpr uint64_t now = 100 * kLivenessRefreshIntervalFileTimeTicks;
  ASSERT_TRUE(WriteLivenessPath(
      path, {"test-app", GetCurrentPlatform(),
             now - kLivenessRefreshIntervalFileTimeTicks + 1}));
  ASSERT_TRUE(RefreshLivenessPath(path, "test-app", GetCurrentPlatform(), now));
  ASSERT_EQ(now - kLivenessRefreshIntervalFileTimeTicks + 1,
            ReadLivenessPath(path)->last_launch);

  ASSERT_TRUE(
      WriteLivenessPath(path, {"test-app", GetCurrentPlatform(),
                               now - kLivenessRefreshIntervalFileTimeTicks}));
  ASSERT_TRUE(RefreshLivenessPath(path, "test-app", GetCurrentPlatform(), now));
  EXPECT_EQ(now, ReadLivenessPath(path)->last_launch);
}

TEST_F(LaunchStateTest, LivenessFutureTimestampIsReplacedAfterClockRollback) {
  base::FilePath path = GetInstallDirLivenessPath(
      install_dir(), L"abc123def4567890", GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(path, {"test-app", GetCurrentPlatform(), 200}));
  ASSERT_TRUE(RefreshLivenessPath(path, "test-app", GetCurrentPlatform(), 100));
  EXPECT_EQ(100u, ReadLivenessPath(path)->last_launch);
}

TEST_F(LaunchStateTest, LivenessMalformedOrOversizedIsReplaced) {
  base::FilePath path = GetInstallDirLivenessPath(
      install_dir(), L"abc123def4567890", GetCurrentPlatform());
  ASSERT_TRUE(base::CreateDirectory(path.DirName()));
  ASSERT_TRUE(
      base::WriteFile(path, std::string(kMaxLaunchStateFileSize + 1, 'x')));
  ASSERT_TRUE(RefreshLivenessPath(path, "test-app", GetCurrentPlatform(), 123));
  ASSERT_EQ(123u, ReadLivenessPath(path)->last_launch);
}

TEST_F(LaunchStateTest, RetentionCollectorChoosesNewestEvidence) {
  const std::string appid = "retention-app";
  const std::string platform = GetCurrentPlatform();
  const std::wstring hash = GetAppidHash(appid);
  ASSERT_TRUE(WriteLivenessPath(
      GetInstallDirLivenessPath(install_dir(), hash, platform),
      {appid, platform, 100}));
  LaunchState older_health;
  older_health.appid = appid;
  older_health.pid_start_time = 90;
  older_health.version = "150.1.0";
  older_health.platform = platform;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(
          install_dir(), hash, Version::Parse(older_health.version), platform),
      older_health));
  LaunchState newest_health = older_health;
  newest_health.pid_start_time = 120;
  newest_health.version = "151.1.0";
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(
          install_dir(), hash, Version::Parse(newest_health.version), platform),
      newest_health));

  RetentionEvidenceMap evidence =
      CollectRetentionEvidence(install_dir(), {{"retention-app", platform}});

  const RetentionEvidence& result = evidence.at({appid, platform});
  EXPECT_FALSE(result.unknown);
  EXPECT_EQ(RetentionEvidenceKind::kHealthSentinel, result.kind);
  EXPECT_EQ(120u, result.timestamp);
}

TEST_F(LaunchStateTest, RetentionCollectorIsPlatformScoped) {
  const std::string appid = "retention-app";
  const std::wstring hash = GetAppidHash(appid);
  ASSERT_TRUE(WriteLivenessPath(
      GetInstallDirLivenessPath(install_dir(), hash, "windows64"),
      {appid, "windows64", 100}));
  ASSERT_TRUE(WriteLivenessPath(
      GetInstallDirLivenessPath(install_dir(), hash, "windows32"),
      {appid, "windows32", 200}));

  RetentionEvidenceMap evidence = CollectRetentionEvidence(
      install_dir(), {{appid, "windows64"}, {appid, "windows32"}});

  EXPECT_EQ(100u, evidence.at({appid, "windows64"}).timestamp);
  EXPECT_EQ(200u, evidence.at({appid, "windows32"}).timestamp);
}

TEST_F(LaunchStateTest,
       RetentionSnapshotUsesOneCanonicalObservationForPlanAndCleanup) {
  const std::string appid = "retention-app";
  const std::string platform = GetCurrentPlatform();
  base::FilePath path =
      GetInstallDirLivenessPath(install_dir(), GetAppidHash(appid), platform);
  ASSERT_TRUE(WriteLivenessPath(path, {appid, platform, 100}));
  SetRetentionCanonicalObservationHookForTesting(base::BindRepeating(
      [](const std::string& hook_appid, const std::string& hook_platform,
         const base::FilePath& hook_path) {
        ASSERT_TRUE(
            WriteLivenessPath(hook_path, {hook_appid, hook_platform, 200}));
      },
      appid, platform));

  RetentionEvidenceSnapshot snapshot =
      CollectRetentionEvidenceSnapshot(install_dir(), {{appid, platform}});

  ASSERT_EQ(1u, snapshot.files.size());
  EXPECT_EQ(100u, snapshot.evidence.at({appid, platform}).timestamp);
  EXPECT_EQ(ConditionalDeleteResult::kChanged,
            DeleteFileWithIntegrityIfMatching(
                path, snapshot.files[0].content,
                snapshot.files[0].integrity_protected,
                snapshot.files[0].resolved_parent_path));
  EXPECT_EQ(200u, ReadLivenessPath(path)->last_launch);
}

TEST_F(LaunchStateTest, RetentionCollectorProtectsNoncanonicalEvidence) {
  const std::string appid = "retention-app";
  const std::string platform = GetCurrentPlatform();
  base::FilePath path =
      GetLaunchStateDir(install_dir()).Append(L"noncanonical");
  ASSERT_TRUE(WriteLivenessPath(path, {appid, platform, 100}));

  RetentionEvidenceMap evidence =
      CollectRetentionEvidence(install_dir(), {{appid, platform}});

  EXPECT_TRUE(evidence.at({appid, platform}).unknown);
  EXPECT_EQ("invalid_or_noncanonical_evidence",
            evidence.at({appid, platform}).diagnostic);
}

TEST_F(LaunchStateTest, RetentionCollectorProtectsMalformedCanonicalEvidence) {
  const std::string appid = "retention-app";
  const std::string platform = GetCurrentPlatform();
  base::FilePath path =
      GetInstallDirLivenessPath(install_dir(), GetAppidHash(appid), platform);
  ASSERT_TRUE(base::CreateDirectory(path.DirName()));
  ASSERT_TRUE(base::WriteFile(path, "malformed"));

  RetentionEvidenceMap evidence =
      CollectRetentionEvidence(install_dir(), {{appid, platform}});

  EXPECT_TRUE(evidence.at({appid, platform}).unknown);
  EXPECT_EQ("invalid_or_noncanonical_evidence",
            evidence.at({appid, platform}).diagnostic);
  EXPECT_TRUE(base::PathExists(path));
}

TEST_F(LaunchStateTest, RetentionCollectorPreservesIntegrityMismatch) {
  const std::string appid = "retention-app";
  const std::string platform = GetCurrentPlatform();
  base::FilePath path =
      GetInstallDirLivenessPath(install_dir(), GetAppidHash(appid), platform);
  ASSERT_TRUE(WriteLivenessPath(path, {appid, platform, 100}));
  std::string corrupt;
  ASSERT_TRUE(base::ReadFileToString(path, &corrupt));
  ASSERT_GT(corrupt.size(), 20u);
  corrupt[5] ^= 0xFF;
  ASSERT_TRUE(base::WriteFile(path, corrupt));

  RetentionEvidenceMap evidence =
      CollectRetentionEvidence(install_dir(), {{appid, platform}});

  EXPECT_TRUE(evidence.at({appid, platform}).unknown);
  std::string after;
  ASSERT_TRUE(base::ReadFileToString(path, &after));
  EXPECT_EQ(corrupt, after);
}

TEST_F(LaunchStateTest, RetentionCollectorMissingDirectoryIsProtected) {
  RetentionEvidenceMap evidence = CollectRetentionEvidence(
      install_dir(), {{"retention-app", GetCurrentPlatform()}});

  ASSERT_EQ(1u, evidence.size());
  const RetentionEvidence& result =
      evidence.at({"retention-app", GetCurrentPlatform()});
  EXPECT_EQ(RetentionEvidenceKind::kNone, result.kind);
  EXPECT_EQ(0u, result.timestamp);
  EXPECT_FALSE(result.unknown);
}

TEST_F(LaunchStateTest, RetentionCollectorDoesNotWrite) {
  const std::string appid = "retention-app";
  const std::string platform = GetCurrentPlatform();
  base::FilePath path =
      GetInstallDirLivenessPath(install_dir(), GetAppidHash(appid), platform);
  ASSERT_TRUE(WriteLivenessPath(path, {appid, platform, 100}));
  std::string before;
  ASSERT_TRUE(base::ReadFileToString(path, &before));

  CollectRetentionEvidence(install_dir(), {{appid, platform}});

  std::string after;
  ASSERT_TRUE(base::ReadFileToString(path, &after));
  EXPECT_EQ(before, after);
}

}  // namespace cef_installer
