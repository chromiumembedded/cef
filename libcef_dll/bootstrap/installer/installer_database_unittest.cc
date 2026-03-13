// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_database.h"

#include <time.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

class InstallerDatabaseTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetDatabasePath() const {
    return temp_dir_.GetPath().Append(L"installer.json");
  }

  void WriteJsonFile(const std::string& content) {
    ASSERT_TRUE(base::WriteFile(GetDatabasePath(), content));
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(InstallerDatabaseTest, CreateEmpty) {
  Database db;
  EXPECT_TRUE(db.IsEmpty());
  EXPECT_EQ(kCurrentSchemaVersion, db.GetSchemaVersion());
  EXPECT_TRUE(db.CanPrune());
}

TEST_F(InstallerDatabaseTest, SaveEmpty) {
  Database db;
  DatabaseError error = db.Save(GetDatabasePath());

  EXPECT_EQ(DatabaseError::kSuccess, error);
  EXPECT_TRUE(base::PathExists(GetDatabasePath()));

  // Verify saved content (read through integrity layer to strip footer)
  std::string content;
  IntegrityResult ir = ReadFileWithIntegrity(GetDatabasePath(), &content);
  EXPECT_EQ(IntegrityResult::kSuccess, ir);
  EXPECT_NE(content.find("\"schema_version\""), std::string::npos);
  EXPECT_NE(content.find("\"apps\""), std::string::npos);
}

TEST_F(InstallerDatabaseTest, LoadNonExistent) {
  Database db;
  DatabaseError error = db.Load(GetDatabasePath());

  // Non-existent file should create empty database
  EXPECT_EQ(DatabaseError::kSuccess, error);
  EXPECT_TRUE(db.IsEmpty());
}

TEST_F(InstallerDatabaseTest, RegisterAppNew) {
  Database db;

  AppEntry entry;
  entry.uuid = "12345678-1234-1234-1234-123456789abc";
  entry.platform = "windows64";
  entry.vmin = "137.1";
  entry.vmax = "137.99";
  entry.abi_hash = "abc123";

  EXPECT_TRUE(db.RegisterApp(entry));

  EXPECT_FALSE(db.IsEmpty());
  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ(entry, apps[0]);
}

TEST_F(InstallerDatabaseTest, RegisterAppUpdate) {
  Database db;

  AppEntry entry1;
  entry1.uuid = "12345678-1234-1234-1234-123456789abc";
  entry1.platform = "windows64";
  entry1.vmin = "137.1";
  entry1.vmax = "137.99";

  EXPECT_TRUE(db.RegisterApp(entry1));

  // Update same (uuid, platform) with different values
  AppEntry entry2;
  entry2.uuid = "12345678-1234-1234-1234-123456789abc";
  entry2.platform = "windows64";
  entry2.vmin = "138.0";
  entry2.vmax = "138.99";

  EXPECT_TRUE(db.RegisterApp(entry2));

  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ("138.0", apps[0].vmin);
  EXPECT_EQ("138.99", apps[0].vmax);
}

TEST_F(InstallerDatabaseTest, RegisterAppUnchanged) {
  Database db;

  AppEntry entry;
  entry.uuid = "12345678-1234-1234-1234-123456789abc";
  entry.platform = "windows64";
  entry.vmin = "137.1";
  entry.vmax = "137.99";
  entry.abi_hash = "abc123";

  EXPECT_TRUE(db.RegisterApp(entry));
  EXPECT_FALSE(db.RegisterApp(entry));

  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ(entry, apps[0]);
}

TEST_F(InstallerDatabaseTest, RegisterAppDifferentPlatforms) {
  Database db;

  // Same UUID but different platforms should be separate entries
  AppEntry entry1;
  entry1.uuid = "12345678-1234-1234-1234-123456789abc";
  entry1.platform = "windows64";
  entry1.vmin = "137.1";

  AppEntry entry2;
  entry2.uuid = "12345678-1234-1234-1234-123456789abc";
  entry2.platform = "windows32";
  entry2.vmin = "138.0";

  EXPECT_TRUE(db.RegisterApp(entry1));
  EXPECT_TRUE(db.RegisterApp(entry2));

  auto apps = db.GetAllApps();
  ASSERT_EQ(2u, apps.size());
}

TEST_F(InstallerDatabaseTest, UnregisterApp) {
  Database db;

  AppEntry entry;
  entry.uuid = "12345678-1234-1234-1234-123456789abc";
  entry.platform = "windows64";
  entry.vmin = "137.1";

  db.RegisterApp(entry);
  EXPECT_FALSE(db.IsEmpty());

  db.UnregisterApp(entry.uuid, entry.platform);
  EXPECT_TRUE(db.IsEmpty());
}

TEST_F(InstallerDatabaseTest, UnregisterAppOnlyMatchingPlatform) {
  Database db;

  // Register same UUID on two platforms
  AppEntry entry1;
  entry1.uuid = "12345678-1234-1234-1234-123456789abc";
  entry1.platform = "windows64";
  entry1.vmin = "137.1";

  AppEntry entry2;
  entry2.uuid = "12345678-1234-1234-1234-123456789abc";
  entry2.platform = "windows32";
  entry2.vmin = "137.1";

  db.RegisterApp(entry1);
  db.RegisterApp(entry2);
  EXPECT_EQ(2u, db.GetAllApps().size());

  // Unregister only windows64 - windows32 should remain
  db.UnregisterApp(entry1.uuid, "windows64");
  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ("windows32", apps[0].platform);
}

TEST_F(InstallerDatabaseTest, GetAppFound) {
  Database db;

  AppEntry entry;
  entry.uuid = "12345678-1234-1234-1234-123456789abc";
  entry.platform = "windows64";
  entry.vmin = "137.1";
  entry.abi_hash = "a1a2a3";

  db.RegisterApp(entry);

  auto result = db.GetApp(entry.uuid, entry.platform);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(entry, *result);
}

TEST_F(InstallerDatabaseTest, GetAppNotFound) {
  Database db;

  auto result = db.GetApp("nonexistent-uuid", "windows64");
  EXPECT_FALSE(result.has_value());
}

TEST_F(InstallerDatabaseTest, GetAppWrongPlatform) {
  Database db;

  AppEntry entry;
  entry.uuid = "12345678-1234-1234-1234-123456789abc";
  entry.platform = "windows64";
  entry.vmin = "137.1";

  db.RegisterApp(entry);

  // Same UUID but different platform should not be found
  auto result = db.GetApp(entry.uuid, "windows32");
  EXPECT_FALSE(result.has_value());
}

TEST_F(InstallerDatabaseTest, GetAllApps) {
  Database db;

  AppEntry entry1;
  entry1.uuid = "uuid-1";
  entry1.platform = "windows64";
  entry1.vmin = "137.1";

  AppEntry entry2;
  entry2.uuid = "uuid-2";
  entry2.platform = "windows64";
  entry2.vmin = "138.0";

  db.RegisterApp(entry1);
  db.RegisterApp(entry2);

  auto apps = db.GetAllApps();
  ASSERT_EQ(2u, apps.size());
}

TEST_F(InstallerDatabaseTest, SchemaVersionCurrent) {
  WriteJsonFile(R"({
    "schema_version": 1,
    "apps": [
      {
        "uuid": "12345678-1234-1234-1234-123456789abc",
        "platform": "windows64",
        "vmin": "137.1"
      }
    ]
  })");

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());

  EXPECT_EQ(DatabaseError::kSuccess, error);
  EXPECT_EQ(1, db.GetSchemaVersion());
  EXPECT_TRUE(db.CanPrune());
}

