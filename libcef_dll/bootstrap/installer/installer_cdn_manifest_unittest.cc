// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_cdn_manifest.h"

#include <set>

#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

// ============================================================================
// ParseStableMilestone tests
// ============================================================================

TEST(InstallerCdnManifestTest, ParseStableMilestoneValid) {
  int milestone = 0;
  ManifestError result = ParseStableMilestone("137", &milestone);

  EXPECT_EQ(ManifestError::kSuccess, result);
  EXPECT_EQ(137, milestone);
}

TEST(InstallerCdnManifestTest, ParseStableMilestoneWithWhitespace) {
  int milestone = 0;
  ManifestError result = ParseStableMilestone("  137\n", &milestone);

  EXPECT_EQ(ManifestError::kSuccess, result);
  EXPECT_EQ(137, milestone);
}

TEST(InstallerCdnManifestTest, ParseStableMilestoneEmpty) {
  int milestone = 0;
  ManifestError result = ParseStableMilestone("", &milestone);

  EXPECT_EQ(ManifestError::kMissingRequiredField, result);
}

TEST(InstallerCdnManifestTest, ParseStableMilestoneNonNumeric) {
  int milestone = 0;
  ManifestError result = ParseStableMilestone("abc", &milestone);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

TEST(InstallerCdnManifestTest, ParseStableMilestoneNegative) {
  int milestone = 0;
  ManifestError result = ParseStableMilestone("-1", &milestone);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

TEST(InstallerCdnManifestTest, ParseStableMilestoneZero) {
  int milestone = 0;
  ManifestError result = ParseStableMilestone("0", &milestone);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

// ============================================================================
// ParseMilestoneManifest tests
// ============================================================================

TEST(InstallerCdnManifestTest, ParseMilestoneManifestValid) {
  const char* json = R"({
    "windows64": {
      "abi_hash": "abc123def4567890",
      "file": "cef_binary_137.3.5_windows64_signed.tar.xz",
      "last_modified": "2026-02-10T12:00:00.000000Z",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    },
    "linux64": {
      "file": "cef_binary_137.3.5_linux64_signed.tar.xz",
      "last_modified": "2026-02-10T12:00:00.000000Z",
      "sha1": "1234567890abcdef1234567890abcdef12345678",
      "version": "137.3.5"
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kSuccess, result);
  ASSERT_EQ(2u, entries.size());

  ASSERT_TRUE(entries.count("windows64") > 0);
  EXPECT_EQ("137.3.5", entries["windows64"].version.ToString());
  EXPECT_EQ("abc123def4567890", entries["windows64"].abi_hash);
  EXPECT_EQ("cef_binary_137.3.5_windows64_signed.tar.xz",
            entries["windows64"].file);

  ASSERT_TRUE(entries.count("linux64") > 0);
  EXPECT_EQ("137.3.5", entries["linux64"].version.ToString());
  EXPECT_TRUE(entries["linux64"].abi_hash.empty());  // Not present in JSON
}

TEST(InstallerCdnManifestTest, ParseMilestoneManifestMissingField) {
  // Missing "file" field
  const char* json = R"({
    "windows64": {
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kMissingRequiredField, result);
}

TEST(InstallerCdnManifestTest, ParseMilestoneManifestMalformed) {
  const char* json = "not valid json {";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kJsonParseError, result);
}

// ============================================================================
// ParsePlatformManifest tests
// ============================================================================

TEST(InstallerCdnManifestTest, ParsePlatformManifestArray) {
  const char* json = R"([
    {
      "abi_hash": "abc123def4567890",
      "file": "cef_binary_137.3.5_windows64_signed.tar.xz",
      "last_modified": "2026-02-10T12:00:00.000000Z",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    },
    {
      "abi_hash": "abc123def4567890",
      "file": "cef_binary_137.2.0_windows64_signed.tar.xz",
      "last_modified": "2026-02-05T12:00:00.000000Z",
      "sha1": "1234567890abcdef1234567890abcdef12345678",
      "version": "137.2.0"
    }
  ])";

  std::vector<CdnBuildEntry> entries;
  ManifestError result = ParsePlatformManifest(json, &entries);

  EXPECT_EQ(ManifestError::kSuccess, result);
  ASSERT_EQ(2u, entries.size());

  EXPECT_EQ("137.3.5", entries[0].version.ToString());
  EXPECT_EQ("137.2.0", entries[1].version.ToString());
}

