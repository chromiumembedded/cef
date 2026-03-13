// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_version_resolver.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

// Helper to create InstalledVersion for testing
InstalledVersion MakeInstalled(const std::string& version_str,
                               const std::string& abi_hash = "",
                               const std::string& platform = "windows64",
                               const std::string& path = "") {
  InstalledVersion iv;
  iv.metadata.version = Version::Parse(version_str);
  iv.metadata.platform = platform;
  iv.metadata.abi_hash = abi_hash;
  iv.path = base::FilePath::FromUTF8Unsafe(path);
  return iv;
}

// Helper to create VersionKey for testing
VersionKey MakeKey(const std::string& version_str,
                   const std::string& platform = "windows64") {
  VersionKey key;
  key.version = Version::Parse(version_str);
  key.platform = platform;
  return key;
}

// Helper to create Config for testing
Config MakeConfig(const std::string& appid,
                  const std::string& vmin,
                  const std::string& vmax = "",
                  const std::string& abi_hash = "") {
  Config c;
  c.appid = appid;
  c.vmin = vmin;
  c.vmax = vmax;
  c.abi_hash = abi_hash;
  return c;
}

// Helper to create RevokedVersionRange (point revocation) for testing.
RevokedVersionRange MakeRevoked(const std::string& version_str) {
  RevokedVersionRange rv;
  rv.version_min = Version::Parse(version_str);
  rv.version_max = rv.version_min;
  rv.reason = "test";
  rv.revoked_at = "2024-01-01T00:00:00Z";
  return rv;
}

// ============================================================================
// FindBestVersion tests
// ============================================================================

TEST(InstallerVersionResolverTest, FindBestSingleMatch) {
  std::vector<InstalledVersion> available;
  available.push_back(MakeInstalled("137.1.0", "abc123"));

  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");

  auto result = FindBestVersion(config, available);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("137.1.0", result->metadata.version.ToString());
}

TEST(InstallerVersionResolverTest, FindBestSelectsNewest) {
  std::vector<InstalledVersion> available;
  available.push_back(MakeInstalled("137.1.0", "abc123"));
  available.push_back(MakeInstalled("137.3.0", "abc123"));
  available.push_back(MakeInstalled("137.2.0", "abc123"));

  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");

  auto result = FindBestVersion(config, available);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("137.3.0", result->metadata.version.ToString());
}

TEST(InstallerVersionResolverTest, FindBestNoMatch) {
  std::vector<InstalledVersion> available;
  available.push_back(MakeInstalled("136.1.0", "abc123"));  // Below range

  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");

  auto result = FindBestVersion(config, available);

  EXPECT_FALSE(result.has_value());
}

TEST(InstallerVersionResolverTest, FindBestAbiHashFilter) {
  std::vector<InstalledVersion> available;
  available.push_back(MakeInstalled("137.1.0", "abc123"));
  available.push_back(MakeInstalled("137.2.0", "different"));  // Different hash

  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");

  auto result = FindBestVersion(config, available);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("137.1.0",
            result->metadata.version.ToString());  // Only abc123 matches
}

TEST(InstallerVersionResolverTest, FindBestEmptyAbiMatchesAny) {
  std::vector<InstalledVersion> available;
  available.push_back(MakeInstalled("137.1.0", "abc123"));
  available.push_back(MakeInstalled("137.2.0", "xyz789"));

  // Empty abi_hash means match any
  Config config = MakeConfig("app-uuid", "137.0", "137.99", "");

  auto result = FindBestVersion(config, available);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("137.2.0",
            result->metadata.version.ToString());  // Newest regardless of hash
}

TEST(InstallerVersionResolverTest, FindBestExcludesRevoked) {
  std::vector<InstalledVersion> available;
  available.push_back(MakeInstalled("137.1.0", "abc123"));
  available.push_back(
      MakeInstalled("137.3.0", "abc123"));  // Newest but revoked
  available.push_back(MakeInstalled("137.2.0", "abc123"));

  std::vector<RevokedVersionRange> revoked;
  revoked.push_back(MakeRevoked("137.3.0"));

  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");

  auto result = FindBestVersion(config, available, revoked);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(
      "137.2.0",
      result->metadata.version.ToString());  // Second newest (137.3 revoked)
}

