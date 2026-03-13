// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_version_metadata.h"

#include <atomic>
#include <thread>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

class InstallerVersionMetadataTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetMetadataPath() const {
    return temp_dir_.GetPath().Append(kVersionMetadataFilename);
  }

  void WriteJsonFile(const std::string& content) {
    ASSERT_TRUE(base::WriteFile(GetMetadataPath(), content));
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(InstallerVersionMetadataTest, ReadValid) {
  WriteJsonFile(R"({
    "version": "137.3.5",
    "abi_hash": "abc123def4567890",
    "platform": "windows64",
    "version_full": "137.3.5+g62d140e+chromium-137.0.7204.6"
  })");

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);

  EXPECT_EQ(MetadataError::kSuccess, error);
  EXPECT_TRUE(metadata.IsValid());
  EXPECT_EQ("137.3.5", metadata.version.ToString());
  EXPECT_EQ("abc123def4567890", metadata.abi_hash);
  EXPECT_EQ("windows64", metadata.platform);
  EXPECT_EQ("137.3.5+g62d140e+chromium-137.0.7204.6", metadata.version_full);
}

TEST_F(InstallerVersionMetadataTest, ReadMissingFile) {
  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);

  EXPECT_EQ(MetadataError::kFileNotFound, error);
}

TEST_F(InstallerVersionMetadataTest, ReadInvalidJson) {
  WriteJsonFile("not valid json {{{");

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);

  EXPECT_EQ(MetadataError::kJsonParseError, error);
}

TEST_F(InstallerVersionMetadataTest, ReadMissingAbiHash) {
  // Missing abi_hash
  WriteJsonFile(R"({
    "version": "137.3.5",
    "platform": "windows64"
  })");

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);

  EXPECT_EQ(MetadataError::kMissingRequiredField, error);
}

TEST_F(InstallerVersionMetadataTest, ReadMissingPlatform) {
  // Missing platform
  WriteJsonFile(R"({
    "version": "137.3.5",
    "abi_hash": "abc123def4567890"
  })");

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);

  EXPECT_EQ(MetadataError::kMissingRequiredField, error);
}

TEST_F(InstallerVersionMetadataTest, WriteCreatesFile) {
  VersionMetadata metadata;
  metadata.version = Version::Parse("137.3.5");
  metadata.abi_hash = "abc123def4567890";
  metadata.platform = "windows64";
  metadata.version_full = "137.3.5+g62d140e+chromium-137.0.7204.6";

  MetadataError error = WriteVersionMetadata(temp_dir_.GetPath(), metadata);

  EXPECT_EQ(MetadataError::kSuccess, error);
  EXPECT_TRUE(base::PathExists(GetMetadataPath()));

  // Verify content is valid JSON
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(GetMetadataPath(), &content));
  EXPECT_NE(content.find("137.3.5"), std::string::npos);
  EXPECT_NE(content.find("abc123def4567890"), std::string::npos);
  EXPECT_NE(content.find("windows64"), std::string::npos);
}

TEST_F(InstallerVersionMetadataTest, WriteOverwrites) {
  // Write initial content
  WriteJsonFile(R"({"old": "data"})");

  VersionMetadata metadata;
  metadata.version = Version::Parse("138.0.0");
  metadata.abi_hash = "beb1123456789";
  metadata.platform = "windows64";

  MetadataError error = WriteVersionMetadata(temp_dir_.GetPath(), metadata);

  EXPECT_EQ(MetadataError::kSuccess, error);

  // Read back and verify new content
  VersionMetadata read_metadata;
  error = ReadVersionMetadata(temp_dir_.GetPath(), &read_metadata);
  EXPECT_EQ(MetadataError::kSuccess, error);
  EXPECT_EQ("138.0.0", read_metadata.version.ToString());
  EXPECT_EQ("beb1123456789", read_metadata.abi_hash);
  EXPECT_EQ("windows64", read_metadata.platform);
}

