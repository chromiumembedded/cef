// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_signature.h"

#include <windows.h>

// clang-format off
#include <bcrypt.h>
#include <wincrypt.h>
#include <mscat.h>
// clang-format on

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "cef/include/wrapper/cef_certificate_util_win.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")

namespace cef_installer {

// Testing mode state - accepts self-signed cert if thumbprint matches.
// No-op in official release builds. Follows the same pattern as
// SetTestingMode() in installer_controller.cc.
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
namespace {
bool g_signature_testing_mode = false;
}  // namespace
#endif

namespace internal {

#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
void SetSignatureTestingMode(bool enabled) {}
#else
void SetSignatureTestingMode(bool enabled) {
  g_signature_testing_mode = enabled;
}
#endif

std::wstring BytesToHexWide(const std::vector<BYTE>& bytes) {
  static const wchar_t kHexChars[] = L"0123456789ABCDEF";
  std::wstring result;
  result.reserve(bytes.size() * 2);
  for (BYTE b : bytes) {
    result.push_back(kHexChars[(b >> 4) & 0xF]);
    result.push_back(kHexChars[b & 0xF]);
  }
  return result;
}

SignatureError VerifyFileSignature(const base::FilePath& file_path,
                                   const std::string& expected_thumbprint) {
  // In production builds, an empty thumbprint means no certificate pinning,
  // which would allow any valid Authenticode signature to pass. Crash to
  // surface this as a bug via crash reporting.
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  CHECK(!expected_thumbprint.empty());
#endif

  if (!base::PathExists(file_path)) {
    return SignatureError::kFileNotFound;
  }

  cef_certificate_util::ThumbprintsInfo info;
  bool valid = cef_certificate_util::ValidateCodeSigning(
      file_path.value(),
      expected_thumbprint.empty() ? nullptr : expected_thumbprint.c_str(),
      /*allow_unsigned=*/false, info);

  if (valid) {
    return SignatureError::kSuccess;
  }

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  // In testing mode, accept self-signed cert if signature exists and
  // thumbprint matches. For self-signed certs: has_signature=true,
  // valid_thumbprints is empty, and the thumbprint appears in
  // invalid_thumbprints.
  if (g_signature_testing_mode && info.has_signature &&
      !expected_thumbprint.empty()) {
    for (const auto& tp : info.invalid_thumbprints) {
      if (base::EqualsCaseInsensitiveASCII(tp, expected_thumbprint)) {
        return SignatureError::kSuccess;
      }
    }
    for (const auto& tp : info.valid_thumbprints) {
      if (base::EqualsCaseInsensitiveASCII(tp, expected_thumbprint)) {
        return SignatureError::kSuccess;
      }
    }
    return SignatureError::kThumbprintMismatch;
  }
#endif

  if (!info.has_signature) {
    return SignatureError::kNotSigned;
  }

  if (!info.errors.empty()) {
    // Signature exists but validation failed.
    return SignatureError::kSignatureInvalid;
  }

  // Valid signature but thumbprint didn't match.
  if (!expected_thumbprint.empty()) {
    return SignatureError::kThumbprintMismatch;
  }
  return SignatureError::kSignatureInvalid;
}

SignatureError VerifyFileAgainstCatalog(const base::FilePath& file_path,
                                        const base::FilePath& catalog_path) {
  if (!base::PathExists(file_path)) {
    return SignatureError::kFileNotFound;
  }
  if (!base::PathExists(catalog_path)) {
    return SignatureError::kCatalogNotFound;
  }

  // Compute the file's SHA256 hash using CryptCATAdminCalcHashFromFileHandle2.
  // This produces the same hash format that catalog members use when the
  // catalog is created with HashAlgorithms=SHA256.
  //
  // We use CryptCATAdminAcquireContext2 + CryptCATAdminCalcHashFromFileHandle2
  // instead of the legacy CryptCATAdminCalcHashFromFileHandle (which defaults
  // to SHA1). SHA1 is cryptographically broken for collisions.
  HCATADMIN hCatAdmin = NULL;
  if (!CryptCATAdminAcquireContext2(&hCatAdmin, nullptr,
                                    BCRYPT_SHA256_ALGORITHM, nullptr, 0)) {
    return SignatureError::kFileNotInCatalog;
  }

  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    CryptCATAdminReleaseContext(hCatAdmin, 0);
    return SignatureError::kFileNotFound;
  }