TEST(InstallerVersionResolverTest, OfflineSelectorInstalledWinsBundledTie) {
  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");
  std::vector<InstalledVersion> installed = {
      MakeInstalled("137.2.0", "abc123", "windows64", "first")};
  auto bundled = MakeInstalled("137.2.0", "abc123", "windows64", "bundled");

  auto result = SelectOfflineCandidate(config, "windows64", installed, bundled);

  ASSERT_TRUE(result.preferred);
  EXPECT_EQ(CandidateSource::kInstalled, result.preferred_source);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("first"), result.preferred->path);
}

TEST(InstallerVersionResolverTest, OfflineSelectorKeepsFirstDuplicate) {
  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");
  std::vector<InstalledVersion> installed = {
      MakeInstalled("137.2.0", "abc123", "windows64", "high-priority"),
      MakeInstalled("137.2.0", "abc123", "windows64", "low-priority")};

  auto result =
      SelectOfflineCandidate(config, "windows64", installed, std::nullopt);

  ASSERT_TRUE(result.preferred);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("high-priority"),
            result.preferred->path);
  ASSERT_EQ(1u, result.rejected.size());
  EXPECT_EQ(CandidateRejection::kDuplicate, result.rejected[0].reason);
}

TEST(InstallerVersionResolverTest,
     OfflineSelectorInvalidDuplicateDoesNotShadowValidCandidate) {
  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");
  std::vector<InstalledVersion> installed = {
      MakeInstalled("137.2.0", "", "windows64", "invalid"),
      MakeInstalled("137.2.0", "abc123", "windows64", "valid")};

  auto result =
      SelectOfflineCandidate(config, "windows64", installed, std::nullopt);

  ASSERT_TRUE(result.preferred);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("valid"), result.preferred->path);
  ASSERT_EQ(1u, result.rejected.size());
  EXPECT_EQ(CandidateRejection::kInvalidMetadata, result.rejected[0].reason);
}

TEST(InstallerVersionResolverTest,
     OfflineSelectorRevokedBundledPrecedesDisqualifiedInstalled) {
  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");
  std::vector<InstalledVersion> installed = {
      MakeInstalled("137.2.0", "abc123")};
  auto bundled = MakeInstalled("137.1.0", "abc123");
  std::set<VersionKey> disqualified = {MakeKey("137.2.0")};
  std::vector<RevokedVersionRange> revoked = {MakeRevoked("137.1.0")};

  auto result = SelectOfflineCandidate(config, "windows64", installed, bundled,
                                       revoked, disqualified);

  EXPECT_FALSE(result.preferred);
  ASSERT_TRUE(result.last_resort);
  EXPECT_EQ(CandidateSource::kBundled, result.last_resort_source);
  EXPECT_TRUE(result.last_resort_is_revoked_bundled);
}

TEST(InstallerVersionResolverTest,
     OfflineSelectorDisqualificationProducesRollbackProvenance) {
  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");
  std::vector<InstalledVersion> installed = {
      MakeInstalled("137.3.0", "abc123"), MakeInstalled("137.2.0", "abc123")};
  std::set<VersionKey> disqualified = {MakeKey("137.3.0")};

  auto result = SelectOfflineCandidate(config, "windows64", installed,
                                       std::nullopt, {}, disqualified);

  ASSERT_TRUE(result.preferred);
  EXPECT_EQ("137.2.0", result.preferred->metadata.version.ToString());
  EXPECT_TRUE(result.preferred_is_rollback);
}

