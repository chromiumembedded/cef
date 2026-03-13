// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_revocation.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

RevokedVersionRange MakeRevokedRange(const std::string& version_min,
                                     const std::string& version_max,
                                     const std::string& reason = "",
                                     const std::string& revoked_at = "") {
  RevokedVersionRange rv;
  rv.version_min = Version::Parse(version_min);
  rv.version_max = Version::Parse(version_max);
  rv.reason = reason;
  rv.revoked_at = revoked_at;
  return rv;
}

RevokedVersionRange MakeRevokedPoint(const std::string& version,
                                     const std::string& reason = "",
                                     const std::string& revoked_at = "") {
  return MakeRevokedRange(version, version, reason, revoked_at);
}

// ============================================================================
// Parsing Tests
// ============================================================================

TEST(InstallerRevocationTest, ParseValid) {
  const char* json = R"({
    "revoked_versions": [
      {"version": "137.1.0", "reason": "CVE-2024-12345", "revoked_at": "2024-06-01T00:00:00Z"},
      {"version": "137.2.0", "reason": "Security issue", "revoked_at": "2024-06-15T12:00:00Z"}
    ],
    "signature": "test_signature"
  })";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);

  EXPECT_EQ(RevocationError::kSuccess, result);
  ASSERT_EQ(2u, revoked.size());

  // Single-version entries: min == max.
  EXPECT_EQ("137.1.0", revoked[0].version_min.ToString());
  EXPECT_EQ("137.1.0", revoked[0].version_max.ToString());
  EXPECT_EQ("CVE-2024-12345", revoked[0].reason);
  EXPECT_EQ("2024-06-01T00:00:00Z", revoked[0].revoked_at);

  EXPECT_EQ("137.2.0", revoked[1].version_min.ToString());
  EXPECT_EQ("137.2.0", revoked[1].version_max.ToString());
  EXPECT_EQ("Security issue", revoked[1].reason);
  EXPECT_EQ("2024-06-15T12:00:00Z", revoked[1].revoked_at);
}

TEST(InstallerRevocationTest, ParseRangeFormat) {
  const char* json = R"({
    "revoked_versions": [
      {"version_min": "137", "version_max": "137.4", "reason": "CVE-A"},
      {"version_min": "138.1.0", "version_max": "138.3.0", "reason": "CVE-B"}
    ]
  })";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);

  EXPECT_EQ(RevocationError::kSuccess, result);
  ASSERT_EQ(2u, revoked.size());

  EXPECT_EQ("137", revoked[0].version_min.ToString());
  EXPECT_EQ("137.4", revoked[0].version_max.ToString());
  EXPECT_EQ("CVE-A", revoked[0].reason);

  EXPECT_EQ("138.1.0", revoked[1].version_min.ToString());
  EXPECT_EQ("138.3.0", revoked[1].version_max.ToString());
  EXPECT_EQ("CVE-B", revoked[1].reason);
}

TEST(InstallerRevocationTest, ParseMixedFormats) {
  const char* json = R"({
    "revoked_versions": [
      {"version": "137.1.0", "reason": "point"},
      {"version_min": "138", "version_max": "138.5", "reason": "range"}
    ]
  })";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);

  EXPECT_EQ(RevocationError::kSuccess, result);
  ASSERT_EQ(2u, revoked.size());

  EXPECT_EQ("137.1.0", revoked[0].version_min.ToString());
  EXPECT_EQ("137.1.0", revoked[0].version_max.ToString());

  EXPECT_EQ("138", revoked[1].version_min.ToString());
  EXPECT_EQ("138.5", revoked[1].version_max.ToString());
}

TEST(InstallerRevocationTest, ParseRangeMinExceedsMax) {
  const char* json = R"({
    "revoked_versions": [
      {"version_min": "138.0", "version_max": "137.0"}
    ]
  })";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);
  EXPECT_EQ(RevocationError::kJsonParseError, result);
}

TEST(InstallerRevocationTest, ParseRangeMinOnlyRejects) {
  // version_min without version_max is invalid.
  const char* json = R"({
    "revoked_versions": [
      {"version_min": "137.0"}
    ]
  })";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);
  EXPECT_EQ(RevocationError::kJsonParseError, result);
}

