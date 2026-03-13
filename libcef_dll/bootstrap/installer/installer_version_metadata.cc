// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_version_metadata.h"

#include <algorithm>
#include <set>
#include <string_view>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_e2e_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"
#include "cef/libcef_dll/bootstrap/installer/installer_logger.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "cef/libcef_dll/bootstrap/installer/installer_validation.h"

namespace cef_installer {

namespace {

// JSON field names (shared between per-version metadata and version index)
constexpr char kVersionField[] = "version";
constexpr char kAbiHashField[] = "abi_hash";
constexpr char kPlatformField[] = "platform";
constexpr char kVersionFullField[] = "version_full";

// Version index field names
constexpr char kIndexVersionsField[] = "versions";
constexpr char kIndexPathField[] = "path";

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
VersionIndexFault g_version_index_fault = VersionIndexFault::kNone;
#endif

VersionIndexFault EffectiveVersionIndexFault() {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (g_version_index_fault != VersionIndexFault::kNone) {
    return g_version_index_fault;
  }
  switch (internal::GetInstallerE2EConfig().version_index_fault) {
    case internal::InstallerE2EVersionIndexFault::kNone:
      break;
    case internal::InstallerE2EVersionIndexFault::kWrite:
      return VersionIndexFault::kWrite;
    case internal::InstallerE2EVersionIndexFault::kReplace:
      return VersionIndexFault::kReplace;
    case internal::InstallerE2EVersionIndexFault::kReread:
      return VersionIndexFault::kReread;
    case internal::InstallerE2EVersionIndexFault::kValidation:
      return VersionIndexFault::kValidation;
  }
#endif
  return VersionIndexFault::kNone;
}

bool SameInstalledVersion(const InstalledVersion& left,
                          const InstalledVersion& right) {
  return left.metadata.version == right.metadata.version &&
         left.metadata.abi_hash == right.metadata.abi_hash &&
         left.metadata.platform == right.metadata.platform &&
         left.metadata.version_full == right.metadata.version_full &&
         left.path == right.path;
}

bool CanonicalLess(const InstalledVersion& left,
                   const InstalledVersion& right) {
  if (left.metadata.version != right.metadata.version) {
    return left.metadata.version > right.metadata.version;
  }
  return left.metadata.platform < right.metadata.platform;
}

}  // namespace

void SetVersionIndexFaultForTesting(VersionIndexFault fault) {
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  (void)fault;
#else
  g_version_index_fault = fault;
#endif
}

bool VersionMetadata::IsValid() const {
  return version.IsValid() && !abi_hash.empty() && !platform.empty();
}

MetadataError ReadVersionMetadata(const base::FilePath& version_dir,
                                  VersionMetadata* metadata) {
  if (!metadata) {
    return MetadataError::kMissingRequiredField;
  }

  base::FilePath metadata_path = version_dir.Append(kVersionMetadataFilename);

  if (!base::PathExists(metadata_path)) {
    return MetadataError::kFileNotFound;
  }

  std::string json_content;
  if (!base::ReadFileToString(metadata_path, &json_content)) {
    return MetadataError::kFileReadError;
  }

  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(json_content, base::JSON_PARSE_RFC);
  if (!parsed) {
    return MetadataError::kJsonParseError;
  }

  const base::DictValue& dict = *parsed;

  // Read version (required)
  const std::string* version_str = dict.FindString(kVersionField);
  if (!version_str || version_str->size() > kMaxVersionLength) {
    return MetadataError::kMissingRequiredField;
  }
  metadata->version = Version::Parse(*version_str);
  if (!metadata->version.IsValid()) {
    return MetadataError::kMissingRequiredField;
  }

  // Read abi_hash (required, hex-only)
  const std::string* abi_hash = dict.FindString(kAbiHashField);
  if (!abi_hash || !IsValidAbiHash(*abi_hash)) {
    return MetadataError::kMissingRequiredField;
  }
  metadata->abi_hash = *abi_hash;

  // Read platform (required)
  const std::string* platform = dict.FindString(kPlatformField);
  if (!platform || platform->empty() || platform->size() > kMaxPlatformLength) {
    return MetadataError::kMissingRequiredField;
  }
  metadata->platform = *platform;

  // Read version_full (optional)
  const std::string* version_full = dict.FindString(kVersionFullField);
  if (version_full && version_full->size() > kMaxVersionFullLength) {
    return MetadataError::kMissingRequiredField;
  }
  metadata->version_full = version_full ? *version_full : "";

  return MetadataError::kSuccess;
}

MetadataError WriteVersionMetadata(const base::FilePath& version_dir,
                                   const VersionMetadata& metadata) {
  if (!metadata.IsValid()) {
    return MetadataError::kMissingRequiredField;
  }

  // Ensure the directory exists
  if (!base::CreateDirectory(version_dir)) {
    return MetadataError::kFileWriteError;
  }

  base::DictValue dict;
  dict.Set(kVersionField, metadata.version.ToString());
  dict.Set(kAbiHashField, metadata.abi_hash);
  dict.Set(kPlatformField, metadata.platform);
  if (!metadata.version_full.empty()) {
    dict.Set(kVersionFullField, metadata.version_full);
  }

  std::string json_content;
  if (!base::JSONWriter::WriteWithOptions(
          dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_content)) {
    return MetadataError::kFileWriteError;
  }

  base::FilePath metadata_path = version_dir.Append(kVersionMetadataFilename);

  if (!base::WriteFile(metadata_path, json_content)) {
    return MetadataError::kFileWriteError;
  }

  return MetadataError::kSuccess;
}

std::vector<InstalledVersion> ScanInstalledVersionsWithMetadata(
    const base::FilePath& install_dir) {
  std::vector<InstalledVersion> result;

  // Scan <install_dir>\Versions\ for version directories
  base::FilePath versions_dir = install_dir.Append(kVersionsSubdirectory);
  if (!base::DirectoryExists(versions_dir)) {
    return result;
  }

  // Reject if the Versions/ directory itself is a reparse point
  // (symlink/junction) to prevent enumerating attacker-controlled content.
  if (IsReparsePoint(versions_dir)) {
    return result;
  }

  // Known platform subdirectory names
  static const char* kPlatforms[] = {"windows64", "windows32", "windowsarm64"};

  // Enumerate version directories (e.g., "137.3.5")
  base::FileEnumerator version_enumerator(versions_dir, false,
                                          base::FileEnumerator::DIRECTORIES);
  for (base::FilePath version_path = version_enumerator.Next();
       !version_path.empty(); version_path = version_enumerator.Next()) {
    std::string version_name = version_path.BaseName().AsUTF8Unsafe();
    Version version = Version::Parse(version_name);
    if (!version.IsValid()) {
      continue;
    }

    // Reject version directories that are reparse points (symlinks/junctions).
    if (IsReparsePoint(version_path)) {
      Logger::GetInstance().Warning(
          "Skipping reparse point version directory: " +
          version_path.AsUTF8Unsafe());
      continue;
    }

    // Check each platform subdirectory
    for (const char* platform : kPlatforms) {
      base::FilePath platform_path =
          version_path.Append(base::FilePath::FromUTF8Unsafe(platform));
      if (!base::DirectoryExists(platform_path)) {
        continue;
      }

      // Reject platform directories that are reparse points.
      if (IsReparsePoint(platform_path)) {
        Logger::GetInstance().Warning(
            "Skipping reparse point platform directory: " +
            platform_path.AsUTF8Unsafe());
        continue;
      }

      InstalledVersion installed;
      installed.path = platform_path;

      // Metadata is required — without it we don't know the sandbox hash.
      MetadataError meta_err =
          ReadVersionMetadata(platform_path, &installed.metadata);
      if (meta_err != MetadataError::kSuccess) {
        Logger::GetInstance().Warning(
            "Skipping " + platform_path.AsUTF8Unsafe() +
            ": metadata read failed: " + MetadataErrorToString(meta_err));
        continue;
      }

      // Verify metadata matches the directory structure. A mismatch
      // indicates a corrupted or tampered metadata file.
      if (installed.metadata.version != version ||
          installed.metadata.platform != platform) {
        Logger::GetInstance().Warning(
            "Skipping " + platform_path.AsUTF8Unsafe() +
            ": metadata mismatch (expected " + version.ToString() + "/" +
            platform + ", got " + installed.metadata.version.ToString() + "/" +
            installed.metadata.platform + ")");
        continue;
      }

      result.push_back(std::move(installed));
    }
  }

  // Sort newest-first by version (platform order within same version is stable)
  std::sort(result.begin(), result.end(),
            [](const InstalledVersion& a, const InstalledVersion& b) {
              return a.metadata.version > b.metadata.version;
            });

  return result;
}

BoundedInstalledVersionScanResult ScanInstalledVersionsWithMetadataBounded(
    const base::FilePath& install_dir,
    size_t max_version_entries,
    base::TimeTicks deadline) {
  BoundedInstalledVersionScanResult result;
  auto time_expired = [&]() {
    if (!deadline.is_null() && base::TimeTicks::Now() >= deadline) {
      result.time_limit_reached = true;
      return true;
    }
    return false;
  };

  if (time_expired()) {
    return result;
  }
  base::FilePath versions_dir = install_dir.Append(kVersionsSubdirectory);
  if (!base::DirectoryExists(versions_dir) || time_expired() ||
      IsReparsePoint(versions_dir)) {
    return result;
  }

  static const char* kPlatforms[] = {"windows64", "windows32", "windowsarm64"};
  base::FileEnumerator version_enumerator(versions_dir, false,
                                          base::FileEnumerator::DIRECTORIES);
  while (!time_expired()) {
    if (result.entries_visited >= max_version_entries) {
      result.entry_limit_reached = true;
      break;
    }

    base::FilePath version_path = version_enumerator.Next();
    if (version_path.empty()) {
      break;
    }
    ++result.entries_visited;

    Version version = Version::Parse(version_path.BaseName().AsUTF8Unsafe());
    if (!version.IsValid() || time_expired()) {
      continue;
    }
    if (IsReparsePoint(version_path)) {
      Logger::GetInstance().Warning(
          "Skipping reparse point version directory: " +
          version_path.AsUTF8Unsafe());
      continue;
    }

    for (const char* platform : kPlatforms) {
      if (time_expired()) {
        break;
      }
      base::FilePath platform_path =
          version_path.Append(base::FilePath::FromUTF8Unsafe(platform));
      if (!base::DirectoryExists(platform_path) || time_expired()) {
        continue;
      }
      if (IsReparsePoint(platform_path)) {
        Logger::GetInstance().Warning(
            "Skipping reparse point platform directory: " +
            platform_path.AsUTF8Unsafe());
        continue;
      }
      if (time_expired()) {
        break;
      }

      InstalledVersion installed;
      installed.path = platform_path;
      MetadataError meta_err =
          ReadVersionMetadata(platform_path, &installed.metadata);
      if (time_expired()) {
        break;
      }
      if (meta_err != MetadataError::kSuccess) {
        Logger::GetInstance().Warning(
            "Skipping " + platform_path.AsUTF8Unsafe() +
            ": metadata read failed: " + MetadataErrorToString(meta_err));
        continue;
      }
      if (installed.metadata.version != version ||
          installed.metadata.platform != platform) {
        Logger::GetInstance().Warning(
            "Skipping " + platform_path.AsUTF8Unsafe() +
            ": metadata does not match its recovery path");
        continue;
      }
      if (platform_path != GetVersionPath(install_dir,
                                          installed.metadata.version,
                                          installed.metadata.platform) ||
          ValidateDistribution(install_dir, platform_path, installed.metadata,
                               deadline, &result.time_limit_reached) !=
              DistributionValidation::kComplete) {
        Logger::GetInstance().Warning(
            "Skipping invalid emergency startup recovery candidate: " +
            platform_path.AsUTF8Unsafe());
        if (result.time_limit_reached) {
          break;
        }
        continue;
      }
      result.versions.push_back(std::move(installed));
    }
  }

  std::sort(result.versions.begin(), result.versions.end(),
            [](const InstalledVersion& a, const InstalledVersion& b) {
              return a.metadata.version > b.metadata.version;
            });
  return result;
}

DistributionValidation ValidateDistribution(const base::FilePath& trusted_root,
                                            const base::FilePath& version_dir,
                                            const VersionMetadata& expected,
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
    return DistributionValidation::kIoError;
  }
  if (trusted_root.empty() || version_dir.empty() || !expected.IsValid() ||
      !trusted_root.IsAbsolute() || !version_dir.IsAbsolute() ||
      !trusted_root.IsParent(version_dir)) {
    return DistributionValidation::kUnsafe;
  }
  if (!base::DirectoryExists(version_dir) || time_expired()) {
    return DistributionValidation::kInvalid;
  }
  if (IsReparsePoint(version_dir) || time_expired()) {
    return DistributionValidation::kUnsafe;
  }