TEST_F(InstallerVersionMetadataTest, RoundTrip) {
  VersionMetadata original;
  original.version = Version::Parse("137.3.5");
  original.abi_hash = "abc123def4567890";
  original.platform = "windows64";
  original.version_full = "137.3.5+g62d140e+chromium-137.0.7204.6";

  MetadataError write_error =
      WriteVersionMetadata(temp_dir_.GetPath(), original);
  EXPECT_EQ(MetadataError::kSuccess, write_error);

  VersionMetadata read_back;
  MetadataError read_error =
      ReadVersionMetadata(temp_dir_.GetPath(), &read_back);
  EXPECT_EQ(MetadataError::kSuccess, read_error);

  EXPECT_EQ(original.version.ToString(), read_back.version.ToString());
  EXPECT_EQ(original.abi_hash, read_back.abi_hash);
  EXPECT_EQ(original.platform, read_back.platform);
  EXPECT_EQ(original.version_full, read_back.version_full);
}

TEST_F(InstallerVersionMetadataTest, ScanWithMetadata) {
  std::string current_platform = GetCurrentPlatform();

  // Create version 137.3.5 with metadata
  Version v1 = Version::Parse("137.3.5");
  base::FilePath v1_dir = GetVersionPath(temp_dir_.GetPath(), v1);
  ASSERT_TRUE(base::CreateDirectory(v1_dir));
  VersionMetadata v1_meta;
  v1_meta.version = v1;
  v1_meta.abi_hash = "ab1137ab1137";
  v1_meta.platform = current_platform;
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionMetadata(v1_dir, v1_meta));

  // Create version 138.0.0 with metadata
  Version v2 = Version::Parse("138.0.0");
  base::FilePath v2_dir = GetVersionPath(temp_dir_.GetPath(), v2);
  ASSERT_TRUE(base::CreateDirectory(v2_dir));
  VersionMetadata v2_meta;
  v2_meta.version = v2;
  v2_meta.abi_hash = "ab1138ab1138";
  v2_meta.platform = current_platform;
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionMetadata(v2_dir, v2_meta));

  std::vector<InstalledVersion> versions =
      ScanInstalledVersionsWithMetadata(temp_dir_.GetPath());

  ASSERT_EQ(2u, versions.size());
  // Should be sorted newest-first
  EXPECT_EQ("138.0.0", versions[0].metadata.version.ToString());
  EXPECT_EQ("ab1138ab1138", versions[0].metadata.abi_hash);
  EXPECT_EQ("137.3.5", versions[1].metadata.version.ToString());
  EXPECT_EQ("ab1137ab1137", versions[1].metadata.abi_hash);
}

TEST_F(InstallerVersionMetadataTest, BoundedScanStopsAtEntryLimit) {
  Version version = Version::Parse("137.3.5");
  base::FilePath version_dir = GetVersionPath(temp_dir_.GetPath(), version);
  ASSERT_TRUE(base::CreateDirectory(version_dir));

  BoundedInstalledVersionScanResult result =
      ScanInstalledVersionsWithMetadataBounded(
          temp_dir_.GetPath(), 0, base::TimeTicks::Now() + base::Seconds(1));

  EXPECT_TRUE(result.versions.empty());
  EXPECT_EQ(0u, result.entries_visited);
  EXPECT_TRUE(result.entry_limit_reached);
  EXPECT_FALSE(result.time_limit_reached);
}

TEST_F(InstallerVersionMetadataTest, BoundedScanStopsAtSoftDeadline) {
  BoundedInstalledVersionScanResult result =
      ScanInstalledVersionsWithMetadataBounded(temp_dir_.GetPath(), 1,
                                               base::TimeTicks::Now());

  EXPECT_TRUE(result.versions.empty());
  EXPECT_FALSE(result.entry_limit_reached);
  EXPECT_TRUE(result.time_limit_reached);
}

