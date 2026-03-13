// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_signature.h"

#include <windows.h>

#include "cef/include/wrapper/cef_certificate_util_win.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"

// clang-format off
#include <wincrypt.h>
#include <mscat.h>
// clang-format on

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "cef/libcef_dll/bootstrap/installer/installer_archive.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_archive.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_catalog.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {

using internal::BytesToHexWide;
using internal::SetSignatureTestingMode;
using internal::VerifyCatalogSignature;
using internal::VerifyFileAgainstCatalog;
using internal::VerifyFileSignature;

namespace {

// Get the certificate thumbprint from a validly signed file.
// Returns empty string if the file is unsigned or signature is invalid.
std::string GetSignatureThumbprint(const base::FilePath& file_path) {
  if (!base::PathExists(file_path)) {
    return std::string();
  }
  cef_certificate_util::ThumbprintsInfo info;
  cef_certificate_util::GetClientThumbprints(file_path.value(),
                                             /*verify_binary=*/true, info);
  if (info.IsSignedAndValid() && !info.valid_thumbprints.empty()) {
    return info.valid_thumbprints[0];
  }
  return std::string();
}

// Get path to a signed Windows system DLL for testing.
// kernel32.dll is always present and signed on Windows.
base::FilePath GetSignedSystemFile() {
  wchar_t system_dir[MAX_PATH];
  GetSystemDirectoryW(system_dir, MAX_PATH);
  return base::FilePath(system_dir).Append(L"kernel32.dll");
}

class InstallerSignatureTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath CreateTempFile(const std::string& content) {
    base::FilePath file_path = temp_dir_.GetPath().Append(L"test_file.txt");
    EXPECT_TRUE(base::WriteFile(file_path, content));
    return file_path;
  }

  base::ScopedTempDir temp_dir_;
};

// Test: Verify a signed file passes verification.
TEST_F(InstallerSignatureTest, VerifySignedFile) {
  base::FilePath signed_file = GetSignedSystemFile();
  ASSERT_TRUE(base::PathExists(signed_file));

  SignatureError result = VerifyFileSignature(signed_file, std::string());
  EXPECT_EQ(result, SignatureError::kSuccess);
}

// Test: Unsigned file returns kNotSigned or kSignatureInvalid.
// Note: A plain text file might return either error depending on how
// Windows interprets the file format.
TEST_F(InstallerSignatureTest, VerifyUnsignedFile) {
  base::FilePath unsigned_file = CreateTempFile("This is an unsigned file");

  SignatureError result = VerifyFileSignature(unsigned_file, std::string());
  // For a non-PE file, Windows may return either "not signed" or
  // "invalid format" depending on implementation details.
  EXPECT_TRUE(result == SignatureError::kNotSigned ||
              result == SignatureError::kSignatureInvalid)
      << "Got: " << SignatureErrorToString(result);
}

// Test: Tampered/corrupted file returns appropriate error.
// Note: We can't easily create a truly tampered signed file, so we test
// with a file that has invalid PE structure (just text content).
TEST_F(InstallerSignatureTest, VerifyTamperedFile) {
  // A text file isn't a valid PE and will fail signature verification
  // as "not signed" or "invalid format".
  base::FilePath invalid_file = CreateTempFile("Not a valid PE file");

  SignatureError result = VerifyFileSignature(invalid_file, std::string());
  // The exact error depends on Windows version, but it should not be kSuccess.
  EXPECT_NE(result, SignatureError::kSuccess);
}

// Test: Wrong thumbprint returns kThumbprintMismatch.
TEST_F(InstallerSignatureTest, VerifyThumbprintMismatch) {
  base::FilePath signed_file = GetSignedSystemFile();
  ASSERT_TRUE(base::PathExists(signed_file));

  // Use an obviously wrong thumbprint (all zeros).
  std::string wrong_thumbprint(40, '0');

  SignatureError result = VerifyFileSignature(signed_file, wrong_thumbprint);
  EXPECT_EQ(result, SignatureError::kThumbprintMismatch);
}

