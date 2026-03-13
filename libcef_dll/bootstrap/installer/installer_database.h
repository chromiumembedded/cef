// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_DATABASE_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_DATABASE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"

namespace cef_installer {

// Current schema version - increment when adding fields.
//
// BACKWARDS COMPATIBILITY:
// Multiple installer versions may share the same database file. Older
// installers must be able to read databases written by newer ones (they
// skip unknown fields) and newer installers must not break older ones.
// Rules:
// - New fields: add with sensible defaults so older readers work without them.
// - Existing fields: never rename, remove, or change semantics.
// - Apps array: per-app entries are re-serialized from AppEntry, so any new
//   per-app fields must be added to AppEntry and handled in Load/Save.
// - Increment kCurrentSchemaVersion when adding fields. Older installers
//   seeing a higher version will return kSchemaVersionTooNew and skip
//   saving (avoiding data loss).
constexpr int kCurrentSchemaVersion = 1;

// Grace period (seconds) before pruning resumes after corruption recovery.
// 7 days gives apps time to re-register on their next launch.
constexpr int64_t kPruningSuspensionSeconds = 7 * 24 * 60 * 60;

// Per-application registration entry
// Unique key is (uuid, platform) - same app on different architectures are
// separate entries.
struct AppEntry {
  std::string uuid;      // Unique app identifier (never changes)
  std::string platform;  // Platform identifier (e.g., "windows64", "windows32")
  std::string vmin;      // Minimum version requirement
  std::string vmax;      // Maximum version requirement (optional)
  std::string abi_hash;  // Sandbox compatibility hash

  bool operator==(const AppEntry& other) const;
};

// Error codes for database operations
enum class DatabaseError {
  kSuccess,
  kFileNotFound,           // Database file doesn't exist (OK for new install)
  kFileReadError,          // Could not read database file
  kFileWriteError,         // Could not write database file
  kJsonParseError,         // Database file contains invalid JSON
  kSchemaVersionTooNew,    // Database schema newer than we understand
  kLockAcquisitionFailed,  // Could not acquire database lock
  kIntegrityMismatch,      // CRC32 mismatch — corrupted file was deleted
};

// Deterministic save failure for transaction tests. Typed E2E configuration
// maps the matching non-official subprocess fault at this component boundary.
void SetDatabaseSaveFailureForTesting(bool fail);

// Manages the shared installer.json database.
// Thread-safety: Not thread-safe. Use with external locking if needed.
class Database {
 public:
  Database();
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  // Load database from file. Creates empty database if file doesn't exist.
  // If the file has a CRC32 integrity footer and the checksum mismatches,
  // |mismatch_action| selects deletion for writer recovery or preservation for
  // read-only inspection (returns kIntegrityMismatch either way). Legacy files
  // without a footer are accepted.
  // Returns kSchemaVersionTooNew if schema_version > kCurrentSchemaVersion.
  DatabaseError Load(
      const base::FilePath& path,
      IntegrityMismatchAction mismatch_action = IntegrityMismatchAction::kDoom);

  // Save database to file with CRC32 integrity footer.
  // Uses atomic write (temp file + rename) to prevent partial writes.
  DatabaseError Save(const base::FilePath& path);

  // Get current schema version (from loaded database or kCurrentSchemaVersion)
  int GetSchemaVersion() const;

  // CRUD operations - keyed by (uuid, platform)
  // - Add or update by (uuid, platform)
  bool RegisterApp(const AppEntry& entry);
  // - Remove by (uuid, platform)
  void UnregisterApp(const std::string& uuid, const std::string& platform);
  std::optional<AppEntry> GetApp(const std::string& uuid,
                                 const std::string& platform) const;
  const std::vector<AppEntry>& GetAllApps() const;

  // Check if database is empty (no registered apps)
  bool IsEmpty() const;

  // Return the minimum vmin across all registered apps for the given
  // platform, or an invalid Version if none match.
  Version GetGlobalVmin(const std::string& platform) const;

  // Check if we can safely prune versions.
  // Returns false if schema_version > kCurrentSchemaVersion (skip pruning)
  // or if pruning is suspended (grace period after corruption recovery).
  bool CanPrune() const;

  // Suspend pruning for kPruningSuspensionSeconds from now.
  // Called after corruption recovery to give apps time to re-register.
  // The suspension timestamp is persisted in the database JSON.
  void SuspendPruning();

 private:
  int schema_version_ = kCurrentSchemaVersion;
  std::vector<AppEntry> apps_;

  // Unix timestamp (seconds since epoch). Pruning is suspended until this
  // time. 0 means not suspended. Persisted as "pruning_suspended_until" in
  // the database JSON.
  int64_t pruning_suspended_until_ = 0;
};

// Convert error code to human-readable string for logging
const char* DatabaseErrorToString(DatabaseError error);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_DATABASE_H_