TEST_F(InstallerDatabaseTest, SchemaVersionTooNew) {
  WriteJsonFile(R"({
    "schema_version": 999,
    "apps": [
      {
        "uuid": "12345678-1234-1234-1234-123456789abc",
        "platform": "windows64",
        "vmin": "137.1"
      }
    ]
  })");

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());

  // Should return error but still load data
  EXPECT_EQ(DatabaseError::kSchemaVersionTooNew, error);
  EXPECT_EQ(999, db.GetSchemaVersion());
  EXPECT_FALSE(db.IsEmpty());  // Data was still loaded
}

TEST_F(InstallerDatabaseTest, CanPruneFalseWhenSchemaTooNew) {
  WriteJsonFile(R"({
    "schema_version": 999,
    "apps": []
  })");

  Database db;
  db.Load(GetDatabasePath());

  EXPECT_FALSE(db.CanPrune());
}

TEST_F(InstallerDatabaseTest, JsonRoundTrip) {
  Database db1;

  AppEntry entry;
  entry.uuid = "12345678-1234-1234-1234-123456789abc";
  entry.platform = "windows64";
  entry.vmin = "137.1";
  entry.vmax = "137.99";
  entry.abi_hash = "abc123def4567890";

  db1.RegisterApp(entry);
  ASSERT_EQ(DatabaseError::kSuccess, db1.Save(GetDatabasePath()));

  Database db2;
  ASSERT_EQ(DatabaseError::kSuccess, db2.Load(GetDatabasePath()));

  auto apps = db2.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ(entry, apps[0]);
}