  VersionMetadata actual;
  MetadataError error = ReadVersionMetadata(version_dir, &actual);
  if (time_expired()) {
    return DistributionValidation::kIoError;
  }
  if (error == MetadataError::kFileReadError) {
    return DistributionValidation::kIoError;
  }
  if (error != MetadataError::kSuccess || actual.version != expected.version ||
      actual.platform != expected.platform ||
      actual.abi_hash != expected.abi_hash) {
    return DistributionValidation::kInvalid;
  }

  base::FilePath catalog = version_dir.Append(kCatalogFilename);
  base::FilePath libcef = GetLibcefPath(version_dir);
  if (!base::PathExists(catalog) || time_expired() ||
      !base::PathExists(libcef) || time_expired()) {
    return DistributionValidation::kInvalid;
  }
  if (!IsPathSafeForLoading(trusted_root, catalog, deadline,
                            time_limit_reached) ||
      !IsPathSafeForLoading(trusted_root, libcef, deadline,
                            time_limit_reached)) {
    return DistributionValidation::kUnsafe;
  }
  return DistributionValidation::kComplete;
}

MetadataError WriteVersionIndex(const base::FilePath& install_dir,
                                const std::vector<InstalledVersion>& versions) {
  VersionIndexFault fault = EffectiveVersionIndexFault();
  std::vector<InstalledVersion> canonical = versions;
  std::set<std::pair<Version, std::string>> seen;
  for (const auto& iv : canonical) {
    if (!iv.metadata.IsValid() ||
        iv.path != GetVersionPath(install_dir, iv.metadata.version,
                                  iv.metadata.platform) ||
        !seen.insert({iv.metadata.version, iv.metadata.platform}).second) {
      return MetadataError::kIndexValidationError;
    }
  }
  std::sort(canonical.begin(), canonical.end(), CanonicalLess);

  // Build JSON array of installed versions.
  base::ListValue versions_list;
  for (const auto& iv : canonical) {
    base::DictValue entry;
    entry.Set(kVersionField, iv.metadata.version.ToString());
    entry.Set(kAbiHashField, iv.metadata.abi_hash);
    entry.Set(kPlatformField, iv.metadata.platform);
    if (!iv.metadata.version_full.empty()) {
      entry.Set(kVersionFullField, iv.metadata.version_full);
    }

    // Store path relative to install_dir.
    base::FilePath relative_path;
    if (!install_dir.AppendRelativePath(iv.path, &relative_path)) {
      return MetadataError::kIndexValidationError;
    }
    entry.Set(kIndexPathField, relative_path.AsUTF8Unsafe());

    versions_list.Append(std::move(entry));
  }

  base::DictValue root;
  root.Set(kIndexVersionsField, std::move(versions_list));

  std::string json_content;
  if (!base::JSONWriter::WriteWithOptions(
          root, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_content)) {
    return MetadataError::kFileWriteError;
  }

  // Atomic write with CRC32 integrity footer: write to temp, rename.
  base::FilePath index_path = install_dir.Append(kVersionIndexFilename);
  base::FilePath temp_path;
  if (fault == VersionIndexFault::kWrite) {
    return MetadataError::kFileWriteError;
  }
  if (!base::CreateTemporaryFileInDir(install_dir, &temp_path)) {
    return MetadataError::kFileWriteError;
  }

  if (!WriteFileWithIntegrity(temp_path, json_content)) {
    base::DeleteFile(temp_path);
    return MetadataError::kFileWriteError;
  }

  if (fault == VersionIndexFault::kReplace ||
      !base::ReplaceFile(temp_path, index_path, nullptr)) {
    base::DeleteFile(temp_path);
    return MetadataError::kFileWriteError;
  }

  if (fault == VersionIndexFault::kReread) {
    return MetadataError::kFileReadError;
  }

  std::vector<InstalledVersion> reread;
  MetadataError read_error = ReadVersionIndex(install_dir, &reread);
  if (read_error != MetadataError::kSuccess) {
    return read_error;
  }
  std::sort(reread.begin(), reread.end(), CanonicalLess);
  if (fault == VersionIndexFault::kValidation ||
      reread.size() != canonical.size()) {
    return MetadataError::kIndexValidationError;
  }
  for (size_t i = 0; i < canonical.size(); ++i) {
    if (!SameInstalledVersion(reread[i], canonical[i])) {
      return MetadataError::kIndexValidationError;
    }
  }
  return MetadataError::kSuccess;
}

