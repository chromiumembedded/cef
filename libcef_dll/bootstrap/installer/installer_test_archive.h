// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_TEST_ARCHIVE_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_TEST_ARCHIVE_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"

namespace cef_installer {
namespace test {

struct TestDistribution {
  std::string version;          // e.g., "137.3.5"
  std::string abi_hash;         // e.g., "abc123def456"
  std::string platform;         // e.g., "windows64"
  base::FilePath archive_path;  // Path to created .tar.xz
  std::string archive_sha1;     // SHA1 hash of archive
};

// Builds a test CEF distribution archive.
//
// The archive contains:
//   - Release/libcef.dll (copy of a signed Windows system DLL)
//   - cef_version.json (version metadata)
//   - catalog.cat (signed catalog of all files)
//
// Parameters:
//   version: Version string (e.g., "137.3.5")
//   abi_hash: ABI hash for version matching
//   output_dir: Directory to create archive in
//   pfx_path: Path to PKCS#12 file containing signing certificate
//   pfx_password: Password for the PFX file
//
// Returns TestDistribution with archive path and SHA1, or nullopt on failure.
std::optional<TestDistribution> BuildTestDistribution(
    const std::string& version,
    const std::string& abi_hash,
    const base::FilePath& output_dir,
    const base::FilePath& pfx_path,
    const std::string& pfx_password);

// Builds a test bundled CEF directory (not archived).
//
// The directory contains the same files as BuildTestDistribution:
//   - Release/libcef.dll (copy of a signed Windows system DLL)
//   - cef_version.json (version metadata)
//   - catalog.cat (signed catalog of all files)
//
// Unlike BuildTestDistribution, the directory is left in place (not archived)
// for use as a bundled_cef_path in tests.
//
// Parameters:
//   version: Version string (e.g., "137.3.5")
//   abi_hash: ABI hash for version matching
//   output_dir: Parent directory; the bundled dir is created as a subdirectory
//   pfx_path: Path to PKCS#12 file containing signing certificate
//   pfx_password: Password for the PFX file
//
// Returns the path to the created bundled directory, or empty path on failure.
base::FilePath BuildTestBundledDirectory(const std::string& version,
                                         const std::string& abi_hash,
                                         const base::FilePath& output_dir,
                                         const base::FilePath& pfx_path,
                                         const std::string& pfx_password);

// Gets the path to a small signed Windows system DLL suitable for testing.
// Returns empty path if no suitable DLL is found.
base::FilePath GetSignedSystemDllForTesting();

// Creates a .tar.xz archive of a directory using Windows System32 tar.exe.
// Returns true on success.
bool CreateTarXzArchive(const base::FilePath& source_dir,
                        const base::FilePath& archive_path);

}  // namespace test
}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_TEST_ARCHIVE_H_
