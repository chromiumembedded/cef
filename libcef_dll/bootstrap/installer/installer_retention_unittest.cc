// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_retention.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

AppEntry MakeApp(const std::string& appid,
                 const std::string& vmin,
                 const std::string& platform = "windows64",
                 const std::string& abi_hash = "") {
  return {appid, platform, vmin, "", abi_hash};
}

InstalledVersion MakeInstalled(const std::string& version,
                               const std::string& platform = "windows64",
                               const std::string& abi_hash = "") {
  InstalledVersion installed;
  installed.metadata.version = Version::Parse(version);
  installed.metadata.platform = platform;
  installed.metadata.abi_hash = abi_hash;
  return installed;
}

RetentionOptions OptionsAt(uint64_t now, int days = 180) {
  return {.max_age_days = days, .now = now};
}

TEST(InstallerRetentionTest, ExactThresholdIsStale) {
  Database database;
  database.RegisterApp(MakeApp("app", "100"));
  const uint64_t now = 1000 * kFileTimeTicksPerDay;
  RetentionEvidenceMap evidence;
  evidence[{"app", "windows64"}] = {
      RetentionEvidenceKind::kLiveness,
      now - 180 * kFileTimeTicksPerDay,
  };

  RetentionPlan plan =
      BuildRetentionPlan(DirectoryRole::kPerUserDefault, database, evidence, {},
                         {}, {}, OptionsAt(now));

  ASSERT_EQ(1u, plan.registrations.size());
  EXPECT_EQ(RetentionRegistrationDecision::kReclaim,
            plan.registrations[0].decision);
  EXPECT_EQ(180u, plan.registrations[0].age_days);
  EXPECT_EQ(1u, plan.candidates.size());
}

TEST(InstallerRetentionTest, UnknownMissingAndFutureAreProtected) {
  Database database;
  database.RegisterApp(MakeApp("invalid", "100"));
  database.RegisterApp(MakeApp("missing", "100"));
  database.RegisterApp(MakeApp("future", "100"));
  const uint64_t now = 1000 * kFileTimeTicksPerDay;
  RetentionEvidenceMap evidence;
  evidence[{"invalid", "windows64"}] = {
      .unknown = true, .diagnostic = "invalid_or_noncanonical_evidence"};
  evidence[{"future", "windows64"}] = {RetentionEvidenceKind::kHealthSentinel,
                                       now + 1};

  RetentionPlan plan = BuildRetentionPlan(DirectoryRole::kCustom, database,
                                          evidence, {}, {}, {}, OptionsAt(now));

  EXPECT_TRUE(plan.eligible);
  EXPECT_TRUE(plan.candidates.empty());
  ASSERT_EQ(3u, plan.registrations.size());
  EXPECT_EQ(RetentionReason::kFutureEvidence, plan.registrations[0].reason);
  EXPECT_EQ(RetentionReason::kInvalidEvidence, plan.registrations[1].reason);
  EXPECT_EQ("invalid_or_noncanonical_evidence",
            plan.registrations[1].evidence.diagnostic);
  EXPECT_EQ(RetentionReason::kMissingEvidence, plan.registrations[2].reason);
}

TEST(InstallerRetentionTest, ReasonStringsAreClosedAndRoundTrip) {
  constexpr RetentionReason kReasons[] = {
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
  };
  for (RetentionReason reason : kReasons) {
    EXPECT_EQ(reason,
              RetentionReasonFromString(RetentionReasonToString(reason)));
  }
  EXPECT_FALSE(RetentionReasonFromString("arbitrary_diagnostic"));
}

TEST(InstallerRetentionTest, ProvisioningRolesAreIneligible) {
  Database database;
  database.RegisterApp(MakeApp("app", "100"));
  const uint64_t now = 1000 * kFileTimeTicksPerDay;

  for (DirectoryRole role :
       {DirectoryRole::kHklmDefault, DirectoryRole::kProgramFilesDefault}) {
    RetentionPlan plan =
        BuildRetentionPlan(role, database, {}, {}, {}, {}, OptionsAt(now));
    EXPECT_FALSE(plan.eligible);
    EXPECT_EQ("provisioning_store_ineligible", plan.blocker);
    EXPECT_TRUE(plan.candidates.empty());
  }
}

TEST(InstallerRetentionTest, ProvisioningIneligibilityTakesPrecedence) {
  Database database;
  database.RegisterApp(MakeApp("app", "100"));
  database.SuspendPruning();

  RetentionPlan plan =
      BuildRetentionPlan(DirectoryRole::kHklmDefault, database, {}, {}, {}, {},
                         {.max_age_days = 1, .now = 0});

  EXPECT_FALSE(plan.eligible);
  EXPECT_FALSE(plan.store_blocked);
  EXPECT_EQ("provisioning_store_ineligible", plan.blocker);
  ASSERT_EQ(1u, plan.registrations.size());
  EXPECT_EQ(RetentionReason::kProvisioningStoreIneligible,
            plan.registrations[0].reason);
}

