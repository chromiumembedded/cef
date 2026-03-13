// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_retention.h"

#include <algorithm>
#include <limits>
#include <tuple>

namespace cef_installer {
namespace {

VersionKey MakeVersionKey(const InstalledVersion& installed) {
  return {installed.metadata.version, installed.metadata.platform};
}

}  // namespace

bool IsValidRetentionMaxAgeDays(int days) {
  return days >= kMinRetentionMaxAgeDays && days <= kMaxRetentionMaxAgeDays;
}

RetentionPlan BuildRetentionPlan(
    DirectoryRole role,
    const Database& database,
    const RetentionEvidenceMap& evidence,
    const std::vector<InstalledVersion>& installed,
    const std::vector<RevokedVersionRange>& revoked,
    const std::set<VersionKey>& confirmed_protected,
    const RetentionOptions& options) {
  RetentionPlan plan;
  if (!IsUserRetentionEligible(role)) {
    plan.eligible = false;
    plan.blocker = "provisioning_store_ineligible";
  } else if (!database.CanPrune()) {
    plan.store_blocked = true;
    plan.blocker = "database_pruning_blocked";
  } else if (!IsValidRetentionMaxAgeDays(options.max_age_days) ||
             options.now == 0) {
    plan.store_blocked = true;
    plan.blocker = "invalid_retention_options";
  }

  const uint64_t days = static_cast<uint64_t>(options.max_age_days);
  const bool threshold_overflows =
      days > std::numeric_limits<uint64_t>::max() / kFileTimeTicksPerDay;
  const uint64_t threshold =
      threshold_overflows ? 0 : days * kFileTimeTicksPerDay;

  std::vector<AppEntry> apps = database.GetAllApps();
  std::sort(apps.begin(), apps.end(), [](const AppEntry& a, const AppEntry& b) {
    return std::tie(a.uuid, a.platform) < std::tie(b.uuid, b.platform);
  });
  std::set<RetentionRegistrationKey> removed;
  for (const auto& app : apps) {
    RetentionRegistrationReport report;
    report.entry = app;
    auto it = evidence.find({app.uuid, app.platform});
    if (it != evidence.end()) {
      report.evidence = it->second;
    }
    if (!plan.eligible) {
      report.reason = RetentionReason::kProvisioningStoreIneligible;
    } else if (plan.store_blocked) {
      report.reason =
          !IsValidRetentionMaxAgeDays(options.max_age_days) || options.now == 0
              ? RetentionReason::kInvalidRetentionOptions
              : RetentionReason::kDatabasePruningBlocked;
    } else if (report.evidence.unknown) {
      report.reason = RetentionReason::kInvalidEvidence;
    } else if (report.evidence.kind == RetentionEvidenceKind::kNone ||
               report.evidence.timestamp == 0) {
      report.reason = RetentionReason::kMissingEvidence;
    } else if (report.evidence.timestamp > options.now) {
      report.reason = RetentionReason::kFutureEvidence;
    } else {
      const uint64_t age = options.now - report.evidence.timestamp;
      report.age_days = age / kFileTimeTicksPerDay;
      if (!threshold_overflows && age >= threshold) {
        report.decision = RetentionRegistrationDecision::kReclaim;
        report.reason = RetentionReason::kStaleEvidence;
        RetentionRegistrationKey key{app.uuid, app.platform};
        plan.candidates.push_back(key);
        removed.insert(std::move(key));
      } else {
        report.reason = RetentionReason::kFreshEvidence;
      }
    }
    plan.registrations.push_back(std::move(report));
  }

  Database after;
  for (const auto& app : database.GetAllApps()) {
    if (!removed.contains({app.uuid, app.platform})) {
      after.RegisterApp(app);
    }
  }
  const std::set<VersionKey> before_required =
      GetRequiredVersionSet(database, installed, revoked);
  const std::set<VersionKey> after_required =
      GetRequiredVersionSet(after, installed, revoked);
  std::vector<InstalledVersion> sorted_installed = installed;
  std::sort(sorted_installed.begin(), sorted_installed.end(),
            [](const InstalledVersion& a, const InstalledVersion& b) {
              if (a.metadata.version != b.metadata.version) {
                return a.metadata.version < b.metadata.version;
              }
              return a.metadata.platform < b.metadata.platform;
            });
  for (const auto& item : sorted_installed) {
    RetentionVersionReport report;
    report.version = item.metadata.version;
    report.platform = item.metadata.platform;
    VersionKey key = MakeVersionKey(item);
    report.required_before = before_required.contains(key);
    report.required_after = after_required.contains(key);
    const bool revoked_version =
        IsVersionRevoked(item.metadata.version, revoked);
    if (revoked_version) {
      report.decision = RetentionVersionDecision::kRevoked;
      report.reason = RetentionReason::kRevoked;
    } else if (report.required_after) {
      report.decision = RetentionVersionDecision::kRetainRequired;
      report.reason = RetentionReason::kRequiredByRemainingRegistration;
    } else if (confirmed_protected.contains(key)) {
      report.decision = RetentionVersionDecision::kConfirmedProtected;
      report.reason = RetentionReason::kConfirmedLaunchProtection;
    } else if (report.required_before) {
      report.decision = RetentionVersionDecision::kNewlyReclaimable;
      report.expected_removal = true;
      report.reason = RetentionReason::kNewlyUnreferenced;
    } else {
      report.decision = RetentionVersionDecision::kAlreadyUnreferenced;
      report.reason = RetentionReason::kAlreadyUnreferenced;
    }
    plan.versions.push_back(std::move(report));
  }
  return plan;
}

const char* RetentionEvidenceKindToString(RetentionEvidenceKind kind) {
  switch (kind) {
    case RetentionEvidenceKind::kNone:
      return "none";
    case RetentionEvidenceKind::kHealthSentinel:
      return "health_sentinel";
    case RetentionEvidenceKind::kLiveness:
      return "liveness";
  }
}

const char* RetentionReasonToString(RetentionReason reason) {
  switch (reason) {
    case RetentionReason::kProvisioningStoreIneligible:
      return "provisioning_store_ineligible";
    case RetentionReason::kDatabasePruningBlocked:
      return "database_pruning_blocked";
    case RetentionReason::kInvalidRetentionOptions:
      return "invalid_retention_options";
    case RetentionReason::kInvalidEvidence:
      return "invalid_evidence";
    case RetentionReason::kMissingEvidence:
      return "missing_evidence";
    case RetentionReason::kFutureEvidence:
      return "future_evidence";
    case RetentionReason::kStaleEvidence:
      return "stale_evidence";
    case RetentionReason::kFreshEvidence:
      return "fresh_evidence";
    case RetentionReason::kRevoked:
      return "revoked";
    case RetentionReason::kRequiredByRemainingRegistration:
      return "required_by_remaining_registration";
    case RetentionReason::kConfirmedLaunchProtection:
      return "confirmed_launch_protection";
    case RetentionReason::kNewlyUnreferenced:
      return "newly_unreferenced";
    case RetentionReason::kAlreadyUnreferenced:
      return "already_unreferenced";
  }
}

std::optional<RetentionReason> RetentionReasonFromString(
    std::string_view reason) {
  for (RetentionReason value : {
           RetentionReason::kProvisioningStoreIneligible,
           RetentionReason::kDatabasePruningBlocked,
           RetentionReason::kInvalidRetentionOptions,
           RetentionReason::kInvalidEvidence,
           RetentionReason::kMissingEvidence,
           RetentionReason::kFutureEvidence,
           RetentionReason::kStaleEvidence,
           RetentionReason::kFreshEvidence,
           RetentionReason::kRevoked,
           RetentionReason::kRequiredByRemainingRegistration,
           RetentionReason::kConfirmedLaunchProtection,
           RetentionReason::kNewlyUnreferenced,
           RetentionReason::kAlreadyUnreferenced,
       }) {
    if (reason == RetentionReasonToString(value)) {
      return value;
    }
  }
  return std::nullopt;
}

const char* RetentionRegistrationDecisionToString(
    RetentionRegistrationDecision decision) {
  return decision == RetentionRegistrationDecision::kReclaim ? "reclaim"
                                                             : "protected";
}

const char* RetentionVersionDecisionToString(
    RetentionVersionDecision decision) {
  switch (decision) {
    case RetentionVersionDecision::kRetainRequired:
      return "retain_required";
    case RetentionVersionDecision::kNewlyReclaimable:
      return "newly_reclaimable";
    case RetentionVersionDecision::kAlreadyUnreferenced:
      return "already_unreferenced";
    case RetentionVersionDecision::kRevoked:
      return "revoked";
    case RetentionVersionDecision::kConfirmedProtected:
      return "confirmed_protected";
  }
}

}  // namespace cef_installer
