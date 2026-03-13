// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper tool that wraps test::BuildTestDistribution() to create a local CDN
// directory for E2E tests. Produces the same signed .tar.xz archives and
// manifests that the integration tests use, but writes them to disk in the
// CDN directory layout that the installer expects with /cef-download-path.
//
// Usage:
//   cef_e2e_build_test_cdn --output-dir=C:\tmp\cdn --version=150.1
//       --abi-hash=a1b2c3d4e5f6

#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "cef/libcef_dll/bootstrap/installer/installer_download.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_archive.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_helpers.h"

namespace {

constexpr char kSwitchOutputDir[] = "output-dir";
constexpr char kSwitchVersion[] = "version";
constexpr char kSwitchAbiHash[] = "abi-hash";

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();

  std::string output_dir_str = cmd.GetSwitchValueASCII(kSwitchOutputDir);
  std::string version = cmd.GetSwitchValueASCII(kSwitchVersion);
  std::string abi_hash = cmd.GetSwitchValueASCII(kSwitchAbiHash);

  if (output_dir_str.empty() || version.empty()) {
    std::cerr << "Usage: cef_e2e_build_test_cdn --output-dir=<path> "
              << "--version=<ver> --abi-hash=<hash>" << std::endl;
    return 1;
  }

  if (abi_hash.empty()) {
    abi_hash = "a1b2c3d4e5f6";
  }

  base::FilePath output_dir = base::FilePath::FromUTF8Unsafe(output_dir_str);
  base::CreateDirectory(output_dir);

  base::FilePath pfx_path =
      cef_installer::test::GetTestDataPath().AppendASCII("test_signing.pfx");

  auto dist = cef_installer::test::BuildTestDistribution(
      version, abi_hash, output_dir, pfx_path, "test");
  if (!dist.has_value()) {
    std::cerr << "Failed to build test distribution for " << version
              << std::endl;
    return 2;
  }

  // Rename archive to match CDN naming convention.
  std::string cdn_filename =
      "cef_" + version + "_" + dist->platform + ".tar.xz";
  base::FilePath cdn_archive = output_dir.AppendASCII(cdn_filename);
  if (cdn_archive != dist->archive_path) {
    base::Move(dist->archive_path, cdn_archive);
  }

  // Write SHA256 sidecar.
  std::string sha256 = cef_installer::ComputeFileSha256(cdn_archive);
  base::FilePath sha256_path = output_dir.AppendASCII(cdn_filename + ".sha256");
  base::WriteFile(sha256_path, sha256);

  // Write manifest JSON.
  std::string manifest =
      R"([{"version": ")" + version + R"(", "file": ")" + cdn_filename +
      R"(", "sha1": ")" + dist->archive_sha1 +
      R"(", "last_modified": "2026-01-01T00:00:00Z", "abi_hash": ")" +
      abi_hash + R"("}])";

  // Write manifest under both naming conventions:
  // - milestone-based: 150_windows64.json (used when no abi_hash in config)
  // - abi_hash-based: a1b2c3d4e5f6_windows64.json (used when abi_hash set)
  std::string milestone = version.substr(0, version.find('.'));
  std::vector<std::string> manifest_names = {
      milestone + "_" + dist->platform + ".json",
      abi_hash + "_" + dist->platform + ".json",
  };

  for (const auto& name : manifest_names) {
    base::FilePath manifest_path = output_dir.AppendASCII(name);

    // If manifest already exists, merge (for multi-version).
    std::string existing;
    if (base::ReadFileToString(manifest_path, &existing) &&
        existing.size() > 2) {
      std::string merged = existing;
      merged.pop_back();                   // Remove trailing ]
      merged += "," + manifest.substr(1);  // Append new entry (skip leading [)
      base::WriteFile(manifest_path, merged);
    } else {
      base::WriteFile(manifest_path, manifest);
    }
  }

  // Write empty revoked.json if not present.
  base::FilePath revoked_path = output_dir.AppendASCII("revoked.json");
  if (!base::PathExists(revoked_path)) {
    base::WriteFile(revoked_path, R"({"revoked_versions": []})");
  }

  std::cout << "Built CDN distribution: " << cdn_filename << std::endl;
  return 0;
}
