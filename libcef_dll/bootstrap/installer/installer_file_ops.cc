// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"

#include <windows.h>

#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_e2e_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"

namespace cef_installer {
namespace {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
FileOpsFault g_file_ops_fault = FileOpsFault::kNone;
#endif

FileOpsFault EffectiveFileOpsFault() {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (g_file_ops_fault != FileOpsFault::kNone) {
    return g_file_ops_fault;
  }
  switch (internal::GetInstallerE2EConfig().file_ops_fault) {
    case internal::InstallerE2EFileOpsFault::kNone:
      break;
    case internal::InstallerE2EFileOpsFault::kQuarantineMove:
      return FileOpsFault::kQuarantineMove;
    case internal::InstallerE2EFileOpsFault::kRepairMove:
      return FileOpsFault::kRepairMove;
    case internal::InstallerE2EFileOpsFault::kTrashMove:
      return FileOpsFault::kTrashMove;
    case internal::InstallerE2EFileOpsFault::kTrashReclaim:
      return FileOpsFault::kTrashReclaim;
  }
#endif
  return FileOpsFault::kNone;
}

// Validate that a directory path is safe for file operations.
// Returns false if the path is a reparse point (symlink/junction).
bool IsDirectorySafe(const base::FilePath& dir_path) {
  if (!base::DirectoryExists(dir_path)) {
    return false;
  }
  return !IsReparsePoint(dir_path);
}

// Generate a unique trash entry path under trash_root.
base::FilePath GenerateTrashPath(const base::FilePath& trash_root,
                                 const Version& version) {
  // Format: <version>_<random>
  uint64_t random_value = base::RandUint64();
  std::string random_suffix = base::HexEncode(base::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(&random_value), sizeof(random_value)));
  std::string entry_name = version.ToString() + "_" + random_suffix;
  return trash_root.Append(base::FilePath::FromUTF8Unsafe(entry_name));
}

// Map Windows error code to FileOpsError
FileOpsError MapWindowsError(DWORD error) {
  switch (error) {
    case ERROR_SUCCESS:
      return FileOpsError::kSuccess;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      return FileOpsError::kSourceNotFound;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
      return FileOpsError::kDestinationExists;
    case ERROR_ACCESS_DENIED:
      return FileOpsError::kAccessDenied;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
      return FileOpsError::kDiskFull;
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
      return FileOpsError::kInUse;
    default:
      return FileOpsError::kRenameFailed;
  }
}

// Delete a file or directory without ever enumerating a reparse point. A
// partially deleted directory remains retryable when an entry is in use.
bool TryDeletePath(const base::FilePath& path) {
  DWORD attributes = ::GetFileAttributesW(path.value().c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    DWORD error = ::GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
  }

  if (attributes & FILE_ATTRIBUTE_READONLY) {
    if (!::SetFileAttributesW(path.value().c_str(),
                              attributes & ~FILE_ATTRIBUTE_READONLY)) {
      return false;
    }
    attributes &= ~FILE_ATTRIBUTE_READONLY;
  }

  if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    return attributes & FILE_ATTRIBUTE_DIRECTORY
               ? !!::RemoveDirectoryW(path.value().c_str())
               : !!::DeleteFileW(path.value().c_str());
  }

  if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
    return !!::DeleteFileW(path.value().c_str());
  }

  base::FileEnumerator enumerator(
      path, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
      base::FilePath::StringType(),
      base::FileEnumerator::FolderSearchPolicy::MATCH_ONLY,
      base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
  bool all_deleted = true;
  for (base::FilePath child = enumerator.Next(); !child.empty();
       child = enumerator.Next()) {
    if (!TryDeletePath(child)) {
      all_deleted = false;
    }
  }
  if (enumerator.GetError() != base::File::FILE_OK) {
    all_deleted = false;
  }
  if (!all_deleted) {
    return false;
  }
  return !!::RemoveDirectoryW(path.value().c_str());
}

bool IsSafeCollisionTarget(const base::FilePath& install_dir,
                           const base::FilePath& target) {
  if (!IsPathSafeForLoading(install_dir, target)) {
    return false;
  }
  DWORD attributes = ::GetFileAttributesW(target.value().c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES ||
      (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE))) {
    return false;
  }
  return true;
}

}  // namespace

void SetFileOpsFaultForTesting(FileOpsFault fault) {
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  (void)fault;
#else
  g_file_ops_fault = fault;
#endif
}