TEST(InstallerCdnManifestTest, ParsePlatformManifestEmpty) {
  const char* json = "[]";

  std::vector<CdnBuildEntry> entries;
  ManifestError result = ParsePlatformManifest(json, &entries);

  EXPECT_EQ(ManifestError::kSuccess, result);
  EXPECT_TRUE(entries.empty());
}

TEST(InstallerCdnManifestTest, ParsePlatformManifestMalformed) {
  const char* json = "not json";

  std::vector<CdnBuildEntry> entries;
  ManifestError result = ParsePlatformManifest(json, &entries);

  EXPECT_EQ(ManifestError::kJsonParseError, result);
}

// ============================================================================
// URL building tests
// ============================================================================

TEST(InstallerCdnManifestTest, BuildUrls) {
  const std::string base = "https://cef-builds.spotifycdn.com/";

  EXPECT_EQ("https://cef-builds.spotifycdn.com/stable.txt",
            BuildStableUrl(base));

  EXPECT_EQ("https://cef-builds.spotifycdn.com/137.json",
            BuildMilestoneUrl(base, 137));

  EXPECT_EQ("https://cef-builds.spotifycdn.com/137_windows64.json",
            BuildPlatformUrl(base, 137, "windows64"));

  EXPECT_EQ("https://cef-builds.spotifycdn.com/abc123_windows64.json",
            BuildAbiHashUrl(base, "abc123", "windows64"));

  EXPECT_EQ("https://cef-builds.spotifycdn.com/archive.tar.xz",
            BuildArchiveUrl(base, "archive.tar.xz"));

  EXPECT_EQ("https://cef-builds.spotifycdn.com/archive.tar.xz.sha256",
            BuildHashFileUrl(base, "archive.tar.xz"));

  EXPECT_EQ("https://cef-builds.spotifycdn.com/revoked.json",
            BuildRevocationListUrl(base));
}

TEST(InstallerCdnManifestTest, BuildUrlsNoTrailingSlash) {
  // Base URL without trailing slash should still work
  const std::string base = "https://cef-builds.spotifycdn.com";

  EXPECT_EQ("https://cef-builds.spotifycdn.com/stable.txt",
            BuildStableUrl(base));

  EXPECT_EQ("https://cef-builds.spotifycdn.com/137.json",
            BuildMilestoneUrl(base, 137));
}

// ============================================================================
// Channel URL building tests
// ============================================================================

TEST(InstallerCdnManifestTest, BuildChannelUrlStable) {
  const std::string base = "https://cef-builds.spotifycdn.com/";

  // Empty channel = stable
  EXPECT_EQ("https://cef-builds.spotifycdn.com/stable.txt",
            BuildChannelUrl(base, ""));
}

TEST(InstallerCdnManifestTest, BuildChannelUrlBeta) {
  const std::string base = "https://cef-builds.spotifycdn.com/";

  EXPECT_EQ("https://cef-builds.spotifycdn.com/beta.txt",
            BuildChannelUrl(base, "beta"));
}

TEST(InstallerCdnManifestTest, BuildMilestoneUrlWithChannel) {
  const std::string base = "https://cef-builds.spotifycdn.com/";

  // Empty channel = stable (no suffix)
  EXPECT_EQ("https://cef-builds.spotifycdn.com/137.json",
            BuildMilestoneUrl(base, 137, ""));

  // Beta channel
  EXPECT_EQ("https://cef-builds.spotifycdn.com/137_beta.json",
            BuildMilestoneUrl(base, 137, "beta"));
}

TEST(InstallerCdnManifestTest, BuildPlatformUrlWithChannel) {
  const std::string base = "https://cef-builds.spotifycdn.com/";

  // Empty channel = stable (no suffix)
  EXPECT_EQ("https://cef-builds.spotifycdn.com/137_windows64.json",
            BuildPlatformUrl(base, 137, "windows64", ""));

  // Beta channel
  EXPECT_EQ("https://cef-builds.spotifycdn.com/137_windows64_beta.json",
            BuildPlatformUrl(base, 137, "windows64", "beta"));
}

TEST(InstallerCdnManifestTest, BuildAbiHashUrlWithChannel) {
  const std::string base = "https://cef-builds.spotifycdn.com/";

  // Empty channel = stable (no suffix)
  EXPECT_EQ("https://cef-builds.spotifycdn.com/abc123_windows64.json",
            BuildAbiHashUrl(base, "abc123", "windows64", ""));

  // Beta channel
  EXPECT_EQ("https://cef-builds.spotifycdn.com/abc123_windows64_beta.json",
            BuildAbiHashUrl(base, "abc123", "windows64", "beta"));
}

// ============================================================================
// FindBestBuildEntry tests
// ============================================================================

