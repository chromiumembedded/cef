// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_cdn_manifest.h"

#include <algorithm>
#include <set>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "cef/libcef_dll/bootstrap/installer/installer_validation.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"

namespace cef_installer {

namespace {

// Validate that a filename is safe for URL construction.
// Prevents path traversal and URL redirection attacks.
bool IsValidArchiveFilename(const std::string& filename) {
  if (filename.empty() || filename.size() > kMaxFilenameLength) {
    return false;
  }

  // Must not contain path separators or URL indicators
  if (filename.find('/') != std::string::npos ||
      filename.find('\\') != std::string::npos ||
      filename.find(':') != std::string::npos ||
      filename.find("..") != std::string::npos) {
    return false;
  }

  // Must not start with a dot (hidden files)
  if (filename[0] == '.') {
    return false;
  }

  // Must look like a valid archive filename.
  // Only .tar.xz is supported by the extraction code (ExtractTarXz).
  return base::EndsWith(filename, ".tar.xz",
                        base::CompareCase::INSENSITIVE_ASCII);
}

const char* CdnBuildExclusionReasonToString(CdnBuildExclusionReason reason) {
  switch (reason) {
    case CdnBuildExclusionReason::kAlreadyInstalled:
      return "already installed";
    case CdnBuildExclusionReason::kRevoked:
      return "revoked";
    case CdnBuildExclusionReason::kLaunchDisqualified:
      return "disqualified by launch health";
    case CdnBuildExclusionReason::kPriorDownloadFailure:
      return "prior download failure";
  }
  return "unknown exclusion";
}

// Helper to parse a build entry from a JSON dict
// Includes field length validation to prevent memory issues from malformed
// data.
ManifestError ParseBuildEntry(const base::DictValue& dict,
                              CdnBuildEntry* entry) {
  const std::string* version = dict.FindString("version");
  if (!version || version->empty()) {
    return ManifestError::kMissingRequiredField;
  }
  if (version->size() > kMaxVersionLength) {
    return ManifestError::kInvalidFieldValue;
  }
  entry->version = Version::Parse(*version);
  if (!entry->version.IsValid()) {
    return ManifestError::kInvalidFieldValue;
  }

  const std::string* file = dict.FindString("file");
  if (!file || file->empty()) {
    return ManifestError::kMissingRequiredField;
  }
  if (!IsValidArchiveFilename(*file)) {
    return ManifestError::kInvalidFieldValue;
  }
  entry->file = *file;

  const std::string* sha1 = dict.FindString("sha1");
  if (!sha1 || sha1->empty()) {
    return ManifestError::kMissingRequiredField;
  }
  if (sha1->size() > kMaxSha1Length) {
    return ManifestError::kInvalidFieldValue;
  }
  entry->sha1 = *sha1;

  // last_modified is optional but expected
  const std::string* last_modified = dict.FindString("last_modified");
  if (last_modified && last_modified->size() > kMaxTimestampLength) {
    return ManifestError::kInvalidFieldValue;
  }
  entry->last_modified = last_modified ? *last_modified : "";

  // abi_hash is optional (may not be present on all platforms).
  // Must be hex-only to prevent URL metacharacter injection in BuildAbiHashUrl.
  const std::string* abi_hash = dict.FindString("abi_hash");
  if (abi_hash && !abi_hash->empty()) {
    if (!IsValidAbiHash(*abi_hash)) {
      return ManifestError::kInvalidFieldValue;
    }
    entry->abi_hash = *abi_hash;
  } else {
    entry->abi_hash.clear();
  }

  return ManifestError::kSuccess;
}

// Ensure URL ends with a slash
std::string NormalizeBaseUrl(const std::string& base_url) {
  if (base_url.empty() || base_url.back() == '/') {
    return base_url;
  }
  return base_url + "/";
}

}  // namespace

ManifestError ParseStableMilestone(const std::string& content, int* milestone) {
  if (!milestone) {
    return ManifestError::kMissingRequiredField;
  }

  // Trim whitespace
  std::string trimmed = content;
  base::TrimWhitespaceASCII(trimmed, base::TRIM_ALL, &trimmed);

  if (trimmed.empty()) {
    return ManifestError::kMissingRequiredField;
  }

  int value = 0;
  if (!base::StringToInt(trimmed, &value)) {
    return ManifestError::kInvalidFieldValue;
  }

  if (value <= 0) {
    return ManifestError::kInvalidFieldValue;
  }

  *milestone = value;
  return ManifestError::kSuccess;
}

ManifestError ParseMilestoneManifest(
    const std::string& json,
    std::map<std::string, CdnBuildEntry>* entries) {
  if (!entries) {
    return ManifestError::kMissingRequiredField;
  }

  entries->clear();

  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!parsed) {
    return ManifestError::kJsonParseError;
  }