  HANDLE file_handle = reinterpret_cast<HANDLE>(file.GetPlatformFile());
  DWORD hash_size = 0;
  CryptCATAdminCalcHashFromFileHandle2(hCatAdmin, file_handle, &hash_size,
                                       nullptr, 0);
  if (hash_size == 0) {
    CryptCATAdminReleaseContext(hCatAdmin, 0);
    return SignatureError::kFileNotInCatalog;
  }

  std::vector<BYTE> file_hash(hash_size);
  if (!CryptCATAdminCalcHashFromFileHandle2(hCatAdmin, file_handle, &hash_size,
                                            file_hash.data(), 0)) {
    CryptCATAdminReleaseContext(hCatAdmin, 0);
    return SignatureError::kFileNotInCatalog;
  }

  CryptCATAdminReleaseContext(hCatAdmin, 0);

  // Convert hash to hex string for comparison with catalog member tags.
  std::wstring file_hash_hex = internal::BytesToHexWide(file_hash);

  // Open the catalog file directly (not the system catalog store).
  // This avoids requiring admin privileges to register catalogs.
  // dwPublicVersion=0 auto-detects the catalog version.
  HANDLE hCatalog = CryptCATOpen(
      const_cast<LPWSTR>(catalog_path.value().c_str()), CRYPTCAT_OPEN_EXISTING,
      NULL,  // hProv
      0,     // dwPublicVersion (auto-detect)
      0);    // dwEncodingType

  if (hCatalog == INVALID_HANDLE_VALUE) {
    return SignatureError::kCatalogInvalid;
  }

  // Enumerate catalog members looking for a matching hash.
  SignatureError result = SignatureError::kFileNotInCatalog;
  CRYPTCATMEMBER* pMember = nullptr;

  while ((pMember = CryptCATEnumerateMember(hCatalog, pMember)) != nullptr) {
    // The member's indirect data contains the hash.
    if (pMember->pIndirectData &&
        pMember->pIndirectData->Digest.cbData == hash_size) {
      if (memcmp(pMember->pIndirectData->Digest.pbData, file_hash.data(),
                 hash_size) == 0) {
        result = SignatureError::kSuccess;
        break;
      }
    }

    // Alternative: Compare the member's reference tag (hash hex string).
    // Some catalogs store the hash as the member tag in hex format.
    if (pMember->pwszReferenceTag &&
        _wcsicmp(pMember->pwszReferenceTag, file_hash_hex.c_str()) == 0) {
      result = SignatureError::kSuccess;
      break;
    }
  }

  CryptCATClose(hCatalog);
  return result;
}

SignatureError VerifyCatalogSignature(const base::FilePath& catalog_path,
                                      const std::string& expected_thumbprint) {
  if (!base::PathExists(catalog_path)) {
    return SignatureError::kCatalogNotFound;
  }

  // Reject reparse points (symlinks/junctions) to prevent an attacker from
  // redirecting catalog verification to a differently-signed file.
  if (IsReparsePoint(catalog_path)) {
    return SignatureError::kCatalogInvalid;
  }

  // First try standard Authenticode verification (works for catalogs signed
  // with trusted certificates in production).
  SignatureError result =
      VerifyFileSignature(catalog_path, expected_thumbprint);
  if (result == SignatureError::kSuccess) {
    return result;
  }

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  // In testing mode, catalog files may be signed with self-signed certs.
  // ValidateCodeSigning doesn't extract thumbprints from catalog PKCS#7
  // signatures, so use GetClientThumbprints to read the signer cert directly.
  if (g_signature_testing_mode && !expected_thumbprint.empty()) {
    cef_certificate_util::ThumbprintsInfo info;
    cef_certificate_util::GetClientThumbprints(catalog_path.value(),
                                               /*verify_binary=*/false, info);
    if (!info.has_signature) {
      return SignatureError::kNotSigned;
    }
    for (const auto& tp : info.valid_thumbprints) {
      if (base::EqualsCaseInsensitiveASCII(tp, expected_thumbprint)) {
        return SignatureError::kSuccess;
      }
    }
    for (const auto& tp : info.invalid_thumbprints) {
      if (base::EqualsCaseInsensitiveASCII(tp, expected_thumbprint)) {
        return SignatureError::kSuccess;
      }
    }
    return SignatureError::kThumbprintMismatch;
  }
#endif

  return result;
}

}  // namespace internal

