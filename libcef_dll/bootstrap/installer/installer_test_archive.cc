// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_test_archive.h"

#include <windows.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_controller.h"
#include "cef/libcef_dll/bootstrap/installer/installer_download.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "cef/libcef_dll/bootstrap/installer/installer_signature.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_catalog.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version_metadata.h"

namespace cef_installer {
namespace test {

base::FilePath GetSignedSystemDllForTesting() {
  wchar_t system_dir[MAX_PATH];
  GetSystemDirectoryW(system_dir, MAX_PATH);
  base::FilePath system_path(system_dir);

  // Try small, commonly available signed DLLs.
  // version.dll is ~35KB and present on all Windows versions.
  const std::wstring candidates[] = {
      L"version.dll",
      L"secur32.dll",
      L"normaliz.dll",
  };

  for (const auto& name : candidates) {
    base::FilePath dll_path = system_path.Append(name);
    if (base::PathExists(dll_path)) {
      if (internal::VerifyFileSignature(dll_path, std::string()) ==
          SignatureError::kSuccess) {
        return dll_path;
      }
    }
  }

  return base::FilePath();
}

bool CreateTarXzArchive(const base::FilePath& source_dir,
                        const base::FilePath& archive_path) {
  // Windows System32 tar.exe (bsdtar) has liblzma built-in.
  base::FilePath tar_exe(L"C:\\Windows\\System32\\tar.exe");

  base::CommandLine cmd(tar_exe);
  cmd.AppendArg("-cJf");
  cmd.AppendArgPath(archive_path);
  cmd.AppendArg("-C");
  cmd.AppendArgPath(source_dir);
  cmd.AppendArg(".");

  std::string output;
  int exit_code;
  if (!base::GetAppOutputWithExitCode(cmd, &output, &exit_code)) {
    LOG(ERROR) << "Failed to run tar: " << output;
    return false;
  }

  if (exit_code != 0) {
    LOG(ERROR) << "tar exited with code " << exit_code << ": " << output;
    return false;
  }

  return true;
}

// Creates a signed CEF directory with libcef.dll, cef_version.json, and
// catalog.cat. Shared implementation for BuildTestDistribution and
// BuildTestBundledDirectory.
base::FilePath BuildSignedDirectory(const std::string& version,
                                    const std::string& abi_hash,
                                    const base::FilePath& dir_path,
                                    const base::FilePath& pfx_path,
                                    const std::string& pfx_password) {
  if (!base::CreateDirectory(dir_path)) {
    LOG(ERROR) << "Failed to create directory: " << dir_path;
    return base::FilePath();
  }

  // Copy signed system DLL as Release/libcef.dll (matching real distribution
  // layout where binaries live in a Release/ subdirectory).
  base::FilePath source_dll = GetSignedSystemDllForTesting();
  if (source_dll.empty()) {
    LOG(ERROR) << "No signed system DLL found for testing";
    return base::FilePath();
  }

  base::FilePath release_dir = dir_path.Append(kReleaseSubdirectory);
  if (!base::CreateDirectory(release_dir)) {
    LOG(ERROR) << "Failed to create Release directory: " << release_dir;
    return base::FilePath();
  }

  base::FilePath libcef_path = release_dir.Append(kLibcefFilename);
  if (!base::CopyFile(source_dll, libcef_path)) {
    LOG(ERROR) << "Failed to copy " << source_dll << " to " << libcef_path;
    return base::FilePath();
  }

  // Create cef_version.json.
  VersionMetadata metadata;
  metadata.version = Version::Parse(version);
  metadata.abi_hash = abi_hash;
  metadata.platform = GetCurrentPlatform();
  metadata.version_full = version + "+gtest+chromium-" + version;

  if (WriteVersionMetadata(dir_path, metadata) != MetadataError::kSuccess) {
    LOG(ERROR) << "Failed to write version metadata";
    return base::FilePath();
  }

  // Create and sign catalog.
  base::FilePath catalog_path = dir_path.Append(kCatalogFilename);
  std::vector<base::FilePath> files_to_catalog = {
      libcef_path,
      dir_path.Append(kVersionMetadataFilename),
  };

  if (CreateSignedCatalog(files_to_catalog, catalog_path, pfx_path,
                          pfx_password) != CatalogError::kSuccess) {
    LOG(ERROR) << "Failed to create signed catalog";
    return base::FilePath();
  }

  return dir_path;
}

std::optional<TestDistribution> BuildTestDistribution(
    const std::string& version,
    const std::string& abi_hash,
    const base::FilePath& output_dir,
    const base::FilePath& pfx_path,
    const std::string& pfx_password) {
  base::FilePath staging_dir = output_dir.Append(L"staging");
  if (BuildSignedDirectory(version, abi_hash, staging_dir, pfx_path,
                           pfx_password)
          .empty()) {
    return std::nullopt;
  }

  // Create tar.xz archive.
  std::string platform = GetCurrentPlatform();
  std::string archive_name = "cef_" + version + "_" + platform + ".tar.xz";
  base::FilePath archive_path =
      output_dir.Append(base::UTF8ToWide(archive_name));

  if (!CreateTarXzArchive(staging_dir, archive_path)) {
    LOG(ERROR) << "Failed to create tar.xz archive";
    return std::nullopt;
  }

  std::string sha1 = ComputeFileSha1(archive_path);
  if (sha1.empty()) {
    LOG(ERROR) << "Failed to compute SHA1 of archive";
    return std::nullopt;
  }

  // Clean up staging directory.
  base::DeletePathRecursively(staging_dir);

  TestDistribution result;
  result.version = version;
  result.abi_hash = abi_hash;
  result.platform = platform;
  result.archive_path = archive_path;
  result.archive_sha1 = sha1;

  return result;
}

base::FilePath BuildTestBundledDirectory(const std::string& version,
                                         const std::string& abi_hash,
                                         const base::FilePath& output_dir,
                                         const base::FilePath& pfx_path,
                                         const std::string& pfx_password) {
  return BuildSignedDirectory(version, abi_hash,
                              output_dir.Append(L"bundled_cef"), pfx_path,
                              pfx_password);
}

}  // namespace test
}  // namespace cef_installer