TEST(InstallerRevocationTest, ParseEmpty) {
  const char* json = R"({
    "revoked_versions": [],
    "signature": "test_signature"
  })";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);

  EXPECT_EQ(RevocationError::kSuccess, result);
  EXPECT_TRUE(revoked.empty());
}

TEST(InstallerRevocationTest, ParseEmptyObject) {
  const char* json = R"({})";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);

  EXPECT_EQ(RevocationError::kSuccess, result);
  EXPECT_TRUE(revoked.empty());
}

TEST(InstallerRevocationTest, ParseMalformed) {
  const char* json = "not valid json {";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);

  EXPECT_EQ(RevocationError::kJsonParseError, result);
}

TEST(InstallerRevocationTest, ParseMalformedNotObject) {
  const char* json = R"([1, 2, 3])";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);

  EXPECT_EQ(RevocationError::kJsonParseError, result);
}

TEST(InstallerRevocationTest, ParseMalformedMissingVersion) {
  const char* json = R"({
    "revoked_versions": [
      {"reason": "CVE-2024-12345", "revoked_at": "2024-06-01T00:00:00Z"}
    ]
  })";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);

  EXPECT_EQ(RevocationError::kJsonParseError, result);
}

// ============================================================================
// IsVersionRevoked Tests
// ============================================================================

TEST(InstallerRevocationTest, IsRevokedTrue) {
  std::vector<RevokedVersionRange> revoked;
  revoked.push_back(
      MakeRevokedPoint("137.1.0", "CVE-2024-12345", "2024-06-01T00:00:00Z"));
  revoked.push_back(
      MakeRevokedPoint("137.2.0", "Security issue", "2024-06-15T12:00:00Z"));

  EXPECT_TRUE(IsVersionRevoked(Version::Parse("137.1.0"), revoked));
}

TEST(InstallerRevocationTest, IsRevokedFalse) {
  std::vector<RevokedVersionRange> revoked;
  revoked.push_back(
      MakeRevokedPoint("137.1.0", "CVE-2024-12345", "2024-06-01T00:00:00Z"));
  revoked.push_back(
      MakeRevokedPoint("137.2.0", "Security issue", "2024-06-15T12:00:00Z"));

  EXPECT_FALSE(IsVersionRevoked(Version::Parse("137.3.0"), revoked));
}

TEST(InstallerRevocationTest, IsRevokedInRange) {
  std::vector<RevokedVersionRange> revoked;
  revoked.push_back(MakeRevokedRange("137", "137.4", "CVE-A"));

  // In range.
  EXPECT_TRUE(IsVersionRevoked(Version::Parse("137.0.0"), revoked));
  EXPECT_TRUE(IsVersionRevoked(Version::Parse("137.2.0"), revoked));
  EXPECT_TRUE(IsVersionRevoked(Version::Parse("137.4"), revoked));

  // Out of range.
  EXPECT_FALSE(IsVersionRevoked(Version::Parse("137.4.1"), revoked));
  EXPECT_FALSE(IsVersionRevoked(Version::Parse("137.5"), revoked));
  EXPECT_FALSE(IsVersionRevoked(Version::Parse("136.9.9"), revoked));
}

TEST(InstallerRevocationTest, IsRevokedPartialVersionRange) {
  // min=137 means 137.0.0..., max=137.4 means up to 137.4.0...
  std::vector<RevokedVersionRange> revoked;
  revoked.push_back(MakeRevokedRange("137", "137.4"));

  // "137" equals "137.0" equals "137.0.0" — should be in range.
  EXPECT_TRUE(IsVersionRevoked(Version::Parse("137"), revoked));
  EXPECT_TRUE(IsVersionRevoked(Version::Parse("137.0"), revoked));
  EXPECT_TRUE(IsVersionRevoked(Version::Parse("137.0.0"), revoked));
  EXPECT_TRUE(IsVersionRevoked(Version::Parse("137.3.5"), revoked));
  EXPECT_TRUE(IsVersionRevoked(Version::Parse("137.4"), revoked));
  EXPECT_TRUE(IsVersionRevoked(Version::Parse("137.4.0"), revoked));

  // 137.4.1 > 137.4 — out of range.
  EXPECT_FALSE(IsVersionRevoked(Version::Parse("137.4.1"), revoked));
}

