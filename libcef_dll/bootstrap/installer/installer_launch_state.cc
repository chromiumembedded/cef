// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_launch_state.h"

#include <windows.h>

#include <algorithm>
#include <tuple>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"
#include "cef/libcef_dll/bootstrap/installer/installer_lock.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"

namespace cef_installer {

namespace {

uint64_t FileTimeToTicks(FILETIME ft) {
  ULARGE_INTEGER li;
  li.LowPart = ft.dwLowDateTime;
  li.HighPart = ft.dwHighDateTime;
  return li.QuadPart;
}

// Process-global active launch state path. See header for the threading
// contract: set once before RunWinMain spawns threads, read-only thereafter.
// Thread creation provides the happens-before edge, so no lock is needed.
base::FilePath& ActiveLaunchStatePath() {
  static base::NoDestructor<base::FilePath> path;
  return *path;
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
base::NoDestructor<RetentionCanonicalObservationHookForTesting>
    g_retention_canonical_observation_hook;
std::optional<uint64_t> g_launch_state_gc_time_for_testing;
#endif

uint64_t GetLaunchStateGcCurrentTime() {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (g_launch_state_gc_time_for_testing.has_value()) {
    return *g_launch_state_gc_time_for_testing;
  }
#endif
  FILETIME now = {};
  ::GetSystemTimePreciseAsFileTime(&now);
  return FileTimeToTicks(now);
}

std::optional<base::DictValue> ReadJsonWithIntegrity(
    const base::FilePath& path,
    IntegrityMismatchAction mismatch_action) {
  if (IsReparsePoint(path)) {
    return std::nullopt;
  }
  std::optional<int64_t> size = base::GetFileSize(path);
  if (!size || *size > kMaxLaunchStateFileSize) {
    return std::nullopt;
  }
  std::string content;
  IntegrityResult integrity = ReadFileWithIntegrity(
      path, &content, mismatch_action, kMaxLaunchStateFileSize);
  if (integrity != IntegrityResult::kSuccess) {
    return std::nullopt;
  }
  return base::JSONReader::ReadDict(content, base::JSON_PARSE_RFC);
}

std::optional<LivenessRecord> ParseLivenessDict(const base::DictValue& parsed) {
  const std::string* appid = parsed.FindString("appid");
  const std::string* platform = parsed.FindString("platform");
  const std::string* last_launch = parsed.FindString("last_launch");
  uint64_t timestamp = 0;
  if (!appid || appid->empty() || appid->size() > kMaxUuidLength || !platform ||
      platform->empty() || platform->size() > kMaxPlatformLength ||
      !last_launch || !base::StringToUint64(*last_launch, &timestamp) ||
      timestamp == 0) {
    return std::nullopt;
  }
  return LivenessRecord{*appid, *platform, timestamp};
}

std::optional<LaunchState> ParseLaunchStateDict(const base::DictValue& parsed) {
  const std::string* appid = parsed.FindString("appid");
  std::optional<int> pid_val = parsed.FindInt("pid");
  const std::string* pid_start_str = parsed.FindString("pid_start_time");
  const std::string* last_update_str = parsed.FindString("last_update");
  std::optional<int> failures = parsed.FindInt("consecutive_failures");
  std::optional<bool> running = parsed.FindBool("running");
  std::optional<bool> confirmed = parsed.FindBool("confirmed");
  if (!appid || !pid_val || !pid_start_str || !last_update_str || !failures ||
      !running || !confirmed || appid->size() > kMaxUuidLength) {
    return std::nullopt;
  }
  uint64_t pid_start_time = 0;
  uint64_t last_update = 0;
  if (!base::StringToUint64(*pid_start_str, &pid_start_time) ||
      !base::StringToUint64(*last_update_str, &last_update) ||
      last_update == 0) {
    return std::nullopt;
  }
  const std::string* ver = parsed.FindString("version");
  const std::string* plat = parsed.FindString("platform");
  if (!ver || ver->empty() || ver->size() > kMaxVersionLength || !plat ||
      plat->empty() || plat->size() > kMaxPlatformLength) {
    return std::nullopt;
  }
  LaunchState state;
  state.appid = *appid;
  state.pid = static_cast<uint32_t>(*pid_val);
  state.pid_start_time = pid_start_time;
  state.consecutive_failures = *failures;
  state.running = *running;
  state.confirmed = *confirmed;
  state.version = *ver;
  state.platform = *plat;
  state.last_update = last_update;
  return state;
}

}  // namespace

std::wstring GetAppidHash(const std::string& appid) {
  return base::UTF8ToWide(internal::ComputeSha1HexPrefix(appid));
}

bool IsProcessAlive(uint32_t pid, uint64_t pid_start_time) {
  base::Process process = base::Process::Open(pid);
  if (!process.IsValid()) {
    return false;
  }
  if (!process.IsRunning()) {
    return false;
  }
  return FileTimeToTicks(process.CreationTime().ToFileTime()) == pid_start_time;
}

uint64_t GetCurrentPidStartTime() {
  return FileTimeToTicks(base::Process::Current().CreationTime().ToFileTime());
}

uint64_t GetCurrentWallTime() {
  FILETIME now = {};
  ::GetSystemTimePreciseAsFileTime(&now);
  return FileTimeToTicks(now);
}

LaunchStateGcAge ClassifyLaunchStateGcAge(uint64_t timestamp) {
  if (timestamp == 0) {
    return LaunchStateGcAge::kInvalid;
  }
  const uint64_t now = GetLaunchStateGcCurrentTime();
  if (now == 0) {
    return LaunchStateGcAge::kUnavailable;
  }
  if (timestamp > now) {
    return LaunchStateGcAge::kFresh;
  }
  return now - timestamp >= kLaunchStateGcMaxAge ? LaunchStateGcAge::kExpired
                                                 : LaunchStateGcAge::kFresh;
}

void SetLaunchStateGcTimeForTesting(std::optional<uint64_t> now) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  g_launch_state_gc_time_for_testing = now;
#endif
}

std::optional<uint64_t> GetCurrentBootTime() {
  FILETIME now_file_time = {};
  ::GetSystemTimePreciseAsFileTime(&now_file_time);
  const uint64_t now = FileTimeToTicks(now_file_time);
  constexpr uint64_t kFileTimeTicksPerMillisecond = 10000;
  const uint64_t uptime = ::GetTickCount64();
  if (uptime > now / kFileTimeTicksPerMillisecond) {
    return std::nullopt;
  }
  return now - uptime * kFileTimeTicksPerMillisecond;
}

LaunchStateAssessment AssessLaunchState(
    LaunchHealthMode mode,
    const LaunchState& state,
    std::optional<uint64_t> current_boot_time,
    std::optional<bool> process_alive) {
  LaunchStateAssessment result;
  result.consecutive_failures = state.consecutive_failures;
  if (mode == LaunchHealthMode::kOff) {
    return result;
  }
  if (!state.running) {
    result.status = state.confirmed && state.consecutive_failures == 0
                        ? LaunchStateStatus::kConfirmed
                        : LaunchStateStatus::kNeutral;
    return result;
  }
  if (!current_boot_time || state.pid_start_time < *current_boot_time ||
      !process_alive) {
    return result;
  }
  if (*process_alive) {
    result.status = LaunchStateStatus::kRunning;
    return result;
  }
  result.status = LaunchStateStatus::kFailure;
  result.consecutive_failures = state.consecutive_failures + 1;
  return result;
}

bool ApplyLaunchExit(LaunchHealthMode mode,
                     bool normal_exit,
                     bool neutral_exit,
                     LaunchState* state) {
  if (!state || mode == LaunchHealthMode::kOff || !state->running) {
    return false;
  }
  if (mode == LaunchHealthMode::kExplicit) {
    state->running = false;
    state->confirmed = false;
    return true;
  }
  if (normal_exit) {
    state->consecutive_failures = 0;
    state->running = false;
    state->confirmed = true;
    return true;
  }
  if (neutral_exit) {
    state->running = false;
    state->confirmed = false;
    return true;
  }
  return false;
}

AppLaunchHealthEvaluation EvaluateAppLaunchHealth(
    const base::FilePath& install_dir,
    const std::string& appid,
    LaunchHealthMode mode,
    int failure_threshold) {
  AppLaunchHealthEvaluation result;
  if (mode == LaunchHealthMode::kOff || install_dir.empty() || appid.empty()) {
    return result;
  }

  result.states = ScanLaunchStates(install_dir, GetAppidHash(appid));
  const std::optional<uint64_t> boot_time = GetCurrentBootTime();
  FILETIME now_file_time = {};
  ::GetSystemTimePreciseAsFileTime(&now_file_time);
  const uint64_t now = FileTimeToTicks(now_file_time);
  for (const auto& [version, state] : result.states) {
    std::optional<bool> process_alive;
    if (boot_time && state.pid_start_time >= *boot_time &&
        state.pid_start_time <= now) {
      process_alive = IsProcessAlive(state.pid, state.pid_start_time);
    }
    LaunchStateAssessment assessment =
        AssessLaunchState(mode, state, boot_time, process_alive);
    result.assessments.emplace(version, assessment);
    if (assessment.status == LaunchStateStatus::kFailure &&
        assessment.consecutive_failures >= failure_threshold) {
      result.disqualified_versions.insert(version);
    }
  }
  return result;
}

void SetActiveLaunchStatePath(const base::FilePath& path) {
  // Set-once contract: called during single-threaded startup before
  // RunWinMain spawns any threads. Never mutate after startup.
  ActiveLaunchStatePath() = path;
}

base::FilePath GetActiveLaunchStatePath() {
  return ActiveLaunchStatePath();
}

// ============================================================================
// Install-dir-level API (.launch/ directory)
// ============================================================================

base::FilePath GetLaunchStateDir(const base::FilePath& install_dir) {
  return install_dir.Append(kLaunchStateDirName);
}

base::FilePath GetInstallDirLaunchStatePath(const base::FilePath& install_dir,
                                            const std::wstring& appid_hash,
                                            const Version& version,
                                            const std::string& platform) {
  std::wstring filename = appid_hash + L"_" +
                          base::UTF8ToWide(version.ToString()) + L"_" +
                          base::UTF8ToWide(platform);
  return GetLaunchStateDir(install_dir).Append(filename);
}

base::FilePath GetInstallDirLivenessPath(const base::FilePath& install_dir,
                                         const std::wstring& appid_hash,
                                         const std::string& platform) {
  std::wstring filename = appid_hash + L"_" + base::UTF8ToWide(platform);
  return GetLaunchStateDir(install_dir).Append(filename);
}

std::optional<LivenessRecord> ReadLivenessPath(
    const base::FilePath& path,
    IntegrityMismatchAction mismatch_action) {
  std::optional<base::DictValue> parsed =
      ReadJsonWithIntegrity(path, mismatch_action);
  if (!parsed) {
    return std::nullopt;
  }
  return ParseLivenessDict(*parsed);
}

namespace {

bool WriteLivenessPathImpl(const base::FilePath& path,
                           const LivenessRecord& record) {
  if (record.appid.empty() || record.appid.size() > kMaxUuidLength ||
      record.platform.empty() || record.platform.size() > kMaxPlatformLength) {
    return false;
  }
  if (record.last_launch == 0) {
    return false;
  }
  base::FilePath dir = path.DirName();
  if (IsReparsePoint(dir) ||
      (!base::DirectoryExists(dir) &&
       (!VerifySafeDirectoryPath(dir) || !base::CreateDirectory(dir))) ||
      !VerifySafeFilePath(path)) {
    return false;
  }

  base::DictValue dict;
  dict.Set("appid", record.appid);
  dict.Set("platform", record.platform);
  dict.Set("last_launch", base::NumberToString(record.last_launch));
  std::string json;
  return base::JSONWriter::Write(dict, &json) &&
         WriteFileWithIntegrity(path, json);
}

}  // namespace

bool WriteLivenessPath(const base::FilePath& path,
                       const LivenessRecord& record) {
  return WriteLivenessPathImpl(path, record);
}

bool RefreshLivenessPath(const base::FilePath& path,
                         const std::string& appid,
                         const std::string& platform,
                         uint64_t now) {
  if (now == 0) {
    return false;
  }
  std::optional<LivenessRecord> current = ReadLivenessPath(path);
  if (current && current->appid == appid && current->platform == platform &&
      current->last_launch <= now &&
      now - current->last_launch < kLivenessRefreshIntervalFileTimeTicks) {
    return true;
  }
  return WriteLivenessPathImpl(path, LivenessRecord{appid, platform, now});
}

std::optional<LaunchState> ReadLaunchStatePath(
    const base::FilePath& path,
    IntegrityMismatchAction mismatch_action) {
  if (IsReparsePoint(path)) {
    return std::nullopt;
  }

  std::optional<base::DictValue> parsed =
      ReadJsonWithIntegrity(path, mismatch_action);
  if (!parsed) {
    return std::nullopt;
  }
  return ParseLaunchStateDict(*parsed);
}

std::optional<LaunchRecordSnapshot> ReadLaunchRecordSnapshot(
    const base::FilePath& path,
    const base::FilePath& expected_resolved_parent) {
  if (expected_resolved_parent.empty()) {
    return std::nullopt;
  }
  if (IsReparsePoint(path)) {
    LaunchRecordSnapshot snapshot;
    snapshot.path = path;
    snapshot.resolved_parent_path = expected_resolved_parent;
    snapshot.reparse_point = true;
    return snapshot;
  }
  std::string content;
  std::string raw_content;
  base::FilePath resolved_path;
  bool too_large = false;
  IntegrityResult integrity = ReadFileWithIntegrity(
      path, &content, IntegrityMismatchAction::kPreserve,
      kMaxLaunchStateFileSize, &resolved_path, &raw_content, &too_large);
  const bool bounded_raw_invalid =
      integrity == IntegrityResult::kReadError && !raw_content.empty();
  if ((!too_large && !bounded_raw_invalid &&
       integrity != IntegrityResult::kSuccess &&
       integrity != IntegrityResult::kSuccessNoFooter &&
       integrity != IntegrityResult::kIntegrityMismatch) ||
      resolved_path.empty() ||
      !base::FilePath::CompareEqualIgnoreCase(
          resolved_path.DirName().value(), expected_resolved_parent.value())) {
    return std::nullopt;
  }
  LaunchRecordSnapshot snapshot;
  snapshot.path = path;
  snapshot.resolved_parent_path = expected_resolved_parent;
  snapshot.content = std::move(content);
  snapshot.raw_content = std::move(raw_content);
  snapshot.oversized = too_large;
  if (too_large) {
    return snapshot;
  }
  if (integrity != IntegrityResult::kSuccess) {
    return snapshot;
  }
  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(snapshot.content, base::JSON_PARSE_RFC);
  if (!parsed) {
    return snapshot;
  }
  if (std::optional<LaunchState> health = ParseLaunchStateDict(*parsed)) {
    snapshot.kind = LaunchRecordKind::kHealth;
    snapshot.health = std::move(*health);
    return snapshot;
  }
  if (std::optional<LivenessRecord> liveness = ParseLivenessDict(*parsed)) {
    snapshot.kind = LaunchRecordKind::kLiveness;
    snapshot.liveness = std::move(*liveness);
    return snapshot;
  }
  return snapshot;
}

RetentionEvidenceSnapshot CollectRetentionEvidenceSnapshot(
    const base::FilePath& install_dir,
    const std::vector<AppEntry>& registrations) {
  RetentionEvidenceSnapshot snapshot;
  RetentionEvidenceMap& result = snapshot.evidence;
  for (const auto& app : registrations) {
    result[{app.uuid, app.platform}] = {};
  }

  base::FilePath launch_dir = GetLaunchStateDir(install_dir);
  if (!base::DirectoryExists(launch_dir)) {
    return snapshot;
  }
  if (IsReparsePoint(launch_dir)) {
    for (auto& [key, evidence] : result) {
      evidence.unknown = true;
      evidence.diagnostic = "unsafe_launch_directory";
    }
    return snapshot;
  }
  std::optional<base::FilePath> resolved_launch_dir =
      GetSafeDirectoryResolvedPath(launch_dir);
  if (!resolved_launch_dir) {
    for (auto& [key, evidence] : result) {
      evidence.unknown = true;
      evidence.diagnostic = "unsafe_launch_directory";
    }
    return snapshot;
  }

  std::set<base::FilePath> canonical_liveness_paths;
  auto observe_path = [&](const base::FilePath& path) {
    if (!base::PathExists(path)) {
      return;
    }
    const std::wstring filename = path.BaseName().value();
    const bool reparse = IsReparsePoint(path);
    std::string content;
    base::FilePath resolved_path;
    IntegrityResult integrity = IntegrityResult::kReadError;
    if (!reparse) {
      std::optional<int64_t> size = base::GetFileSize(path);
      if (size && *size <= kMaxLaunchStateFileSize) {
        integrity = ReadFileWithIntegrity(
            path, &content, IntegrityMismatchAction::kPreserve,
            kMaxLaunchStateFileSize, &resolved_path);
      }
    }
    const bool resolved_parent_matches =
        !resolved_path.empty() &&
        base::FilePath::CompareEqualIgnoreCase(resolved_path.DirName().value(),
                                               resolved_launch_dir->value());
    const bool readable =
        resolved_parent_matches && integrity == IntegrityResult::kSuccess;
    std::optional<base::DictValue> parsed =
        readable ? base::JSONReader::ReadDict(content, base::JSON_PARSE_RFC)
                 : std::nullopt;
    std::optional<LivenessRecord> liveness =
        parsed ? ParseLivenessDict(*parsed) : std::nullopt;
    std::optional<LaunchState> health =
        parsed ? ParseLaunchStateDict(*parsed) : std::nullopt;

    for (const auto& app : registrations) {
      const std::wstring hash = GetAppidHash(app.uuid);
      const std::wstring prefix = hash + L"_";
      const std::wstring platform = base::UTF8ToWide(app.platform);
      const base::FilePath canonical_liveness =
          GetInstallDirLivenessPath(install_dir, hash, app.platform);
      const bool potential =
          path == canonical_liveness ||
          (filename.starts_with(prefix) && filename.ends_with(L"_" + platform));
      const bool liveness_identity = liveness && liveness->appid == app.uuid &&
                                     liveness->platform == app.platform;
      const bool health_identity = health && health->appid == app.uuid &&
                                   health->platform == app.platform;
      if (!potential && !liveness_identity && !health_identity) {
        continue;
      }

      RetentionEvidence& evidence = result[{app.uuid, app.platform}];
      if (reparse) {
        evidence.unknown = true;
        evidence.diagnostic = "invalid_or_noncanonical_evidence";
        continue;
      }

      uint64_t timestamp = 0;
      RetentionEvidenceKind kind = RetentionEvidenceKind::kNone;
      bool canonical = false;
      if (liveness_identity) {
        timestamp = liveness->last_launch;
        kind = RetentionEvidenceKind::kLiveness;
        canonical = path == canonical_liveness;
      } else if (health_identity) {
        Version version = Version::Parse(health->version);
        if (version.IsValid()) {
          canonical = path == GetInstallDirLaunchStatePath(
                                  install_dir, hash, version, app.platform);
        }
        timestamp = health->pid_start_time;
        kind = RetentionEvidenceKind::kHealthSentinel;
      }

      if (!canonical || kind == RetentionEvidenceKind::kNone) {
        evidence.unknown = true;
        evidence.diagnostic = "invalid_or_noncanonical_evidence";
        continue;
      }
      if (readable) {
        snapshot.files.push_back({{app.uuid, app.platform},
                                  path,
                                  *resolved_launch_dir,
                                  content,
                                  integrity == IntegrityResult::kSuccess});
      }
      if (timestamp > evidence.timestamp) {
        evidence.timestamp = timestamp;
        evidence.kind = kind;
      }
    }
  };

  // Read each registration's canonical liveness path directly. This read is
  // that registration's launch-intent ordering point; a later directory scan
  // must not reinterpret a replacement as pre-cutoff evidence.
  for (const auto& app : registrations) {
    base::FilePath path = GetInstallDirLivenessPath(
        install_dir, GetAppidHash(app.uuid), app.platform);
    canonical_liveness_paths.insert(path);
    observe_path(path);
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
    if (*g_retention_canonical_observation_hook) {
      g_retention_canonical_observation_hook->Run(path);
    }
#endif
  }

  base::FileEnumerator enumerator(launch_dir, /*recursive=*/false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (!canonical_liveness_paths.contains(path)) {
      observe_path(path);
    }
  }
  std::sort(snapshot.files.begin(), snapshot.files.end(),
            [](const auto& a, const auto& b) {
              return std::tie(a.key, a.path) < std::tie(b.key, b.path);
            });
  return snapshot;
}

void SetRetentionCanonicalObservationHookForTesting(
    RetentionCanonicalObservationHookForTesting callback) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  *g_retention_canonical_observation_hook = std::move(callback);
#endif
}