MetadataError ReadVersionIndex(const base::FilePath& install_dir,
                               std::vector<InstalledVersion>* versions,
                               VersionIndexReadMode read_mode) {
  if (!versions) {
    return MetadataError::kMissingRequiredField;
  }
  versions->clear();

  base::FilePath index_path = install_dir.Append(kVersionIndexFilename);

  std::string json_content;
  IntegrityResult ir =
      ReadFileWithIntegrity(index_path, &json_content,
                            read_mode == VersionIndexReadMode::kDoomCorrupt
                                ? IntegrityMismatchAction::kDoom
                                : IntegrityMismatchAction::kPreserve);
  switch (ir) {
    case IntegrityResult::kFileNotFound:
      return MetadataError::kFileNotFound;
    case IntegrityResult::kReadError:
      return MetadataError::kFileReadError;
    case IntegrityResult::kIntegrityMismatch:
      return MetadataError::kIntegrityMismatch;
    case IntegrityResult::kSuccess:
    case IntegrityResult::kSuccessNoFooter:
      break;
  }

  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(json_content, base::JSON_PARSE_RFC);
  if (!parsed) {
    return MetadataError::kJsonParseError;
  }

  const base::ListValue* versions_list = parsed->FindList(kIndexVersionsField);
  if (!versions_list) {
    return MetadataError::kMissingRequiredField;
  }

  std::set<std::pair<Version, std::string>> seen;
  for (const base::Value& item : *versions_list) {
    const base::DictValue* dict = item.GetIfDict();
    if (!dict) {
      return MetadataError::kIndexValidationError;
    }

    InstalledVersion iv;

    const std::string* version_str = dict->FindString(kVersionField);
    if (!version_str || version_str->size() > kMaxVersionLength) {
      return MetadataError::kIndexValidationError;
    }
    iv.metadata.version = Version::Parse(*version_str);
    if (!iv.metadata.version.IsValid()) {
      return MetadataError::kIndexValidationError;
    }

    const std::string* abi_hash = dict->FindString(kAbiHashField);
    if (!abi_hash || !IsValidAbiHash(*abi_hash)) {
      return MetadataError::kIndexValidationError;
    }
    iv.metadata.abi_hash = *abi_hash;

    const std::string* platform = dict->FindString(kPlatformField);
    if (!platform || platform->empty() ||
        platform->size() > kMaxPlatformLength) {
      return MetadataError::kIndexValidationError;
    }
    iv.metadata.platform = *platform;

    const std::string* version_full = dict->FindString(kVersionFullField);
    if (version_full && version_full->size() <= kMaxVersionFullLength) {
      iv.metadata.version_full = *version_full;
    }

    const std::string* path_str = dict->FindString(kIndexPathField);
    if (!path_str || path_str->empty()) {
      return MetadataError::kIndexValidationError;
    }
    base::FilePath relative = base::FilePath::FromUTF8Unsafe(*path_str);
    if (relative.IsAbsolute() || relative.ReferencesParent()) {
      return MetadataError::kIndexValidationError;
    }
    iv.path = install_dir.Append(relative);
    if (iv.path != GetVersionPath(install_dir, iv.metadata.version,
                                  iv.metadata.platform) ||
        !seen.insert({iv.metadata.version, iv.metadata.platform}).second) {
      return MetadataError::kIndexValidationError;
    }

    versions->push_back(std::move(iv));
  }

  return MetadataError::kSuccess;
}