TEST_F(InstallerDatabaseTest, UnknownFieldsDroppedOnSave) {
  // Unknown top-level fields from older schemas are not round-tripped.
  // We only save when schema_version <= kCurrentSchemaVersion, so unknown
  // fields can only come from older versions and are safe to drop.
  WriteJsonFile(R"({
    "schema_version": 1,
    "apps": [
      {
        "uuid": "12345678-1234-1234-1234-123456789abc",
        "platform": "windows64",
        "vmin": "137.1"
      }
    ],
    "future_field": "some value",
    "another_unknown": 42
  })");

  Database db;
  ASSERT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath()));
  ASSERT_EQ(DatabaseError::kSuccess, db.Save(GetDatabasePath()));

  // Read back and verify unknown fields are not preserved
  std::string content;
  IntegrityResult ir = ReadFileWithIntegrity(GetDatabasePath(), &content);
  EXPECT_EQ(IntegrityResult::kSuccess, ir);
  EXPECT_EQ(content.find("future_field"), std::string::npos);
  EXPECT_EQ(content.find("another_unknown"), std::string::npos);
}

TEST_F(InstallerDatabaseTest, DatabaseErrorToString) {
  EXPECT_STREQ("Success", DatabaseErrorToString(DatabaseError::kSuccess));
  EXPECT_STREQ("Database file not found",
               DatabaseErrorToString(DatabaseError::kFileNotFound));
  EXPECT_STREQ("Could not read database file",
               DatabaseErrorToString(DatabaseError::kFileReadError));
  EXPECT_STREQ("Could not write database file",
               DatabaseErrorToString(DatabaseError::kFileWriteError));
  EXPECT_STREQ("Invalid JSON in database file",
               DatabaseErrorToString(DatabaseError::kJsonParseError));
  EXPECT_STREQ("Database schema version too new",
               DatabaseErrorToString(DatabaseError::kSchemaVersionTooNew));
  EXPECT_STREQ("Could not acquire database lock",
               DatabaseErrorToString(DatabaseError::kLockAcquisitionFailed));
  EXPECT_STREQ("Database file corrupted (CRC32 mismatch, file deleted)",
               DatabaseErrorToString(DatabaseError::kIntegrityMismatch));
}

// ============================================================================
// Security: Field Length Limit Tests (M2)
// ============================================================================

