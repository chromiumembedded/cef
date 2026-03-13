// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_database.h"

#include <time.h>

#include <algorithm>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_e2e_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"

namespace cef_installer {

namespace {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
bool g_fail_database_save = false;
#endif

bool ShouldFailDatabaseSave() {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (g_fail_database_save) {
    return true;
  }
  return internal::GetInstallerE2EConfig().database_save_failure;
#else
  return false;
#endif
}

// JSON field names
constexpr char kSchemaVersionField[] = "schema_version";
constexpr char kAppsField[] = "apps";
constexpr char kUuidField[] = "uuid";
constexpr char kPlatformField[] = "platform";
constexpr char kVminField[] = "vmin";
constexpr char kVmaxField[] = "vmax";
constexpr char kAbiHashField[] = "abi_hash";
constexpr char kPruningSuspendedUntilField[] = "pruning_suspended_until";

}  // namespace

void SetDatabaseSaveFailureForTesting(bool fail) {
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  (void)fail;
#else
  g_fail_database_save = fail;
#endif
}

bool AppEntry::operator==(const AppEntry& other) const {
  return uuid == other.uuid && platform == other.platform &&
         vmin == other.vmin && vmax == other.vmax && abi_hash == other.abi_hash;
}

Database::Database() = default;
Database::~Database() = default;

DatabaseError Database::Load(const base::FilePath& path,
                             IntegrityMismatchAction mismatch_action) {
  // Reset to defaults
  schema_version_ = kCurrentSchemaVersion;
  apps_.clear();
  pruning_suspended_until_ = 0;

  std::string json_content;
  IntegrityResult ir =
      ReadFileWithIntegrity(path, &json_content, mismatch_action);
  switch (ir) {
    case IntegrityResult::kFileNotFound:
      // File doesn't exist - start with empty database
      return DatabaseError::kSuccess;
    case IntegrityResult::kReadError:
      return DatabaseError::kFileReadError;
    case IntegrityResult::kIntegrityMismatch:
      // Corrupted file was already deleted by ReadFileWithIntegrity.
      // Return empty database — caller should call SuspendPruning().
      return DatabaseError::kIntegrityMismatch;
    case IntegrityResult::kSuccess:
    case IntegrityResult::kSuccessNoFooter:
      // Continue loading below.
      break;
  }

  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(json_content, base::JSON_PARSE_RFC);
  if (!parsed) {
    return DatabaseError::kJsonParseError;
  }

  // Read schema version
  std::optional<int> version = parsed->FindInt(kSchemaVersionField);
  if (version) {
    schema_version_ = *version;
    if (schema_version_ > kCurrentSchemaVersion) {
      // Schema is newer than we understand. Continue loading known fields
      // so the caller can still query, but return kSchemaVersionTooNew
      // below so the caller knows not to save (which would drop unknown
      // fields from the newer schema).
    }
  }

  // Read apps array
  const base::ListValue* apps_list = parsed->FindList(kAppsField);
  if (apps_list) {
    for (const base::Value& app_value : *apps_list) {
      const base::DictValue* app_dict = app_value.GetIfDict();
      if (!app_dict) {
        continue;
      }

      AppEntry entry;
      const std::string* uuid = app_dict->FindString(kUuidField);
      if (!uuid || uuid->empty() || uuid->size() > kMaxUuidLength) {
        continue;  // Skip invalid entries
      }
      entry.uuid = *uuid;

      const std::string* platform = app_dict->FindString(kPlatformField);
      if (!platform || platform->empty() ||
          platform->size() > kMaxPlatformLength) {
        continue;  // Skip entries without valid platform
      }
      entry.platform = *platform;

      const std::string* vmin = app_dict->FindString(kVminField);
      if (vmin && vmin->size() > kMaxVersionLength) {
        continue;  // Skip entries with invalid fields
      }
      entry.vmin = vmin ? *vmin : "";

      const std::string* vmax = app_dict->FindString(kVmaxField);
      if (vmax && vmax->size() > kMaxVersionLength) {
        continue;  // Skip entries with invalid fields
      }
      entry.vmax = vmax ? *vmax : "";

      const std::string* abi_hash = app_dict->FindString(kAbiHashField);
      if (abi_hash && abi_hash->size() > kMaxAbiHashLength) {
        continue;  // Skip entries with invalid fields
      }
      entry.abi_hash = abi_hash ? *abi_hash : "";

      apps_.push_back(std::move(entry));
    }
  }

  // Read pruning suspension timestamp (if present)
  auto suspension_value = parsed->Find(kPruningSuspendedUntilField);
  if (suspension_value) {
    if (suspension_value->is_string()) {
      int64_t ts;
      if (base::StringToInt64(suspension_value->GetString(), &ts)) {
        pruning_suspended_until_ = ts;
      }
    }
  }

  // Return error if schema is too new (after loading what we can)
  if (schema_version_ > kCurrentSchemaVersion) {
    return DatabaseError::kSchemaVersionTooNew;
  }

  return DatabaseError::kSuccess;
}

DatabaseError Database::Save(const base::FilePath& path) {
  if (ShouldFailDatabaseSave()) {
    return DatabaseError::kFileWriteError;
  }
  base::DictValue root;
  root.Set(kSchemaVersionField, schema_version_);

  if (pruning_suspended_until_ > 0) {
    root.Set(kPruningSuspendedUntilField,
             base::NumberToString(pruning_suspended_until_));
  }

  base::ListValue apps_list;
  for (const AppEntry& entry : apps_) {
    base::DictValue app_dict;
    app_dict.Set(kUuidField, entry.uuid);
    app_dict.Set(kPlatformField, entry.platform);
    app_dict.Set(kVminField, entry.vmin);
    if (!entry.vmax.empty()) {
      app_dict.Set(kVmaxField, entry.vmax);
    }
    if (!entry.abi_hash.empty()) {
      app_dict.Set(kAbiHashField, entry.abi_hash);
    }
    apps_list.Append(std::move(app_dict));
  }
  root.Set(kAppsField, std::move(apps_list));

  std::string json_content;
  if (!base::JSONWriter::WriteWithOptions(
          root, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_content)) {
    return DatabaseError::kFileWriteError;
  }

  // Write to a temp file then atomically rename to prevent partial writes.
  base::FilePath temp_path;
  if (!base::CreateTemporaryFileInDir(path.DirName(), &temp_path)) {
    return DatabaseError::kFileWriteError;
  }
  ScopedFileDeleter temp_deleter(temp_path);

  if (!WriteFileWithIntegrity(temp_path, json_content)) {
    return DatabaseError::kFileWriteError;
  }

  base::File::Error error;
  if (!base::ReplaceFile(temp_path, path, &error)) {
    return DatabaseError::kFileWriteError;
  }

  temp_deleter.Release();
  return DatabaseError::kSuccess;
}

int Database::GetSchemaVersion() const {
  return schema_version_;
}

bool Database::RegisterApp(const AppEntry& entry) {
  // Find existing entry by (uuid, platform)
  auto it =
      std::find_if(apps_.begin(), apps_.end(), [&entry](const AppEntry& e) {
        return e.uuid == entry.uuid && e.platform == entry.platform;
      });

  if (it != apps_.end()) {
    if (*it == entry) {
      return false;
    }
    *it = entry;
  } else {
    apps_.push_back(entry);
  }
  return true;
}

void Database::UnregisterApp(const std::string& uuid,
                             const std::string& platform) {
  apps_.erase(std::remove_if(apps_.begin(), apps_.end(),
                             [&uuid, &platform](const AppEntry& e) {
                               return e.uuid == uuid && e.platform == platform;
                             }),
              apps_.end());
}

std::optional<AppEntry> Database::GetApp(const std::string& uuid,
                                         const std::string& platform) const {
  auto it = std::find_if(apps_.begin(), apps_.end(),
                         [&uuid, &platform](const AppEntry& e) {
                           return e.uuid == uuid && e.platform == platform;
                         });

  if (it != apps_.end()) {
    return *it;
  }
  return std::nullopt;
}

const std::vector<AppEntry>& Database::GetAllApps() const {
  return apps_;
}

bool Database::IsEmpty() const {
  return apps_.empty();
}

Version Database::GetGlobalVmin(const std::string& platform) const {
  Version result;
  for (const auto& app : apps_) {
    if (app.platform != platform) {
      continue;
    }
    Version v = Version::Parse(app.vmin);
    if (v.IsValid() && (!result.IsValid() || v < result)) {
      result = v;
    }
  }
  return result;
}

bool Database::CanPrune() const {
  // Don't prune if schema is newer than we understand.
  // This prevents older installers from deleting versions that
  // newer schema fields might reference.
  if (schema_version_ > kCurrentSchemaVersion) {
    return false;
  }

  // Don't prune while suspension is active. After corruption recovery,
  // app entries are lost and pruning would delete versions that other
  // (unregistered) apps still need.
  if (pruning_suspended_until_ > 0) {
    int64_t now = static_cast<int64_t>(time(nullptr));
    if (now < pruning_suspended_until_) {
      return false;
    }
  }

  return true;
}

void Database::SuspendPruning() {
  pruning_suspended_until_ =
      static_cast<int64_t>(time(nullptr)) + kPruningSuspensionSeconds;
}

const char* DatabaseErrorToString(DatabaseError error) {
  switch (error) {
    case DatabaseError::kSuccess:
      return "Success";
    case DatabaseError::kFileNotFound:
      return "Database file not found";
    case DatabaseError::kFileReadError:
      return "Could not read database file";
    case DatabaseError::kFileWriteError:
      return "Could not write database file";
    case DatabaseError::kJsonParseError:
      return "Invalid JSON in database file";
    case DatabaseError::kSchemaVersionTooNew:
      return "Database schema version too new";
    case DatabaseError::kLockAcquisitionFailed:
      return "Could not acquire database lock";
    case DatabaseError::kIntegrityMismatch:
      return "Database file corrupted (CRC32 mismatch, file deleted)";
  }
  return "Unknown error";
}

}  // namespace cef_installer