TEST_F(InstallerVersionMetadataTest, ScanMissingMetadata) {
  std::string current_platform = GetCurrentPlatform();

  // Create version 137.3.5 WITH metadata
  Version v1 = Version::Parse("137.3.5");
  base::FilePath v1_dir = GetVersionPath(temp_dir_.GetPath(), v1);
  ASSERT_TRUE(base::CreateDirectory(v1_dir));
  VersionMetadata v1_meta;
  v1_meta.version = v1;
  v1_meta.abi_hash = "ab1137ab1137";
  v1_meta.platform = current_platform;
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionMetadata(v1_dir, v1_meta));

  // Create version 138.0.0 WITHOUT metadata (just directory)
  Version v2 = Version::Parse("138.0.0");
  base::FilePath v2_dir = GetVersionPath(temp_dir_.GetPath(), v2);
  ASSERT_TRUE(base::CreateDirectory(v2_dir));

  std::vector<InstalledVersion> versions =
      ScanInstalledVersionsWithMetadata(temp_dir_.GetPath());

  // Only the version with metadata should be returned; missing metadata
  // is fatal because we need the sandbox hash.
  ASSERT_EQ(1u, versions.size());
  EXPECT_EQ("137.3.5", versions[0].metadata.version.ToString());
  EXPECT_EQ("ab1137ab1137", versions[0].metadata.abi_hash);
}

TEST_F(InstallerVersionMetadataTest, MetadataErrorToString) {
  EXPECT_STREQ("Success", MetadataErrorToString(MetadataError::kSuccess));
  EXPECT_STREQ("Metadata file not found",
               MetadataErrorToString(MetadataError::kFileNotFound));
  EXPECT_STREQ("Could not read metadata file",
               MetadataErrorToString(MetadataError::kFileReadError));
  EXPECT_STREQ("Invalid JSON format",
               MetadataErrorToString(MetadataError::kJsonParseError));
  EXPECT_STREQ("Required field missing",
               MetadataErrorToString(MetadataError::kMissingRequiredField));
  EXPECT_STREQ("Could not write metadata file",
               MetadataErrorToString(MetadataError::kFileWriteError));
  EXPECT_STREQ("Integrity mismatch",
               MetadataErrorToString(MetadataError::kIntegrityMismatch));
}

// ============================================================================
// Security: Field Length Limit Tests (L3)
// ============================================================================

TEST_F(InstallerVersionMetadataTest, ReadRejectsOverlyLongVersion) {
  // Version longer than 64 chars should be rejected
  std::string long_version(100, '1');
  std::string json = R"({
    "version": ")" + long_version +
                     R"(",
    "abi_hash": "abc123"
  })";
  WriteJsonFile(json);

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);

  EXPECT_EQ(MetadataError::kMissingRequiredField, error);
}

TEST_F(InstallerVersionMetadataTest, ReadRejectsOverlyLongAbiHash) {
  // abi_hash longer than 256 chars should be rejected
  std::string long_hash(300, 'h');
  std::string json = R"({
    "version": "137.3.5",
    "abi_hash": ")" + long_hash +
                     R"("
  })";
  WriteJsonFile(json);

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);

  EXPECT_EQ(MetadataError::kMissingRequiredField, error);
}

TEST_F(InstallerVersionMetadataTest, ReadRejectsOverlyLongVersionFull) {
  // version_full longer than 256 chars should be rejected
  std::string long_hash(300, 'b');
  std::string json = R"({
    "version": "137.3.5",
    "abi_hash": "abc123",
    "platform": "windows64",
    "version_full": ")" +
                     long_hash + R"("
  })";
  WriteJsonFile(json);

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);

  EXPECT_EQ(MetadataError::kMissingRequiredField, error);
}

TEST_F(InstallerVersionMetadataTest, ReadRejectsOverlyLongPlatform) {
  // platform longer than 32 chars should be rejected
  std::string long_platform(50, 'p');
  std::string json = R"({
    "version": "137.3.5",
    "abi_hash": "abc123",
    "platform": ")" + long_platform +
                     R"("
  })";
  WriteJsonFile(json);

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);

  EXPECT_EQ(MetadataError::kMissingRequiredField, error);
}

