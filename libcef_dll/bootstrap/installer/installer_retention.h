// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_RETENTION_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_RETENTION_H_

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cef/libcef_dll/bootstrap/installer/installer_database.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "cef/libcef_dll/bootstrap/installer/installer_revocation.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version_metadata.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version_resolver.h"

namespace cef_installer {

constexpr int kDefaultRetentionMaxAgeDays = 180;
constexpr int kMinRetentionMaxAgeDays = 90;
constexpr int kMaxRetentionMaxAgeDays = 3650;
constexpr uint64_t kFileTimeTicksPerDay = 24ULL * 60 * 60 * 10000000;

enum class RetentionEvidenceKind {
  kNone,
  kHealthSentinel,
  kLiveness,
};

struct RetentionEvidence {
  RetentionEvidenceKind kind = RetentionEvidenceKind::kNone;
  uint64_t timestamp = 0;
  bool unknown = false;
  std::string diagnostic;
};

using RetentionRegistrationKey = std::pair<std::string, std::string>;
using RetentionEvidenceMap =
    std::map<RetentionRegistrationKey, RetentionEvidence>;

struct RetentionOptions {
  int max_age_days = kDefaultRetentionMaxAgeDays;
  uint64_t now = 0;
};

enum class RetentionRegistrationDecision {
  kReclaim,
  kProtected,
};

// Stable symbolic values emitted by retention reports. These values are a
// closed set and may be used as localization keys. Variable diagnostic text is
// reported separately and must not be used for program logic.
enum class RetentionReason {
  kProvisioningStoreIneligible,
  kDatabasePruningBlocked,
  kInvalidRetentionOptions,
  kInvalidEvidence,
  kMissingEvidence,
  kFutureEvidence,
  kStaleEvidence,
  kFreshEvidence,
  kRevoked,
  kRequiredByRemainingRegistration,
  kConfirmedLaunchProtection,
  kNewlyUnreferenced,
  kAlreadyUnreferenced,
};

struct RetentionRegistrationReport {
  AppEntry entry;
  RetentionEvidence evidence;
  std::optional<uint64_t> age_days;
  RetentionRegistrationDecision decision =
      RetentionRegistrationDecision::kProtected;
  RetentionReason reason = RetentionReason::kInvalidEvidence;
};

enum class RetentionVersionDecision {
  kRetainRequired,
  kNewlyReclaimable,
  kAlreadyUnreferenced,
  kRevoked,
  kConfirmedProtected,
};

struct RetentionVersionReport {
  Version version;
  std::string platform;
  bool required_before = false;
  bool required_after = false;
  bool expected_removal = false;
  bool cleanup_deferred = false;
  RetentionVersionDecision decision = RetentionVersionDecision::kRetainRequired;
  RetentionReason reason = RetentionReason::kInvalidEvidence;
};

struct RetentionPlan {
  bool eligible = true;
  bool store_blocked = false;
  std::string blocker;
  std::vector<RetentionRegistrationReport> registrations;
  std::vector<RetentionVersionReport> versions;
  std::vector<RetentionRegistrationKey> candidates;
};

bool IsValidRetentionMaxAgeDays(int days);

// Pure retention planning. The caller supplies an authoritative database,
// side-effect-free evidence snapshot, installed metadata, cached revocations,
// confirmed launch protection and a clock value.
RetentionPlan BuildRetentionPlan(
    DirectoryRole role,
    const Database& database,
    const RetentionEvidenceMap& evidence,
    const std::vector<InstalledVersion>& installed,
    const std::vector<RevokedVersionRange>& revoked,
    const std::set<VersionKey>& confirmed_protected,
    const RetentionOptions& options);

const char* RetentionEvidenceKindToString(RetentionEvidenceKind kind);
const char* RetentionReasonToString(RetentionReason reason);
std::optional<RetentionReason> RetentionReasonFromString(
    std::string_view reason);
const char* RetentionRegistrationDecisionToString(
    RetentionRegistrationDecision decision);
const char* RetentionVersionDecisionToString(RetentionVersionDecision decision);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_RETENTION_H_