TEST_F(InstallerDatabaseTest, LoadRejectsOverlyLongUuid) {
  // UUID longer than 256 chars should be skipped
  std::string long_uuid(300, 'u');
  std::string json = R"({
    "schema_version": 1,
    "apps": [
      {
        "uuid": ")" + long_uuid +
                     R"(",
        "platform": "windows64",
        "vmin": "137.1"
      },
      {
        "uuid": "valid-uuid",
        "platform": "windows64",
        "vmin": "138.0"
      }
    ]
  })";
  WriteJsonFile(json);

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());

  EXPECT_EQ(DatabaseError::kSuccess, error);
  // Only the valid entry should be loaded
  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ("valid-uuid", apps[0].uuid);
}

TEST_F(InstallerDatabaseTest, LoadRejectsOverlyLongVmin) {
  // vmin longer than 64 chars should cause entry to be skipped
  std::string long_version(100, '1');
  std::string json = R"({
    "schema_version": 1,
    "apps": [
      {
        "uuid": "uuid-1",
        "platform": "windows64",
        "vmin": ")" + long_version +
                     R"("
      },
      {
        "uuid": "uuid-2",
        "platform": "windows64",
        "vmin": "138.0"
      }
    ]
  })";
  WriteJsonFile(json);

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());

  EXPECT_EQ(DatabaseError::kSuccess, error);
  // Only the valid entry should be loaded
  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ("uuid-2", apps[0].uuid);
}

TEST_F(InstallerDatabaseTest, LoadRejectsOverlyLongVmax) {
  // vmax longer than 64 chars should cause entry to be skipped
  std::string long_version(100, '9');
  std::string json = R"({
    "schema_version": 1,
    "apps": [
      {
        "uuid": "uuid-1",
        "platform": "windows64",
        "vmin": "137.0",
        "vmax": ")" + long_version +
                     R"("
      },
      {
        "uuid": "uuid-2",
        "platform": "windows64",
        "vmin": "138.0"
      }
    ]
  })";
  WriteJsonFile(json);

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());

  EXPECT_EQ(DatabaseError::kSuccess, error);
  // Only the valid entry should be loaded
  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ("uuid-2", apps[0].uuid);
}

TEST_F(InstallerDatabaseTest, LoadRejectsOverlyLongAbiHash) {
  // abi_hash longer than 256 chars should cause entry to be skipped
  std::string long_hash(300, 'h');
  std::string json = R"({
    "schema_version": 1,
    "apps": [
      {
        "uuid": "uuid-1",
        "platform": "windows64",
        "vmin": "137.0",
        "abi_hash": ")" +
                     long_hash + R"("
      },
      {
        "uuid": "uuid-2",
        "platform": "windows64",
        "vmin": "138.0"
      }
    ]
  })";
  WriteJsonFile(json);

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());

  EXPECT_EQ(DatabaseError::kSuccess, error);
  // Only the valid entry should be loaded
  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ("uuid-2", apps[0].uuid);
}

TEST_F(InstallerDatabaseTest, LoadAcceptsValidFieldLengths) {
  // Fields at exactly the limit should be accepted
  std::string uuid_256(256, 'u');
  std::string version_64(64, '1');
  std::string hash_256(256, 'h');

  std::string json = R"({
    "schema_version": 1,
    "apps": [
      {
        "uuid": ")" + uuid_256 +
                     R"(",
        "platform": "windows64",
        "vmin": ")" + version_64 +
                     R"(",
        "abi_hash": ")" +
                     hash_256 + R"("
      }
    ]
  })";
  WriteJsonFile(json);

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());

  EXPECT_EQ(DatabaseError::kSuccess, error);
  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ(uuid_256, apps[0].uuid);
}

