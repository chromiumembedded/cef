// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_revocation.h"

#include <windows.h>

#include <algorithm>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"

namespace cef_installer {

namespace {

// JSON field names for revocation list format.
constexpr char kRevokedVersionsField[] = "revoked_versions";
constexpr char kVersionField[] = "version";
constexpr char kVersionMinField[] = "version_min";
constexpr char kVersionMaxField[] = "version_max";
constexpr char kReasonField[] = "reason";
constexpr char kRevokedAtField[] = "revoked_at";

}  // namespace

RevocationError ParseRevocationList(const std::string& json,
                                    std::vector<RevokedVersionRange>* revoked) {
  if (!revoked) {
    return RevocationError::kJsonParseError;
  }

  revoked->clear();

  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!parsed) {
    return RevocationError::kJsonParseError;
  }

  const base::ListValue* versions_list =
      parsed->FindList(kRevokedVersionsField);
  if (!versions_list) {
    // Empty revocation list is valid - just return success with empty vector
    return RevocationError::kSuccess;
  }

  for (const base::Value& entry : *versions_list) {
    const base::DictValue* entry_dict = entry.GetIfDict();
    if (!entry_dict) {
      return RevocationError::kJsonParseError;
    }

    RevokedVersionRange rv;

    // Support both single-version ("version") and range
    // ("version_min"/"version_max") formats.
    const std::string* version_min = entry_dict->FindString(kVersionMinField);
    const std::string* version_max = entry_dict->FindString(kVersionMaxField);
    const std::string* version = entry_dict->FindString(kVersionField);

    if (version_min && version_max) {
      // Range format.
      rv.version_min = Version::Parse(*version_min);
      rv.version_max = Version::Parse(*version_max);
    } else if (version) {
      // Single-version format (point revocation: min == max).
      rv.version_min = Version::Parse(*version);
      rv.version_max = rv.version_min;
    } else {
      return RevocationError::kJsonParseError;
    }

    if (!rv.version_min.IsValid() || !rv.version_max.IsValid()) {
      return RevocationError::kJsonParseError;
    }

    // version_min must not exceed version_max.
    if (rv.version_min > rv.version_max) {
      return RevocationError::kJsonParseError;
    }

    // Reason and revoked_at are optional but length-bounded to prevent
    // memory exhaustion from tampered disk cache files.
    const std::string* reason = entry_dict->FindString(kReasonField);
    if (reason) {
      if (reason->size() > kMaxReasonLength) {
        return RevocationError::kJsonParseError;
      }
      rv.reason = *reason;
    }

    const std::string* revoked_at = entry_dict->FindString(kRevokedAtField);
    if (revoked_at) {
      if (revoked_at->size() > kMaxTimestampLength) {
        return RevocationError::kJsonParseError;
      }
      rv.revoked_at = *revoked_at;
    }

    revoked->push_back(std::move(rv));

    // Enforce entry count limit to prevent CPU exhaustion during parsing
    // and merge operations with large tampered cache files.
    if (revoked->size() > kMaxRevocationEntryCount) {
      revoked->clear();
      return RevocationError::kJsonParseError;
    }
  }

  return RevocationError::kSuccess;
}

bool IsVersionRevoked(const Version& version,
                      const std::vector<RevokedVersionRange>& revoked_list) {
  if (!version.IsValid()) {
    return false;
  }

  for (const auto& range : revoked_list) {
    if (version >= range.version_min && version <= range.version_max) {
      return true;
    }
  }
  return false;
}

std::vector<Version> FilterRevokedVersions(
    const std::vector<Version>& versions,
    const std::vector<RevokedVersionRange>& revoked_list) {
  std::vector<Version> result;
  result.reserve(versions.size());

  for (const auto& v : versions) {
    if (!IsVersionRevoked(v, revoked_list)) {
      result.push_back(v);
    }
  }

  return result;
}

const char* RevocationErrorToString(RevocationError error) {
  switch (error) {
    case RevocationError::kSuccess:
      return "Success";
    case RevocationError::kJsonParseError:
      return "JSON parse error";
    case RevocationError::kWriteError:
      return "Write error";
  }
  return "Unknown error";
}

