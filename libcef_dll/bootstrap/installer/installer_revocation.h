// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_REVOCATION_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_REVOCATION_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"

namespace cef_installer {

// Represents a range of revoked versions [version_min, version_max].
// A single-version revocation is represented as version_min == version_max.
//
// Both bounds are inclusive. Missing version components are treated as 0
// during comparison (e.g., "137.4" == "137.4.0"), so to revoke all of
// milestone 137.3 without including 137.4.0, use version_max "137.3.99"
// rather than "137.4".
struct RevokedVersionRange {
  Version version_min;     // Inclusive lower bound (e.g., "137" or "137.1.0")
  Version version_max;     // Inclusive upper bound (e.g., "137.3.99")
  std::string reason;      // E.g., "CVE-2024-XXXXX"
  std::string revoked_at;  // ISO 8601 timestamp
};

// Error codes for revocation operations.
enum class RevocationError {
  kSuccess,
  kJsonParseError,
  kWriteError,  // Failed to write cache file to disk
};

// Parse revocation list from JSON string.
// Supports both single-version and range entries:
// {
//   "revoked_versions": [
//     {"version": "137.1.0", "reason": "CVE-...", "revoked_at": "..."},
//     {"version_min": "137", "version_max": "137.3.99", "reason": "..."}
//   ]
// }
RevocationError ParseRevocationList(const std::string& json,
                                    std::vector<RevokedVersionRange>* revoked);

// Check if a specific version falls within any revoked range.
bool IsVersionRevoked(const Version& version,
                      const std::vector<RevokedVersionRange>& revoked_list);

// Filter a list of versions to exclude revoked ones.
std::vector<Version> FilterRevokedVersions(
    const std::vector<Version>& versions,
    const std::vector<RevokedVersionRange>& revoked_list);

// Convert error code to human-readable string for logging.
const char* RevocationErrorToString(RevocationError error);

// Load the compiled-in revocation list from the bootstrap's resources.
// Returns the baseline list. Returns empty if the resource is missing
// (e.g., in test binaries without the resource).
std::vector<RevokedVersionRange> LoadCompiledRevocationList();

// Build the effective revocation list by merging the compiled baseline
// with additional ranges (from CDN fetch or disk cache).
// Sorts by version_min, merges overlapping/contiguous ranges, and produces
// the minimum set of non-overlapping ranges. Reasons are concatenated.
// The compiled list can never shrink — additional entries can only add.
std::vector<RevokedVersionRange> MergeRevocationLists(
    const std::vector<RevokedVersionRange>& compiled,
    const std::vector<RevokedVersionRange>& additional);

// Write CDN revocation list to disk cache.
// File: <dir>/revocation_cache.json
// The full CDN list is written so that any bootstrap version (which may
// have a different compiled-in baseline) can merge it correctly at load time.
RevocationError WriteRevocationCache(
    const base::FilePath& dir,
    const std::vector<RevokedVersionRange>& cdn_fetched);

// Load CDN revocation list from disk cache. Returns empty list on any error.
// Reads <dir>/revocation_cache.json.
std::vector<RevokedVersionRange> LoadRevocationCache(
    const base::FilePath& dir,
    IntegrityMismatchAction mismatch_action = IntegrityMismatchAction::kDoom);

// Load compiled baseline + disk caches from all given directories,
// returning the merged effective revocation list.
// This is the standard way to get the revocation list for offline code paths
// (query, fallback).
std::vector<RevokedVersionRange> LoadEffectiveRevocationList(
    const std::vector<base::FilePath>& read_dirs,
    IntegrityMismatchAction mismatch_action = IntegrityMismatchAction::kDoom);

// Returns true only when the cache is recent, integrity-protected, and parses
// successfully. A malformed cache must not suppress a network refresh.
bool IsRevocationCacheFresh(const base::FilePath& dir, base::Time now);

// Integrity-protected, source-scoped failure backoff. Future timestamps beyond
// one backoff window are treated as malformed clock skew and do not suppress.
bool IsRevocationRefreshBackedOff(const base::FilePath& dir,
                                  const std::string& source,
                                  base::Time now);
bool RecordRevocationRefreshFailure(const base::FilePath& dir,
                                    const std::string& source,
                                    base::Time now);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_REVOCATION_H_