TEST_F(InstallerVersionMetadataTest, ReadAcceptsValidFieldLengths) {
  // Fields at exactly the limit should be accepted
  std::string hash_256(256, 'a');
  std::string platform_32(32, 'p');

  // Use a simple valid version since the version parser is strict
  std::string json = R"({
    "version": "137.3.5",
    "abi_hash": ")" + hash_256 +
                     R"(",
    "platform": ")" + platform_32 +
                     R"(",
    "version_full": ")" +
                     hash_256 + R"("
  })";
  WriteJsonFile(json);

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);

  EXPECT_EQ(MetadataError::kSuccess, error);
  EXPECT_EQ("137.3.5", metadata.version.ToString());
  EXPECT_EQ(hash_256, metadata.abi_hash);
  EXPECT_EQ(platform_32, metadata.platform);
}

TEST_F(InstallerVersionMetadataTest, ReadNullMetadataPointer) {
  WriteJsonFile(
      R"({"version": "137.3.5", "abi_hash": "abc", "platform": "windows64"})");

  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), nullptr);
  EXPECT_EQ(MetadataError::kMissingRequiredField, error);
}

TEST_F(InstallerVersionMetadataTest, ReadMissingVersion) {
  WriteJsonFile(R"({
    "abi_hash": "abc123",
    "platform": "windows64"
  })");

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);
  EXPECT_EQ(MetadataError::kMissingRequiredField, error);
}

TEST_F(InstallerVersionMetadataTest, ReadInvalidVersionString) {
  WriteJsonFile(R"({
    "version": "not_a_version",
    "abi_hash": "abc123",
    "platform": "windows64"
  })");

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);
  EXPECT_EQ(MetadataError::kMissingRequiredField, error);
}

TEST_F(InstallerVersionMetadataTest, ReadEmptyAbiHash) {
  WriteJsonFile(R"({
    "version": "137.3.5",
    "abi_hash": "",
    "platform": "windows64"
  })");

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);
  EXPECT_EQ(MetadataError::kMissingRequiredField, error);
}

TEST_F(InstallerVersionMetadataTest, ReadEmptyPlatform) {
  WriteJsonFile(R"({
    "version": "137.3.5",
    "abi_hash": "abc123",
    "platform": ""
  })");

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);
  EXPECT_EQ(MetadataError::kMissingRequiredField, error);
}

TEST_F(InstallerVersionMetadataTest, WriteInvalidMetadata) {
  VersionMetadata metadata;  // All fields empty — IsValid() returns false.

  MetadataError error = WriteVersionMetadata(temp_dir_.GetPath(), metadata);
  EXPECT_EQ(MetadataError::kMissingRequiredField, error);
}

TEST_F(InstallerVersionMetadataTest, ScanEmptyVersionsDir) {
  // Create the Versions directory but leave it empty.
  ASSERT_TRUE(
      base::CreateDirectory(temp_dir_.GetPath().Append(kVersionsSubdirectory)));

  std::vector<InstalledVersion> versions =
      ScanInstalledVersionsWithMetadata(temp_dir_.GetPath());
  EXPECT_TRUE(versions.empty());
}

TEST_F(InstallerVersionMetadataTest, ScanNoVersionsDir) {
  // No Versions subdirectory at all.
  std::vector<InstalledVersion> versions =
      ScanInstalledVersionsWithMetadata(temp_dir_.GetPath());
  EXPECT_TRUE(versions.empty());
}

TEST_F(InstallerVersionMetadataTest, ScanSkipsInvalidVersionDirNames) {
  base::FilePath versions_dir =
      temp_dir_.GetPath().Append(kVersionsSubdirectory);
  ASSERT_TRUE(base::CreateDirectory(versions_dir));

  // Create directories that are not valid version strings.
  ASSERT_TRUE(base::CreateDirectory(versions_dir.Append(L"not-a-version")));
  ASSERT_TRUE(base::CreateDirectory(versions_dir.Append(L"abc.def.ghi")));
  ASSERT_TRUE(base::CreateDirectory(versions_dir.Append(L".hidden")));

  std::vector<InstalledVersion> versions =
      ScanInstalledVersionsWithMetadata(temp_dir_.GetPath());
  EXPECT_TRUE(versions.empty());
}