TEST(InstallerRevocationTest, IsRevokedInvalidVersion) {
  std::vector<RevokedVersionRange> revoked;
  revoked.push_back(
      MakeRevokedPoint("137.1.0", "CVE-2024-12345", "2024-06-01T00:00:00Z"));

  Version v;  // Invalid version
  EXPECT_FALSE(IsVersionRevoked(v, revoked));
}

TEST(InstallerRevocationTest, IsRevokedEmptyList) {
  std::vector<RevokedVersionRange> revoked;

  EXPECT_FALSE(IsVersionRevoked(Version::Parse("137.1.0"), revoked));
}

// ============================================================================
// Filter Tests
// ============================================================================

TEST(InstallerRevocationTest, FilterRemovesRevoked) {
  std::vector<RevokedVersionRange> revoked;
  revoked.push_back(MakeRevokedRange("137.1.0", "137.3.0", "CVE-A"));

  std::vector<Version> versions;
  versions.push_back(Version::Parse("137.0.0"));  // Not revoked
  versions.push_back(Version::Parse("137.1.0"));  // Revoked (at range start)
  versions.push_back(Version::Parse("137.2.0"));  // Revoked (in range)
  versions.push_back(Version::Parse("137.3.0"));  // Revoked (at range end)
  versions.push_back(Version::Parse("137.4.0"));  // Not revoked

  std::vector<Version> filtered = FilterRevokedVersions(versions, revoked);

  ASSERT_EQ(2u, filtered.size());
  EXPECT_EQ("137.0.0", filtered[0].ToString());
  EXPECT_EQ("137.4.0", filtered[1].ToString());
}

TEST(InstallerRevocationTest, FilterEmptyInput) {
  std::vector<RevokedVersionRange> revoked;
  revoked.push_back(
      MakeRevokedPoint("137.1.0", "CVE-2024-12345", "2024-06-01T00:00:00Z"));

  std::vector<Version> versions;
  std::vector<Version> filtered = FilterRevokedVersions(versions, revoked);

  EXPECT_TRUE(filtered.empty());
}

TEST(InstallerRevocationTest, FilterEmptyRevokedList) {
  std::vector<RevokedVersionRange> revoked;

  std::vector<Version> versions;
  versions.push_back(Version::Parse("137.1.0"));
  versions.push_back(Version::Parse("137.2.0"));

  std::vector<Version> filtered = FilterRevokedVersions(versions, revoked);

  ASSERT_EQ(2u, filtered.size());
  EXPECT_EQ("137.1.0", filtered[0].ToString());
  EXPECT_EQ("137.2.0", filtered[1].ToString());
}

// ============================================================================
// Error String Tests
// ============================================================================

TEST(InstallerRevocationTest, ErrorToString) {
  EXPECT_STREQ("Success", RevocationErrorToString(RevocationError::kSuccess));
  EXPECT_STREQ("JSON parse error",
               RevocationErrorToString(RevocationError::kJsonParseError));
  EXPECT_STREQ("Write error",
               RevocationErrorToString(RevocationError::kWriteError));
}

// ============================================================================
// Compiled-In Revocation Baseline Tests
// ============================================================================

TEST(InstallerRevocationTest, LoadCompiledRevocationList_ReturnsEntries) {
  std::vector<RevokedVersionRange> compiled = LoadCompiledRevocationList();
  EXPECT_TRUE(compiled.empty());

  // Verify range parsing works with actual data.
  const char* json = R"({
    "revoked_versions": [
      {"version_min": "148", "version_max": "148.2.0", "reason": "CVE-2026-XXXXX", "revoked_at": "2026-03-05T00:00:00Z"},
      {"version": "149.0.0", "reason": "CVE-2026-YYYYY", "revoked_at": "2026-03-06T00:00:00Z"}
    ]
  })";
  std::vector<RevokedVersionRange> parsed;
  EXPECT_EQ(RevocationError::kSuccess, ParseRevocationList(json, &parsed));
  ASSERT_EQ(2u, parsed.size());
  EXPECT_EQ("148", parsed[0].version_min.ToString());
  EXPECT_EQ("148.2.0", parsed[0].version_max.ToString());
  EXPECT_EQ("149.0.0", parsed[1].version_min.ToString());
  EXPECT_EQ("149.0.0", parsed[1].version_max.ToString());
}

// ============================================================================
// Merge Tests
// ============================================================================