// Test: GetThumbprint extracts the correct thumbprint from a signed file.
TEST_F(InstallerSignatureTest, GetThumbprint) {
  base::FilePath signed_file = GetSignedSystemFile();
  ASSERT_TRUE(base::PathExists(signed_file));

  std::string thumbprint = GetSignatureThumbprint(signed_file);

  // Thumbprint should be 40 hex characters (SHA-1).
  EXPECT_EQ(thumbprint.length(), 40u);

  // Verify it's valid hex (all uppercase).
  for (char c : thumbprint) {
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))
        << "Invalid hex character: " << c;
  }
}

// Test: GetThumbprint returns empty for unsigned file.
TEST_F(InstallerSignatureTest, GetThumbprintUnsigned) {
  base::FilePath unsigned_file = CreateTempFile("Unsigned content");

  std::string thumbprint = GetSignatureThumbprint(unsigned_file);
  EXPECT_TRUE(thumbprint.empty());
}

// Test: FileNotFound error.
TEST_F(InstallerSignatureTest, FileNotFound) {
  base::FilePath nonexistent =
      temp_dir_.GetPath().Append(L"nonexistent_file.dll");

  SignatureError result = VerifyFileSignature(nonexistent, std::string());
  EXPECT_EQ(result, SignatureError::kFileNotFound);
}

// Test: GetThumbprint for nonexistent file returns empty.
TEST_F(InstallerSignatureTest, GetThumbprintNonexistent) {
  base::FilePath nonexistent =
      temp_dir_.GetPath().Append(L"nonexistent_file.dll");

  std::string thumbprint = GetSignatureThumbprint(nonexistent);
  EXPECT_TRUE(thumbprint.empty());
}

// Test: SignatureErrorToString returns non-null for all error codes.
TEST_F(InstallerSignatureTest, ErrorToString) {
  EXPECT_STREQ(SignatureErrorToString(SignatureError::kSuccess), "Success");
  EXPECT_STREQ(SignatureErrorToString(SignatureError::kFileNotFound),
               "File not found");
  EXPECT_STREQ(SignatureErrorToString(SignatureError::kNotSigned),
               "File is not signed");
  EXPECT_STREQ(SignatureErrorToString(SignatureError::kSignatureInvalid),
               "Signature is invalid");
  EXPECT_STREQ(SignatureErrorToString(SignatureError::kCertificateExpired),
               "Certificate has expired");
  EXPECT_STREQ(SignatureErrorToString(SignatureError::kCertificateRevoked),
               "Certificate has been revoked");
  EXPECT_STREQ(SignatureErrorToString(SignatureError::kThumbprintMismatch),
               "Certificate thumbprint does not match expected value");
  EXPECT_STREQ(SignatureErrorToString(SignatureError::kCatalogNotFound),
               "Catalog file not found");
  EXPECT_STREQ(SignatureErrorToString(SignatureError::kCatalogInvalid),
               "Catalog file signature is invalid");
  EXPECT_STREQ(SignatureErrorToString(SignatureError::kFileNotInCatalog),
               "File is not listed in the catalog");
  EXPECT_STREQ(SignatureErrorToString(SignatureError::kHashMismatch),
               "File hash does not match catalog entry");
}

// Test: Verify signed file with correct thumbprint succeeds.
TEST_F(InstallerSignatureTest, VerifyWithCorrectThumbprint) {
  base::FilePath signed_file = GetSignedSystemFile();
  ASSERT_TRUE(base::PathExists(signed_file));

  // First get the actual thumbprint.
  std::string actual_thumbprint = GetSignatureThumbprint(signed_file);
  ASSERT_FALSE(actual_thumbprint.empty());

  // Verify with the correct thumbprint.
  SignatureError result = VerifyFileSignature(signed_file, actual_thumbprint);
  EXPECT_EQ(result, SignatureError::kSuccess);
}

