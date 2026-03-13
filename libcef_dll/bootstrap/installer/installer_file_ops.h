// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_FILE_OPS_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_FILE_OPS_H_

#include <windows.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"

namespace cef_installer {

// Error codes for file operations
enum class FileOpsError {
  kSuccess,
  kSourceNotFound,       // Source directory doesn't exist
  kDestinationExists,    // Destination already exists (for install)
  kDestinationNotFound,  // Destination doesn't exist (for uninstall)
  kAccessDenied,         // Permission denied
  kDiskFull,             // Not enough space
  kInUse,                // Files are in use by another process
  kRenameFailed,         // Atomic rename operation failed
  kDeleteFailed,         // Could not delete files
  kQuarantineFailed,     // Existing destination could not be quarantined
  kRepairFailed,         // Verified staging could not replace quarantine
};

enum class FileOpsFault {
  kNone,
  kQuarantineMove,
  kRepairMove,
  kTrashMove,
  kTrashReclaim,
};

enum class VersionLeaseError {
  kSuccess,
  kLostRace,
  kInvalidPath,
  kOpenFailed,
};

// Move-only RAII lease that keeps a selected shared-store distribution usable.
// The fixed libcef.dll handle permits other readers but denies delete sharing,
// preventing a whole-version directory move until the final lease is released.
class VersionLease {
 public:
  ~VersionLease();

  VersionLease(VersionLease&& other) noexcept;
  VersionLease& operator=(VersionLease&& other) noexcept;

  VersionLease(const VersionLease&) = delete;
  VersionLease& operator=(const VersionLease&) = delete;

  bool IsValid() const;

 private:
  friend VersionLeaseError AcquireVersionLease(
      const base::FilePath& trusted_root,
      const base::FilePath& version_dir,
      std::unique_ptr<VersionLease>* lease);

  explicit VersionLease(HANDLE handle);

