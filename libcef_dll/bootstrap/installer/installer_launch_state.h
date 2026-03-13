// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LAUNCH_STATE_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LAUNCH_STATE_H_

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "cef/libcef_dll/bootstrap/installer/installer_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_retention.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"

namespace cef_installer {

struct LaunchState {
  std::string appid;
  uint32_t pid = 0;
  uint64_t pid_start_time = 0;
  int consecutive_failures = 0;
  bool running = false;
  // True only after explicit confirmation or a successful exit-code-mode
  // launch.
  bool confirmed = false;
  std::string version;
  std::string platform;
  // Writer-owned publication time used only for launch-record garbage
  // collection. Callers cannot preserve or choose this value.
  uint64_t last_update = 0;
};

struct LivenessRecord {
  std::string appid;
  std::string platform;
  uint64_t last_launch = 0;
};

enum class LaunchRecordKind {
  kInvalid,
  kHealth,
  kLiveness,
};

// One bounded, no-follow observation of a launch record. Valid records expose
// their integrity-verified payload; all conclusive observations expose the
// exact raw publication for race-safe repair.
struct LaunchRecordSnapshot {
  LaunchRecordKind kind = LaunchRecordKind::kInvalid;
  LaunchState health;
  LivenessRecord liveness;
  base::FilePath path;
  base::FilePath resolved_parent_path;
  std::string content;
  std::string raw_content;
  bool oversized = false;
  bool reparse_point = false;
};

enum class LaunchStateGcAge {
  kInvalid,
  kUnavailable,
  kFresh,
  kExpired,
};

// Exact canonical evidence file observed while building a retention plan.
// |content| is the integrity-verified payload without the integrity footer.
// |resolved_parent_path| binds later compare-before-delete cleanup to the
// observed directory location after logical publication.
struct RetentionEvidenceFileSnapshot {
  RetentionRegistrationKey key;
  base::FilePath path;
  base::FilePath resolved_parent_path;
  std::string content;
  bool integrity_protected = false;
};

struct RetentionEvidenceSnapshot {
  RetentionEvidenceMap evidence;
  std::vector<RetentionEvidenceFileSnapshot> files;
};

enum class LaunchStateStatus {
  kConfirmed,
  kNeutral,
  kRunning,
  kFailure,
  kIndeterminate,
};

struct LaunchStateAssessment {
  LaunchStateStatus status = LaunchStateStatus::kIndeterminate;
  int consecutive_failures = 0;
};

// Selection-policy-free evaluation for one app and the current platform.
// Off mode returns an empty result without enumerating .launch/. Callers pass
// only |disqualified_versions| to the shared selector and may use |states| and
// |assessments| to seed the selected version's next sentinel.
struct AppLaunchHealthEvaluation {
  std::map<Version, LaunchState> states;
  std::map<Version, LaunchStateAssessment> assessments;
  std::set<Version> disqualified_versions;
};

// Estimate the current Windows boot boundary in FILETIME ticks using the
// system wall clock minus GetTickCount64(). Returns nullopt if the conversion
// is internally inconsistent. Clock changes can make the estimate conservative
// (indeterminate) but never turn a known pre-boot record into a confirmed run.
std::optional<uint64_t> GetCurrentBootTime();

// Pure classification for an existing record. |process_alive| is consulted
// only for a running record from the current boot. A missing boot boundary is
// indeterminate, and a start time before the boundary never counts as failure.
LaunchStateAssessment AssessLaunchState(
    LaunchHealthMode mode,
    const LaunchState& state,
    std::optional<uint64_t> current_boot_time,
    std::optional<bool> process_alive);

// Apply a completed RunWinMain return to |state|. Returns true when the state
// should be persisted. Explicit mode treats every ordinary return as neutral;
// exit-code mode confirms zero, preserves neutral codes, and leaves abnormal
// exits running for failure classification on the next launch.
bool ApplyLaunchExit(LaunchHealthMode mode,
                     bool normal_exit,
                     bool neutral_exit,
                     LaunchState* state);

AppLaunchHealthEvaluation EvaluateAppLaunchHealth(
    const base::FilePath& install_dir,
    const std::string& appid,
    LaunchHealthMode mode,
    int failure_threshold);

// Key for identifying a confirmed version (version + platform).
// Defined here to avoid depending on VersionKey from
// installer_version_resolver.
struct ConfirmedVersionKey {
  Version version;
  std::string platform;

  bool operator<(const ConfirmedVersionKey& other) const {
    if (version != other.version) {
      return version < other.version;
    }
    return platform < other.platform;
  }