std::string ExtractVersionFullFromFilename(const std::string& filename) {
  static constexpr std::string_view kPrefix = "cef_binary_";
  if (!filename.starts_with(kPrefix)) {
    return "";
  }

  std::string remainder = filename.substr(kPrefix.size());

  // The version_full is everything up to the first '_' in the remainder.
  // Full versions use '+' as internal separator (e.g.,
  // "145.0.28+g51162e8+chromium-145.0.7632.160"), never '_'.
  auto pos = remainder.find('_');
  if (pos == std::string::npos || pos == 0) {
    return "";
  }

  return remainder.substr(0, pos);
}

const char* MetadataErrorToString(MetadataError error) {
  switch (error) {
    case MetadataError::kSuccess:
      return "Success";
    case MetadataError::kFileNotFound:
      return "Metadata file not found";
    case MetadataError::kFileReadError:
      return "Could not read metadata file";
    case MetadataError::kJsonParseError:
      return "Invalid JSON format";
    case MetadataError::kMissingRequiredField:
      return "Required field missing";
    case MetadataError::kFileWriteError:
      return "Could not write metadata file";
    case MetadataError::kIndexValidationError:
      return "Version index validation failed";
    case MetadataError::kIntegrityMismatch:
      return "Integrity mismatch";
  }
  return "Unknown error";
}

}  // namespace cef_installer
