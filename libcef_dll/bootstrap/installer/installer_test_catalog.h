// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_TEST_CATALOG_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_TEST_CATALOG_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"

namespace cef_installer {
namespace test {

// Result of catalog generation.
enum class CatalogError {
  kSuccess,
  kFileNotFound,
  kCatalogCreationFailed,
  kSigningFailed,
  kCertificateNotFound,
};

// Creates a signed catalog file containing hashes of the specified files.
//
// Parameters:
//   files: List of files to include in the catalog (absolute paths)
//   catalog_path: Output path for the .cat file
//   pfx_path: Path to PKCS#12 file containing signing certificate
//   pfx_password: Password for the PFX file
//
// The catalog will contain SHA256 hashes for each file.
// File paths in the catalog are stored as basenames only.
CatalogError CreateSignedCatalog(const std::vector<base::FilePath>& files,
                                 const base::FilePath& catalog_path,
                                 const base::FilePath& pfx_path,
                                 const std::string& pfx_password);

// Convert CatalogError to human-readable string for logging.
const char* CatalogErrorToString(CatalogError error);

}  // namespace test
}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_TEST_CATALOG_H_
