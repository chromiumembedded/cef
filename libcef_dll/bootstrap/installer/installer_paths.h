// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_PATHS_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_PATHS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"

namespace cef_installer {

// Error codes for path operations
enum class PathError {
  kSuccess,
  kNotFound,      // No valid install directory found
  kAccessDenied,  // Directory exists but not writable
  kInvalidPath,   // Path exists but is not a directory
};

// Source-derived role of an install directory. Roles never depend on path
// spelling, ACLs, or current writability.
enum class DirectoryRole {
  kHklmDefault,
  kProgramFilesDefault,
  kPerUserDefault,
  kCustom,
};

// Operation-specific controls for directory resolution.
struct DirectoryResolutionContext {
  // Query/read-only discovery does not create directories or write probes.
  bool mutation_capable = true;

  // Whether this operation may mutate HKLM/Program Files default stores.
  bool allow_admin_mutation = true;

  // Elevated operations never traverse into the lower-integrity per-user
  // default store. Injectable by callers for deterministic tests.
  bool is_elevated = false;

  // Enterprise policy can exclude the standard shared LocalAppData store
  // from both readable and writable candidates.
  bool allow_shared_user_store = true;

  // A mutation-enabled re-resolution may only probe a candidate after all of
  // these previously validated readable directories would remain readable.
  // This prevents a failed revalidation from creating or write-probing a
  // higher-priority directory as a side effect.
  std::vector<base::FilePath> required_readable_dirs;
};

// Result of install directory resolution.
struct InstallDirectories {
  // Error code for write access. kSuccess if writable_dir is valid.
  PathError write_error = PathError::kNotFound;

  // First writable directory found. Empty if write_error != kSuccess.
  base::FilePath writable_dir;

  // Source role of writable_dir. Empty when no writable directory exists.
  std::optional<DirectoryRole> writable_role;

  // Readable directories, in priority order, up to and including the
  // writable directory. Never contains directories with lower priority
  // than the writable directory.
  std::vector<base::FilePath> readable_dirs;

  // Source roles corresponding one-to-one with readable_dirs.
  std::vector<DirectoryRole> readable_roles;

