// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_SIGNATURE_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_SIGNATURE_H_

// =============================================================================
// SECURITY NOTES
// =============================================================================
//
// This module provides Authenticode signature verification for CEF installer
// security. Key security properties:
//
// 1. CERTIFICATE PINNING: Always use VerifyFileSignature() with an
//    expected_thumbprint to pin to a specific certificate. Without pinning,
//    any valid Authenticode signature (including attacker-controlled certs)
//    will pass verification.
//
// 2. NO NETWORK CALLS: Revocation checking is disabled (WTD_REVOKE_NONE) to
//    prevent attackers from DoS'ing revocation servers. Compromised versions
//    are handled via the separate version revocation mechanism.
//
// 3. CATALOG VERIFICATION: When using VerifyFileAgainstCatalog() directly,
//    the caller MUST first verify the catalog file's signature using
//    VerifyFileSignature(). The VerifyWithCatalog() function does this
//    automatically.
//
// 4. TIMESTAMPS: Signatures should include RFC 3161 timestamps to remain
//    valid after certificate expiration. This is enforced by the signing
//    process, not verification.
//
// =============================================================================

#include <windows.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"

namespace cef_installer {

// Callback for catalog verification progress.
// Parameters: files_checked, files_total.
// Return false to cancel verification.
using CatalogProgressCallback =
    base::RepeatingCallback<bool(uint64_t, uint64_t)>;

// Error codes for signature operations.
enum class SignatureError {
  kSuccess,
  kFileNotFound,        // File doesn't exist
  kNotSigned,           // File has no signature
  kSignatureInvalid,    // Signature doesn't verify
  kCertificateExpired,  // Signing certificate has expired (but see timestamps)
  kCertificateRevoked,  // Signing certificate is revoked
  kThumbprintMismatch,  // Certificate doesn't match expected thumbprint
  kCatalogNotFound,     // Catalog file not found
  kCatalogInvalid,      // Catalog file signature invalid
  kFileNotInCatalog,    // File not listed in catalog
  kHashMismatch,        // File hash doesn't match catalog entry
  kCancelled,           // Operation cancelled via progress callback
};

// Verify all files in a directory against its catalog file (catalog.cat).
//
// Parameters:
// - directory: Directory containing files and catalog.cat to verify
// - expected_thumbprint: Expected certificate thumbprint for the catalog
//                        signature
//
// Process:
// 1. Verify catalog.cat signature (must be signed with expected cert)
// 2. Reject reparse points in directory or any contents
// 3. For each file in directory, verify its hash matches catalog entry
SignatureError VerifyWithCatalog(const base::FilePath& directory,
                                 const std::string& expected_thumbprint,
                                 CatalogProgressCallback progress = {});

// Convert error code to human-readable string for logging.
const char* SignatureErrorToString(SignatureError error);

// ============================================================================
// Internal helpers exposed for testing. Not part of the public API.
// ============================================================================
namespace internal {

// Enable/disable signature testing mode.
// When enabled, self-signed certificate signatures are accepted if the
// thumbprint matches. This allows testing with self-signed certificates
// without requiring them to be installed in the trusted root store.
//
// SECURITY: No-op in official release builds (OFFICIAL_BUILD && NDEBUG).
void SetSignatureTestingMode(bool enabled);

// Convert byte array to uppercase wide hex string.
// Used for comparing file hashes against catalog member tags.
std::wstring BytesToHexWide(const std::vector<BYTE>& bytes);

// Verify Authenticode signature on a single file.
// Uses WinVerifyTrust with WINTRUST_ACTION_GENERIC_VERIFY_V2.
//
// Parameters:
// - file_path: Path to the file to verify
// - expected_thumbprint: Certificate must match this thumbprint
//                        (hex string, uppercase, 40 characters for SHA-1).
//                        Empty is allowed in test builds for "any valid
//                        signature" checks.
//
// SECURITY: In production builds (OFFICIAL_BUILD && NDEBUG), passing an empty
// thumbprint triggers CHECK failure. This prevents accidentally skipping
// certificate pinning.
//
// Note: Uses WTD_REVOKE_NONE to avoid network calls (per spec recommendation).
// Revocation is handled via the version revocation mechanism instead.
SignatureError VerifyFileSignature(const base::FilePath& file_path,
                                   const std::string& expected_thumbprint);

// Verify a single file's hash is listed in a catalog file.
//
// SECURITY WARNING: This function does NOT verify the catalog's signature.
// An attacker could craft a malicious catalog containing hashes of malware.
// Callers MUST first verify the catalog using VerifyFileSignature() with
// certificate pinning. Use VerifyWithCatalog() instead for automatic
// catalog signature verification.
//
// Parameters:
// - file_path: Path to the file to verify
// - catalog_path: Path to the .cat file
//
// Returns kSuccess if the file's hash is found in the catalog.
// Returns kFileNotInCatalog if the file is not listed.
// Returns kFileNotFound if the file doesn't exist.
// Returns kCatalogNotFound if the catalog doesn't exist.
SignatureError VerifyFileAgainstCatalog(const base::FilePath& file_path,
                                        const base::FilePath& catalog_path);

// Verify the PKCS#7 signature on a catalog (.cat) file.
// Catalog files embed signatures differently from PE binaries (PKCS#7 vs
// Authenticode), so this uses GetClientThumbprints rather than
// ValidateCodeSigning for thumbprint extraction.
//
// Parameters:
// - catalog_path: Path to the .cat file
// - expected_thumbprint: Expected certificate thumbprint (hex, 40 chars)
//
// In testing mode (SetSignatureTestingMode), accepts self-signed catalogs
// if the signer thumbprint matches.
SignatureError VerifyCatalogSignature(const base::FilePath& catalog_path,
                                      const std::string& expected_thumbprint);

}  // namespace internal

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_SIGNATURE_H_