TEST(InstallerVersionResolverTest,
     OfflineSelectorDisqualificationIsInstalledSourceSpecific) {
  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");
  std::vector<InstalledVersion> installed = {
      MakeInstalled("137.2.0", "abc123")};
  auto bundled = MakeInstalled("137.2.0", "abc123");
  std::set<VersionKey> disqualified = {MakeKey("137.2.0")};

  auto result = SelectOfflineCandidate(config, "windows64", installed, bundled,
                                       {}, disqualified);

  ASSERT_TRUE(result.preferred);
  EXPECT_EQ(CandidateSource::kBundled, result.preferred_source);
}

TEST(InstallerVersionResolverTest, OfflineSelectorRejectsAllConstraints) {
  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");
  std::vector<InstalledVersion> installed = {
      MakeInstalled("137.2.0", "abc123", "windows32"),
      MakeInstalled("137.2.0", "different"),
      MakeInstalled("138.0.0", "abc123")};

  auto result =
      SelectOfflineCandidate(config, "windows64", installed, std::nullopt);

  EXPECT_FALSE(result.preferred);
  ASSERT_EQ(3u, result.rejected.size());
  EXPECT_EQ(CandidateRejection::kWrongPlatform, result.rejected[0].reason);
  EXPECT_EQ(CandidateRejection::kAbiMismatch, result.rejected[1].reason);
  EXPECT_EQ(CandidateRejection::kOutOfRange, result.rejected[2].reason);
}

TEST(InstallerVersionResolverTest,
     NoMatchingInstalledVersionMessageListsFailedRequirements) {
  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");
  std::vector<InstalledVersion> installed = {
      MakeInstalled("137.2.0", "abc123", "windows32"),
      MakeInstalled("137.2.0", "different"),
      MakeInstalled("138.0.0", "abc123")};
  auto result =
      SelectOfflineCandidate(config, "windows64", installed, std::nullopt);

  const std::string message = BuildNoMatchingInstalledVersionMessage(
      config, "windows64", result.rejected);

  EXPECT_NE(std::string::npos,
            message.find("platform = windows64, version >= 137.0, version <= "
                         "137.99, ABI hash = abc123"));
  EXPECT_NE(std::string::npos,
            message.find("installed 137.2.0 [platform mismatch (got "
                         "windows32)]"));
  EXPECT_NE(std::string::npos,
            message.find("installed 137.2.0 [ABI hash mismatch (got "
                         "different)]"));
  EXPECT_NE(std::string::npos,
            message.find("installed 138.0.0 [version above vmax]"));
}

TEST(InstallerVersionResolverTest,
     NoMatchingInstalledVersionMessageReportsMissingCandidates) {
  Config config = MakeConfig("app-uuid", "151.1", "", "abc123");

  EXPECT_EQ(
      "No installed or bundled version matches requirements (platform = "
      "windows64, version >= 151.1, ABI hash = abc123); no installed or "
      "bundled candidates were found",
      BuildNoMatchingInstalledVersionMessage(config, "windows64", {}));
}

// ============================================================================
// GetRequiredVersionSet tests
// ============================================================================

TEST(InstallerVersionResolverTest, GetRequiredVersionSetOverlapping) {
  // Two apps that can share the same version — deduped in the set
  Database db;
  AppEntry app1;
  app1.uuid = "app1-uuid";
  app1.platform = "windows64";
  app1.vmin = "137.0";
  app1.vmax = "137.99";
  app1.abi_hash = "abc123";
  db.RegisterApp(app1);

  AppEntry app2;
  app2.uuid = "app2-uuid";
  app2.platform = "windows64";
  app2.vmin = "137.1";
  app2.vmax = "137.5";
  app2.abi_hash = "abc123";
  db.RegisterApp(app2);

  std::vector<InstalledVersion> available;
  available.push_back(MakeInstalled("137.2.0", "abc123"));

  auto required = GetRequiredVersionSet(db, available);

  EXPECT_EQ(1u, required.size());
  EXPECT_TRUE(required.count(MakeKey("137.2.0")) > 0);
}