RetentionEvidenceMap CollectRetentionEvidence(
    const base::FilePath& install_dir,
    const std::vector<AppEntry>& registrations) {
  return CollectRetentionEvidenceSnapshot(install_dir, registrations).evidence;
}

std::set<VersionKey> CollectConfirmedVersionProtection(
    const base::FilePath& install_dir,
    const std::vector<AppEntry>& registrations) {
  std::set<VersionKey> result;
  std::set<RetentionRegistrationKey> registered;
  for (const auto& app : registrations) {
    registered.insert({app.uuid, app.platform});
  }
  base::FilePath launch_dir = GetLaunchStateDir(install_dir);
  if (!base::DirectoryExists(launch_dir) || IsReparsePoint(launch_dir)) {
    return result;
  }
  base::FileEnumerator enumerator(launch_dir, /*recursive=*/false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (IsReparsePoint(path)) {
      continue;
    }
    std::optional<LaunchState> health =
        ReadLaunchStatePath(path, IntegrityMismatchAction::kPreserve);
    if (!health || health->running || !health->confirmed ||
        health->consecutive_failures != 0 ||
        !registered.contains({health->appid, health->platform})) {
      continue;
    }
    Version version = Version::Parse(health->version);
    if (!version.IsValid() ||
        path != GetInstallDirLaunchStatePath(install_dir,
                                             GetAppidHash(health->appid),
                                             version, health->platform)) {
      continue;
    }
    result.insert({version, health->platform});
  }
  return result;
}