  HANDLE handle_ = INVALID_HANDLE_VALUE;
};

// Acquire a lease for Release/libcef.dll beneath a validated installed version
// root. kLostRace means removal won and selection may be retried; other errors
// are hard validation/open failures.
VersionLeaseError AcquireVersionLease(const base::FilePath& trusted_root,
                                      const base::FilePath& version_dir,
                                      std::unique_ptr<VersionLease>* lease);

// Deterministic mutation-boundary faults. Typed E2E configuration maps the
// equivalent non-official subprocess values quarantine_move, repair_move,
// trash_move, and trash_reclaim at this component boundary.
void SetFileOpsFaultForTesting(FileOpsFault fault);

// Install a CEF version from extracted temp directory to final location.
// Uses atomic rename for safety (all-or-nothing).
//
// Prerequisites: Caller must have already verified the source directory
// contents (signature/catalog verification) before calling this function.
//
// Parameters:
// - source_dir: Operation-owned temp directory containing extracted and
//   verified CEF files. It is consumed only by a successful rename.
// - install_dir: Base install directory (e.g., "C:\Program Files\CEF")
// - version: CEF version being installed
// - cleanup_deferred: Set when publication succeeds but reclamation of a
//   quarantined target fails. The caller must surface this only after all
//   later logical publication and result validation succeeds.
//
// Process:
// 1. Verify source_dir exists and is not a reparse point
// 2. Construct dest: <install_dir>\Versions\<version>\<platform>
// 3. Walk dest up to install_dir checking existing ancestors for reparse points
// 4. If dest exists as a safe regular file or directory, move it opaquely to
//    reparse-safe .trash/. A quarantine failure preserves both dest and source.
// 5. Create parent directories if needed
// 6. Rename source_dir to dest (atomic, same volume). Failure after quarantine
//    preserves source, leaves the old target in .trash/, and leaves dest
//    absent.
// 7. Reclaim a quarantined target without traversing reparse points;
//    reclamation failure is deferred physical cleanup and never rolls back
//    publication.
//
// IMPORTANT: source_dir MUST be on the same volume as install_dir. The
// controller ensures this by staging under <install_dir>\.staging\.
// Cross-volume renames will fail with kRenameFailed.
FileOpsError InstallVersion(const base::FilePath& source_dir,
                            const base::FilePath& install_dir,
                            const Version& version,
                            bool* cleanup_deferred = nullptr);

// Remove a CEF version that is no longer needed.
//
// Process:
// 1. Verify source exists and is not a reparse point
// 2. Check intermediate directories for reparse points
// 3. Create <install_dir>\.trash\ if needed (with reparse point check)
// 4. Rename source to <install_dir>\.trash\<version>_<random> (atomic)
// 5. Delete trash entry recursively
// 6. If delete fails (files in use), leave for retry on next run
// 7. If parent version directory is empty, remove it too
//
// Returns kInUse if files are locked; caller should retry later.
// Uses current platform.
FileOpsError UninstallVersion(const base::FilePath& install_dir,
                              const Version& version);

// Uninstall a specific platform of a CEF version.
// Same as above but for a specified platform instead of current platform.
// Returns kInUse if files are locked; caller should retry later.
FileOpsError UninstallVersion(const base::FilePath& install_dir,
                              const Version& version,
                              const std::string& platform);

// Retry deletion of previously failed uninstalls.
// Called automatically at the start of each installer operation.
// Returns the number of successfully deleted trash entries (files or
// directories).
int RetryPendingDeletions(const base::FilePath& install_dir);

// Check if a path is a reparse point (symlink or junction).
// Used to prevent symlink/junction attacks where an attacker creates a
// junction pointing to a sensitive directory.
// Returns false if path doesn't exist or attributes can't be read.
bool IsReparsePoint(const base::FilePath& path);

// Verify a file path is safe (not a reparse point). If the path is a reparse
// point, removes the reparse point itself (not its target).
// If |delete_if_exists| is true, also deletes the file at the path.
// Returns true if the path is safe to use (was not a reparse point, or the
// reparse point was successfully removed). Returns false only if a reparse
// point exists but could not be removed.
bool VerifySafeFilePath(const base::FilePath& path,
                        bool delete_if_exists = false);

// Verify a directory path is safe (not a reparse point). If the path is a
// reparse point, removes the reparse point itself (not its target).
// If |delete_if_exists| is true, also deletes the directory recursively.
// Returns true if the path is safe to use (was not a reparse point, or the
// reparse point was successfully removed). Returns false only if a reparse
// point exists but could not be removed.
bool VerifySafeDirectoryPath(const base::FilePath& path,
                             bool delete_if_exists = false);

// Check if a directory exists and is not a reparse point (read-only check).
// Does NOT require write access. Does NOT create the directory.
bool IsReadableDirectory(const base::FilePath& path);

// Validate that every path component between |trusted_root| (exclusive) and
// |path| (inclusive) exists and is not a reparse point.
// Both paths must be absolute. |trusted_root| itself is not checked —
// assumed safe by the caller.
// Returns false if any component is a reparse point, does not exist, or
// either path is relative.
bool IsPathSafeForLoading(const base::FilePath& trusted_root,
                          const base::FilePath& path,
                          base::TimeTicks deadline = {},
                          bool* time_limit_reached = nullptr);

// Returns true if two paths resolve to the same directory.
// Uses MakeAbsoluteFilePath for canonicalization, with case-insensitive
// fallback comparison on Windows.
bool IsSameDirectory(const base::FilePath& a, const base::FilePath& b);

// Convert error code to human-readable string for logging
const char* FileOpsErrorToString(FileOpsError error);

// RAII helper that deletes a file when it goes out of scope.
// Call Release() to prevent deletion (e.g., after successful rename).
class ScopedFileDeleter {
 public:
  explicit ScopedFileDeleter(const base::FilePath& path);
  ~ScopedFileDeleter();

  ScopedFileDeleter(const ScopedFileDeleter&) = delete;
  ScopedFileDeleter& operator=(const ScopedFileDeleter&) = delete;

  void Release();

 private:
  base::FilePath path_;
};

// RAII helper that recursively deletes a directory when it goes out of scope.
// Call Release() to prevent deletion.
class ScopedDirectoryDeleter {
 public:
  explicit ScopedDirectoryDeleter(const base::FilePath& path);
  ~ScopedDirectoryDeleter();

  ScopedDirectoryDeleter(const ScopedDirectoryDeleter&) = delete;
  ScopedDirectoryDeleter& operator=(const ScopedDirectoryDeleter&) = delete;

  void Release();

 private:
  base::FilePath path_;
};

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_FILE_OPS_H_