// ============================================================================
// Catalog verification tests using Windows system files.
// ============================================================================

// Helper to find the catalog that contains a given system file.
base::FilePath FindCatalogForFile(const base::FilePath& file_path) {
  HCATADMIN hCatAdmin = nullptr;
  if (!CryptCATAdminAcquireContext(&hCatAdmin, nullptr, 0)) {
    return base::FilePath();
  }

  base::FilePath catalog_path;

  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (file.IsValid()) {
    HANDLE file_handle = reinterpret_cast<HANDLE>(file.GetPlatformFile());

    DWORD hash_size = 0;
    CryptCATAdminCalcHashFromFileHandle(file_handle, &hash_size, nullptr, 0);

    if (hash_size > 0) {
      std::vector<BYTE> file_hash(hash_size);
      if (CryptCATAdminCalcHashFromFileHandle(file_handle, &hash_size,
                                              file_hash.data(), 0)) {
        HCATINFO hCatInfo = CryptCATAdminEnumCatalogFromHash(
            hCatAdmin, file_hash.data(), hash_size, 0, nullptr);

        if (hCatInfo) {
          CATALOG_INFO catInfo = {};
          catInfo.cbStruct = sizeof(catInfo);
          if (CryptCATCatalogInfoFromContext(hCatInfo, &catInfo, 0)) {
            catalog_path = base::FilePath(catInfo.wszCatalogFile);
          }
          CryptCATAdminReleaseCatalogContext(hCatAdmin, hCatInfo, 0);
        }
      }
    }
  }

  CryptCATAdminReleaseContext(hCatAdmin, 0);
  return catalog_path;
}

// Test: VerifyFileAgainstCatalog succeeds for system file in its catalog.
TEST_F(InstallerSignatureTest, VerifyFileAgainstCatalogSuccess) {
  base::FilePath system_file = GetSignedSystemFile();
  ASSERT_TRUE(base::PathExists(system_file));

  // Find which catalog contains this system file.
  base::FilePath catalog_path = FindCatalogForFile(system_file);

  // Some system files might not be in a catalog (signed directly).
  // Skip test if no catalog found.
  if (catalog_path.empty()) {
    GTEST_SKIP() << "System file not in a catalog (may be directly signed)";
  }

  ASSERT_TRUE(base::PathExists(catalog_path))
      << "Catalog should exist: " << catalog_path;

  // Verify the file against its catalog.
  SignatureError result = VerifyFileAgainstCatalog(system_file, catalog_path);
  EXPECT_EQ(result, SignatureError::kSuccess)
      << "File: " << system_file << ", Catalog: " << catalog_path;
}

// Test: VerifyFileAgainstCatalog fails for file not in the catalog.
TEST_F(InstallerSignatureTest, VerifyFileAgainstCatalogNotInCatalog) {
  // Create a temp file that won't be in any system catalog.
  base::FilePath temp_file = CreateTempFile("Not in any catalog");

  // Find a system catalog to test against.
  base::FilePath system_file = GetSignedSystemFile();
  base::FilePath catalog_path = FindCatalogForFile(system_file);

  if (catalog_path.empty()) {
    GTEST_SKIP() << "Could not find a system catalog for testing";
  }

  // Our temp file should not be in the system catalog.
  SignatureError result = VerifyFileAgainstCatalog(temp_file, catalog_path);
  EXPECT_EQ(result, SignatureError::kFileNotInCatalog);
}

// Test: VerifyFileAgainstCatalog with nonexistent file.
TEST_F(InstallerSignatureTest, VerifyFileAgainstCatalogFileNotFound) {
  base::FilePath nonexistent = temp_dir_.GetPath().Append(L"nonexistent.dll");
  base::FilePath fake_catalog = temp_dir_.GetPath().Append(L"fake.cat");
  ASSERT_TRUE(base::WriteFile(fake_catalog, "placeholder"));

  SignatureError result = VerifyFileAgainstCatalog(nonexistent, fake_catalog);
  EXPECT_EQ(result, SignatureError::kFileNotFound);
}