namespace {

bool WriteLaunchStatePathImpl(const base::FilePath& path,
                              const LaunchState& state) {
  const uint64_t last_update = GetLaunchStateGcCurrentTime();
  if (last_update == 0) {
    return false;
  }
  base::FilePath dir = path.DirName();
  if (IsReparsePoint(dir)) {
    return false;
  }
  if (!base::DirectoryExists(dir)) {
    if (!VerifySafeDirectoryPath(dir)) {
      return false;
    }
    if (!base::CreateDirectory(dir)) {
      return false;
    }
  }

  if (!VerifySafeFilePath(path)) {
    return false;
  }

  if (state.appid.empty() || state.version.empty() || state.platform.empty()) {
    return false;
  }

  base::DictValue dict;
  dict.Set("appid", state.appid);
  dict.Set("pid", static_cast<int>(state.pid));
  dict.Set("pid_start_time", base::NumberToString(state.pid_start_time));
  dict.Set("consecutive_failures", state.consecutive_failures);
  dict.Set("running", state.running);
  dict.Set("confirmed", state.confirmed);
  dict.Set("version", state.version);
  dict.Set("platform", state.platform);
  dict.Set("last_update", base::NumberToString(last_update));

  std::string json;
  if (!base::JSONWriter::Write(dict, &json)) {
    return false;
  }

  return WriteFileWithIntegrity(path, json);
}

}  // namespace

bool WriteLaunchStatePath(const base::FilePath& path,
                          const LaunchState& state) {
  return WriteLaunchStatePathImpl(path, state);
}

std::map<Version, LaunchState> ScanLaunchStates(
    const base::FilePath& install_dir,
    const std::wstring& appid_hash) {
  std::map<Version, LaunchState> result;

  base::FilePath launch_dir = GetLaunchStateDir(install_dir);
  if (!base::DirectoryExists(launch_dir)) {
    return result;
  }
  if (IsReparsePoint(launch_dir)) {
    return result;
  }

  std::string current_platform = GetCurrentPlatform();

  base::FileEnumerator enumerator(launch_dir, /*recursive=*/false,
                                  base::FileEnumerator::FILES,
                                  appid_hash + L"_*");

  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    auto ls = ReadLaunchStatePath(path);
    if (!ls) {
      continue;
    }
    if (ls->platform != current_platform) {
      continue;
    }
    Version ver = Version::Parse(ls->version);
    if (!ver.IsValid()) {
      continue;
    }
    result.emplace(ver, *ls);
  }

  return result;
}

}  // namespace cef_installer