VersionLease::VersionLease(HANDLE handle) : handle_(handle) {}

VersionLease::~VersionLease() {
  if (IsValid()) {
    CloseHandle(handle_);
  }
}

VersionLease::VersionLease(VersionLease&& other) noexcept
    : handle_(std::exchange(other.handle_, INVALID_HANDLE_VALUE)) {}

VersionLease& VersionLease::operator=(VersionLease&& other) noexcept {
  if (this != &other) {
    if (IsValid()) {
      CloseHandle(handle_);
    }
    handle_ = std::exchange(other.handle_, INVALID_HANDLE_VALUE);
  }
  return *this;
}

bool VersionLease::IsValid() const {
  return handle_ != INVALID_HANDLE_VALUE;
}

VersionLeaseError AcquireVersionLease(const base::FilePath& trusted_root,
                                      const base::FilePath& version_dir,
                                      std::unique_ptr<VersionLease>* lease) {
  if (!lease) {
    return VersionLeaseError::kInvalidPath;
  }
  lease->reset();

  base::FilePath libcef_path = GetLibcefPath(version_dir);
  if (!IsPathSafeForLoading(trusted_root, libcef_path)) {
    if (!base::PathExists(version_dir) || !base::PathExists(libcef_path)) {
      return VersionLeaseError::kLostRace;
    }
    return VersionLeaseError::kInvalidPath;
  }

  HANDLE handle = CreateFileW(libcef_path.value().c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
      return VersionLeaseError::kLostRace;
    }
    return VersionLeaseError::kOpenFailed;
  }

  *lease = std::unique_ptr<VersionLease>(new VersionLease(handle));
  return VersionLeaseError::kSuccess;
}

FileOpsError InstallVersion(const base::FilePath& source_dir,
                            const base::FilePath& install_dir,
                            const Version& version,
                            bool* cleanup_deferred) {
  if (cleanup_deferred) {
    *cleanup_deferred = false;
  }
  FileOpsFault fault = EffectiveFileOpsFault();
  // Validate inputs
  if (!version.IsValid()) {
    return FileOpsError::kSourceNotFound;
  }

  // 1. Verify source exists and is not a reparse point (symlink/junction)
  // This prevents attacks where an attacker replaces the temp directory with
  // a junction pointing to sensitive files.
  if (!IsDirectorySafe(source_dir)) {
    return FileOpsError::kSourceNotFound;
  }

  // 2. Construct destination path
  base::FilePath dest_dir = GetVersionPath(install_dir, version);

  // 3. Check existing ancestors before any collision mutation.
  base::FilePath parent_dir = dest_dir.DirName();
  for (base::FilePath dir = parent_dir;
       dir != install_dir && dir != dir.DirName(); dir = dir.DirName()) {
    if (base::PathExists(dir) && IsReparsePoint(dir)) {
      return FileOpsError::kAccessDenied;
    }
  }

  base::FilePath quarantined;

  // 4. Quarantine every safe existing target opaquely. Destination contents
  // are never read or classified; verified staging always wins the collision.
  if (base::PathExists(dest_dir)) {
    if (!IsSafeCollisionTarget(install_dir, dest_dir)) {
      return FileOpsError::kQuarantineFailed;
    }

    base::FilePath trash_root = install_dir.Append(kTrashSubdirectory);
    if (!VerifySafeDirectoryPath(trash_root) ||
        (!base::DirectoryExists(trash_root) &&
         !base::CreateDirectory(trash_root))) {
      return FileOpsError::kQuarantineFailed;
    }
    quarantined = GenerateTrashPath(trash_root, version);
    if (fault == FileOpsFault::kQuarantineMove ||
        !::MoveFileExW(dest_dir.value().c_str(), quarantined.value().c_str(),
                       MOVEFILE_WRITE_THROUGH)) {
      return FileOpsError::kQuarantineFailed;
    }
  }

  // 5. Create parent directories if needed.
  if (!base::DirectoryExists(parent_dir) &&
      !base::CreateDirectory(parent_dir)) {
    return FileOpsError::kAccessDenied;
  }

  // 6. Perform atomic rename
  // On Windows, MoveFileEx with MOVEFILE_WRITE_THROUGH ensures durability.
  // The caller is expected to place source_dir on the same volume as
  // install_dir (e.g., via the .staging/ subdirectory) so this always
  // succeeds as an atomic rename.
  if (fault == FileOpsFault::kRepairMove && !quarantined.empty()) {
    return FileOpsError::kRepairFailed;
  }
  if (!::MoveFileExW(source_dir.value().c_str(), dest_dir.value().c_str(),
                     MOVEFILE_WRITE_THROUGH)) {
    if (!quarantined.empty()) {
      return FileOpsError::kRepairFailed;
    }
    return MapWindowsError(::GetLastError());
  }

  // 7. Reclaim the quarantined path best-effort without traversing reparse
  // points. Publication is already logically complete at this boundary.
  if (!quarantined.empty() &&
      (fault == FileOpsFault::kTrashReclaim || !TryDeletePath(quarantined)) &&
      cleanup_deferred) {
    *cleanup_deferred = true;
  }

  return FileOpsError::kSuccess;
}