TEST(InstallerRevocationTest, MergeRevocationLists_CombinesDisjoint) {
  std::vector<RevokedVersionRange> compiled = {
      MakeRevokedPoint("137.1.0", "reason_a", "2024-01-01T00:00:00Z"),
  };
  std::vector<RevokedVersionRange> additional = {
      MakeRevokedPoint("148.2.0", "reason_b", "2026-03-05T00:00:00Z"),
  };

  auto merged = MergeRevocationLists(compiled, additional);

  // Disjoint ranges stay separate, sorted by version_min.
  ASSERT_EQ(2u, merged.size());
  EXPECT_EQ("137.1.0", merged[0].version_min.ToString());
  EXPECT_EQ("148.2.0", merged[1].version_min.ToString());
}

TEST(InstallerRevocationTest, MergeRevocationLists_MergesOverlapping) {
  std::vector<RevokedVersionRange> compiled = {
      MakeRevokedRange("137.1.0", "137.5.0", "CVE-A"),
  };
  std::vector<RevokedVersionRange> additional = {
      MakeRevokedRange("137.3.0", "137.8.0", "CVE-B"),
  };

  auto merged = MergeRevocationLists(compiled, additional);

  ASSERT_EQ(1u, merged.size());
  EXPECT_EQ("137.1.0", merged[0].version_min.ToString());
  EXPECT_EQ("137.8.0", merged[0].version_max.ToString());
  EXPECT_EQ("CVE-A; CVE-B", merged[0].reason);
}

TEST(InstallerRevocationTest, MergeRevocationLists_MergesTouching) {
  // Ranges that touch (curr.min == prev.max) should merge.
  std::vector<RevokedVersionRange> compiled = {
      MakeRevokedRange("137.1.0", "137.5.0", "CVE-A"),
  };
  std::vector<RevokedVersionRange> additional = {
      MakeRevokedRange("137.5.0", "137.8.0", "CVE-B"),
  };

  auto merged = MergeRevocationLists(compiled, additional);

  ASSERT_EQ(1u, merged.size());
  EXPECT_EQ("137.1.0", merged[0].version_min.ToString());
  EXPECT_EQ("137.8.0", merged[0].version_max.ToString());
}

TEST(InstallerRevocationTest, MergeRevocationLists_ContainedRange) {
  // One range completely contains the other.
  std::vector<RevokedVersionRange> compiled = {
      MakeRevokedRange("137.0.0", "137.9.0", "CVE-A"),
  };
  std::vector<RevokedVersionRange> additional = {
      MakeRevokedRange("137.3.0", "137.5.0", "CVE-B"),
  };

  auto merged = MergeRevocationLists(compiled, additional);

  ASSERT_EQ(1u, merged.size());
  EXPECT_EQ("137.0.0", merged[0].version_min.ToString());
  EXPECT_EQ("137.9.0", merged[0].version_max.ToString());
  EXPECT_EQ("CVE-A; CVE-B", merged[0].reason);
}

TEST(InstallerRevocationTest, MergeRevocationLists_DuplicatePointRanges) {
  // Same version appears in both lists as point revocations.
  std::vector<RevokedVersionRange> compiled = {
      MakeRevokedPoint("137.1.0", "reason_compiled"),
  };
  std::vector<RevokedVersionRange> additional = {
      MakeRevokedPoint("137.1.0", "reason_cdn"),
      MakeRevokedPoint("148.2.0", "reason_new"),
  };

  auto merged = MergeRevocationLists(compiled, additional);

  ASSERT_EQ(2u, merged.size());
  EXPECT_EQ("137.1.0", merged[0].version_min.ToString());
  EXPECT_EQ("137.1.0", merged[0].version_max.ToString());
  // Reasons concatenated.
  EXPECT_EQ("reason_compiled; reason_cdn", merged[0].reason);
  EXPECT_EQ("148.2.0", merged[1].version_min.ToString());
}

TEST(InstallerRevocationTest, MergeRevocationLists_CompiledCannotShrink) {
  std::vector<RevokedVersionRange> compiled = {
      MakeRevokedPoint("137.1.0", "reason_a", "2024-01-01T00:00:00Z"),
      MakeRevokedPoint("137.2.0", "reason_b", "2024-02-01T00:00:00Z"),
  };
  std::vector<RevokedVersionRange> additional;  // Empty — compiled preserved.

  auto merged = MergeRevocationLists(compiled, additional);

  ASSERT_EQ(2u, merged.size());
  EXPECT_EQ("137.1.0", merged[0].version_min.ToString());
  EXPECT_EQ("137.2.0", merged[1].version_min.ToString());
}