  // Each key is a platform name, value is a build entry
  for (const auto [platform, value] : *parsed) {
    const base::DictValue* entry_dict = value.GetIfDict();
    if (!entry_dict) {
      return ManifestError::kJsonParseError;
    }

    CdnBuildEntry entry;
    ManifestError entry_err = ParseBuildEntry(*entry_dict, &entry);
    if (entry_err != ManifestError::kSuccess) {
      return entry_err;
    }

    (*entries)[platform] = std::move(entry);
  }

  return ManifestError::kSuccess;
}

ManifestError ParsePlatformManifest(const std::string& json,
                                    std::vector<CdnBuildEntry>* entries) {
  if (!entries) {
    return ManifestError::kMissingRequiredField;
  }

  entries->clear();

  std::optional<base::ListValue> parsed =
      base::JSONReader::ReadList(json, base::JSON_PARSE_RFC);
  if (!parsed) {
    return ManifestError::kJsonParseError;
  }

  for (const base::Value& item : *parsed) {
    const base::DictValue* entry_dict = item.GetIfDict();
    if (!entry_dict) {
      return ManifestError::kJsonParseError;
    }

    CdnBuildEntry entry;
    ManifestError entry_err = ParseBuildEntry(*entry_dict, &entry);
    if (entry_err != ManifestError::kSuccess) {
      return entry_err;
    }

    entries->push_back(std::move(entry));
  }

  return ManifestError::kSuccess;
}

namespace {

// Returns the channel suffix for URLs (empty for stable, "_beta" for beta)
std::string GetChannelSuffix(const std::string& channel) {
  if (!channel.empty()) {
    return "_" + channel;
  }
  return "";  // empty = stable, no suffix
}

// Returns the channel name for txt files ("stable" if empty, otherwise channel)
std::string GetChannelName(const std::string& channel) {
  return channel.empty() ? std::string(kChannelStable) : channel;
}

}  // namespace

std::string BuildChannelUrl(const std::string& base_url,
                            const std::string& channel) {
  // stable.txt or beta.txt
  return NormalizeBaseUrl(base_url) + GetChannelName(channel) + ".txt";
}

std::string BuildStableUrl(const std::string& base_url) {
  return BuildChannelUrl(base_url, "");
}

std::string BuildMilestoneUrl(const std::string& base_url,
                              int milestone,
                              const std::string& channel) {
  // stable: 137.json, beta: 137_beta.json
  return NormalizeBaseUrl(base_url) + base::NumberToString(milestone) +
         GetChannelSuffix(channel) + ".json";
}

std::string BuildPlatformUrl(const std::string& base_url,
                             int milestone,
                             const std::string& platform,
                             const std::string& channel) {
  // stable: 137_windows64.json, beta: 137_windows64_beta.json
  return NormalizeBaseUrl(base_url) + base::NumberToString(milestone) + "_" +
         platform + GetChannelSuffix(channel) + ".json";
}

std::string BuildAbiHashUrl(const std::string& base_url,
                            const std::string& abi_hash,
                            const std::string& platform,
                            const std::string& channel) {
  // stable: abc123_windows64.json, beta: abc123_windows64_beta.json
  return NormalizeBaseUrl(base_url) + abi_hash + "_" + platform +
         GetChannelSuffix(channel) + ".json";
}

std::string BuildArchiveUrl(const std::string& base_url,
                            const std::string& filename) {
  return NormalizeBaseUrl(base_url) + filename;
}

std::string BuildHashFileUrl(const std::string& base_url,
                             const std::string& filename) {
  return NormalizeBaseUrl(base_url) + filename + ".sha256";
}