FileOpsError UninstallVersion(const base::FilePath& install_dir,
                              const Version& version,
                              const std::string& platform) {
  // Validate inputs
  if (!version.IsValid() || platform.empty()) {
    return FileOpsError::kDestinationNotFound;
  }
  FileOpsFault fault = EffectiveFileOpsFault();

  // 1. Verify source exists and is not a reparse point.
  base::FilePath source_dir = GetVersionPath(install_dir, version, platform);
  if (!IsDirectorySafe(source_dir)) {
    return FileOpsError::kDestinationNotFound;
  }

  // 2. Check intermediate directories for reparse points
  //    (walk from source_dir up to install_dir).
  for (base::FilePath dir = source_dir.DirName();
       dir != install_dir && dir != dir.DirName(); dir = dir.DirName()) {
    if (IsReparsePoint(dir)) {
      return FileOpsError::kDestinationNotFound;
    }
  }

  // 3. Create .trash/ directory if needed, checking for reparse points.
  base::FilePath trash_root = install_dir.Append(kTrashSubdirectory);
  if (!VerifySafeDirectoryPath(trash_root)) {
    return FileOpsError::kAccessDenied;
  }
  if (!base::DirectoryExists(trash_root) &&
      !base::CreateDirectory(trash_root)) {
    return FileOpsError::kAccessDenied;
  }

  // 4. Generate trash entry path and atomic rename
  base::FilePath trash_dir = GenerateTrashPath(trash_root, version);
  if (fault == FileOpsFault::kTrashMove) {
    return FileOpsError::kRenameFailed;
  }
  if (!::MoveFileExW(source_dir.value().c_str(), trash_dir.value().c_str(),
                     MOVEFILE_WRITE_THROUGH)) {
    return MapWindowsError(::GetLastError());
  }

  // 5. Try to delete trash directory
  FileOpsError result = FileOpsError::kSuccess;
  if (fault == FileOpsFault::kTrashReclaim || !TryDeletePath(trash_dir)) {
    // Files are in use - leave for retry on next run
    // This is not a failure; the version is effectively uninstalled
    // (moved to trash) and will be cleaned up later
    result = FileOpsError::kInUse;
  }

  // 6. Try to remove empty parent version directory (e.g., Versions/137.3.5/)
  // This cleans up after the last platform for a version is uninstalled.
  // DeleteFile on a directory only succeeds if empty, so this is safe.
  base::FilePath version_parent = source_dir.DirName();
  base::DeleteFile(version_parent);  // Ignore result - fails if not empty

  return result;
}

FileOpsError UninstallVersion(const base::FilePath& install_dir,
                              const Version& version) {
  return UninstallVersion(install_dir, version, GetCurrentPlatform());
}

int RetryPendingDeletions(const base::FilePath& install_dir) {
  int deleted_count = 0;

  base::FilePath trash_root = install_dir.Append(kTrashSubdirectory);
  if (!base::DirectoryExists(trash_root) || IsReparsePoint(trash_root)) {
    return 0;
  }

  // Scan entries in .trash/ and try to delete them
  base::FileEnumerator enumerator(
      trash_root, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    // TryDeletePath removes reparse points themselves and never enumerates
    // their targets.
    if (TryDeletePath(path)) {
      deleted_count++;
    }
  }

  return deleted_count;
}