// Test: VerifyFileAgainstCatalog with nonexistent catalog.
TEST_F(InstallerSignatureTest, VerifyFileAgainstCatalogCatalogNotFound) {
  base::FilePath temp_file = CreateTempFile("test content");
  base::FilePath nonexistent_catalog =
      temp_dir_.GetPath().Append(L"nonexistent.cat");

  SignatureError result =
      VerifyFileAgainstCatalog(temp_file, nonexistent_catalog);
  EXPECT_EQ(result, SignatureError::kCatalogNotFound);
}

// ============================================================================
// Catalog error handling tests.
// ============================================================================

// Test: CatalogNotFound when catalog.cat doesn't exist in directory.
TEST_F(InstallerSignatureTest, CatalogNotFound) {
  // Empty temp dir — no catalog.cat present.
  SignatureError result = VerifyWithCatalog(temp_dir_.GetPath(), "");
  EXPECT_EQ(result, SignatureError::kCatalogNotFound);
}

// Test: CatalogInvalid when catalog.cat exists but is not a valid catalog.
TEST_F(InstallerSignatureTest, CatalogInvalid) {
  base::FilePath fake_catalog = temp_dir_.GetPath().Append(kCatalogFilename);
  ASSERT_TRUE(base::WriteFile(fake_catalog, "This is not a real catalog"));

  SignatureError result = VerifyWithCatalog(temp_dir_.GetPath(), "");
  // Should fail as not signed or invalid.
  EXPECT_TRUE(result == SignatureError::kCatalogInvalid ||
              result == SignatureError::kNotSigned);
}

// ============================================================================
// BytesToHexWide tests.
// ============================================================================

TEST_F(InstallerSignatureTest, BytesToHexWideEmpty) {
  std::vector<BYTE> empty;
  EXPECT_EQ(BytesToHexWide(empty), L"");
}

TEST_F(InstallerSignatureTest, BytesToHexWideBasic) {
  std::vector<BYTE> bytes = {0x00, 0xFF, 0xAB, 0x12, 0x0F};
  EXPECT_EQ(BytesToHexWide(bytes), L"00FFAB120F");
}

// ============================================================================
// Direct catalog parsing tests (VerifyFileAgainstCatalog with CryptCATOpen).
// ============================================================================

// Test: Direct catalog parsing returns kCatalogInvalid for non-catalog files.
TEST_F(InstallerSignatureTest, VerifyFileAgainstCatalogInvalidCatalog) {
  base::FilePath temp_file = CreateTempFile("test content");
  base::FilePath fake_catalog = temp_dir_.GetPath().Append(L"fake.cat");
  ASSERT_TRUE(base::WriteFile(fake_catalog, "not a real catalog"));

  EXPECT_EQ(SignatureError::kCatalogInvalid,
            VerifyFileAgainstCatalog(temp_file, fake_catalog));
}

// ============================================================================
// Testing mode tests.
// ============================================================================

// Test: Testing mode does not break normal (trusted) signature verification.
TEST_F(InstallerSignatureTest, TestingModeDoesNotBreakNormalVerification) {
  base::FilePath signed_file = GetSignedSystemFile();
  ASSERT_TRUE(base::PathExists(signed_file));

  std::string thumbprint = GetSignatureThumbprint(signed_file);
  ASSERT_FALSE(thumbprint.empty());

  // Enable testing mode - should not affect already-valid signatures.
  SetSignatureTestingMode(true);

  EXPECT_EQ(SignatureError::kSuccess,
            VerifyFileSignature(signed_file, thumbprint));

  // Wrong thumbprint should still fail even in testing mode.
  // The Microsoft thumbprint is in valid_thumbprints, not invalid_thumbprints,
  // so testing mode's invalid_thumbprints check won't find it either.
  EXPECT_EQ(SignatureError::kThumbprintMismatch,
            VerifyFileSignature(signed_file, std::string(40, '0')));

  SetSignatureTestingMode(false);
}