TEST_F(InstallerDatabaseTest, LoadRejectsOverlyLongPlatform) {
  // platform longer than 32 chars should cause entry to be skipped
  std::string long_platform(50, 'p');
  std::string json = R"({
    "schema_version": 1,
    "apps": [
      {
        "uuid": "uuid-1",
        "platform": ")" +
                     long_platform +
                     R"(",
        "vmin": "137.0"
      },
      {
        "uuid": "uuid-2",
        "platform": "windows64",
        "vmin": "138.0"
      }
    ]
  })";
  WriteJsonFile(json);

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());

  EXPECT_EQ(DatabaseError::kSuccess, error);
  // Only the valid entry should be loaded
  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ("uuid-2", apps[0].uuid);
}

TEST_F(InstallerDatabaseTest, LoadRejectsMissingPlatform) {
  // Entry without platform field should be skipped
  std::string json = R"({
    "schema_version": 1,
    "apps": [
      {
        "uuid": "uuid-1",
        "vmin": "137.0"
      },
      {
        "uuid": "uuid-2",
        "platform": "windows64",
        "vmin": "138.0"
      }
    ]
  })";
  WriteJsonFile(json);

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());

  EXPECT_EQ(DatabaseError::kSuccess, error);
  // Only the valid entry should be loaded
  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ("uuid-2", apps[0].uuid);
}

// ============================================================================
// Integrity & Atomic Write Tests
// ============================================================================

TEST_F(InstallerDatabaseTest, SaveWritesIntegrityFooter) {
  Database db;
  AppEntry entry;
  entry.uuid = "test-uuid";
  entry.platform = "windows64";
  entry.vmin = "137.1";
  db.RegisterApp(entry);

  ASSERT_EQ(DatabaseError::kSuccess, db.Save(GetDatabasePath()));

  // File should have a valid integrity footer
  std::string content;
  IntegrityResult ir = ReadFileWithIntegrity(GetDatabasePath(), &content);
  EXPECT_EQ(IntegrityResult::kSuccess, ir);
  EXPECT_NE(content.find("test-uuid"), std::string::npos);
}

TEST_F(InstallerDatabaseTest, LoadAcceptsLegacyFileWithoutFooter) {
  // Legacy files (written without integrity footer) should still load
  WriteJsonFile(R"({
    "schema_version": 1,
    "apps": [
      {
        "uuid": "legacy-uuid",
        "platform": "windows64",
        "vmin": "137.1"
      }
    ]
  })");

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());

  EXPECT_EQ(DatabaseError::kSuccess, error);
  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ("legacy-uuid", apps[0].uuid);
}

TEST_F(InstallerDatabaseTest, LoadDetectsCorruption) {
  // Write a valid file with integrity footer
  Database db;
  AppEntry entry;
  entry.uuid = "test-uuid";
  entry.platform = "windows64";
  entry.vmin = "137.1";
  db.RegisterApp(entry);
  ASSERT_EQ(DatabaseError::kSuccess, db.Save(GetDatabasePath()));

  // Corrupt the file by flipping a byte in the content
  std::string raw;
  ASSERT_TRUE(base::ReadFileToString(GetDatabasePath(), &raw));
  ASSERT_GT(raw.size(), 20u);
  raw[5] ^= 0xFF;  // Flip a byte in the JSON content
  ASSERT_TRUE(base::WriteFile(GetDatabasePath(), raw));

  // Load should detect corruption, delete the file, return empty DB
  Database db2;
  DatabaseError error = db2.Load(GetDatabasePath());

  EXPECT_EQ(DatabaseError::kIntegrityMismatch, error);
  EXPECT_TRUE(db2.IsEmpty());
  EXPECT_FALSE(base::PathExists(GetDatabasePath()));
}