  // True when a write probe was refused because it would have truncated the
  // required readable set. No probe or directory creation has occurred.
  bool mutation_blocked_by_required_readable_dirs = false;
};

// Resolve install directories using a priority-ordered search that collects
// readable directories and stops at the first writable one.
//
// If |custom_path| is non-empty (install_path from config), it is used
// exclusively. Safe existing read-only directories remain readable; invalid,
// unsafe, inaccessible, or missing read-only paths fail without fallback.
//
// Otherwise, searches in priority order:
// 1. HKLM\SOFTWARE\CEF\InstallLocation (requires admin to set)
// 2. %ProgramFiles%\CEF (e.g., "C:\Program Files\CEF")
// 3. %LocalAppData%\CEF (e.g., "C:\Users\<user>\AppData\Local\CEF")
//
// NOTE: HKCU\Software\CEF\InstallLocation is intentionally NOT supported
// for security reasons (unprivileged modification).
//
// For each candidate directory in order:
// - If it is readable, it is added to readable_dirs.
// - If it is physically writable and mutation-eligible for this operation, it
//   is set as writable_dir and the search STOPS.
//
// Security: This ensures that readable_dirs never contains directories
// with lower security priority than the writable directory. Elevated
// operations stop before %LocalAppData%. Source roles and the trusted admin
// mutation gate are applied before any write probe or directory creation.
InstallDirectories ResolveInstallDirectories(
    const std::string& custom_path,
    const DirectoryResolutionContext& context = {});

// Returns whether the current process token is elevated.
bool IsCurrentProcessElevated();

// Applies the official-build trusted gate for admin-default mutation.
bool IsAdminMutationAllowed(bool automatic_startup, bool enable_explicit_modes);

// Explicit user-retention maintenance may operate only on application-owned
// stores. Ownership is derived from the configuration source, never path
// spelling, ACLs, elevation, or current writability. Custom install_path
// stores and the per-user default are application-owned; HKLM and Program
// Files defaults are provisioning-owned.
bool IsUserRetentionEligible(DirectoryRole role);

// Get the path to the installer database file within the install directory.
// Returns: <install_dir>\installer.json
base::FilePath GetDatabasePath(const base::FilePath& install_dir);

// Get the path to a specific CEF version directory for the current platform.
// Returns: <install_dir>\Versions\<version>\<current_platform>
// Example: C:\Program Files\CEF\Versions\137.3.5\windows64
base::FilePath GetVersionPath(const base::FilePath& install_dir,
                              const Version& version);

// Get the path to a specific CEF version directory for a specific platform.
// Returns: <install_dir>\Versions\<version>\<platform>
// Example: C:\Program Files\CEF\Versions\137.3.5\windows32
base::FilePath GetVersionPath(const base::FilePath& install_dir,
                              const Version& version,
                              const std::string& platform);

// Get the path to libcef.dll within a version directory.
// Returns: <version_dir>\Release\libcef.dll
// CEF distributions place binaries in a Release/ subdirectory.
base::FilePath GetLibcefPath(const base::FilePath& version_dir);

// Enumerate installed CEF versions by scanning <install_dir>\Versions\
// Returns versions sorted newest-first.
std::vector<Version> ScanInstalledVersions(const base::FilePath& install_dir);

// Get the appropriate platform string for the current system.
// Returns: "windows64", "windows32", or "windowsarm64"
std::string GetCurrentPlatform();

// Resolve a UTF-8 path against a base directory. If |path_utf8| is absolute,
// it is normalized and returned directly. If relative, it is joined with
// |base_dir| and normalized. This is a purely lexical operation on Windows
// (_wfullpath) — the returned path may not exist on disk. Callers must
// verify existence separately (e.g., base::DirectoryExists).
base::FilePath ResolvePathRelativeTo(const std::string& path_utf8,
                                     const base::FilePath& base_dir);

// Get the Windows temp directory path (%TEMP%), with trailing separator
// stripped. Returns an empty path on failure.
base::FilePath GetTempDirectoryPath();

// Check if an existing executable is physically contained by the Windows temp
// directory (for the uninstall security check). Returns false when either
// identity cannot be established.
bool IsRunningFromTempDirectory(const base::FilePath& exe_path);

// Convert error code to human-readable string for logging
const char* PathErrorToString(PathError error);

// ============================================================================
// Internal helpers exposed for testing. Not part of the public API.
// ============================================================================
namespace internal {

// Physical containment result used by uninstall preflight. Indeterminate is
// fail-closed: callers could not establish both path identities safely.
enum class PathContainment {
  kContained,
  kOutside,
  kIndeterminate,
};

// Returns whether |path| is the same as or physically below |directory|.
// Both inputs must be existing absolute paths. Windows path identities are
// resolved through handles before complete components are compared, so case,
// separator, junction, and short-name aliases do not change the result.
PathContainment GetPhysicalPathContainment(const base::FilePath& directory,
                                           const base::FilePath& path);

struct TestDirectoryCandidate {
  base::FilePath path;
  DirectoryRole role;
  std::optional<bool> readable;
  std::optional<bool> writable;
};

// Override directory resolution for testing. When set,
// ResolveInstallDirectories returns |readable_dirs| and |writable_dir|
// (or kNotFound if empty). Overrides bypass the custom_path parameter
// entirely.
void OverrideInstallDirectoriesForTesting(
    std::vector<base::FilePath> readable_dirs,
    std::optional<base::FilePath> writable_dir);
void ClearInstallDirectoryOverridesForTesting();

// Supplies source-tagged default candidates and optional physical permission
// state. A null permission uses the real filesystem check. Resolution still
// applies operation mutation/integrity gates and deduplication. Custom-path
// resolution is not overridden.
void OverrideInstallDirectoryCandidatesForTesting(
    std::vector<TestDirectoryCandidate> candidates);

// Returns the number of write probes or directory-creation attempts made by
// directory resolution since the test overrides were last cleared.
size_t GetInstallDirectoryMutationProbeCountForTesting();
void OverrideProcessElevationForTesting(std::optional<bool> elevated);
void OverrideAdminMutationAllowedForTesting(std::optional<bool> allowed);

}  // namespace internal

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_PATHS_H_
