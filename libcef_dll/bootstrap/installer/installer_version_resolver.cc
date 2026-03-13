// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_version_resolver.h"

#include <algorithm>

namespace cef_installer {

namespace {

VersionKey MakeVersionKey(const InstalledVersion& candidate) {
  return {candidate.metadata.version, candidate.metadata.platform};
}

bool IsBetter(const InstalledVersion& candidate,
              CandidateSource candidate_source,
              const std::optional<InstalledVersion>& current,
              CandidateSource current_source) {
  if (!current) {
    return true;
  }
  if (candidate.metadata.version != current->metadata.version) {
    return candidate.metadata.version > current->metadata.version;
  }
  // Installed content wins a tie with bundled content.
  return candidate_source == CandidateSource::kInstalled &&
         current_source == CandidateSource::kBundled;
}

std::optional<CandidateRejection> CheckCandidate(
    const Config& config,
    const std::string& platform,
    const InstalledVersion& candidate,
    const std::vector<RevokedVersionRange>& revoked_versions) {
  if (!candidate.metadata.IsValid()) {
    return CandidateRejection::kInvalidMetadata;
  }
  if (candidate.metadata.platform != platform) {
    return CandidateRejection::kWrongPlatform;
  }
  if (!config.abi_hash.empty() &&
      candidate.metadata.abi_hash != config.abi_hash) {
    return CandidateRejection::kAbiMismatch;
  }
  if (!candidate.metadata.version.IsInRange(config.vmin, config.vmax)) {
    return CandidateRejection::kOutOfRange;
  }
  if (IsVersionRevoked(candidate.metadata.version, revoked_versions)) {
    return CandidateRejection::kRevoked;
  }
  return std::nullopt;
}

// Internal helper to find the best version given the filter criteria.
std::optional<InstalledVersion> FindBestVersionInternal(
    const std::string& vmin,
    const std::string& vmax,
    const std::string& abi_hash,
    const std::vector<InstalledVersion>& available_versions,
    const std::vector<RevokedVersionRange>& revoked_versions) {
  std::optional<InstalledVersion> best;

  for (const auto& installed : available_versions) {
    // 1. Filter by abi_hash (must match if specified)
    if (!abi_hash.empty() && installed.metadata.abi_hash != abi_hash) {
      continue;
    }

    // 2. Filter by version range [vmin, vmax]
    if (!installed.metadata.version.IsInRange(vmin, vmax)) {
      continue;
    }

    // 3. Filter out revoked versions
    if (IsVersionRevoked(installed.metadata.version, revoked_versions)) {
      continue;
    }

    // 4. Select the newest matching version
    if (!best || installed.metadata.version > best->metadata.version) {
      best = installed;
    }
  }

  return best;
}

}  // namespace

OfflineSelectionResult SelectOfflineCandidate(
    const Config& config,
    const std::string& platform,
    const std::vector<InstalledVersion>& installed,
    const std::optional<InstalledVersion>& bundled,
    const std::vector<RevokedVersionRange>& revoked_versions,
    const std::set<VersionKey>& disqualified_versions) {
  OfflineSelectionResult result;
  std::optional<InstalledVersion> unfiltered_best;
  CandidateSource unfiltered_source = CandidateSource::kNone;
  std::optional<InstalledVersion> disqualified_best;
  CandidateSource disqualified_source = CandidateSource::kNone;
  std::set<VersionKey> seen;

  auto reject = [&](const InstalledVersion& candidate, CandidateSource source,
                    CandidateRejection reason) {
    result.rejected.push_back({candidate, source, reason});
  };

  auto consider = [&](const InstalledVersion& candidate,
                      CandidateSource source) {
    auto base_rejection =
        CheckCandidate(config, platform, candidate, revoked_versions);
    if (base_rejection) {
      reject(candidate, source, *base_rejection);
      if (*base_rejection == CandidateRejection::kRevoked &&
          source == CandidateSource::kBundled) {
        result.last_resort = candidate;
        result.last_resort_source = source;
        result.last_resort_is_revoked_bundled = true;
      }
      return;
    }

    // Directory priority applies to usable installed entries. A malformed
    // higher-priority duplicate must not hide a later valid entry.
    if (source == CandidateSource::kInstalled &&
        !seen.insert(MakeVersionKey(candidate)).second) {
      reject(candidate, source, CandidateRejection::kDuplicate);
      return;
    }

    if (IsBetter(candidate, source, unfiltered_best, unfiltered_source)) {
      unfiltered_best = candidate;
      unfiltered_source = source;
    }

    if (source == CandidateSource::kInstalled &&
        disqualified_versions.contains(MakeVersionKey(candidate))) {
      reject(candidate, source, CandidateRejection::kDisqualified);
      if (IsBetter(candidate, source, disqualified_best, disqualified_source)) {
        disqualified_best = candidate;
        disqualified_source = source;
      }
      return;
    }

    if (IsBetter(candidate, source, result.preferred,
                 result.preferred_source)) {
      result.preferred = candidate;
      result.preferred_source = source;
    }
  };

  for (const auto& candidate : installed) {
    consider(candidate, CandidateSource::kInstalled);
  }
  if (bundled) {
    consider(*bundled, CandidateSource::kBundled);
  }

  if (!result.last_resort && disqualified_best) {
    result.last_resort = std::move(disqualified_best);
    result.last_resort_source = disqualified_source;
  }

  result.preferred_is_rollback = result.preferred && unfiltered_best &&
                                 (result.preferred->metadata.version !=
                                      unfiltered_best->metadata.version ||
                                  result.preferred_source != unfiltered_source);
  return result;
}

std::string BuildNoMatchingInstalledVersionMessage(
    const Config& config,
    const std::string& platform,
    const std::vector<RejectedCandidate>& rejected) {
  std::string message =
      "No installed or bundled version matches requirements (platform = " +
      platform + ", version >= " + config.vmin;
  if (!config.vmax.empty()) {
    message += ", version <= " + config.vmax;
  }
  if (!config.abi_hash.empty()) {
    message += ", ABI hash = " + config.abi_hash;
  }
  message += ")";

  if (rejected.empty()) {
    return message + "; no installed or bundled candidates were found";
  }

  constexpr size_t kMaxDiagnosticCandidates = 5;
  const size_t candidate_count =
      std::min(rejected.size(), kMaxDiagnosticCandidates);
  const Version min_version = Version::Parse(config.vmin);
  const Version max_version = Version::Parse(config.vmax);
  message += "; candidate failures: ";
  for (size_t i = 0; i < candidate_count; ++i) {
    if (i > 0) {
      message += "; ";
    }
    const RejectedCandidate& item = rejected[i];
    const VersionMetadata& metadata = item.candidate.metadata;
    message +=
        item.source == CandidateSource::kBundled ? "bundled " : "installed ";
    message += metadata.version.IsValid() ? metadata.version.ToString()
                                          : "<invalid version>";
    message += " [";
    switch (item.reason) {
      case CandidateRejection::kInvalidMetadata:
        message += "invalid metadata";
        break;
      case CandidateRejection::kWrongPlatform:
        message += "platform mismatch (got " +
                   (metadata.platform.empty() ? std::string("<empty>")
                                              : metadata.platform) +
                   ")";
        break;
      case CandidateRejection::kAbiMismatch:
        message += "ABI hash mismatch (got " +
                   (metadata.abi_hash.empty() ? std::string("<empty>")
                                              : metadata.abi_hash) +
                   ")";
        break;
      case CandidateRejection::kOutOfRange:
        if (min_version.IsValid() && metadata.version < min_version) {
          message += "version below vmin";
        } else if (max_version.IsValid() && metadata.version > max_version) {
          message += "version above vmax";
        } else {
          message += "version outside required range";
        }
        break;
      case CandidateRejection::kRevoked:
        message += "revoked";
        break;
      case CandidateRejection::kDisqualified:
        message += "disqualified by launch health";
        break;
      case CandidateRejection::kDuplicate:
        message += "lower-priority duplicate";
        break;
    }
    message += "]";
  }
  if (rejected.size() > candidate_count) {
    message += "; ... " + std::to_string(rejected.size() - candidate_count) +
               " more candidate(s)";
  }
  return message;
}

std::optional<InstalledVersion> FindBestVersion(
    const Config& config,
    const std::vector<InstalledVersion>& available_versions,
    const std::vector<RevokedVersionRange>& revoked_versions) {
  return FindBestVersionInternal(config.vmin, config.vmax, config.abi_hash,
                                 available_versions, revoked_versions);
}

std::optional<InstalledVersion> FindBestVersion(
    const AppEntry& entry,
    const std::vector<InstalledVersion>& available_versions,
    const std::vector<RevokedVersionRange>& revoked_versions) {
  return FindBestVersionInternal(entry.vmin, entry.vmax, entry.abi_hash,
                                 available_versions, revoked_versions);
}

std::set<VersionKey> GetRequiredVersionSet(
    const Database& database,
    const std::vector<InstalledVersion>& available_versions,
    const std::vector<RevokedVersionRange>& revoked_versions) {
  std::set<VersionKey> required;

  for (const auto& app : database.GetAllApps()) {
    // Filter available versions to only those matching the app's platform
    std::vector<InstalledVersion> platform_versions;
    for (const auto& v : available_versions) {
      if (v.metadata.platform == app.platform) {
        platform_versions.push_back(v);
      }
    }

    auto best = FindBestVersion(app, platform_versions, revoked_versions);
    if (best) {
      VersionKey key;
      key.version = best->metadata.version;
      key.platform = best->metadata.platform;
      required.insert(std::move(key));
    }
  }

  return required;
}

std::vector<InstalledVersion> GetPrunableVersions(
    const std::vector<InstalledVersion>& installed_versions,
    const std::set<VersionKey>& required_versions,
    const std::vector<RevokedVersionRange>& revoked_versions,
    const std::set<VersionKey>& confirmed_protected_versions) {
  std::vector<InstalledVersion> prunable;

  for (const auto& installed : installed_versions) {
    // A version is prunable if:
    // 1. It's revoked (always prunable, even if "required")
    // 2. OR it's not in the required set (checking both version AND platform)
    bool is_revoked =
        IsVersionRevoked(installed.metadata.version, revoked_versions);

    VersionKey key;
    key.version = installed.metadata.version;
    key.platform = installed.metadata.platform;
    bool is_required = required_versions.contains(key) ||
                       confirmed_protected_versions.contains(key);

    if (is_revoked || !is_required) {
      prunable.push_back(installed);
    }
  }

  return prunable;
}

}  // namespace cef_installer