// Test: Testing mode rejects unsigned files.
TEST_F(InstallerSignatureTest, TestingModeRejectsUnsignedFiles) {
  base::FilePath unsigned_file = CreateTempFile("Not a valid PE file");

  SetSignatureTestingMode(true);

  SignatureError result =
      VerifyFileSignature(unsigned_file, std::string(40, 'A'));
  // Unsigned file has no signature at all, so testing mode can't help.
  EXPECT_NE(SignatureError::kSuccess, result);

  SetSignatureTestingMode(false);
}

// ============================================================================
// Test catalog generation.
// ============================================================================

using test::GetTestCertificateThumbprint;
using test::GetTestDataPath;

TEST_F(InstallerSignatureTest, CreateSignedCatalogSmokeTest) {
  base::FilePath pfx_path = GetTestDataPath().Append(L"test_signing.pfx");
  if (!base::PathExists(pfx_path)) {
    GTEST_SKIP() << "Test certificate not found at: " << pfx_path;
  }

  // Copy a system DLL into temp dir as a test file.
  base::FilePath system_dll = GetSignedSystemFile();
  base::FilePath test_file = temp_dir_.GetPath().Append(L"test.dll");
  ASSERT_TRUE(base::CopyFile(system_dll, test_file));

  // Create a signed catalog containing the test file.
  base::FilePath catalog = temp_dir_.GetPath().Append(L"test.cat");
  test::CatalogError result =
      test::CreateSignedCatalog({test_file}, catalog, pfx_path, "test");
  EXPECT_EQ(result, test::CatalogError::kSuccess)
      << "Error: " << test::CatalogErrorToString(result);

  // Verify the catalog file was created.
  EXPECT_TRUE(base::PathExists(catalog));

  // Verify the test file's hash is in the catalog.
  EXPECT_EQ(SignatureError::kSuccess,
            VerifyFileAgainstCatalog(test_file, catalog));

  // A different file should NOT be in the catalog.
  base::FilePath other_file = temp_dir_.GetPath().Append(L"other.txt");
  ASSERT_TRUE(base::WriteFile(other_file, "different content"));
  EXPECT_EQ(SignatureError::kFileNotInCatalog,
            VerifyFileAgainstCatalog(other_file, catalog));
}

TEST_F(InstallerSignatureTest, CreateSignedCatalogVerifySignature) {
  base::FilePath pfx_path = GetTestDataPath().Append(L"test_signing.pfx");
  if (!base::PathExists(pfx_path)) {
    GTEST_SKIP() << "Test certificate not found";
  }

  std::string thumbprint = GetTestCertificateThumbprint();
  ASSERT_FALSE(thumbprint.empty()) << "Could not read test thumbprint";

  // Create a test file and catalog.
  base::FilePath system_dll = GetSignedSystemFile();
  base::FilePath test_file = temp_dir_.GetPath().Append(L"test.dll");
  ASSERT_TRUE(base::CopyFile(system_dll, test_file));

  base::FilePath catalog = temp_dir_.GetPath().Append(L"test.cat");
  ASSERT_EQ(test::CatalogError::kSuccess,
            test::CreateSignedCatalog({test_file}, catalog, pfx_path, "test"));

  // Catalog is signed with self-signed cert, so normal verification fails.
  EXPECT_NE(SignatureError::kSuccess,
            VerifyCatalogSignature(catalog, thumbprint));

  // With testing mode, VerifyCatalogSignature should accept self-signed certs.
  SetSignatureTestingMode(true);
  EXPECT_EQ(SignatureError::kSuccess,
            VerifyCatalogSignature(catalog, thumbprint));

  // Wrong thumbprint should still fail.
  EXPECT_EQ(SignatureError::kThumbprintMismatch,
            VerifyCatalogSignature(catalog, std::string(40, '0')));
  SetSignatureTestingMode(false);
}