TEST(InstallerRetentionTest, SharedVersionRemainsRequired) {
  Database database;
  database.RegisterApp(MakeApp("old", "100"));
  database.RegisterApp(MakeApp("fresh", "100"));
  const uint64_t now = 1000 * kFileTimeTicksPerDay;
  RetentionEvidenceMap evidence;
  evidence[{"old", "windows64"}] = {RetentionEvidenceKind::kLiveness,
                                    now - 181 * kFileTimeTicksPerDay};
  evidence[{"fresh", "windows64"}] = {RetentionEvidenceKind::kLiveness,
                                      now - 1 * kFileTimeTicksPerDay};

  RetentionPlan plan =
      BuildRetentionPlan(DirectoryRole::kPerUserDefault, database, evidence,
                         {MakeInstalled("100.1")}, {}, {}, OptionsAt(now));

  ASSERT_EQ(1u, plan.versions.size());
  EXPECT_TRUE(plan.versions[0].required_before);
  EXPECT_TRUE(plan.versions[0].required_after);
  EXPECT_FALSE(plan.versions[0].expected_removal);
}

TEST(InstallerRetentionTest, PlatformAndAbiRemainIndependent) {
  Database database;
  database.RegisterApp(MakeApp("app", "100", "windows64", "a"));
  database.RegisterApp(MakeApp("app", "100", "windows32", "b"));
  const uint64_t now = 1000 * kFileTimeTicksPerDay;
  RetentionEvidenceMap evidence;
  evidence[{"app", "windows64"}] = {RetentionEvidenceKind::kHealthSentinel,
                                    now - 181 * kFileTimeTicksPerDay};
  evidence[{"app", "windows32"}] = {RetentionEvidenceKind::kHealthSentinel,
                                    now - 1 * kFileTimeTicksPerDay};

  RetentionPlan plan =
      BuildRetentionPlan(DirectoryRole::kPerUserDefault, database, evidence,
                         {MakeInstalled("100.1", "windows64", "a"),
                          MakeInstalled("100.1", "windows32", "b")},
                         {}, {}, OptionsAt(now));

  ASSERT_EQ(2u, plan.versions.size());
  EXPECT_TRUE(plan.versions[0].required_after);
  EXPECT_FALSE(plan.versions[0].expected_removal);
  EXPECT_FALSE(plan.versions[1].required_after);
  EXPECT_TRUE(plan.versions[1].expected_removal);
}

TEST(InstallerRetentionTest, RevocationOverridesConfirmedProtection) {
  Database database;
  database.RegisterApp(MakeApp("app", "100"));
  const uint64_t now = 1000 * kFileTimeTicksPerDay;
  RetentionEvidenceMap evidence;
  evidence[{"app", "windows64"}] = {RetentionEvidenceKind::kLiveness,
                                    now - 181 * kFileTimeTicksPerDay};
  RevokedVersionRange revoked;
  revoked.version_min = Version::Parse("100.1");
  revoked.version_max = revoked.version_min;
  VersionKey confirmed{Version::Parse("100.1"), "windows64"};

  RetentionPlan plan = BuildRetentionPlan(
      DirectoryRole::kCustom, database, evidence, {MakeInstalled("100.1")},
      {revoked}, {confirmed}, OptionsAt(now));

  ASSERT_EQ(1u, plan.versions.size());
  EXPECT_EQ(RetentionVersionDecision::kRevoked, plan.versions[0].decision);
  EXPECT_FALSE(plan.versions[0].expected_removal);
}

TEST(InstallerRetentionTest, ConfirmedVersionRemainsProtected) {
  Database database;
  database.RegisterApp(MakeApp("app", "100"));
  const uint64_t now = 1000 * kFileTimeTicksPerDay;
  RetentionEvidenceMap evidence;
  evidence[{"app", "windows64"}] = {RetentionEvidenceKind::kHealthSentinel,
                                    now - 181 * kFileTimeTicksPerDay};
  VersionKey confirmed{Version::Parse("100.1"), "windows64"};

  RetentionPlan plan = BuildRetentionPlan(DirectoryRole::kCustom, database,
                                          evidence, {MakeInstalled("100.1")},
                                          {}, {confirmed}, OptionsAt(now));

  ASSERT_EQ(1u, plan.versions.size());
  EXPECT_EQ(RetentionVersionDecision::kConfirmedProtected,
            plan.versions[0].decision);
  EXPECT_FALSE(plan.versions[0].expected_removal);
}

TEST(InstallerRetentionTest, DatabasePruningBlockerProtectsAll) {
  Database database;
  database.RegisterApp(MakeApp("app", "100"));
  database.SuspendPruning();
  const uint64_t now = 1000 * kFileTimeTicksPerDay;

  RetentionPlan plan = BuildRetentionPlan(
      DirectoryRole::kPerUserDefault, database, {}, {}, {}, {}, OptionsAt(now));

  EXPECT_TRUE(plan.store_blocked);
  EXPECT_EQ("database_pruning_blocked", plan.blocker);
  EXPECT_TRUE(plan.candidates.empty());
}

}  // namespace
}  // namespace cef_installer