TEST_F(InstallerVersionMetadataTest, ReadAcceptsEmptyOptionalFields) {
  // Optional fields can be missing or empty
  WriteJsonFile(R"({
    "version": "137.3.5",
    "abi_hash": "abc123",
    "platform": "windows64"
  })");

  VersionMetadata metadata;
  MetadataError error = ReadVersionMetadata(temp_dir_.GetPath(), &metadata);

  EXPECT_EQ(MetadataError::kSuccess, error);
  EXPECT_TRUE(metadata.version_full.empty());
}

// ============================================================================
// ExtractVersionFullFromFilename tests
// ============================================================================

TEST(ExtractVersionFullFromFilenameTest, FullVersion) {
  EXPECT_EQ(
      "145.0.28+g51162e8+chromium-145.0.7632.160",
      ExtractVersionFullFromFilename("cef_binary_145.0.28+g51162e8+chromium-"
                                     "145.0.7632.160_windows64_signed.tar.xz"));
}

TEST(ExtractVersionFullFromFilenameTest, ShortVersion) {
  EXPECT_EQ("137.3.5", ExtractVersionFullFromFilename(
                           "cef_binary_137.3.5_windows64_signed.tar.xz"));
}

TEST(ExtractVersionFullFromFilenameTest, NoSuffix) {
  EXPECT_EQ("137.3.5", ExtractVersionFullFromFilename(
                           "cef_binary_137.3.5_linux64.tar.xz"));
}

TEST(ExtractVersionFullFromFilenameTest, WrongPrefix) {
  EXPECT_EQ("",
            ExtractVersionFullFromFilename("archive_137.3.5_windows64.tar.xz"));
}

TEST(ExtractVersionFullFromFilenameTest, Empty) {
  EXPECT_EQ("", ExtractVersionFullFromFilename(""));
}

TEST(ExtractVersionFullFromFilenameTest, PrefixOnly) {
  EXPECT_EQ("", ExtractVersionFullFromFilename("cef_binary_"));
}

TEST(ExtractVersionFullFromFilenameTest, NoUnderscoreAfterVersion) {
  EXPECT_EQ("", ExtractVersionFullFromFilename("cef_binary_137.3.5.tar.xz"));
}

TEST(ExtractVersionFullFromFilenameTest, UnderscoreImmediatelyAfterPrefix) {
  EXPECT_EQ("", ExtractVersionFullFromFilename("cef_binary__windows64.tar.xz"));
}

// ============================================================================
// WriteVersionIndex / ReadVersionIndex tests
// ============================================================================

class VersionIndexTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath install_dir() const { return temp_dir_.GetPath(); }

  // Create a version directory with valid metadata.
  void CreateVersion(const std::string& version_str,
                     const std::string& abi_hash,
                     const std::string& platform) {
    Version version = Version::Parse(version_str);
    base::FilePath version_dir =
        GetVersionPath(install_dir(), version, platform);
    ASSERT_TRUE(base::CreateDirectory(version_dir));
    VersionMetadata meta;
    meta.version = version;
    meta.abi_hash = abi_hash;
    meta.platform = platform;
    ASSERT_EQ(MetadataError::kSuccess, WriteVersionMetadata(version_dir, meta));
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(VersionIndexTest, WriteAndReadRoundTrip) {
  CreateVersion("137.3.5", "abc123def4567890", "windows64");
  CreateVersion("138.0.0", "beb1123456789abc", "windows64");

  // Write index by scanning.
  auto all = ScanInstalledVersionsWithMetadata(install_dir());
  ASSERT_EQ(2u, all.size());
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir(), all));

  // Read it back.
  std::vector<InstalledVersion> read_back;
  ASSERT_EQ(MetadataError::kSuccess,
            ReadVersionIndex(install_dir(), &read_back));
  ASSERT_EQ(2u, read_back.size());

  // Verify fields (sorted newest-first by scan).
  EXPECT_EQ("138.0.0", read_back[0].metadata.version.ToString());
  EXPECT_EQ("beb1123456789abc", read_back[0].metadata.abi_hash);
  EXPECT_EQ("windows64", read_back[0].metadata.platform);
  EXPECT_FALSE(read_back[0].path.empty());

  EXPECT_EQ("137.3.5", read_back[1].metadata.version.ToString());
  EXPECT_EQ("abc123def4567890", read_back[1].metadata.abi_hash);
}