bool IsReparsePoint(const base::FilePath& path) {
  DWORD attrs = ::GetFileAttributesW(path.value().c_str());
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  return (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool VerifySafeFilePath(const base::FilePath& path, bool delete_if_exists) {
  if (IsReparsePoint(path)) {
    if (!base::DeleteFile(path)) {
      return false;
    }
  }
  if (delete_if_exists && base::PathExists(path)) {
    base::DeleteFile(path);
  }
  return true;
}

bool VerifySafeDirectoryPath(const base::FilePath& path,
                             bool delete_if_exists) {
  if (IsReparsePoint(path)) {
    if (!base::DeleteFile(path)) {
      return false;
    }
  }
  if (delete_if_exists && base::DirectoryExists(path)) {
    base::DeletePathRecursively(path);
  }
  return true;
}

bool IsReadableDirectory(const base::FilePath& path) {
  if (path.empty()) {
    return false;
  }

  if (!base::DirectoryExists(path)) {
    return false;
  }

  // Security: Reject reparse points (symlinks/junctions).
  return !IsReparsePoint(path);
}

bool IsPathSafeForLoading(const base::FilePath& trusted_root,
                          const base::FilePath& path,
                          base::TimeTicks deadline,
                          bool* time_limit_reached) {
  auto time_expired = [&]() {
    if (!deadline.is_null() && base::TimeTicks::Now() >= deadline) {
      if (time_limit_reached) {
        *time_limit_reached = true;
      }
      return true;
    }
    return false;
  };

  if (time_limit_reached) {
    *time_limit_reached = false;
  }
  if (time_expired()) {
    return false;
  }
  if (trusted_root.empty() || path.empty()) {
    return false;
  }

  if (!trusted_root.IsAbsolute() || !path.IsAbsolute()) {
    return false;
  }

  if (!trusted_root.IsParent(path)) {
    return false;
  }

  // Collect components between root (exclusive) and path (inclusive) by
  // walking up from path to root.
  std::vector<base::FilePath> to_check;
  for (base::FilePath current = path; current != trusted_root;
       current = current.DirName()) {
    if (current == current.DirName()) {
      return false;
    }
    to_check.push_back(current);
  }

  // Check top-down (reverse order) so we fail on the shallowest
  // reparse point first.
  for (auto it = to_check.rbegin(); it != to_check.rend(); ++it) {
    if (time_expired() || !base::PathExists(*it) || time_expired()) {
      return false;
    }
    if (IsReparsePoint(*it) || time_expired()) {
      return false;
    }
  }

  return true;
}

bool IsSameDirectory(const base::FilePath& a, const base::FilePath& b) {
  base::FilePath abs_a = base::MakeAbsoluteFilePath(a);
  base::FilePath abs_b = base::MakeAbsoluteFilePath(b);
  if (abs_a.empty() || abs_b.empty()) {
    // MakeAbsoluteFilePath failed; fall back to case-insensitive compare.
    return base::FilePath::CompareEqualIgnoreCase(a.value(), b.value());
  }
  return base::FilePath::CompareEqualIgnoreCase(abs_a.value(), abs_b.value());
}

const char* FileOpsErrorToString(FileOpsError error) {
  switch (error) {
    case FileOpsError::kSuccess:
      return "Success";
    case FileOpsError::kSourceNotFound:
      return "Source directory not found";
    case FileOpsError::kDestinationExists:
      return "Destination already exists";
    case FileOpsError::kDestinationNotFound:
      return "Destination not found";
    case FileOpsError::kAccessDenied:
      return "Access denied";
    case FileOpsError::kDiskFull:
      return "Disk full";
    case FileOpsError::kInUse:
      return "Files are in use";
    case FileOpsError::kRenameFailed:
      return "Rename operation failed";
    case FileOpsError::kDeleteFailed:
      return "Delete operation failed";
    case FileOpsError::kQuarantineFailed:
      return "Destination quarantine failed";
    case FileOpsError::kRepairFailed:
      return "Destination repair failed";
  }
  return "Unknown error";
}

ScopedFileDeleter::ScopedFileDeleter(const base::FilePath& path)
    : path_(path) {}

ScopedFileDeleter::~ScopedFileDeleter() {
  if (!path_.empty()) {
    base::DeleteFile(path_);
  }
}

void ScopedFileDeleter::Release() {
  path_ = base::FilePath();
}

ScopedDirectoryDeleter::ScopedDirectoryDeleter(const base::FilePath& path)
    : path_(path) {}

ScopedDirectoryDeleter::~ScopedDirectoryDeleter() {
  if (!path_.empty()) {
    base::DeletePathRecursively(path_);
  }
}

void ScopedDirectoryDeleter::Release() {
  path_ = base::FilePath();
}

}  // namespace cef_installer