TEST(InstallerRevocationTest, MergeRevocationLists_SortsByVersionMin) {
  // Entries are unsorted; merge should sort by version_min.
  std::vector<RevokedVersionRange> compiled = {
      MakeRevokedPoint("148.0.0", "late"),
  };
  std::vector<RevokedVersionRange> additional = {
      MakeRevokedPoint("137.0.0", "early"),
  };

  auto merged = MergeRevocationLists(compiled, additional);

  ASSERT_EQ(2u, merged.size());
  EXPECT_EQ("137.0.0", merged[0].version_min.ToString());
  EXPECT_EQ("148.0.0", merged[1].version_min.ToString());
}

TEST(InstallerRevocationTest, MergeRevocationLists_ChainMerge) {
  // Multiple overlapping ranges should all merge into one.
  std::vector<RevokedVersionRange> compiled = {
      MakeRevokedRange("137.0.0", "137.3.0", "A"),
      MakeRevokedRange("137.5.0", "137.8.0", "C"),
  };
  std::vector<RevokedVersionRange> additional = {
      MakeRevokedRange("137.2.0", "137.6.0", "B"),
  };

  auto merged = MergeRevocationLists(compiled, additional);

  // All three merge into one: [137.0.0, 137.8.0].
  ASSERT_EQ(1u, merged.size());
  EXPECT_EQ("137.0.0", merged[0].version_min.ToString());
  EXPECT_EQ("137.8.0", merged[0].version_max.ToString());
  EXPECT_EQ("A; B; C", merged[0].reason);
}

TEST(InstallerRevocationTest, MergeRevocationLists_PartialVersions) {
  // Support min=137 max=137.4 (different component counts).
  std::vector<RevokedVersionRange> compiled = {
      MakeRevokedRange("137", "137.4", "CVE-A"),
  };
  std::vector<RevokedVersionRange> additional = {
      MakeRevokedRange("137.3", "138", "CVE-B"),
  };

  auto merged = MergeRevocationLists(compiled, additional);

  ASSERT_EQ(1u, merged.size());
  EXPECT_EQ("137", merged[0].version_min.ToString());
  EXPECT_EQ("138", merged[0].version_max.ToString());
  EXPECT_EQ("CVE-A; CVE-B", merged[0].reason);
}

TEST(InstallerRevocationTest, MergeRevocationLists_EmptyReasonSkipped) {
  std::vector<RevokedVersionRange> compiled = {
      MakeRevokedRange("137.0.0", "137.5.0", "CVE-A"),
  };
  std::vector<RevokedVersionRange> additional = {
      MakeRevokedRange("137.3.0", "137.8.0"),  // Empty reason.
  };

  auto merged = MergeRevocationLists(compiled, additional);

  ASSERT_EQ(1u, merged.size());
  // Empty reason not appended.
  EXPECT_EQ("CVE-A", merged[0].reason);
}

// ============================================================================
// Disk Cache Tests
// ============================================================================

TEST(InstallerRevocationTest, WriteAndLoadRevocationCache_RoundTrip) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::vector<RevokedVersionRange> cdn_fetched = {
      MakeRevokedRange("137.1.0", "137.3.0", "reason_a",
                       "2024-01-01T00:00:00Z"),
      MakeRevokedPoint("148.2.0", "reason_b", "2026-03-05T00:00:00Z"),
  };

  EXPECT_EQ(RevocationError::kSuccess,
            WriteRevocationCache(temp_dir.GetPath(), cdn_fetched));

  auto loaded = LoadRevocationCache(temp_dir.GetPath());

  ASSERT_EQ(2u, loaded.size());
  EXPECT_EQ("137.1.0", loaded[0].version_min.ToString());
  EXPECT_EQ("137.3.0", loaded[0].version_max.ToString());
  EXPECT_EQ("reason_a", loaded[0].reason);
  EXPECT_EQ("148.2.0", loaded[1].version_min.ToString());
  EXPECT_EQ("148.2.0", loaded[1].version_max.ToString());
  EXPECT_EQ("reason_b", loaded[1].reason);
}