TEST_F(InstallerDatabaseTest, RetentionReadPreservesCorruption) {
  Database database;
  database.RegisterApp({"test-uuid", "windows64", "137.1", "", ""});
  ASSERT_EQ(DatabaseError::kSuccess, database.Save(GetDatabasePath()));
  std::string raw;
  ASSERT_TRUE(base::ReadFileToString(GetDatabasePath(), &raw));
  ASSERT_GT(raw.size(), 20u);
  raw[5] ^= 0xFF;
  ASSERT_TRUE(base::WriteFile(GetDatabasePath(), raw));

  Database inspected;
  EXPECT_EQ(
      DatabaseError::kIntegrityMismatch,
      inspected.Load(GetDatabasePath(), IntegrityMismatchAction::kPreserve));

  std::string after;
  ASSERT_TRUE(base::ReadFileToString(GetDatabasePath(), &after));
  EXPECT_EQ(raw, after);
}

TEST_F(InstallerDatabaseTest, SaveThenLoadRoundTripsWithIntegrity) {
  Database db1;
  AppEntry entry;
  entry.uuid = "round-trip-uuid";
  entry.platform = "windows64";
  entry.vmin = "138.0";
  entry.vmax = "138.99";
  entry.abi_hash = "abc123";
  db1.RegisterApp(entry);

  ASSERT_EQ(DatabaseError::kSuccess, db1.Save(GetDatabasePath()));

  Database db2;
  ASSERT_EQ(DatabaseError::kSuccess, db2.Load(GetDatabasePath()));

  auto apps = db2.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ(entry, apps[0]);
}

TEST_F(InstallerDatabaseTest, DeterministicSaveFailure) {
  Database database;
  SetDatabaseSaveFailureForTesting(true);
  EXPECT_EQ(DatabaseError::kFileWriteError, database.Save(GetDatabasePath()));
  SetDatabaseSaveFailureForTesting(false);
  EXPECT_FALSE(base::PathExists(GetDatabasePath()));
}

// ============================================================================
// Pruning Suspension Tests
// ============================================================================

TEST_F(InstallerDatabaseTest, CanPruneDefaultTrue) {
  Database db;
  EXPECT_TRUE(db.CanPrune());
}

TEST_F(InstallerDatabaseTest, SuspendPruningPreventsCanPrune) {
  Database db;
  db.SuspendPruning();
  EXPECT_FALSE(db.CanPrune());
}

TEST_F(InstallerDatabaseTest, SuspendPruningPersistsAcrossSaveLoad) {
  Database db1;
  db1.SuspendPruning();
  ASSERT_EQ(DatabaseError::kSuccess, db1.Save(GetDatabasePath()));

  Database db2;
  ASSERT_EQ(DatabaseError::kSuccess, db2.Load(GetDatabasePath()));
  EXPECT_FALSE(db2.CanPrune());
}

TEST_F(InstallerDatabaseTest, ExpiredSuspensionAllowsPruning) {
  // Write a DB with a suspension timestamp in the past
  int64_t past = static_cast<int64_t>(time(nullptr)) - 100;
  std::string json = R"({
    "schema_version": 1,
    "pruning_suspended_until": ")" +
                     std::to_string(past) + R"(",
    "apps": []
  })";
  WriteJsonFile(json);

  Database db;
  ASSERT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath()));
  EXPECT_TRUE(db.CanPrune());
}

TEST_F(InstallerDatabaseTest, ActiveSuspensionBlocksPruning) {
  // Write a DB with a suspension timestamp in the future
  int64_t future = static_cast<int64_t>(time(nullptr)) + 86400;
  std::string json = R"({
    "schema_version": 1,
    "pruning_suspended_until": ")" +
                     std::to_string(future) + R"(",
    "apps": []
  })";
  WriteJsonFile(json);

  Database db;
  ASSERT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath()));
  EXPECT_FALSE(db.CanPrune());
}

TEST_F(InstallerDatabaseTest, SuspensionFieldNotWrittenWhenZero) {
  Database db;
  ASSERT_EQ(DatabaseError::kSuccess, db.Save(GetDatabasePath()));

  std::string content;
  IntegrityResult ir = ReadFileWithIntegrity(GetDatabasePath(), &content);
  EXPECT_EQ(IntegrityResult::kSuccess, ir);
  EXPECT_EQ(content.find("pruning_suspended_until"), std::string::npos);
}