std::vector<RevokedVersionRange> LoadCompiledRevocationList() {
  static const base::NoDestructor<std::vector<RevokedVersionRange>> compiled(
      []() -> std::vector<RevokedVersionRange> {
        HMODULE module = nullptr;
        HRSRC resource =
            FindResourceW(module, kRevocationResourceName, RT_RCDATA);
        if (!resource) {
          return {};
        }

        HGLOBAL loaded = LoadResource(module, resource);
        if (!loaded) {
          return {};
        }

        const void* data = LockResource(loaded);
        DWORD size = SizeofResource(module, resource);
        if (!data || size == 0) {
          return {};
        }

        std::string json(static_cast<const char*>(data), size);
        std::vector<RevokedVersionRange> revoked;
        if (ParseRevocationList(json, &revoked) != RevocationError::kSuccess) {
          return {};
        }
        return revoked;
      }());
  return *compiled;
}

std::vector<RevokedVersionRange> MergeRevocationLists(
    const std::vector<RevokedVersionRange>& compiled,
    const std::vector<RevokedVersionRange>& additional) {
  // 1. Combine both lists.
  std::vector<RevokedVersionRange> all;
  all.reserve(compiled.size() + additional.size());
  all.insert(all.end(), compiled.begin(), compiled.end());
  all.insert(all.end(), additional.begin(), additional.end());

  if (all.empty()) {
    return {};
  }

  // 2. Sort by version_min (ascending).
  std::sort(all.begin(), all.end(),
            [](const RevokedVersionRange& a, const RevokedVersionRange& b) {
              return a.version_min < b.version_min;
            });

  // 3. Merge overlapping ranges.
  std::vector<RevokedVersionRange> result;
  result.push_back(std::move(all[0]));

  for (size_t i = 1; i < all.size(); ++i) {
    auto& prev = result.back();

    // Overlapping or touching: curr.min <= prev.max.
    if (all[i].version_min <= prev.version_max) {
      // Extend if curr reaches further.
      if (all[i].version_max > prev.version_max) {
        prev.version_max = all[i].version_max;
      }
      // Concatenate non-empty reasons.
      if (!all[i].reason.empty()) {
        if (!prev.reason.empty()) {
          prev.reason += "; ";
        }
        prev.reason += all[i].reason;
      }
    } else {
      result.push_back(std::move(all[i]));
    }
  }

  return result;
}

RevocationError WriteRevocationCache(
    const base::FilePath& dir,
    const std::vector<RevokedVersionRange>& cdn_fetched) {
  base::ListValue versions_list;
  for (const auto& rv : cdn_fetched) {
    base::DictValue entry;
    entry.Set(kVersionMinField, rv.version_min.ToString());
    entry.Set(kVersionMaxField, rv.version_max.ToString());
    entry.Set(kReasonField, rv.reason);
    entry.Set(kRevokedAtField, rv.revoked_at);
    versions_list.Append(std::move(entry));
  }

  base::DictValue root;
  root.Set(kRevokedVersionsField, std::move(versions_list));

  std::string json;
  if (!base::JSONWriter::Write(root, &json)) {
    return RevocationError::kWriteError;
  }

  base::FilePath path = dir.Append(kRevocationCacheFilename);

  // Reject if the directory or the target file is a reparse point
  // (symlink/junction). The filename is fixed and predictable, so an attacker
  // could pre-create a symlink at this path to redirect writes.
  if (IsReparsePoint(dir) || IsReparsePoint(path)) {
    return RevocationError::kWriteError;
  }

  if (!WriteFileWithIntegrity(path, json)) {
    return RevocationError::kWriteError;
  }

  return RevocationError::kSuccess;
}