TEST(InstallerVersionResolverTest, GetRequiredVersionSetDifferent) {
  // Two apps with non-overlapping requirements
  Database db;
  AppEntry app1;
  app1.uuid = "app1-uuid";
  app1.platform = "windows64";
  app1.vmin = "137.0";
  app1.vmax = "137.5";
  app1.abi_hash = "abc123";
  db.RegisterApp(app1);

  AppEntry app2;
  app2.uuid = "app2-uuid";
  app2.platform = "windows64";
  app2.vmin = "138.0";
  app2.vmax = "138.99";
  app2.abi_hash = "def456";
  db.RegisterApp(app2);

  std::vector<InstalledVersion> available;
  available.push_back(MakeInstalled("137.2.0", "abc123"));
  available.push_back(MakeInstalled("138.1.0", "def456"));

  auto required = GetRequiredVersionSet(db, available);

  EXPECT_EQ(2u, required.size());
  EXPECT_TRUE(required.count(MakeKey("137.2.0")) > 0);
  EXPECT_TRUE(required.count(MakeKey("138.1.0")) > 0);
}

TEST(InstallerVersionResolverTest, GetRequiredVersionSetDifferentPlatforms) {
  Database db;
  AppEntry app1;
  app1.uuid = "app1-uuid";
  app1.platform = "windows64";
  app1.vmin = "137.0";
  app1.vmax = "137.99";
  app1.abi_hash = "abc123";
  db.RegisterApp(app1);

  AppEntry app2;
  app2.uuid = "app2-uuid";
  app2.platform = "windows32";
  app2.vmin = "137.0";
  app2.vmax = "137.99";
  app2.abi_hash = "abc123";
  db.RegisterApp(app2);

  std::vector<InstalledVersion> available;
  available.push_back(MakeInstalled("137.2.0", "abc123", "windows64"));
  available.push_back(MakeInstalled("137.2.0", "abc123", "windows32"));

  auto required = GetRequiredVersionSet(db, available);

  // Same version on different platforms = 2 unique entries
  EXPECT_EQ(2u, required.size());
  EXPECT_TRUE(required.count(MakeKey("137.2.0", "windows64")) > 0);
  EXPECT_TRUE(required.count(MakeKey("137.2.0", "windows32")) > 0);
}

TEST(InstallerVersionResolverTest, GetRequiredVersionSetUnmetApp) {
  // An app whose requirements can't be met doesn't add to the set
  Database db;
  AppEntry app;
  app.uuid = "unmet-uuid";
  app.platform = "windows64";
  app.vmin = "200.0";
  app.vmax = "200.99";
  app.abi_hash = "abc123";
  db.RegisterApp(app);

  std::vector<InstalledVersion> available;
  available.push_back(MakeInstalled("137.1.0", "abc123"));

  auto required = GetRequiredVersionSet(db, available);

  EXPECT_TRUE(required.empty());
}

TEST(InstallerVersionResolverTest, GetRequiredVersionSetEmptyDatabase) {
  Database db;
  std::vector<InstalledVersion> available;
  available.push_back(MakeInstalled("137.1.0", "abc123"));

  auto required = GetRequiredVersionSet(db, available);

  EXPECT_TRUE(required.empty());
}

// ============================================================================
// GetPrunableVersions tests
// ============================================================================

TEST(InstallerVersionResolverTest, GetPrunableIdentifiesUnused) {
  std::vector<InstalledVersion> installed;
  installed.push_back(MakeInstalled("137.1.0", "abc123"));
  installed.push_back(MakeInstalled("137.2.0", "abc123"));
  installed.push_back(MakeInstalled("136.0.0", "xyz"));  // Unused

  std::set<VersionKey> required;
  required.insert(MakeKey("137.1.0"));
  required.insert(MakeKey("137.2.0"));

  auto prunable = GetPrunableVersions(installed, required);

  ASSERT_EQ(1u, prunable.size());
  EXPECT_EQ("136.0.0", prunable[0].metadata.version.ToString());
  EXPECT_EQ("windows64", prunable[0].metadata.platform);
}