// ============================================================================
// Coverage gap tests
// ============================================================================

TEST_F(InstallerDatabaseTest, LoadFileReadError) {
  // Create a directory at the database path — reading it as a file fails.
  ASSERT_TRUE(base::CreateDirectory(GetDatabasePath()));

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());
  EXPECT_EQ(DatabaseError::kFileReadError, error);
}

TEST_F(InstallerDatabaseTest, LoadInvalidJsonLegacyFile) {
  // Legacy file (no integrity footer) with invalid JSON.
  WriteJsonFile("this is not valid json {{{");

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());
  EXPECT_EQ(DatabaseError::kJsonParseError, error);
}

TEST_F(InstallerDatabaseTest, LoadMissingAppsArray) {
  // Valid JSON but no "apps" key — should load with empty app list.
  WriteJsonFile(R"({"schema_version": 1})");

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());
  EXPECT_EQ(DatabaseError::kSuccess, error);
  EXPECT_TRUE(db.IsEmpty());
}

TEST_F(InstallerDatabaseTest, LoadNonDictAppEntry) {
  // Apps array contains a non-object entry — should be skipped.
  WriteJsonFile(R"({
    "schema_version": 1,
    "apps": [
      "not an object",
      {
        "uuid": "valid-uuid",
        "platform": "windows64",
        "vmin": "137.1"
      }
    ]
  })");

  Database db;
  DatabaseError error = db.Load(GetDatabasePath());
  EXPECT_EQ(DatabaseError::kSuccess, error);
  auto apps = db.GetAllApps();
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ("valid-uuid", apps[0].uuid);
}

TEST_F(InstallerDatabaseTest, GetGlobalVmin_Empty) {
  Database db;
  EXPECT_FALSE(db.GetGlobalVmin("windows64").IsValid());
}

TEST_F(InstallerDatabaseTest, GetGlobalVmin_SingleApp) {
  Database db;
  AppEntry entry;
  entry.uuid = "app1";
  entry.platform = "windows64";
  entry.vmin = "150.1.0";
  db.RegisterApp(entry);

  Version vmin = db.GetGlobalVmin("windows64");
  EXPECT_TRUE(vmin.IsValid());
  EXPECT_EQ("150.1.0", vmin.ToString());
}

TEST_F(InstallerDatabaseTest, GetGlobalVmin_MultipleApps) {
  Database db;
  AppEntry e1;
  e1.uuid = "app1";
  e1.platform = "windows64";
  e1.vmin = "150.2.0";
  db.RegisterApp(e1);

  AppEntry e2;
  e2.uuid = "app2";
  e2.platform = "windows64";
  e2.vmin = "149.1.0";
  db.RegisterApp(e2);

  AppEntry e3;
  e3.uuid = "app3";
  e3.platform = "windows64";
  e3.vmin = "151.0.0";
  db.RegisterApp(e3);

  Version vmin = db.GetGlobalVmin("windows64");
  EXPECT_TRUE(vmin.IsValid());
  EXPECT_EQ("149.1.0", vmin.ToString());
}

TEST_F(InstallerDatabaseTest, GetGlobalVmin_FiltersPlatform) {
  Database db;
  AppEntry e1;
  e1.uuid = "app1";
  e1.platform = "windows64";
  e1.vmin = "150.0.0";
  db.RegisterApp(e1);

  AppEntry e2;
  e2.uuid = "app2";
  e2.platform = "windowsarm64";
  e2.vmin = "148.0.0";
  db.RegisterApp(e2);

  EXPECT_EQ("150.0.0", db.GetGlobalVmin("windows64").ToString());
  EXPECT_EQ("148.0.0", db.GetGlobalVmin("windowsarm64").ToString());
  EXPECT_FALSE(db.GetGlobalVmin("linux64").IsValid());
}

}  // namespace
}  // namespace cef_installer