TEST(InstallerRevocationTest, WriteRevocationCache_WritesRangeFormat) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::vector<RevokedVersionRange> cdn_fetched = {
      MakeRevokedRange("137.1.0", "137.5.0", "reason_a",
                       "2024-01-01T00:00:00Z"),
      MakeRevokedPoint("149.0.0", "reason_c", "2026-03-06T00:00:00Z"),
  };

  EXPECT_EQ(RevocationError::kSuccess,
            WriteRevocationCache(temp_dir.GetPath(), cdn_fetched));

  base::FilePath path = temp_dir.GetPath().Append(kRevocationCacheFilename);
  std::string content;
  IntegrityResult ir = ReadFileWithIntegrity(path, &content);
  ASSERT_TRUE(ir == IntegrityResult::kSuccess ||
              ir == IntegrityResult::kSuccessNoFooter);
  // Written in version_min/version_max format.
  EXPECT_NE(std::string::npos, content.find("version_min"));
  EXPECT_NE(std::string::npos, content.find("version_max"));
  EXPECT_NE(std::string::npos, content.find("137.1.0"));
  EXPECT_NE(std::string::npos, content.find("137.5.0"));
  EXPECT_NE(std::string::npos, content.find("149.0.0"));
}

TEST(InstallerRevocationTest, LoadRevocationCache_MissingFile_ReturnsEmpty) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  auto loaded = LoadRevocationCache(temp_dir.GetPath());
  EXPECT_TRUE(loaded.empty());
}

TEST(InstallerRevocationTest, LoadRevocationCache_CorruptJson_ReturnsEmpty) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath path = temp_dir.GetPath().Append(kRevocationCacheFilename);
  ASSERT_TRUE(base::WriteFile(path, "not valid json {"));

  auto loaded = LoadRevocationCache(temp_dir.GetPath());
  EXPECT_TRUE(loaded.empty());
}

TEST(InstallerRevocationTest,
     RetentionLoadRevocationCachePreservesIntegrityMismatch) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_EQ(RevocationError::kSuccess,
            WriteRevocationCache(temp_dir.GetPath(),
                                 {MakeRevokedPoint("149.0.0", "reason",
                                                   "2026-03-06T00:00:00Z")}));
  base::FilePath path = temp_dir.GetPath().Append(kRevocationCacheFilename);
  std::string corrupt;
  ASSERT_TRUE(base::ReadFileToString(path, &corrupt));
  ASSERT_GT(corrupt.size(), 20u);
  corrupt[5] ^= 0xFF;
  ASSERT_TRUE(base::WriteFile(path, corrupt));

  EXPECT_TRUE(LoadRevocationCache(temp_dir.GetPath(),
                                  IntegrityMismatchAction::kPreserve)
                  .empty());

  std::string after;
  ASSERT_TRUE(base::ReadFileToString(path, &after));
  EXPECT_EQ(corrupt, after);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(InstallerRevocationTest, ParseRevocationListNullOutput) {
  RevocationError result =
      ParseRevocationList(R"({"revoked_versions": []})", nullptr);

  EXPECT_EQ(RevocationError::kJsonParseError, result);
}

TEST(InstallerRevocationTest, ParseRevocationListEntryNotDict) {
  const char* json = R"({
    "revoked_versions": ["not_a_dict", 42]
  })";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);

  EXPECT_EQ(RevocationError::kJsonParseError, result);
}

TEST(InstallerRevocationTest, ParseRevocationListOptionalFieldsMissing) {
  const char* json = R"({
    "revoked_versions": [
      {"version": "137.1.0"}
    ]
  })";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);

  EXPECT_EQ(RevocationError::kSuccess, result);
  ASSERT_EQ(1u, revoked.size());
  EXPECT_EQ("137.1.0", revoked[0].version_min.ToString());
  EXPECT_EQ("137.1.0", revoked[0].version_max.ToString());
  EXPECT_TRUE(revoked[0].reason.empty());
  EXPECT_TRUE(revoked[0].revoked_at.empty());
}

TEST(InstallerRevocationTest, ParseRevocationListInvalidVersionString) {
  const char* json = R"({
    "revoked_versions": [
      {"version": "not-a-version"}
    ]
  })";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);
  EXPECT_EQ(RevocationError::kJsonParseError, result);
}