TEST_F(VersionIndexTest, WriteEmptyIndex) {
  std::vector<InstalledVersion> empty;
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir(), empty));

  std::vector<InstalledVersion> read_back;
  ASSERT_EQ(MetadataError::kSuccess,
            ReadVersionIndex(install_dir(), &read_back));
  EXPECT_TRUE(read_back.empty());
}

TEST_F(VersionIndexTest, ReadMissingIndexFile) {
  std::vector<InstalledVersion> versions;
  EXPECT_EQ(MetadataError::kFileNotFound,
            ReadVersionIndex(install_dir(), &versions));
  EXPECT_TRUE(versions.empty());
}

TEST_F(VersionIndexTest, ReadCorruptIndexFile) {
  // Write garbage to the index file.
  base::FilePath index_path = install_dir().Append(kVersionIndexFilename);
  ASSERT_TRUE(base::WriteFile(index_path, "not valid json {{{"));

  std::vector<InstalledVersion> versions;
  MetadataError err = ReadVersionIndex(install_dir(), &versions);
  // Could be kJsonParseError or kFileReadError depending on integrity check.
  EXPECT_NE(MetadataError::kSuccess, err);
  EXPECT_TRUE(versions.empty());
}

TEST_F(VersionIndexTest, ReadMalformedJsonIndex) {
  // Valid JSON but wrong structure (missing "versions" array).
  base::FilePath index_path = install_dir().Append(kVersionIndexFilename);
  ASSERT_TRUE(base::WriteFile(index_path, R"({"foo": "bar"})"));

  std::vector<InstalledVersion> versions;
  EXPECT_NE(MetadataError::kSuccess,
            ReadVersionIndex(install_dir(), &versions));
}

TEST_F(VersionIndexTest, ReadRejectsEntriesWithMissingFields) {
  // Index with one valid and one invalid entry (missing abi_hash).
  base::FilePath index_path = install_dir().Append(kVersionIndexFilename);
  ASSERT_TRUE(base::WriteFile(index_path, R"({
    "versions": [
      {
        "version": "137.3.5",
        "abi_hash": "abc123def4567890",
        "platform": "windows64",
        "path": "Versions/137.3.5/windows64"
      },
      {
        "version": "138.0.0",
        "platform": "windows64",
        "path": "Versions/138.0.0/windows64"
      }
    ]
  })"));

  std::vector<InstalledVersion> versions;
  EXPECT_EQ(MetadataError::kIndexValidationError,
            ReadVersionIndex(install_dir(), &versions));
  EXPECT_TRUE(versions.empty());
}

TEST_F(VersionIndexTest, ReadResolvesRelativePaths) {
  CreateVersion("137.3.5", "abc123def4567890", "windows64");

  auto all = ScanInstalledVersionsWithMetadata(install_dir());
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir(), all));

  std::vector<InstalledVersion> read_back;
  ASSERT_EQ(MetadataError::kSuccess,
            ReadVersionIndex(install_dir(), &read_back));
  ASSERT_EQ(1u, read_back.size());

  // Path should be absolute, resolved against install_dir.
  EXPECT_TRUE(read_back[0].path.IsAbsolute());
  EXPECT_TRUE(install_dir().IsParent(read_back[0].path));
}

TEST_F(VersionIndexTest, ReadPreservesOptionalFields) {
  CreateVersion("137.3.5", "abc123def4567890", "windows64");

  // Scan, add optional fields, write.
  auto all = ScanInstalledVersionsWithMetadata(install_dir());
  ASSERT_EQ(1u, all.size());
  all[0].metadata.version_full = "137.3.5+g62d140e+chromium-137.0.7204.6";
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir(), all));

  std::vector<InstalledVersion> read_back;
  ASSERT_EQ(MetadataError::kSuccess,
            ReadVersionIndex(install_dir(), &read_back));
  ASSERT_EQ(1u, read_back.size());
  EXPECT_EQ("137.3.5+g62d140e+chromium-137.0.7204.6",
            read_back[0].metadata.version_full);
}