TEST(InstallerCdnManifestTest, FindBestBuildEntry) {
  std::vector<CdnBuildEntry> entries;

  CdnBuildEntry e1;
  e1.version = Version::Parse("137.1.0");
  e1.abi_hash = "abc123";
  e1.file = "file1.tar.xz";
  e1.sha1 = "hash1";
  entries.push_back(e1);

  CdnBuildEntry e2;
  e2.version = Version::Parse("137.3.0");
  e2.abi_hash = "abc123";
  e2.file = "file2.tar.xz";
  e2.sha1 = "hash2";
  entries.push_back(e2);

  CdnBuildEntry e3;
  e3.version = Version::Parse("137.2.0");
  e3.abi_hash = "def456";  // Different abi_hash
  e3.file = "file3.tar.xz";
  e3.sha1 = "hash3";
  entries.push_back(e3);

  // Should find newest with matching abi_hash
  auto result = FindBestBuildEntry(entries, "137.0", "137.99", "abc123");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("137.3.0", result->version.ToString());
  EXPECT_EQ("abc123", result->abi_hash);
}

TEST(InstallerCdnManifestTest, FindBestBuildEntryEmptyAbiHash) {
  std::vector<CdnBuildEntry> entries;

  CdnBuildEntry e1;
  e1.version = Version::Parse("137.1.0");
  e1.abi_hash = "abc123";
  e1.file = "file1.tar.xz";
  e1.sha1 = "hash1";
  entries.push_back(e1);

  CdnBuildEntry e2;
  e2.version = Version::Parse("137.3.0");
  e2.abi_hash = "def456";
  e2.file = "file2.tar.xz";
  e2.sha1 = "hash2";
  entries.push_back(e2);

  // Empty abi_hash means match any
  auto result = FindBestBuildEntry(entries, "137.0", "137.99", "");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("137.3.0",
            result->version.ToString());  // Newest regardless of hash
}

TEST(InstallerCdnManifestTest, FindBestBuildEntryNoMatch) {
  std::vector<CdnBuildEntry> entries;

  CdnBuildEntry e1;
  e1.version = Version::Parse("136.1.0");  // Below range
  e1.abi_hash = "abc123";
  e1.file = "file1.tar.xz";
  e1.sha1 = "hash1";
  entries.push_back(e1);

  auto result = FindBestBuildEntry(entries, "137.0", "137.99", "abc123");

  EXPECT_FALSE(result.has_value());
}

TEST(InstallerCdnManifestTest, FindBestBuildEntryVersionRange) {
  std::vector<CdnBuildEntry> entries;

  CdnBuildEntry e1;
  e1.version = Version::Parse("137.1.0");
  e1.abi_hash = "abc123";
  e1.file = "file1.tar.xz";
  e1.sha1 = "hash1";
  entries.push_back(e1);

  CdnBuildEntry e2;
  e2.version = Version::Parse("137.5.0");  // Above range
  e2.abi_hash = "abc123";
  e2.file = "file2.tar.xz";
  e2.sha1 = "hash2";
  entries.push_back(e2);

  // Max version is 137.3
  auto result = FindBestBuildEntry(entries, "137.0", "137.3", "abc123");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("137.1.0", result->version.ToString());  // Only one in range
}

TEST(InstallerCdnManifestTest, FindBestBuildEntryEmptyList) {
  std::vector<CdnBuildEntry> entries;

  auto result = FindBestBuildEntry(entries, "137.0", "137.99", "abc123");

  EXPECT_FALSE(result.has_value());
}

TEST(InstallerCdnManifestTest, FindBestBuildEntrySkipsInstalledVersions) {
  std::vector<CdnBuildEntry> entries;

  CdnBuildEntry e1;
  e1.version = Version::Parse("137.1.0");
  e1.abi_hash = "abc123";
  e1.file = "file1.tar.xz";
  e1.sha1 = "hash1";
  entries.push_back(e1);

  CdnBuildEntry e2;
  e2.version = Version::Parse("137.2.0");
  e2.abi_hash = "abc123";
  e2.file = "file2.tar.xz";
  e2.sha1 = "hash2";
  entries.push_back(e2);

  CdnBuildEntry e3;
  e3.version = Version::Parse("137.3.0");
  e3.abi_hash = "abc123";
  e3.file = "file3.tar.xz";
  e3.sha1 = "hash3";
  entries.push_back(e3);

  std::set<Version> skip;
  skip.insert(e2.version);
  skip.insert(e3.version);

  auto result = FindBestBuildEntry(entries, "137.0", "137.99", "abc123", skip);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(e1.version, result->version);
}