  bool operator==(const ConfirmedVersionKey& other) const {
    return version == other.version && platform == other.platform;
  }
};

// Compute the appid hash: first 16 hex chars of SHA-1(appid).
std::wstring GetAppidHash(const std::string& appid);

// Check if a process with the given PID and creation time is still alive.
// Uses OpenProcess + GetProcessTimes to guard against PID reuse.
bool IsProcessAlive(uint32_t pid, uint64_t pid_start_time);

// Return the creation time of the current process as a uint64 FILETIME.
uint64_t GetCurrentPidStartTime();
uint64_t GetCurrentWallTime();

// Classify a nonzero CRC-verified content timestamp against the dedicated
// launch-GC clock. Future timestamps are fresh and unavailable time preserves
// the record.
LaunchStateGcAge ClassifyLaunchStateGcAge(uint64_t timestamp);

// Process-global test seam shared only by launch publication and launch GC.
// Passing nullopt restores the production wall clock. Tests must reset it.
void SetLaunchStateGcTimeForTesting(std::optional<uint64_t> now);

// Returns install_dir/.launch/.
base::FilePath GetLaunchStateDir(const base::FilePath& install_dir);

// Returns install_dir/.launch/<appid_hash>_<version>_<platform>.
base::FilePath GetInstallDirLaunchStatePath(const base::FilePath& install_dir,
                                            const std::wstring& appid_hash,
                                            const Version& version,
                                            const std::string& platform);

// Returns install_dir/.launch/<appid_hash>_<platform>. The version-less
// two-segment shape cannot collide with a health sentinel.
base::FilePath GetInstallDirLivenessPath(const base::FilePath& install_dir,
                                         const std::wstring& appid_hash,
                                         const std::string& platform);

// Read a current-schema liveness record with a valid integrity footer and a
// nonzero last_launch. The default preserves integrity mismatches.
std::optional<LivenessRecord> ReadLivenessPath(
    const base::FilePath& path,
    IntegrityMismatchAction mismatch_action =
        IntegrityMismatchAction::kPreserve);
// Write an integrity-protected liveness record. Zero last_launch is rejected.
bool WriteLivenessPath(const base::FilePath& path,
                       const LivenessRecord& record);

// Refresh only when missing/malformed or at least 30 days old. Future content
// timestamps are treated as fresh so wall-clock rollback does not cause a
// write on every launch.
bool RefreshLivenessPath(const base::FilePath& path,
                         const std::string& appid,
                         const std::string& platform,
                         uint64_t now);

// Read a current-schema launch state from a full path (install-dir-level
// format). A valid integrity footer and nonzero last_update are required.
// Includes version and platform fields from the JSON body. The default
// preserves an integrity mismatch for read-only callers.
std::optional<LaunchState> ReadLaunchStatePath(
    const base::FilePath& path,
    IntegrityMismatchAction mismatch_action =
        IntegrityMismatchAction::kPreserve);

// Observe exactly one bounded publication. Returns kInvalid for conclusively
// corrupt, footerless, schema-invalid, or final-component reparse publication
// so a writer can conditionally repair that exact publication. I/O and
// path-identity uncertainty returns nullopt. The resolved parent must match
// |expected_resolved_parent|.
std::optional<LaunchRecordSnapshot> ReadLaunchRecordSnapshot(
    const base::FilePath& path,
    const base::FilePath& expected_resolved_parent);

// Collect the newest canonical health-sentinel pid_start_time or liveness-only
// last_launch for each durable registration. This is a strictly read-only
// retention snapshot: it never repairs, deletes, creates, or rewrites files.
// A malformed, reparse, mismatched, or noncanonical file that could belong to
// a registration marks that registration's evidence unknown/protected.
RetentionEvidenceMap CollectRetentionEvidence(
    const base::FilePath& install_dir,
    const std::vector<AppEntry>& registrations);

// Collect evidence plus the exact canonical file payloads that produced it.
// This is side-effect-free and is used by apply for final validation and
// compare-before-delete cleanup.
RetentionEvidenceSnapshot CollectRetentionEvidenceSnapshot(
    const base::FilePath& install_dir,
    const std::vector<AppEntry>& registrations);

using RetentionCanonicalObservationHookForTesting =
    base::RepeatingCallback<void(const base::FilePath&)>;
void SetRetentionCanonicalObservationHookForTesting(
    RetentionCanonicalObservationHookForTesting callback);

// Side-effect-free collection of canonical, confirmed health sentinels for
// registrations that remain in the supplied durable database snapshot.
std::set<VersionKey> CollectConfirmedVersionProtection(
    const base::FilePath& install_dir,
    const std::vector<AppEntry>& registrations);

// Write a launch state file to a full path (install-dir-level format).
// Creates the .launch/ directory if it doesn't exist and replaces any
// caller-supplied last_update with the current launch-GC clock.
bool WriteLaunchStatePath(const base::FilePath& path, const LaunchState& state);

// Process-global active launch state path.
//
// Set once by the bootstrap before RunWinMain (during single-threaded
// startup), and read thereafter by HandleLaunchSuccess (e.g. on a background
// task) to resolve the sentinel file for the current launch. No lock is
// needed: the set-once-before-threads contract gives the happens-before edge.

// Set the active launch state path. Called exactly once by the bootstrap
// before RunWinMain spawns any threads.
void SetActiveLaunchStatePath(const base::FilePath& path);

// Return the active launch state path, or an empty path if never set (e.g.
// not running under the bootstrap, or a bundled/unchecked path that does not
// participate in launch health).
base::FilePath GetActiveLaunchStatePath();

// Scan .launch/<appid_hash>_* files for a specific appid.
// Filters to the current platform. Returns map keyed by version.
// Returns empty map if .launch/ doesn't exist.
std::map<Version, LaunchState> ScanLaunchStates(
    const base::FilePath& install_dir,
    const std::wstring& appid_hash);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_LAUNCH_STATE_H_