TEST_F(VersionIndexTest, RejectsDuplicateCanonicalKeys) {
  CreateVersion("137.3.5", "abc123def4567890", "windows64");
  auto all = ScanInstalledVersionsWithMetadata(install_dir());
  ASSERT_EQ(1u, all.size());
  all.push_back(all.front());
  EXPECT_EQ(MetadataError::kIndexValidationError,
            WriteVersionIndex(install_dir(), all));
}

TEST_F(VersionIndexTest, CheckedPublicationFaultBoundaries) {
  CreateVersion("137.3.5", "abc123def4567890", "windows64");
  auto all = ScanInstalledVersionsWithMetadata(install_dir());
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir(), all));

  for (VersionIndexFault fault :
       {VersionIndexFault::kWrite, VersionIndexFault::kReplace,
        VersionIndexFault::kReread, VersionIndexFault::kValidation}) {
    SetVersionIndexFaultForTesting(fault);
    EXPECT_NE(MetadataError::kSuccess, WriteVersionIndex(install_dir(), all));
    SetVersionIndexFaultForTesting(VersionIndexFault::kNone);
  }

  std::vector<InstalledVersion> reread;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir(), &reread));
  ASSERT_EQ(1u, reread.size());
  EXPECT_EQ("137.3.5", reread[0].metadata.version.ToString());
}

TEST_F(VersionIndexTest, ReadRejectsPathEscape) {
  base::FilePath index_path = install_dir().Append(kVersionIndexFilename);
  ASSERT_TRUE(base::WriteFile(index_path, R"({
    "versions": [{
      "version": "137.3.5",
      "abi_hash": "abc123def4567890",
      "platform": "windows64",
      "path": "../escape"
    }]
  })"));
  std::vector<InstalledVersion> versions;
  EXPECT_EQ(MetadataError::kIndexValidationError,
            ReadVersionIndex(install_dir(), &versions));
}

TEST_F(VersionIndexTest, ConcurrentReadersSeeWholePublishedIndexes) {
  CreateVersion("137.3.5", "abc123def4567890", "windows64");
  CreateVersion("138.0.0", "abc123def4567890", "windows64");
  auto all = ScanInstalledVersionsWithMetadata(install_dir());
  ASSERT_EQ(2u, all.size());
  std::vector<InstalledVersion> first = {all[0]};
  std::vector<InstalledVersion> second = {all[1]};
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir(), first));

  std::atomic<bool> stop = false;
  std::atomic<int> invalid_reads = 0;
  std::atomic<int> successful_reads = 0;
  std::thread reader([&] {
    while (!stop.load(std::memory_order_acquire)) {
      std::vector<InstalledVersion> current;
      MetadataError error = ReadVersionIndex(install_dir(), &current);
      // A reader may transiently fall back to a directory scan while Windows
      // replacement is in progress. Any successfully parsed index must be one
      // complete old or new publication, never a torn mixture.
      if (error == MetadataError::kSuccess &&
          (current.size() != 1u ||
           (current[0].metadata.version != first[0].metadata.version &&
            current[0].metadata.version != second[0].metadata.version))) {
        invalid_reads.fetch_add(1, std::memory_order_relaxed);
      }
      if (error == MetadataError::kSuccess) {
        successful_reads.fetch_add(1, std::memory_order_relaxed);
      }
    }
  });

  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(MetadataError::kSuccess,
              WriteVersionIndex(install_dir(), i % 2 ? first : second));
  }
  stop.store(true, std::memory_order_release);
  reader.join();
  EXPECT_EQ(0, invalid_reads.load());
  EXPECT_GT(successful_reads.load(), 0);
}

}  // namespace
}  // namespace cef_installer
