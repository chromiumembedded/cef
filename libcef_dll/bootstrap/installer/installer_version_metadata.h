// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_VERSION_METADATA_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_VERSION_METADATA_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"

namespace cef_installer {

// Metadata stored in <version_dir>/cef_version.json
// This file is created by the server-side build and included in the signed
// catalog. The installer reads it after extraction to verify the archive
// contents match the CDN manifest.
//
// Required fields: version, abi_hash, platform
// Optional fields: version_full
//
// The platform field is validated against GetCurrentPlatform() during
// bundled CEF installation to prevent architecture mismatches (e.g.,
// installing a 32-bit CEF build on a 64-bit system).
struct VersionMetadata {
  // CEF version (e.g., "137.3.5") [required]
  Version version;
  // Sandbox compatibility hash (16-char hex) [required]
  std::string abi_hash;
  // Target platform (e.g., "windows64") [required]
  std::string platform;
  // Full version string from archive filename [optional]
  std::string version_full;

  // Returns true if all required fields are present and valid.
  bool IsValid() const;
};

// Error codes for metadata operations
enum class MetadataError {
  kSuccess,
  kFileNotFound,          // Metadata file doesn't exist
  kFileReadError,         // Could not read file
  kJsonParseError,        // Invalid JSON format
  kMissingRequiredField,  // Required field not present
  kFileWriteError,        // Could not write file
  kIndexValidationError,  // Index content/reread did not match canonical set
  kIntegrityMismatch,     // Integrity footer present with a bad CRC32
};

enum class VersionIndexReadMode {
  kDoomCorrupt,
  kPreserveCorrupt,
};

enum class VersionIndexFault {
  kNone,
  kWrite,
  kReplace,
  kReread,
  kValidation,
};

// Deterministic fault injection for checked index publication tests.
void SetVersionIndexFaultForTesting(VersionIndexFault fault);

// Read version metadata from a version directory.
// Looks for: <version_dir>/cef_version.json
MetadataError ReadVersionMetadata(const base::FilePath& version_dir,
                                  VersionMetadata* metadata);

// Write version metadata to a version directory.
// Creates: <version_dir>/cef_version.json
// NOTE: Only used by tests. Production archives include cef_version.json
// from the server-side build (signed in the catalog).
MetadataError WriteVersionMetadata(const base::FilePath& version_dir,
                                   const VersionMetadata& metadata);

// Information about an installed version including its metadata.
struct InstalledVersion {
  VersionMetadata metadata;
  base::FilePath path;
};

enum class DistributionValidation {
  kComplete,
  kInvalid,
  kUnsafe,
  kIoError,
};

// Validate one published distribution without scanning or repairing any other
// version. The expected version/platform/ABI must match, required metadata,
// catalog and libcef files must exist, and the load path must be free of
// reparse points below |trusted_root|.
DistributionValidation ValidateDistribution(const base::FilePath& trusted_root,
                                            const base::FilePath& version_dir,
                                            const VersionMetadata& expected,
                                            base::TimeTicks deadline = {},
                                            bool* time_limit_reached = nullptr);

// Scan installed versions with their metadata across ALL platforms.
// Returns versions with platform and ABI hash information for resolution.
// Scans all platform subdirectories (windows64, windows32, windowsarm64).
// Versions are sorted newest-first within each platform.
std::vector<InstalledVersion> ScanInstalledVersionsWithMetadata(
    const base::FilePath& install_dir);

// Result from the bounded, read-only variant used only for automatic-startup
// emergency recovery. |entries_visited| counts enumerated version
// directories. The time limit is soft: it is checked between synchronous
// filesystem operations, which cannot themselves be interrupted.
struct BoundedInstalledVersionScanResult {
  std::vector<InstalledVersion> versions;
  size_t entries_visited = 0;
  bool entry_limit_reached = false;
  bool time_limit_reached = false;
};

BoundedInstalledVersionScanResult ScanInstalledVersionsWithMetadataBounded(
    const base::FilePath& install_dir,
    size_t max_version_entries,
    base::TimeTicks deadline);

// Write a version index file with the given version list.
// The index is written atomically (write temp + rename) with a CRC32
// integrity footer so readers never see partial or corrupt content.
// Creates: <install_dir>/versions.json
MetadataError WriteVersionIndex(const base::FilePath& install_dir,
                                const std::vector<InstalledVersion>& versions);

// Read installed versions from the index file.
// Readers use this instead of scanning the directory structure.
// Returns kSuccess with the version list (possibly empty if no versions
// are installed), or an error if the index file is missing or corrupt.
// Lock-free readers use kPreserveCorrupt so integrity failures do not mutate
// the shared store; writer-locked recovery may use the default doom behavior.
MetadataError ReadVersionIndex(
    const base::FilePath& install_dir,
    std::vector<InstalledVersion>* versions,
    VersionIndexReadMode read_mode = VersionIndexReadMode::kDoomCorrupt);

// Extract the full version string from a CEF archive filename.
// Handles "cef_binary_<version>_..." patterns.
// Returns empty string if the filename doesn't match a known pattern.
std::string ExtractVersionFullFromFilename(const std::string& filename);

// Convert error code to human-readable string for logging
const char* MetadataErrorToString(MetadataError error);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_VERSION_METADATA_H_