// ============================================================================
// CatalogProgressCallback tests.
// ============================================================================

TEST_F(InstallerSignatureTest, VerifyWithCatalogProgressReportsAllFiles) {
  base::FilePath pfx_path = GetTestDataPath().Append(L"test_signing.pfx");
  if (!base::PathExists(pfx_path)) {
    GTEST_SKIP() << "Test certificate not found";
  }

  std::string thumbprint = GetTestCertificateThumbprint();
  ASSERT_FALSE(thumbprint.empty());

  // Create multiple test files.
  std::vector<base::FilePath> files;
  for (int i = 0; i < 3; i++) {
    base::FilePath f =
        temp_dir_.GetPath().Append(L"file_" + std::to_wstring(i) + L".dll");
    ASSERT_TRUE(base::CopyFile(GetSignedSystemFile(), f));
    files.push_back(f);
  }

  // Create a signed catalog for all files.
  base::FilePath catalog = temp_dir_.GetPath().Append(L"catalog.cat");
  ASSERT_EQ(test::CatalogError::kSuccess,
            test::CreateSignedCatalog(files, catalog, pfx_path, "test"));

  SetSignatureTestingMode(true);

  // Track progress callbacks.
  std::vector<std::pair<uint64_t, uint64_t>> progress_calls;
  CatalogProgressCallback progress = base::BindRepeating(
      [](std::vector<std::pair<uint64_t, uint64_t>>* calls, uint64_t done,
         uint64_t total) {
        calls->push_back({done, total});
        return true;
      },
      &progress_calls);

  SignatureError result =
      VerifyWithCatalog(temp_dir_.GetPath(), thumbprint, progress);
  EXPECT_EQ(SignatureError::kSuccess, result);

  // Should have one callback per file (3 files, catalog excluded).
  ASSERT_EQ(3u, progress_calls.size());
  for (size_t i = 0; i < progress_calls.size(); i++) {
    EXPECT_EQ(i + 1, progress_calls[i].first) << "files_done at call " << i;
    EXPECT_EQ(3u, progress_calls[i].second) << "files_total at call " << i;
  }

  SetSignatureTestingMode(false);
}

TEST_F(InstallerSignatureTest, VerifyWithCatalogProgressCancels) {
  base::FilePath pfx_path = GetTestDataPath().Append(L"test_signing.pfx");
  if (!base::PathExists(pfx_path)) {
    GTEST_SKIP() << "Test certificate not found";
  }

  std::string thumbprint = GetTestCertificateThumbprint();
  ASSERT_FALSE(thumbprint.empty());

  // Create multiple test files.
  std::vector<base::FilePath> files;
  for (int i = 0; i < 3; i++) {
    base::FilePath f =
        temp_dir_.GetPath().Append(L"file_" + std::to_wstring(i) + L".dll");
    ASSERT_TRUE(base::CopyFile(GetSignedSystemFile(), f));
    files.push_back(f);
  }

  base::FilePath catalog = temp_dir_.GetPath().Append(L"catalog.cat");
  ASSERT_EQ(test::CatalogError::kSuccess,
            test::CreateSignedCatalog(files, catalog, pfx_path, "test"));

  SetSignatureTestingMode(true);

  // Cancel after the first file.
  int call_count = 0;
  CatalogProgressCallback progress = base::BindRepeating(
      [](int* count, uint64_t, uint64_t) {
        (*count)++;
        return *count < 2;  // Cancel on second call
      },
      &call_count);

  SignatureError result =
      VerifyWithCatalog(temp_dir_.GetPath(), thumbprint, progress);
  EXPECT_EQ(SignatureError::kCancelled, result);
  EXPECT_EQ(2, call_count);

  SetSignatureTestingMode(false);
}