TEST(InstallerVersionResolverTest, GetPrunableIncludesRevoked) {
  std::vector<InstalledVersion> installed;
  installed.push_back(MakeInstalled("137.1.0", "abc123"));
  installed.push_back(
      MakeInstalled("137.2.0", "abc123"));  // Required but revoked

  std::set<VersionKey> required;
  required.insert(MakeKey("137.1.0"));
  required.insert(MakeKey("137.2.0"));

  std::vector<RevokedVersionRange> revoked;
  revoked.push_back(MakeRevoked("137.2.0"));

  auto prunable = GetPrunableVersions(installed, required, revoked);

  // 137.2.0 is prunable because it's revoked, even though it's "required"
  ASSERT_EQ(1u, prunable.size());
  EXPECT_EQ("137.2.0", prunable[0].metadata.version.ToString());
}

TEST(InstallerVersionResolverTest, GetPrunableDifferentPlatforms) {
  std::vector<InstalledVersion> installed;
  installed.push_back(MakeInstalled("137.1.0", "abc123", "windows64"));
  installed.push_back(MakeInstalled(
      "137.1.0", "abc123", "windows32"));  // Same version, unused platform

  std::set<VersionKey> required;
  required.insert(
      MakeKey("137.1.0", "windows64"));  // Only windows64 is required

  auto prunable = GetPrunableVersions(installed, required);

  // windows32 version should be prunable
  ASSERT_EQ(1u, prunable.size());
  EXPECT_EQ("137.1.0", prunable[0].metadata.version.ToString());
  EXPECT_EQ("windows32", prunable[0].metadata.platform);
}

TEST(InstallerVersionResolverTest, GetPrunableConfirmedProtectionSeam) {
  std::vector<InstalledVersion> installed = {
      MakeInstalled("137.1.0", "abc123")};
  std::set<VersionKey> required;
  std::set<VersionKey> confirmed = {MakeKey("137.1.0")};

  EXPECT_TRUE(GetPrunableVersions(installed, required, {}, confirmed).empty());
  ASSERT_EQ(1u, GetPrunableVersions(installed, required).size());
}

TEST(InstallerVersionResolverTest,
     GetPrunableRevocationOverridesConfirmedProtection) {
  std::vector<InstalledVersion> installed = {
      MakeInstalled("137.1.0", "abc123")};
  std::set<VersionKey> confirmed = {MakeKey("137.1.0")};
  std::vector<RevokedVersionRange> revoked = {MakeRevoked("137.1.0")};

  auto prunable = GetPrunableVersions(installed, {}, revoked, confirmed);
  ASSERT_EQ(1u, prunable.size());
  EXPECT_EQ("137.1.0", prunable[0].metadata.version.ToString());
}

// ============================================================================
// Additional edge case tests
// ============================================================================

TEST(InstallerVersionResolverTest, FindBestWithAppEntry) {
  // Test the AppEntry overload
  std::vector<InstalledVersion> available;
  available.push_back(MakeInstalled("137.1.0", "abc123"));

  AppEntry entry;
  entry.uuid = "app-uuid";
  entry.platform = "windows64";
  entry.vmin = "137.0";
  entry.vmax = "137.99";
  entry.abi_hash = "abc123";

  auto result = FindBestVersion(entry, available);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("137.1.0", result->metadata.version.ToString());
}

TEST(InstallerVersionResolverTest, FindBestEmptyAvailable) {
  std::vector<InstalledVersion> available;  // Empty

  Config config = MakeConfig("app-uuid", "137.0", "137.99", "abc123");

  auto result = FindBestVersion(config, available);

  EXPECT_FALSE(result.has_value());
}

TEST(InstallerVersionResolverTest, GetPrunableEmptyRequired) {
  std::vector<InstalledVersion> installed;
  installed.push_back(MakeInstalled("137.1.0", "abc123"));
  installed.push_back(MakeInstalled("137.2.0", "def456"));

  std::set<VersionKey> required;  // Nothing required

  auto prunable = GetPrunableVersions(installed, required);

  EXPECT_EQ(2u, prunable.size());  // Both are prunable
}

}  // namespace
}  // namespace cef_installer
