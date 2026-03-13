// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_VERSION_RESOLVER_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_VERSION_RESOLVER_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "cef/libcef_dll/bootstrap/installer/installer_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_database.h"
#include "cef/libcef_dll/bootstrap/installer/installer_revocation.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version_metadata.h"

namespace cef_installer {

// Key for identifying a unique installed version (version + platform).
struct VersionKey {
  Version version;
  std::string platform;

  bool operator<(const VersionKey& other) const {
    if (version != other.version) {
      return version < other.version;
    }
    return platform < other.platform;
  }

  bool operator==(const VersionKey& other) const {
    return version == other.version && platform == other.platform;
  }
};

enum class CandidateSource {
  kNone,
  kInstalled,
  kBundled,
};

enum class CandidateRejection {
  kInvalidMetadata,
  kWrongPlatform,
  kAbiMismatch,
  kOutOfRange,
  kRevoked,
  kDisqualified,
  kDuplicate,
};

struct RejectedCandidate {
  InstalledVersion candidate;
  CandidateSource source = CandidateSource::kNone;
  CandidateRejection reason = CandidateRejection::kInvalidMetadata;
};

// Result of side-effect-free offline selection. |preferred| is safe for normal
// use. |last_resort| is considered only after installation/network resolution
// fails (query, which cannot install, may use it immediately). A revoked
// bundled candidate has precedence over a disqualified candidate as the last
// resort. The selector performs no I/O, locking, mutation, or cleanup.
struct OfflineSelectionResult {
  std::optional<InstalledVersion> preferred;
  CandidateSource preferred_source = CandidateSource::kNone;
  std::optional<InstalledVersion> last_resort;
  CandidateSource last_resort_source = CandidateSource::kNone;
  bool last_resort_is_revoked_bundled = false;
  bool preferred_is_rollback = false;
  std::vector<RejectedCandidate> rejected;
};

// Select from ordered installed candidates and an optional bundled candidate.
// Duplicate installed (version, platform) keys keep the first entry, matching
// readable-directory priority after rejected invalid entries are removed.
// Compatibility and revocation constraints apply to both sources. Optional
// launch-health disqualification keys apply only to installed candidates;
// bundled content remains the app-shipped fallback, matching legacy behavior.
OfflineSelectionResult SelectOfflineCandidate(
    const Config& config,
    const std::string& platform,
    const std::vector<InstalledVersion>& installed,
    const std::optional<InstalledVersion>& bundled,
    const std::vector<RevokedVersionRange>& revoked_versions = {},
    const std::set<VersionKey>& disqualified_versions = {});

// Build a bounded diagnostic when SelectOfflineCandidate() finds no usable
// installed or bundled version.
std::string BuildNoMatchingInstalledVersionMessage(
    const Config& config,
    const std::string& platform,
    const std::vector<RejectedCandidate>& rejected);

// Find the best matching version for a single app configuration.
// Algorithm:
// 1. Filter available versions by abi_hash (must match if specified)
// 2. Filter by version range [vmin, vmax]
// 3. Filter out revoked versions
// 4. Select the newest matching version
//
// Parameters:
// - config: App configuration with vmin, vmax, abi_hash
// - available_versions: Versions currently installed on disk (with metadata)
// - revoked_versions: Versions to exclude from consideration
//
// Returns: Best matching version, or nullopt if no match found
std::optional<InstalledVersion> FindBestVersion(
    const Config& config,
    const std::vector<InstalledVersion>& available_versions,
    const std::vector<RevokedVersionRange>& revoked_versions = {});

// Overload that works with AppEntry from database.
std::optional<InstalledVersion> FindBestVersion(
    const AppEntry& entry,
    const std::vector<InstalledVersion>& available_versions,
    const std::vector<RevokedVersionRange>& revoked_versions = {});

// Get the set of unique (version, platform) pairs required by at least one
// registered app. For each app, finds the best matching version from
// available_versions that matches the app's platform. The result is the set
// of versions that should NOT be pruned.
std::set<VersionKey> GetRequiredVersionSet(
    const Database& database,
    const std::vector<InstalledVersion>& available_versions,
    const std::vector<RevokedVersionRange>& revoked_versions = {});

// Identify versions that can be safely pruned (not needed by any app).
//
// Parameters:
// - installed_versions: All versions currently on disk (all platforms)
// - required_versions: (version, platform) pairs needed by registered apps
// - revoked_versions: Versions that should be removed even if "required"
//
// Returns: Installed versions safe to delete (includes platform info)
std::vector<InstalledVersion> GetPrunableVersions(
    const std::vector<InstalledVersion>& installed_versions,
    const std::set<VersionKey>& required_versions,
    const std::vector<RevokedVersionRange>& revoked_versions = {},
    const std::set<VersionKey>& confirmed_protected_versions = {});

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_VERSION_RESOLVER_H_