TEST(InstallerCdnManifestTest, FindBestBuildEntryAllSkipped) {
  std::vector<CdnBuildEntry> entries;

  CdnBuildEntry e1;
  e1.version = Version::Parse("137.1.0");
  e1.abi_hash = "abc123";
  e1.file = "file1.tar.xz";
  e1.sha1 = "hash1";
  entries.push_back(e1);

  std::set<Version> skip;
  skip.insert(e1.version);

  auto result = FindBestBuildEntry(entries, "137.0", "137.99", "abc123", skip);

  EXPECT_FALSE(result.has_value());
}

TEST(InstallerCdnManifestTest,
     NoMatchingVersionMessageListsFailedRequirements) {
  CdnBuildEntry below_and_wrong_abi;
  below_and_wrong_abi.version = Version::Parse("136.1.0");
  below_and_wrong_abi.abi_hash = "def456";

  CdnBuildEntry above;
  above.version = Version::Parse("138.1.0");
  above.abi_hash = "abc123";

  CdnBuildEntry excluded;
  excluded.version = Version::Parse("137.2.0");
  excluded.abi_hash = "abc123";

  CdnBuildExclusionReasons exclusion_reasons = {
      {excluded.version, {CdnBuildExclusionReason::kRevoked}}};
  const std::string message = BuildNoMatchingCdnVersionMessage(
      {below_and_wrong_abi, above, excluded}, "windows64", "137.0", "137.99",
      "abc123", exclusion_reasons);

  EXPECT_NE(std::string::npos,
            message.find("platform = windows64, version >= 137.0, version <= "
                         "137.99, ABI hash = abc123"));
  EXPECT_NE(std::string::npos,
            message.find("136.1.0 [ABI hash mismatch (got def456), version "
                         "below vmin]"));
  EXPECT_NE(std::string::npos, message.find("138.1.0 [version above vmax]"));
  EXPECT_NE(std::string::npos, message.find("137.2.0 [excluded: revoked]"));
}

TEST(InstallerCdnManifestTest,
     NoMatchingVersionMessageListsEachExclusionReason) {
  CdnBuildEntry installed;
  installed.version = Version::Parse("137.1.0");
  CdnBuildEntry disqualified;
  disqualified.version = Version::Parse("137.2.0");
  CdnBuildEntry failed;
  failed.version = Version::Parse("137.3.0");
  CdnBuildExclusionReasons exclusion_reasons = {
      {installed.version, {CdnBuildExclusionReason::kAlreadyInstalled}},
      {disqualified.version,
       {CdnBuildExclusionReason::kLaunchDisqualified,
        CdnBuildExclusionReason::kRevoked}},
      {failed.version, {CdnBuildExclusionReason::kPriorDownloadFailure}}};

  const std::string message = BuildNoMatchingCdnVersionMessage(
      {installed, disqualified, failed}, "windows64", "137.0", "", "",
      exclusion_reasons);

  EXPECT_NE(std::string::npos,
            message.find("137.1.0 [excluded: already installed]"));
  EXPECT_NE(std::string::npos,
            message.find("137.2.0 [excluded: revoked, disqualified by launch "
                         "health]"));
  EXPECT_NE(std::string::npos,
            message.find("137.3.0 [excluded: prior download failure]"));
}

TEST(InstallerCdnManifestTest,
     NoMatchingVersionMessageReportsMissingCandidates) {
  const std::string message =
      BuildNoMatchingCdnVersionMessage({}, "windows64", "151.1", "", "abc123");

  EXPECT_EQ(
      "No CDN version matches requirements (platform = windows64, version >= "
      "151.1, ABI hash = abc123); no validated CDN manifest candidates were "
      "available",
      message);
}

// ============================================================================
// Security: Filename Validation Tests (M1)
// ============================================================================