std::string BuildRevocationListUrl(const std::string& base_url) {
  return NormalizeBaseUrl(base_url) + std::string(kRevocationListPath);
}

std::optional<CdnBuildEntry> FindBestBuildEntry(
    const std::vector<CdnBuildEntry>& entries,
    const std::string& vmin,
    const std::string& vmax,
    const std::string& abi_hash,
    const std::set<Version>& skip_versions) {
  std::optional<CdnBuildEntry> best;
  Version best_version;

  for (const auto& entry : entries) {
    // Skip versions already installed locally.
    if (skip_versions.count(entry.version) > 0) {
      continue;
    }

    // Filter by abi_hash if specified
    if (!abi_hash.empty() && entry.abi_hash != abi_hash) {
      continue;
    }

    // Filter by version range
    if (!entry.version.IsInRange(vmin, vmax)) {
      continue;
    }

    // Select the newest matching version
    if (!best || entry.version > best_version) {
      best = entry;
      best_version = entry.version;
    }
  }

  return best;
}

std::string BuildNoMatchingCdnVersionMessage(
    const std::vector<CdnBuildEntry>& entries,
    const std::string& platform,
    const std::string& vmin,
    const std::string& vmax,
    const std::string& abi_hash,
    const CdnBuildExclusionReasons& exclusion_reasons) {
  std::string message =
      "No CDN version matches requirements (platform = " + platform +
      ", version >= " + vmin;
  if (!vmax.empty()) {
    message += ", version <= " + vmax;
  }
  if (!abi_hash.empty()) {
    message += ", ABI hash = " + abi_hash;
  }
  message += ")";

  if (entries.empty()) {
    return message + "; no validated CDN manifest candidates were available";
  }

  constexpr size_t kMaxDiagnosticCandidates = 5;
  const size_t candidate_count =
      std::min(entries.size(), kMaxDiagnosticCandidates);
  const Version min_version = Version::Parse(vmin);
  const Version max_version = Version::Parse(vmax);
  message += "; candidate failures: ";
  for (size_t i = 0; i < candidate_count; ++i) {
    if (i > 0) {
      message += "; ";
    }
    const CdnBuildEntry& entry = entries[i];
    message += entry.version.ToString() + " [";
    const auto exclusion = exclusion_reasons.find(entry.version);
    if (exclusion != exclusion_reasons.end()) {
      message += "excluded: ";
      size_t reason_index = 0;
      for (CdnBuildExclusionReason reason : exclusion->second) {
        if (reason_index++ > 0) {
          message += ", ";
        }
        message += CdnBuildExclusionReasonToString(reason);
      }
      if (exclusion->second.empty()) {
        message += "unknown reason";
      }
    } else {
      bool has_failure = false;
      auto AppendFailure = [&](const std::string& failure) {
        if (has_failure) {
          message += ", ";
        }
        message += failure;
        has_failure = true;
      };
      if (!abi_hash.empty() && entry.abi_hash != abi_hash) {
        AppendFailure(
            "ABI hash mismatch (got " +
            (entry.abi_hash.empty() ? std::string("<empty>") : entry.abi_hash) +
            ")");
      }
      if (!entry.version.IsInRange(vmin, vmax)) {
        if (min_version.IsValid() && entry.version < min_version) {
          AppendFailure("version below vmin");
        } else if (max_version.IsValid() && entry.version > max_version) {
          AppendFailure("version above vmax");
        } else {
          AppendFailure("version outside required range");
        }
      }
      if (!has_failure) {
        message += "matched compatibility requirements but was excluded";
      }
    }
    message += "]";
  }
  if (entries.size() > candidate_count) {
    message += "; ... " +
               base::NumberToString(entries.size() - candidate_count) +
               " more candidate(s)";
  }
  return message;
}

const char* ManifestErrorToString(ManifestError error) {
  switch (error) {
    case ManifestError::kSuccess:
      return "Success";
    case ManifestError::kJsonParseError:
      return "JSON parse error";
    case ManifestError::kMissingRequiredField:
      return "Missing required field";
    case ManifestError::kInvalidFieldValue:
      return "Invalid field value";
  }
  return "Unknown error";
}

}  // namespace cef_installer