SignatureError VerifyWithCatalog(const base::FilePath& directory,
                                 const std::string& expected_thumbprint,
                                 CatalogProgressCallback progress) {
  // Reject if the directory itself is a reparse point.
  if (IsReparsePoint(directory)) {
    return SignatureError::kFileNotFound;
  }

  base::FilePath catalog_path = directory.Append(kCatalogFilename);

  // Verify catalog exists and is a file (not a directory).
  if (!base::PathExists(catalog_path) || base::DirectoryExists(catalog_path)) {
    return SignatureError::kCatalogNotFound;
  }

  SignatureError catalog_result =
      internal::VerifyCatalogSignature(catalog_path, expected_thumbprint);
  if (catalog_result != SignatureError::kSuccess) {
    if (catalog_result == SignatureError::kNotSigned ||
        catalog_result == SignatureError::kSignatureInvalid) {
      return SignatureError::kCatalogInvalid;
    }
    return catalog_result;
  }

  // Collect all files first so we can report progress as files_done/total.
  // Reject reparse points — legitimate distributions never contain them,
  // and an attacker could use them to redirect file loads after verification.
  std::vector<base::FilePath> files;
  {
    base::FileEnumerator enumerator(
        directory, true /* recursive */,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    for (base::FilePath path = enumerator.Next(); !path.empty();
         path = enumerator.Next()) {
      if (IsReparsePoint(path)) {
        return SignatureError::kHashMismatch;
      }
      if (enumerator.GetInfo().IsDirectory() || path == catalog_path) {
        continue;
      }
      files.push_back(path);
    }
  }

  const uint64_t total = files.size();
  uint64_t checked = 0;

  for (const auto& file_path : files) {
    SignatureError file_result =
        internal::VerifyFileAgainstCatalog(file_path, catalog_path);
    if (file_result != SignatureError::kSuccess) {
      return file_result;
    }

    ++checked;
    if (progress && !progress.Run(checked, total)) {
      return SignatureError::kCancelled;
    }
  }

  return SignatureError::kSuccess;
}

const char* SignatureErrorToString(SignatureError error) {
  switch (error) {
    case SignatureError::kSuccess:
      return "Success";
    case SignatureError::kFileNotFound:
      return "File not found";
    case SignatureError::kNotSigned:
      return "File is not signed";
    case SignatureError::kSignatureInvalid:
      return "Signature is invalid";
    case SignatureError::kCertificateExpired:
      return "Certificate has expired";
    case SignatureError::kCertificateRevoked:
      return "Certificate has been revoked";
    case SignatureError::kThumbprintMismatch:
      return "Certificate thumbprint does not match expected value";
    case SignatureError::kCatalogNotFound:
      return "Catalog file not found";
    case SignatureError::kCatalogInvalid:
      return "Catalog file signature is invalid";
    case SignatureError::kFileNotInCatalog:
      return "File is not listed in the catalog";
    case SignatureError::kHashMismatch:
      return "File hash does not match catalog entry";
    case SignatureError::kCancelled:
      return "Operation cancelled";
  }
  return "Unknown error";
}

}  // namespace cef_installer