TEST(InstallerCdnManifestTest, ParseBuildEntryRejectsPathTraversal) {
  // Filename with ".." should be rejected
  const char* json = R"({
    "windows64": {
      "file": "../../../evil.tar.xz",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

TEST(InstallerCdnManifestTest, ParseBuildEntryRejectsForwardSlash) {
  // Filename with "/" should be rejected
  const char* json = R"({
    "windows64": {
      "file": "subdir/archive.tar.xz",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

TEST(InstallerCdnManifestTest, ParseBuildEntryRejectsBackslash) {
  // Filename with "\" should be rejected
  const char* json = R"({
    "windows64": {
      "file": "subdir\\archive.tar.xz",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

TEST(InstallerCdnManifestTest, ParseBuildEntryRejectsUrlScheme) {
  // Filename with ":" (URL scheme indicator) should be rejected
  const char* json = R"({
    "windows64": {
      "file": "https://evil.com/malware.tar.xz",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

TEST(InstallerCdnManifestTest, ParseBuildEntryRejectsHiddenFile) {
  // Filename starting with "." should be rejected
  const char* json = R"({
    "windows64": {
      "file": ".hidden.tar.xz",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

TEST(InstallerCdnManifestTest, ParseBuildEntryRejectsInvalidExtension) {
  // Filename without valid archive extension should be rejected
  const char* json = R"({
    "windows64": {
      "file": "archive.exe",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

TEST(InstallerCdnManifestTest, ParseBuildEntryAcceptsValidExtensions) {
  // Only .tar.xz is supported by the extraction code (ExtractTarXz).
  const char* json_tarxz = R"({
    "windows64": {
      "file": "cef_binary_137.3.5.tar.xz",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json_tarxz, &entries);
  EXPECT_EQ(ManifestError::kSuccess, result);
}

TEST(InstallerCdnManifestTest, ParseBuildEntryRejectsUnsupportedExtensions) {
  // .7z and .zip are not supported by the extraction code.
  const char* json_7z = R"({
    "windows64": {
      "file": "cef_binary_137.3.5.7z",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json_7z, &entries);
  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);

  const char* json_zip = R"({
    "windows64": {
      "file": "cef_binary_137.3.5.zip",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    }
  })";

  entries.clear();
  result = ParseMilestoneManifest(json_zip, &entries);
  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

// ============================================================================
// Security: Field Length Limit Tests (M3)
// ============================================================================

TEST(InstallerCdnManifestTest, ParseBuildEntryRejectsOverlyLongVersion) {
  // Version field longer than 64 chars should be rejected
  std::string long_version(100, '1');  // 100 chars
  std::string json = R"({
    "windows64": {
      "file": "archive.tar.xz",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": ")" +
                     long_version + R"("
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

TEST(InstallerCdnManifestTest, ParseBuildEntryRejectsOverlyLongSha1) {
  // SHA1 field longer than 40 chars should be rejected
  std::string long_sha1(50, 'a');  // 50 chars (should be 40)
  std::string json = R"({
    "windows64": {
      "file": "archive.tar.xz",
      "sha1": ")" + long_sha1 +
                     R"(",
      "version": "137.3.5"
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

TEST(InstallerCdnManifestTest, ParseBuildEntryRejectsOverlyLongAbiHash) {
  // abi_hash field longer than 256 chars should be rejected
  std::string long_hash(300, 'x');  // 300 chars
  std::string json = R"({
    "windows64": {
      "file": "archive.tar.xz",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5",
      "abi_hash": ")" +
                     long_hash + R"("
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

TEST(InstallerCdnManifestTest, ParseBuildEntryRejectsOverlyLongTimestamp) {
  // last_modified field longer than 64 chars should be rejected
  std::string long_timestamp(100, 'T');  // 100 chars
  std::string json = R"({
    "windows64": {
      "file": "archive.tar.xz",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5",
      "last_modified": ")" +
                     long_timestamp + R"("
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

TEST(InstallerCdnManifestTest, ParseBuildEntryRejectsOverlyLongFilename) {
  // Filename longer than 256 chars should be rejected
  std::string long_name(300, 'f');
  long_name += ".tar.xz";  // Add valid extension
  std::string json = R"({
    "windows64": {
      "file": ")" + long_name +
                     R"(",
      "sha1": "5a7eeda17c97c9feff93f8c196d15f3145f8127e",
      "version": "137.3.5"
    }
  })";

  std::map<std::string, CdnBuildEntry> entries;
  ManifestError result = ParseMilestoneManifest(json, &entries);

  EXPECT_EQ(ManifestError::kInvalidFieldValue, result);
}

// ============================================================================
// ManifestErrorToString tests
// ============================================================================

TEST(InstallerCdnManifestTest, ManifestErrorToString) {
  EXPECT_STREQ("Success", ManifestErrorToString(ManifestError::kSuccess));
  EXPECT_STREQ("JSON parse error",
               ManifestErrorToString(ManifestError::kJsonParseError));
  EXPECT_STREQ("Missing required field",
               ManifestErrorToString(ManifestError::kMissingRequiredField));
  EXPECT_STREQ("Invalid field value",
               ManifestErrorToString(ManifestError::kInvalidFieldValue));
}

}  // namespace
}  // namespace cef_installer