TEST(InstallerRevocationTest, ParseRevocationListInvalidRangeVersion) {
  const char* json = R"({
    "revoked_versions": [
      {"version_min": "137.0", "version_max": "not-valid"}
    ]
  })";

  std::vector<RevokedVersionRange> revoked;
  RevocationError result = ParseRevocationList(json, &revoked);
  EXPECT_EQ(RevocationError::kJsonParseError, result);
}

TEST(InstallerRevocationTest, CacheFreshnessUsesBoundedFileAge) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(dir.GetPath(), {}));
  base::Time now = base::Time::Now();
  EXPECT_TRUE(IsRevocationCacheFresh(dir.GetPath(), now));

  base::FilePath path = dir.GetPath().Append(kRevocationCacheFilename);
  base::Time stale = now - base::Seconds(kRevocationCacheValiditySeconds);
  ASSERT_TRUE(base::TouchFile(path, stale, stale));
  EXPECT_FALSE(IsRevocationCacheFresh(dir.GetPath(), now));
}

TEST(InstallerRevocationTest, CorruptFreshCacheIsNotFresh) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(dir.GetPath(), {}));
  base::FilePath path = dir.GetPath().Append(kRevocationCacheFilename);

  ASSERT_TRUE(base::WriteFile(path, R"({"revoked_versions": []})"));
  EXPECT_FALSE(IsRevocationCacheFresh(dir.GetPath(), base::Time::Now()));
}

TEST(InstallerRevocationTest, InvalidFreshCacheIsNotFresh) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath path = dir.GetPath().Append(kRevocationCacheFilename);
  ASSERT_TRUE(WriteFileWithIntegrity(path, "{"));

  EXPECT_FALSE(IsRevocationCacheFresh(dir.GetPath(), base::Time::Now()));
}

TEST(InstallerRevocationTest, FailureBackoffIsSourceScopedAndExpires) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::Time now = base::Time::Now();
  ASSERT_TRUE(RecordRevocationRefreshFailure(
      dir.GetPath(), "https://one.example/revoked.json", now));
  EXPECT_TRUE(IsRevocationRefreshBackedOff(
      dir.GetPath(), "https://one.example/revoked.json", now));
  EXPECT_FALSE(IsRevocationRefreshBackedOff(
      dir.GetPath(), "https://two.example/revoked.json", now));
  EXPECT_FALSE(IsRevocationRefreshBackedOff(
      dir.GetPath(), "https://one.example/revoked.json",
      now + base::Seconds(kRevocationFailureBackoffSeconds)));
}

TEST(InstallerRevocationTest, FailureBackoffHandlesClockSkew) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::Time future = base::Time::Now();
  ASSERT_TRUE(RecordRevocationRefreshFailure(
      dir.GetPath(), "https://example/revoked.json", future));
  EXPECT_TRUE(IsRevocationRefreshBackedOff(
      dir.GetPath(), "https://example/revoked.json",
      future - base::Seconds(kRevocationFailureBackoffSeconds)));
  EXPECT_FALSE(IsRevocationRefreshBackedOff(
      dir.GetPath(), "https://example/revoked.json",
      future - base::Seconds(kRevocationFailureBackoffSeconds + 1)));
}

TEST(InstallerRevocationTest, MalformedBackoffFailsOpen) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath path = dir.GetPath().Append(kRevocationBackoffFilename);
  ASSERT_TRUE(base::WriteFile(
      path, std::string(kMaxRevocationBackoffFileSize + 1, 'x')));
  EXPECT_FALSE(IsRevocationRefreshBackedOff(
      dir.GetPath(), "https://example/revoked.json", base::Time::Now()));
}

TEST(InstallerRevocationTest, BackoffWithoutIntegrityFooterFailsOpen) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const std::string source = "https://example/revoked.json";
  base::Time now = base::Time::Now();
  ASSERT_TRUE(RecordRevocationRefreshFailure(dir.GetPath(), source, now));
  base::FilePath path = dir.GetPath().Append(kRevocationBackoffFilename);
  std::string content;
  ASSERT_EQ(IntegrityResult::kSuccess, ReadFileWithIntegrity(path, &content));
  ASSERT_TRUE(base::WriteFile(path, content));

  EXPECT_FALSE(IsRevocationRefreshBackedOff(dir.GetPath(), source, now));
}

}  // namespace
}  // namespace cef_installer