TEST(SignatureErrorToStringTest, CancelledError) {
  EXPECT_STREQ("Operation cancelled",
               SignatureErrorToString(SignatureError::kCancelled));
}

}  // namespace

// ============================================================================
// Test archive builder.
// ============================================================================

using test::GetTestCertificateThumbprint;
using test::GetTestDataPath;

TEST(TestArchiveTest, BuildTestDistributionCreatesValidArchive) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath pfx = GetTestDataPath().AppendASCII("test_signing.pfx");
  if (!base::PathExists(pfx)) {
    GTEST_SKIP() << "Test certificate not found at: " << pfx;
  }

  auto dist = test::BuildTestDistribution("137.3.5", "abc123",
                                          temp_dir.GetPath(), pfx, "test");

  ASSERT_TRUE(dist.has_value()) << "BuildTestDistribution failed";
  EXPECT_EQ("137.3.5", dist->version);
  EXPECT_EQ("abc123", dist->abi_hash);
  EXPECT_TRUE(base::PathExists(dist->archive_path));
  EXPECT_EQ(40u, dist->archive_sha1.length());
}

TEST(TestArchiveTest, ArchiveContainsExpectedFiles) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath pfx = GetTestDataPath().AppendASCII("test_signing.pfx");
  if (!base::PathExists(pfx)) {
    GTEST_SKIP() << "Test certificate not found at: " << pfx;
  }

  auto dist = test::BuildTestDistribution("137.3.5", "abc123",
                                          temp_dir.GetPath(), pfx, "test");
  ASSERT_TRUE(dist.has_value());

  // Extract archive and verify contents.
  base::FilePath extract_dir = temp_dir.GetPath().Append(L"extract");
  ASSERT_TRUE(base::CreateDirectory(extract_dir));

  ArchiveError err = ExtractTarXz(dist->archive_path, extract_dir);
  ASSERT_EQ(ArchiveError::kSuccess, err)
      << "Extraction failed: " << ArchiveErrorToString(err);

  EXPECT_TRUE(base::PathExists(GetLibcefPath(extract_dir)));
  EXPECT_TRUE(base::PathExists(extract_dir.Append(kVersionMetadataFilename)));
  EXPECT_TRUE(base::PathExists(extract_dir.Append(kCatalogFilename)));
}

TEST(TestArchiveTest, CatalogVerificationPasses) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath pfx = GetTestDataPath().AppendASCII("test_signing.pfx");
  if (!base::PathExists(pfx)) {
    GTEST_SKIP() << "Test certificate not found at: " << pfx;
  }

  std::string thumbprint = GetTestCertificateThumbprint();
  ASSERT_FALSE(thumbprint.empty()) << "Could not read test thumbprint";

  auto dist = test::BuildTestDistribution("137.3.5", "abc123",
                                          temp_dir.GetPath(), pfx, "test");
  ASSERT_TRUE(dist.has_value());

  // Extract and verify catalog in testing mode.
  base::FilePath extract_dir = temp_dir.GetPath().Append(L"extract");
  ASSERT_TRUE(base::CreateDirectory(extract_dir));
  ASSERT_EQ(ArchiveError::kSuccess,
            ExtractTarXz(dist->archive_path, extract_dir));

  SetSignatureTestingMode(true);

  base::FilePath catalog = extract_dir.Append(kCatalogFilename);
  EXPECT_EQ(SignatureError::kSuccess,
            VerifyCatalogSignature(catalog, thumbprint));

  // Verify individual files against catalog.
  EXPECT_EQ(SignatureError::kSuccess,
            VerifyFileAgainstCatalog(GetLibcefPath(extract_dir), catalog));
  EXPECT_EQ(SignatureError::kSuccess,
            VerifyFileAgainstCatalog(
                extract_dir.Append(kVersionMetadataFilename), catalog));

  SetSignatureTestingMode(false);
}

}  // namespace cef_installer