std::vector<RevokedVersionRange> LoadRevocationCache(
    const base::FilePath& dir,
    IntegrityMismatchAction mismatch_action) {
  base::FilePath path = dir.Append(kRevocationCacheFilename);

  // Reject if the directory or the target file is a reparse point
  // (symlink/junction) to prevent reading attacker-controlled content.
  // This matches the write path (WriteRevocationCache) which checks both.
  if (IsReparsePoint(dir) || IsReparsePoint(path)) {
    return {};
  }

  // Reject oversized files to prevent memory exhaustion from tampered cache.
  std::optional<int64_t> file_size = base::GetFileSize(path);
  if (!file_size.has_value() || *file_size > kMaxRevocationCacheFileSize) {
    return {};
  }

  std::string content;
  IntegrityResult ir = ReadFileWithIntegrity(path, &content, mismatch_action);
  if (ir != IntegrityResult::kSuccess &&
      ir != IntegrityResult::kSuccessNoFooter) {
    return {};
  }

  std::vector<RevokedVersionRange> revoked;
  if (ParseRevocationList(content, &revoked) != RevocationError::kSuccess) {
    return {};
  }
  return revoked;
}

std::vector<RevokedVersionRange> LoadEffectiveRevocationList(
    const std::vector<base::FilePath>& read_dirs,
    IntegrityMismatchAction mismatch_action) {
  std::vector<RevokedVersionRange> effective = LoadCompiledRevocationList();

  for (const auto& dir : read_dirs) {
    std::vector<RevokedVersionRange> cached =
        LoadRevocationCache(dir, mismatch_action);
    if (!cached.empty()) {
      effective = MergeRevocationLists(effective, cached);
    }
  }

  return effective;
}

bool IsRevocationCacheFresh(const base::FilePath& dir, base::Time now) {
  base::FilePath path = dir.Append(kRevocationCacheFilename);
  if (IsReparsePoint(dir) || IsReparsePoint(path)) {
    return false;
  }
  base::File::Info info;
  if (!base::GetFileInfo(path, &info) || info.last_modified > now) {
    return false;
  }
  if (now - info.last_modified >=
      base::Seconds(kRevocationCacheValiditySeconds)) {
    return false;
  }

  std::optional<int64_t> file_size = base::GetFileSize(path);
  if (!file_size.has_value() || *file_size > kMaxRevocationCacheFileSize) {
    return false;
  }

  std::string content;
  if (ReadFileWithIntegrity(path, &content) != IntegrityResult::kSuccess) {
    return false;
  }

  std::vector<RevokedVersionRange> revoked;
  return ParseRevocationList(content, &revoked) == RevocationError::kSuccess;
}

bool IsRevocationRefreshBackedOff(const base::FilePath& dir,
                                  const std::string& source,
                                  base::Time now) {
  base::FilePath path = dir.Append(kRevocationBackoffFilename);
  if (IsReparsePoint(dir) || IsReparsePoint(path)) {
    return false;
  }
  std::optional<int64_t> size = base::GetFileSize(path);
  if (!size || *size > kMaxRevocationBackoffFileSize) {
    return false;
  }
  std::string content;
  if (ReadFileWithIntegrity(path, &content) != IntegrityResult::kSuccess) {
    return false;
  }
  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(content, base::JSON_PARSE_RFC);
  if (!parsed) {
    return false;
  }
  const std::string* stored_source = parsed->FindString("source");
  const std::string* attempt_string = parsed->FindString("last_attempt_us");
  int64_t attempt_us = 0;
  if (!stored_source || *stored_source != source || !attempt_string ||
      !base::StringToInt64(*attempt_string, &attempt_us)) {
    return false;
  }
  base::Time attempt =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(attempt_us));
  base::TimeDelta age = now - attempt;
  if (age < base::TimeDelta()) {
    return -age <= base::Seconds(kRevocationFailureBackoffSeconds);
  }
  return age < base::Seconds(kRevocationFailureBackoffSeconds);
}

bool RecordRevocationRefreshFailure(const base::FilePath& dir,
                                    const std::string& source,
                                    base::Time now) {
  if (source.empty() || source.size() > 2048 || IsReparsePoint(dir) ||
      !base::CreateDirectory(dir)) {
    return false;
  }
  base::FilePath path = dir.Append(kRevocationBackoffFilename);
  if (!VerifySafeFilePath(path)) {
    return false;
  }
  base::DictValue dict;
  dict.Set("source", source);
  dict.Set(
      "last_attempt_us",
      base::NumberToString(now.ToDeltaSinceWindowsEpoch().InMicroseconds()));
  std::string json;
  return base::JSONWriter::Write(dict, &json) &&
         WriteFileWithIntegrity(path, json);
}

}  // namespace cef_installer
