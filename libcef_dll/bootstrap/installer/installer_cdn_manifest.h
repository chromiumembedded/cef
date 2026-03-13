// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CDN_MANIFEST_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CDN_MANIFEST_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"

namespace cef_installer {

// Represents a single CEF build entry from CDN manifest.
struct CdnBuildEntry {
  Version version;            // Short version (e.g., "137.3.5")
  std::string file;           // Signed archive filename
  std::string sha1;           // SHA1 hash of the signed archive
  std::string last_modified;  // ISO 8601 timestamp
  std::string abi_hash;       // Sandbox compatibility hash (may be empty)
};

enum class CdnBuildExclusionReason {
  kAlreadyInstalled,
  kRevoked,
  kLaunchDisqualified,
  kPriorDownloadFailure,
};

using CdnBuildExclusionReasons =
    std::map<Version, std::set<CdnBuildExclusionReason>>;

// Error codes for CDN manifest operations.
enum class ManifestError {
  kSuccess,
  kJsonParseError,        // Malformed JSON
  kMissingRequiredField,  // Required field not present
  kInvalidFieldValue,     // Field present but invalid format
};

// Parse stable.txt content (just a milestone number).
// Content example: "137"
ManifestError ParseStableMilestone(const std::string& content, int* milestone);

// Parse {milestone}.json - newest build per platform.
// Returns map of platform -> build entry.
// Example platforms: "windows64", "windows32", "windowsarm64", "linux64"
ManifestError ParseMilestoneManifest(
    const std::string& json,
    std::map<std::string, CdnBuildEntry>* entries);

// Parse {milestone}_{platform}.json or {abi_hash}_{platform}.json.
// Returns list of builds sorted newest-first.
ManifestError ParsePlatformManifest(const std::string& json,
                                    std::vector<CdnBuildEntry>* entries);

// Build CDN URLs for various query files.
// Base URL example: "https://cef-builds.spotifycdn.com/"
//
// Channel parameter: "" (stable, default) or "beta"
// - stable (empty): stable.txt, 137.json, 137_windows64.json
// - beta: beta.txt, 137_beta.json, 137_windows64_beta.json

// Build URL for channel milestone file (stable.txt or beta.txt)
std::string BuildChannelUrl(const std::string& base_url,
                            const std::string& channel);

// Legacy: equivalent to BuildChannelUrl(base_url, "")
std::string BuildStableUrl(const std::string& base_url);

// Build URL for milestone manifest with optional channel suffix
std::string BuildMilestoneUrl(const std::string& base_url,
                              int milestone,
                              const std::string& channel = "");

// Build URL for platform manifest with optional channel suffix
std::string BuildPlatformUrl(const std::string& base_url,
                             int milestone,
                             const std::string& platform,
                             const std::string& channel = "");

// Build URL for ABI hash manifest with optional channel suffix
std::string BuildAbiHashUrl(const std::string& base_url,
                            const std::string& abi_hash,
                            const std::string& platform,
                            const std::string& channel = "");

std::string BuildArchiveUrl(const std::string& base_url,
                            const std::string& filename);
std::string BuildHashFileUrl(const std::string& base_url,
                             const std::string& filename);  // .sha256 file

// Find the best build entry for an app's requirements.
// Filters by abi_hash (if specified) and version range, returns newest match.
// Entries whose version appears in |skip_versions| are excluded.
std::optional<CdnBuildEntry> FindBestBuildEntry(
    const std::vector<CdnBuildEntry>& entries,
    const std::string& vmin,
    const std::string& vmax,
    const std::string& abi_hash,
    const std::set<Version>& skip_versions = {});

// Build a bounded diagnostic for a failed FindBestBuildEntry() call. Reports
// the effective requirements and the checks that rejected each candidate.
std::string BuildNoMatchingCdnVersionMessage(
    const std::vector<CdnBuildEntry>& entries,
    const std::string& platform,
    const std::string& vmin,
    const std::string& vmax,
    const std::string& abi_hash,
    const CdnBuildExclusionReasons& exclusion_reasons = {});

// Build URL for revocation list.
// Returns: <base_url>/revoked.json
std::string BuildRevocationListUrl(const std::string& base_url);

// Convert error code to human-readable string for logging.
const char* ManifestErrorToString(ManifestError error);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CDN_MANIFEST_H_
