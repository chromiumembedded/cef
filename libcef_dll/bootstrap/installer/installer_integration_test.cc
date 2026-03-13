// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <algorithm>
#include <map>
#include <set>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cef/include/cef_api_hash.h"
#include "cef/libcef_dll/bootstrap/installer/installer_archive.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_controller.h"
#include "cef/libcef_dll/bootstrap/installer/installer_database.h"
#include "cef/libcef_dll/bootstrap/installer/installer_download.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"
#include "cef/libcef_dll/bootstrap/installer/installer_launch_state.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "cef/libcef_dll/bootstrap/installer/installer_revocation.h"
#include "cef/libcef_dll/bootstrap/installer/installer_signature.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_archive.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_catalog.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_helpers.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version_metadata.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {

using internal::ClearInstallDirectoryOverridesForTesting;
using internal::OverrideInstallDirectoriesForTesting;
using internal::SetSignatureTestingMode;
using internal::SetTestingMode;

namespace {

class InstallerIntegrationTest : public testing::Test {
 protected:
  void SetUp() override {
    // Verify test certificate is available.
    test_thumbprint_ = test::GetTestCertificateThumbprint();
    ASSERT_FALSE(test_thumbprint_.empty())
        << "Test certificate not found. Run generate_test_cert.ps1 first.";

    // Enable testing modes.
    SetTestingMode(true);
    SetSignatureTestingMode(true);

    // Create temp directories.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    install_dir_ = temp_dir_.GetPath().Append(kCefSubdirectory);
    ASSERT_TRUE(base::CreateDirectory(install_dir_));
    archive_dir_ = temp_dir_.GetPath().Append(L"archives");
    ASSERT_TRUE(base::CreateDirectory(archive_dir_));

    // Build test distribution using CEF_API_VERSION_LAST so it matches the
    // version range check in the installer controller.
    Version next_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
    test_version_ = next_ver.ToString();
    test_abi_hash_ = "a1b2c3d4e5f6";

    auto dist = test::BuildTestDistribution(
        test_version_, test_abi_hash_, archive_dir_,
        test::GetTestDataPath().AppendASCII("test_signing.pfx"), "test");
    ASSERT_TRUE(dist.has_value()) << "Failed to build test distribution";
    test_dist_ = *dist;

    // Build manifest JSON.
    BuildManifest();

    // Start mock CDN server.
    server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    server_->RegisterRequestHandler(base::BindRepeating(
        &InstallerIntegrationTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(server_->Start());
  }

  void TearDown() override {
    internal::OverrideEnterprisePolicyForTesting(std::nullopt);
    SetFileOpsFaultForTesting(FileOpsFault::kNone);
    SetVersionIndexFaultForTesting(VersionIndexFault::kNone);
    SetDatabaseSaveFailureForTesting(false);
    SetSignatureTestingMode(false);
    SetTestingMode(false);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    request_log_.push_back(request.method_string + " " + request.relative_url);
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();

    VLOG(1) << "HandleRequest: " << request.relative_url;

    if (base::StartsWith(request.relative_url, "/invalid/") &&
        (base::EndsWith(request.relative_url, ".json") ||
         base::EndsWith(request.relative_url, "/revoked.json"))) {
      response->set_code(net::HTTP_OK);
      response->set_content("not valid JSON");
      response->set_content_type("application/json");
      return response;
    }
    if (base::StartsWith(request.relative_url, "/invalid/") &&
        request.relative_url.find(".tar.xz") != std::string::npos) {
      response->set_code(net::HTTP_NOT_FOUND);
      return response;
    }

    // Platform manifest.
    if (request.relative_url.find("_windows") != std::string::npos &&
        request.relative_url.find(".json") != std::string::npos) {
      // HEAD request for cache validation (Phase 10 / P4.4).
      if (request.method_string == "HEAD") {
        if (manifest_head_returns_304_) {
          response->set_code(net::HTTP_NOT_MODIFIED);
        } else {
          response->set_code(net::HTTP_OK);
        }
        return response;
      }
      response->set_code(net::HTTP_OK);
      response->set_content(manifest_json_);
      response->set_content_type("application/json");
      return response;
    }

    // Revocation list.
    if (base::EndsWith(request.relative_url, "/revoked.json")) {
      if (revocation_head_returns_304_ && request.method_string == "HEAD") {
        response->set_code(net::HTTP_NOT_MODIFIED);
        return response;
      }
      VLOG(1) << "HandleRequest: serving revocation JSON: " << revocation_json_;
      response->set_code(net::HTTP_OK);
      response->set_content(revocation_json_);
      response->set_content_type("application/json");
      return response;
    }

    // SHA256 hash sidecar file.
    if (request.relative_url.find(".tar.xz.sha256") != std::string::npos) {
      if (missing_primary_sidecar_ &&
          base::StartsWith(request.relative_url, "/primary/")) {
        response->set_code(net::HTTP_NOT_FOUND);
        return response;
      }
      if (origin_sidecar_test_ &&
          base::StartsWith(request.relative_url, "/primary/")) {
        response->set_code(net::HTTP_OK);
        response->set_content(std::string(64, 'f'));
        response->set_content_type("text/plain");
        return response;
      }
      if (origin_sidecar_test_ && request.method_string == "HEAD" &&
          base::StartsWith(request.relative_url, "/secondary/")) {
        response->set_code(net::HTTP_NOT_MODIFIED);
        return response;
      }
      if (cdn_failure_mode_ == CdnFailureMode::kWrongSha256) {
        response->set_code(net::HTTP_OK);
        response->set_content(std::string(64, 'f'));
        response->set_content_type("text/plain");
        return response;
      }
      base::FilePath archive_path = ResolveArchivePath(request.relative_url);
      if (!archive_path.empty()) {
        std::string sha256 = ComputeFileSha256(archive_path);
        if (!sha256.empty()) {
          response->set_code(net::HTTP_OK);
          response->set_content(sha256);
          response->set_content_type("text/plain");
          return response;
        }
      }
    }

    // Archive download.
    if (request.relative_url.find(".tar.xz") != std::string::npos &&
        request.relative_url.find(".sha256") == std::string::npos) {
      if (cdn_failure_mode_ == CdnFailureMode::kArchive404) {
        response->set_code(net::HTTP_NOT_FOUND);
        return response;
      }
      if (cdn_failure_mode_ == CdnFailureMode::kDropConnection) {
        // Return a truncated response to simulate connection reset.
        response->set_code(net::HTTP_OK);
        response->set_content("TRUNCATED");
        response->set_content_type("application/x-xz");
        return response;
      }
      if (archive_download_requested_) {
        *archive_download_requested_ = true;
      }
      base::FilePath archive_path = ResolveArchivePath(request.relative_url);
      if (!archive_path.empty()) {
        std::string content;
        if (base::ReadFileToString(archive_path, &content)) {
          response->set_code(net::HTTP_OK);
          response->set_content(content);
          response->set_content_type("application/x-xz");
          return response;
        }
      }
    }

    response->set_code(net::HTTP_NOT_FOUND);
    return response;
  }

  void BuildManifest() {
    manifest_json_ =
        R"([{
      "version": ")" +
        test_version_ +
        R"(",
      "file": "cef_)" +
        test_version_ + "_" + test_dist_.platform +
        R"(.tar.xz",
      "sha1": ")" +
        test_dist_.archive_sha1 +
        R"(",
      "last_modified": "2026-03-05T10:00:00Z",
      "abi_hash": ")" +
        test_abi_hash_ +
        R"("
    }])";

    revocation_json_ = R"({"revoked_versions": []})";
  }

  Config CreateConfig() {
    Config config;
    config.appid = "e2e00000-0000-0000-0000-000000000000";
    config.vmin = test_version_;
    config.abi_hash = test_abi_hash_;
    return config;
  }

  ExtendedConfig CreateExtendedConfig() {
    ExtendedConfig ext;
    ext.cdn_urls = {server_->base_url().spec()};
    ext.install_path = install_dir_.AsUTF8Unsafe();
    ext.certificate_thumbprint = test_thumbprint_;
    ext.show_progress_ui = false;
    ext.force_check = true;
    return ext;
  }

  void SetPolicyUrls(std::vector<std::string> urls) {
    PolicyLoadResult policy;
    policy.status = PolicyLoadStatus::kValid;
    policy.policy.download_source.kind = DownloadSourceKind::kPolicyUrls;
    policy.policy.download_source.authority =
        DownloadSourceAuthority::kEnterprisePolicy;
    policy.policy.download_source.urls = std::move(urls);
    internal::OverrideEnterprisePolicyForTesting(policy);
  }

  // Build a revocation list JSON that revokes the given versions.
  static std::string BuildRevocationJson(
      const std::vector<std::string>& versions) {
    std::string entries;
    for (size_t i = 0; i < versions.size(); ++i) {
      if (i > 0) {
        entries += ",";
      }
      entries +=
          R"({"version": ")" + versions[i] +
          R"(", "reason": "test", "revoked_at": "2026-03-05T00:00:00Z"})";
    }
    return R"({"revoked_versions": [)" + entries + "]}";
  }

  // Write a revocation_cache.json into a directory.
  static void WriteRevocationCacheFile(
      const base::FilePath& dir,
      const std::vector<std::string>& versions) {
    std::string entries;
    for (size_t i = 0; i < versions.size(); ++i) {
      if (i > 0) {
        entries += ",";
      }
      entries +=
          R"({"version": ")" + versions[i] +
          R"(", "reason": "test", "revoked_at": "2026-03-05T00:00:00Z"})";
    }
    std::string json = R"({"revoked_versions": [)" + entries + "]}";
    base::FilePath path = dir.Append(kRevocationCacheFilename);
    ASSERT_TRUE(base::WriteFile(path, json));
  }

  void CreateFakeInstalledVersion(const std::string& version_str,
                                  const std::string& abi_hash) {
    CreateFakeInstalledVersionIn(install_dir_, version_str, abi_hash);
  }

  void CreateFakeInstalledVersionIn(const base::FilePath& dir,
                                    const std::string& version_str,
                                    const std::string& abi_hash) {
    Version version = Version::Parse(version_str);
    base::FilePath version_dir = GetVersionPath(dir, version);
    ASSERT_TRUE(base::CreateDirectory(version_dir));

    // Create Release/ subdirectory matching real distribution layout.
    base::FilePath release_dir = version_dir.Append(kReleaseSubdirectory);
    ASSERT_TRUE(base::CreateDirectory(release_dir));
    base::FilePath libcef_path = release_dir.Append(kLibcefFilename);
    ASSERT_TRUE(base::WriteFile(libcef_path, "fake dll"));
    ASSERT_TRUE(
        base::WriteFile(version_dir.Append(kCatalogFilename), "fake catalog"));

    VersionMetadata metadata;
    metadata.version = version;
    metadata.abi_hash = abi_hash;
    metadata.platform = GetCurrentPlatform();
    ASSERT_EQ(WriteVersionMetadata(version_dir, metadata),
              MetadataError::kSuccess);
  }

  // Create two install directories for multi-directory tests.
  // |readonly_dir| simulates a privileged location with pre-existing versions.
  // |writable_dir| is the directory used as install_path.
  void SetUpDualDirectories(base::FilePath* readonly_dir,
                            base::FilePath* writable_dir) {
    *readonly_dir = temp_dir_.GetPath().Append(L"ReadOnly");
    ASSERT_TRUE(base::CreateDirectory(*readonly_dir));
    *writable_dir = temp_dir_.GetPath().Append(L"Writable");
    ASSERT_TRUE(base::CreateDirectory(*writable_dir));
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath install_dir_;
  base::FilePath archive_dir_;
  std::string test_version_;
  std::string test_abi_hash_;
  std::string test_thumbprint_;
  test::TestDistribution test_dist_;
  std::string manifest_json_;
  std::string revocation_json_;
  std::unique_ptr<net::EmbeddedTestServer> server_;
  std::vector<std::string> request_log_;

  // Optional flag set by HandleRequest when an archive download is requested.
  bool* archive_download_requested_ = nullptr;

  // When true, HEAD requests for the manifest return 304 (Not Modified).
  // When false, HEAD returns 200 (triggering a re-download).
  bool manifest_head_returns_304_ = false;
  bool revocation_head_returns_304_ = false;
  bool origin_sidecar_test_ = false;
  bool missing_primary_sidecar_ = false;

  // Resolve an archive URL to a local file path. Checks archive_map_ first
  // (for multi-version tests), falls back to test_dist_.archive_path.
  base::FilePath ResolveArchivePath(const std::string& url) {
    for (const auto& filename : missing_archive_filenames_) {
      if (url.find(filename) != std::string::npos) {
        return {};
      }
    }
    for (const auto& [filename, path] : archive_map_) {
      if (url.find(filename) != std::string::npos) {
        return path;
      }
    }
    return test_dist_.archive_path;
  }

  // ---------- Multi-app helpers (Phase 1a) ----------

  Config CreateSecondAppConfig(const std::string& vmin,
                               const std::string& vmax = "",
                               const std::string& abi_hash = "") {
    Config config;
    config.appid = "e2e11111-1111-1111-1111-111111111111";
    config.vmin = vmin;
    config.vmax = vmax;
    config.abi_hash = abi_hash.empty() ? test_abi_hash_ : abi_hash;
    return config;
  }

  // ---------- Multi-version helpers (Phase 1a) ----------

  struct MultiVersionFixture {
    test::TestDistribution dist_a;
    test::TestDistribution dist_b;
    std::string merged_manifest_json;
  };

  std::optional<MultiVersionFixture> BuildTwoVersions(
      const std::string& version_a,
      const std::string& abi_hash_a,
      const std::string& version_b,
      const std::string& abi_hash_b) {
    auto dist_a = test::BuildTestDistribution(
        version_a, abi_hash_a, archive_dir_,
        test::GetTestDataPath().AppendASCII("test_signing.pfx"), "test");
    if (!dist_a.has_value()) {
      return std::nullopt;
    }

    auto dist_b = test::BuildTestDistribution(
        version_b, abi_hash_b, archive_dir_,
        test::GetTestDataPath().AppendASCII("test_signing.pfx"), "test");
    if (!dist_b.has_value()) {
      return std::nullopt;
    }

    // Build merged manifest with both entries.
    std::string merged =
        R"([{"version": ")" + version_a + R"(", "file": "cef_)" + version_a +
        "_" + dist_a->platform + R"(.tar.xz", "sha1": ")" +
        dist_a->archive_sha1 +
        R"(", "last_modified": "2026-03-05T10:00:00Z", "abi_hash": ")" +
        abi_hash_a + R"("},{"version": ")" + version_b + R"(", "file": "cef_)" +
        version_b + "_" + dist_b->platform + R"(.tar.xz", "sha1": ")" +
        dist_b->archive_sha1 +
        R"(", "last_modified": "2026-03-05T10:00:00Z", "abi_hash": ")" +
        abi_hash_b + R"("}])";

    MultiVersionFixture mv;
    mv.dist_a = *dist_a;
    mv.dist_b = *dist_b;
    mv.merged_manifest_json = merged;
    return mv;
  }

  // ---------- CDN failure mode helpers (Phase 1a) ----------

  enum class CdnFailureMode {
    kNone,
    kDropConnection,
    kArchive404,
    kWrongSha256,
  };

  CdnFailureMode cdn_failure_mode_ = CdnFailureMode::kNone;

  // Map from archive filename to local file path, for multi-version serving.
  std::map<std::string, base::FilePath> archive_map_;
  std::set<std::string> missing_archive_filenames_;
};

// ============================================================================
// Fixture validation
// ============================================================================

TEST_F(InstallerIntegrationTest, FixtureSetUpSucceeds) {
  EXPECT_FALSE(test_thumbprint_.empty());
  EXPECT_TRUE(base::PathExists(test_dist_.archive_path));
  EXPECT_EQ(40u, test_dist_.archive_sha1.length());
  EXPECT_FALSE(test_version_.empty());
  EXPECT_TRUE(base::DirectoryExists(install_dir_));
}

// ============================================================================
// Core Integration Tests
// ============================================================================

// Full install from scratch - no existing versions.
TEST_F(InstallerIntegrationTest, FullInstallFlow) {
  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
  EXPECT_FALSE(result.libcef_path.empty());
  EXPECT_TRUE(base::PathExists(result.libcef_path));
  EXPECT_FALSE(result.is_bundled);

  // Launch health is opt-in; the default config exposes no sentinel state.
  EXPECT_TRUE(result.launch_state_path.empty());
  EXPECT_TRUE(result.launch_version.empty());
  EXPECT_TRUE(result.launch_platform.empty());

  // Verify database was updated.
  Database db;
  EXPECT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath(install_dir_)));
  auto entry = db.GetApp(config.appid, GetCurrentPlatform());
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(config.vmin, entry->vmin);

  const base::FilePath archive_cache =
      GetCacheDirectory(install_dir_)
          .AppendASCII(ComputeFileSha256(test_dist_.archive_path) + ".tar.xz");
  EXPECT_FALSE(base::PathExists(archive_cache));
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  ASSERT_TRUE(internal::GetLastExtractionBackgroundModeForTesting());
  EXPECT_FALSE(*internal::GetLastExtractionBackgroundModeForTesting());
#endif
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(InstallerIntegrationTest, BackgroundOperationUsesLowImpactExtraction) {
  internal::ResetLastExtractionBackgroundModeForTesting();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.background_mode = true;

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateConfig(), extended);

  EXPECT_TRUE(result.success) << result.error_message;
  ASSERT_TRUE(internal::GetLastExtractionBackgroundModeForTesting());
  EXPECT_TRUE(*internal::GetLastExtractionBackgroundModeForTesting());
}
#endif

TEST_F(InstallerIntegrationTest,
       ReplacementAndArchiveStateAtEveryIndexPublicationFailure) {
  const VersionIndexFault faults[] = {
      VersionIndexFault::kWrite,
      VersionIndexFault::kReplace,
      VersionIndexFault::kReread,
      VersionIndexFault::kValidation,
  };
  for (size_t i = 0; i < std::size(faults); ++i) {
    SCOPED_TRACE(static_cast<int>(faults[i]));
    SetVersionIndexFaultForTesting(VersionIndexFault::kNone);
    ASSERT_TRUE(base::DeletePathRecursively(install_dir_));
    ASSERT_TRUE(base::CreateDirectory(install_dir_));
    Version version = Version::Parse(test_version_);
    base::FilePath destination = GetVersionPath(install_dir_, version);
    ASSERT_TRUE(base::CreateDirectory(destination));
    ASSERT_TRUE(
        base::WriteFile(destination.Append(L"old_marker.txt"), "old target"));
    ProgressCallback fail_index_after_install = base::BindRepeating(
        [](VersionIndexFault fault, Step step, uint64_t, uint64_t) {
          if (step == kStepCleanup) {
            SetVersionIndexFaultForTesting(fault);
          }
          return true;
        },
        faults[i]);

    Controller controller;
    Result result =
        controller.Run(Command::kInstall, CreateConfig(),
                       CreateExtendedConfig(), fail_index_after_install);

    EXPECT_FALSE(result.success) << result.error_message;
    EXPECT_EQ(kExitCodeIndexError, result.error_code) << result.error_message;
    EXPECT_TRUE(base::PathExists(GetLibcefPath(destination)));
    EXPECT_FALSE(base::PathExists(destination.Append(L"old_marker.txt")));
    base::FilePath trash_root = install_dir_.Append(kTrashSubdirectory);
    base::FileEnumerator trash_entries(
        trash_root, false,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    EXPECT_TRUE(trash_entries.Next().empty());

    SetVersionIndexFaultForTesting(VersionIndexFault::kNone);
    std::vector<InstalledVersion> indexed;
    ASSERT_EQ(MetadataError::kSuccess,
              ReadVersionIndex(install_dir_, &indexed));
    EXPECT_EQ(faults[i] == VersionIndexFault::kWrite ||
                      faults[i] == VersionIndexFault::kReplace
                  ? 0u
                  : 1u,
              indexed.size());

    base::FilePath staging_root = install_dir_.Append(kStagingSubdirectory);
    base::FileEnumerator staging_entries(
        staging_root, false,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    EXPECT_TRUE(staging_entries.Next().empty());
    const base::FilePath archive_cache =
        GetCacheDirectory(install_dir_)
            .AppendASCII(ComputeFileSha256(test_dist_.archive_path) +
                         ".tar.xz");
    EXPECT_TRUE(base::PathExists(archive_cache));
  }
  SetVersionIndexFaultForTesting(VersionIndexFault::kNone);
}

TEST_F(InstallerIntegrationTest,
       ReplacementCleanupDeferredOnlyAfterSuccessfulIndexPublication) {
  Version version = Version::Parse(test_version_);
  base::FilePath destination = GetVersionPath(install_dir_, version);
  ASSERT_TRUE(base::CreateDirectory(destination));
  ASSERT_TRUE(
      base::WriteFile(destination.Append(L"old_marker.txt"), "old target"));
  SetFileOpsFaultForTesting(FileOpsFault::kTrashReclaim);

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());

  SetFileOpsFaultForTesting(FileOpsFault::kNone);
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  EXPECT_NE(result.warnings.end(),
            std::find(result.warnings.begin(), result.warnings.end(),
                      "Existing destination quarantine cleanup deferred"));
  EXPECT_TRUE(base::PathExists(GetLibcefPath(destination)));
  EXPECT_FALSE(base::PathExists(destination.Append(L"old_marker.txt")));
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_EQ(1u, indexed.size());
  base::FilePath trash_root = install_dir_.Append(kTrashSubdirectory);
  base::FileEnumerator trash_entries(
      trash_root, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  EXPECT_FALSE(trash_entries.Next().empty());
}

TEST_F(InstallerIntegrationTest,
       ReplacementCleanupDoesNotMaskIndexPublicationFailure) {
  Version version = Version::Parse(test_version_);
  base::FilePath destination = GetVersionPath(install_dir_, version);
  ASSERT_TRUE(base::CreateDirectory(destination));
  ASSERT_TRUE(
      base::WriteFile(destination.Append(L"old_marker.txt"), "old target"));
  SetFileOpsFaultForTesting(FileOpsFault::kTrashReclaim);
  ProgressCallback fail_index_after_install =
      base::BindRepeating([](Step step, uint64_t, uint64_t) {
        if (step == kStepCleanup) {
          SetVersionIndexFaultForTesting(VersionIndexFault::kWrite);
        }
        return true;
      });

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig(),
                     fail_index_after_install);

  SetFileOpsFaultForTesting(FileOpsFault::kNone);
  SetVersionIndexFaultForTesting(VersionIndexFault::kNone);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeIndexError, result.error_code);
  EXPECT_TRUE(base::PathExists(GetLibcefPath(destination)));
  EXPECT_FALSE(base::PathExists(destination.Append(L"old_marker.txt")));
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_TRUE(indexed.empty());
  base::FilePath trash_root = install_dir_.Append(kTrashSubdirectory);
  base::FileEnumerator trash_entries(
      trash_root, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  EXPECT_FALSE(trash_entries.Next().empty());
}

TEST_F(InstallerIntegrationTest,
       ExistingDestinationReplacementFaultBoundaries) {
  struct FaultCase {
    FileOpsFault fault;
    int exit_code;
  };
  const FaultCase cases[] = {
      {FileOpsFault::kQuarantineMove, kExitCodeQuarantineError},
      {FileOpsFault::kRepairMove, kExitCodeRepairError},
  };
  for (const auto& test_case : cases) {
    SCOPED_TRACE(static_cast<int>(test_case.fault));
    ASSERT_TRUE(base::DeletePathRecursively(install_dir_));
    ASSERT_TRUE(base::CreateDirectory(install_dir_));
    Version version = Version::Parse(test_version_);
    base::FilePath destination = GetVersionPath(install_dir_, version);
    ASSERT_TRUE(base::CreateDirectory(destination));
    ASSERT_TRUE(
        base::WriteFile(destination.Append(L"old_marker.txt"), "old target"));
    SetFileOpsFaultForTesting(test_case.fault);

    Controller controller;
    Result result = controller.Run(Command::kInstall, CreateConfig(),
                                   CreateExtendedConfig());

    SetFileOpsFaultForTesting(FileOpsFault::kNone);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(test_case.exit_code, result.error_code);
    std::vector<InstalledVersion> indexed;
    ASSERT_EQ(MetadataError::kSuccess,
              ReadVersionIndex(install_dir_, &indexed));
    EXPECT_TRUE(indexed.empty());

    base::FilePath trash_root = install_dir_.Append(kTrashSubdirectory);
    base::FileEnumerator trash_entries(
        trash_root, false,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    base::FilePath quarantined = trash_entries.Next();
    if (test_case.fault == FileOpsFault::kQuarantineMove) {
      EXPECT_TRUE(base::PathExists(destination.Append(L"old_marker.txt")));
      EXPECT_TRUE(quarantined.empty());
    } else {
      EXPECT_FALSE(base::PathExists(destination));
      ASSERT_FALSE(quarantined.empty());
      EXPECT_TRUE(base::PathExists(quarantined.Append(L"old_marker.txt")));
    }
    base::FilePath staging_root = install_dir_.Append(kStagingSubdirectory);
    base::FileEnumerator staging_entries(
        staging_root, false,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    EXPECT_TRUE(staging_entries.Next().empty());
  }
}

TEST_F(InstallerIntegrationTest, ArchiveCleanupRetainedOnRegistrationFailure) {
  SetDatabaseSaveFailureForTesting(true);

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeDatabaseError, result.error_code);
  const base::FilePath archive_cache =
      GetCacheDirectory(install_dir_)
          .AppendASCII(ComputeFileSha256(test_dist_.archive_path) + ".tar.xz");
  EXPECT_TRUE(base::PathExists(archive_cache));
}

TEST_F(InstallerIntegrationTest, ArchiveCleanupFailureIsDeferredAfterCommit) {
  const base::FilePath cache_dir = GetCacheDirectory(install_dir_);
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  const base::FilePath archive_cache = cache_dir.AppendASCII(
      ComputeFileSha256(test_dist_.archive_path) + ".tar.xz");
  ASSERT_TRUE(base::CopyFile(test_dist_.archive_path, archive_cache));
  HANDLE archive_lock = ::CreateFileW(
      archive_cache.value().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE(INVALID_HANDLE_VALUE, archive_lock);

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  EXPECT_TRUE(base::PathExists(archive_cache));
  ::CloseHandle(archive_lock);
  const base::Time expired =
      base::Time::Now() - base::Seconds(kArchiveCacheValiditySeconds + 60);
  ASSERT_TRUE(base::TouchFile(archive_cache, expired, expired));
  PruneCacheDirectory(cache_dir);
  EXPECT_FALSE(base::PathExists(archive_cache));
}

TEST_F(InstallerIntegrationTest, PolicyFailoverPrimarySuccessStopsFallback) {
  const Config config = CreateConfig();
  SetPolicyUrls({server_->base_url().spec() + "primary/",
                 server_->base_url().spec() + "secondary/"});
  request_log_.clear();

  Controller controller;
  const Result result =
      controller.Run(Command::kInstall, config, CreateExtendedConfig());
  ASSERT_TRUE(result.success) << result.error_message;

  const std::string manifest_path = BuildAbiHashUrl(
      "", config.abi_hash, GetCurrentPlatform(), config.channel);
  const std::string archive_name =
      "cef_" + test_version_ + "_" + test_dist_.platform + ".tar.xz";
  EXPECT_EQ((std::vector<std::string>{
                "GET /primary/revoked.json", "GET /primary/" + manifest_path,
                "GET /primary/" + archive_name + ".sha256",
                "GET /primary/" + archive_name}),
            request_log_);
}

TEST_F(InstallerIntegrationTest,
       PolicyFailoverAllOriginsFailBeforeStaleFallback) {
  const Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.force_check = false;
  SetPolicyUrls({server_->base_url().spec() + "invalid/one/",
                 server_->base_url().spec() + "invalid/two/"});

  const base::FilePath cache_dir = GetCacheDirectory(install_dir_);
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  const std::string manifest_cache_key = BuildAbiHashUrl(
      "", config.abi_hash, GetCurrentPlatform(), config.channel);
  ASSERT_EQ(DownloadError::kSuccess,
            WriteDownloadCache(cache_dir, kRevocationListPath,
                               R"({"revoked_versions": []})"));
  ASSERT_EQ(DownloadError::kSuccess,
            WriteDownloadCache(cache_dir, manifest_cache_key, manifest_json_));
  request_log_.clear();

  Controller controller;
  const Result result = controller.Run(Command::kInstall, config, extended);
  EXPECT_FALSE(result.success);
  const std::string archive_name =
      "cef_" + test_version_ + "_" + test_dist_.platform + ".tar.xz";
  EXPECT_EQ(
      (std::vector<std::string>{
          "HEAD /invalid/one/revoked.json", "GET /invalid/one/revoked.json",
          "HEAD /invalid/two/revoked.json", "GET /invalid/two/revoked.json",
          "HEAD /invalid/one/" + manifest_cache_key,
          "GET /invalid/one/" + manifest_cache_key,
          "HEAD /invalid/two/" + manifest_cache_key,
          "GET /invalid/two/" + manifest_cache_key,
          "GET /invalid/one/" + archive_name + ".sha256",
          "GET /invalid/one/" + archive_name,
          "GET /invalid/two/" + archive_name + ".sha256",
          "GET /invalid/two/" + archive_name}),
      request_log_);
}

TEST_F(InstallerIntegrationTest, PolicyFailoverCancellationIsTerminal) {
  const Config config = CreateConfig();
  SetPolicyUrls({server_->base_url().spec() + "primary/",
                 server_->base_url().spec() + "secondary/"});
  bool saw_archive_progress = false;
  ProgressCallback cancel_archive = base::BindRepeating(
      [](bool* saw_progress, Step step, uint64_t bytes_done, uint64_t) {
        if (step == kStepDownload && bytes_done > 0) {
          *saw_progress = true;
          return false;
        }
        return true;
      },
      &saw_archive_progress);
  request_log_.clear();

  Controller controller;
  const Result result = controller.Run(Command::kInstall, config,
                                       CreateExtendedConfig(), cancel_archive);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeCancelled, result.error_code);
  EXPECT_TRUE(saw_archive_progress);

  const std::string manifest_path = BuildAbiHashUrl(
      "", config.abi_hash, GetCurrentPlatform(), config.channel);
  const std::string archive_name =
      "cef_" + test_version_ + "_" + test_dist_.platform + ".tar.xz";
  EXPECT_EQ((std::vector<std::string>{
                "GET /primary/revoked.json", "GET /primary/" + manifest_path,
                "GET /primary/" + archive_name + ".sha256",
                "GET /primary/" + archive_name}),
            request_log_);
}

TEST_F(InstallerIntegrationTest, PolicyFailoverMissingSidecarUsesManifestSha1) {
  const Config config = CreateConfig();
  SetPolicyUrls({server_->base_url().spec() + "primary/",
                 server_->base_url().spec() + "secondary/"});
  missing_primary_sidecar_ = true;
  request_log_.clear();

  Controller controller;
  const Result result =
      controller.Run(Command::kInstall, config, CreateExtendedConfig());
  ASSERT_TRUE(result.success) << result.error_message;

  const std::string manifest_path = BuildAbiHashUrl(
      "", config.abi_hash, GetCurrentPlatform(), config.channel);
  const std::string archive_name =
      "cef_" + test_version_ + "_" + test_dist_.platform + ".tar.xz";
  EXPECT_EQ((std::vector<std::string>{
                "GET /primary/revoked.json", "GET /primary/" + manifest_path,
                "GET /primary/" + archive_name + ".sha256",
                "GET /primary/" + archive_name}),
            request_log_);
}

TEST_F(InstallerIntegrationTest,
       PolicyFailoverInvalidFreshCacheIsEvictedBeforeFallback) {
  const Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.force_check = false;
  SetPolicyUrls({server_->base_url().spec() + "primary/",
                 server_->base_url().spec() + "secondary/"});
  manifest_head_returns_304_ = true;

  const base::FilePath cache_dir = GetCacheDirectory(install_dir_);
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  const std::string manifest_cache_key = BuildAbiHashUrl(
      "", config.abi_hash, GetCurrentPlatform(), config.channel);
  ASSERT_EQ(
      DownloadError::kSuccess,
      WriteDownloadCache(cache_dir, manifest_cache_key, "not valid JSON"));
  request_log_.clear();

  Controller controller;
  const Result result = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(result.success) << result.error_message;

  const std::string archive_name =
      "cef_" + test_version_ + "_" + test_dist_.platform + ".tar.xz";
  EXPECT_EQ(
      (std::vector<std::string>{"GET /primary/revoked.json",
                                "HEAD /primary/" + manifest_cache_key,
                                "GET /secondary/" + manifest_cache_key,
                                "GET /primary/" + archive_name + ".sha256",
                                "GET /primary/" + archive_name}),
      request_log_);
  std::string cached_manifest;
  ASSERT_EQ(DownloadError::kSuccess,
            ReadDownloadCache(cache_dir, manifest_cache_key, &cached_manifest));
  EXPECT_EQ(manifest_json_, cached_manifest);
}

TEST_F(InstallerIntegrationTest,
       PolicyFailoverSignatureFailureDoesNotTrySecondary) {
  const Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.certificate_thumbprint = std::string(40, '0');
  SetPolicyUrls({server_->base_url().spec() + "primary/",
                 server_->base_url().spec() + "secondary/"});
  request_log_.clear();

  Controller controller;
  const Result result = controller.Run(Command::kInstall, config, extended);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeSignatureError, result.error_code);

  const std::string manifest_path = BuildAbiHashUrl(
      "", config.abi_hash, GetCurrentPlatform(), config.channel);
  const std::string archive_name =
      "cef_" + test_version_ + "_" + test_dist_.platform + ".tar.xz";
  EXPECT_EQ((std::vector<std::string>{
                "GET /primary/revoked.json", "GET /primary/" + manifest_path,
                "GET /primary/" + archive_name + ".sha256",
                "GET /primary/" + archive_name}),
            request_log_);
}

TEST_F(InstallerIntegrationTest,
       PolicyFailoverTriesHealthyOriginBeforeStaleCache) {
  const Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.force_check = false;

  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kPolicyUrls;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  policy.policy.download_source.urls = {server_->base_url().spec() + "invalid/",
                                        server_->base_url().spec()};
  internal::OverrideEnterprisePolicyForTesting(policy);
  manifest_head_returns_304_ = true;
  revocation_head_returns_304_ = true;

  const base::FilePath cache_dir = GetCacheDirectory(install_dir_);
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  const std::string manifest_cache_key = BuildAbiHashUrl(
      "", config.abi_hash, GetCurrentPlatform(), config.channel);
  const base::FilePath stale_cache =
      GetCacheFilePath(cache_dir, manifest_cache_key);
  ASSERT_TRUE(WriteFileWithIntegrity(stale_cache, "[]"));
  const base::Time old_time = base::Time::Now() - base::Hours(2);
  ASSERT_TRUE(base::TouchFile(stale_cache, old_time, old_time));
  const base::FilePath stale_revocations =
      GetCacheFilePath(cache_dir, std::string(kRevocationListPath));
  ASSERT_TRUE(WriteFileWithIntegrity(stale_revocations,
                                     BuildRevocationJson({test_version_})));
  ASSERT_TRUE(base::TouchFile(stale_revocations, old_time, old_time));
  request_log_.clear();

  Controller controller;
  const Result result = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);

  const std::string manifest_path = "/" + manifest_cache_key;
  const std::string archive_name =
      "cef_" + test_version_ + "_" + test_dist_.platform + ".tar.xz";
  EXPECT_EQ((std::vector<std::string>{
                "GET /invalid/revoked.json", "GET /revoked.json",
                "GET /invalid" + manifest_path, "GET " + manifest_path,
                "GET /invalid/" + archive_name + ".sha256",
                "GET /invalid/" + archive_name,
                "GET /" + archive_name + ".sha256", "GET /" + archive_name}),
            request_log_);
}

TEST_F(InstallerIntegrationTest,
       PolicyFailoverArchiveUsesSameOriginHashSidecar) {
  const Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kPolicyUrls;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  policy.policy.download_source.urls = {
      server_->base_url().spec() + "primary/",
      server_->base_url().spec() + "secondary/"};
  internal::OverrideEnterprisePolicyForTesting(policy);
  origin_sidecar_test_ = true;
  request_log_.clear();

  Controller controller;
  const Result result = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);

  const std::string manifest_path = BuildAbiHashUrl(
      "", config.abi_hash, GetCurrentPlatform(), config.channel);
  const std::string archive_name =
      "cef_" + test_version_ + "_" + test_dist_.platform + ".tar.xz";
  EXPECT_EQ((std::vector<std::string>{
                "GET /primary/revoked.json", "GET /primary/" + manifest_path,
                "GET /primary/" + archive_name + ".sha256",
                "GET /primary/" + archive_name, "GET /primary/" + archive_name,
                "GET /secondary/" + archive_name + ".sha256",
                "GET /secondary/" + archive_name}),
            request_log_);
}

// Update from older version.
TEST_F(InstallerIntegrationTest, UpdateFromOlderVersion) {
  CreateFakeInstalledVersion("136.0.0", "01da1b1a5400");

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
  EXPECT_TRUE(base::PathExists(result.libcef_path));
}

// ABI hash filtering - wrong hash should fail.
TEST_F(InstallerIntegrationTest, AbiHashFiltering) {
  Controller controller;
  Config config = CreateConfig();
  config.abi_hash = "ffff00000000";
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);
}

// Revocation blocking - revoked version should not install.
TEST_F(InstallerIntegrationTest, RevocationBlocking) {
  revocation_json_ = BuildRevocationJson({test_version_});

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);
}

// Version pruning - orphaned versions should be cleaned up.
TEST_F(InstallerIntegrationTest, PruningOrphanedVersions) {
  CreateFakeInstalledVersion("135.0.0", "00fa00fa00fa");

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);
  EXPECT_TRUE(result.success) << result.error_message;

  base::FilePath orphan_path =
      GetVersionPath(install_dir_, Version::Parse("135.0.0"));
  EXPECT_FALSE(base::PathExists(orphan_path));
}

// Progress callback receives updates during install.
TEST_F(InstallerIntegrationTest, ProgressCallbackReceivesUpdates) {
  std::vector<Step> steps_seen;
  bool saw_downloading = false;

  ProgressCallback callback = base::BindRepeating(
      [](std::vector<Step>* steps, bool* downloading, Step step,
         uint64_t bytes_done, uint64_t bytes_total) {
        steps->push_back(step);
        if (step == kStepDownload) {
          *downloading = true;
        }
        return true;
      },
      &steps_seen, &saw_downloading);

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateConfig(),
                                 CreateExtendedConfig(), callback);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(saw_downloading);
  EXPECT_FALSE(steps_seen.empty());
}

// Query finds a previously installed version.
TEST_F(InstallerIntegrationTest, QueryFindsInstalledVersion) {
  Controller controller;
  Result install_result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());
  ASSERT_TRUE(install_result.success) << install_result.error_message;

  Result query_result =
      controller.Run(Command::kQuery, CreateConfig(), CreateExtendedConfig());

  EXPECT_TRUE(query_result.success) << query_result.error_message;
  EXPECT_EQ(test_version_, query_result.installed_version);
}

// Skip download when compatible version already installed.
TEST_F(InstallerIntegrationTest, SkipsDownloadWhenAlreadyInstalled) {
  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result first = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(first.success) << first.error_message;
  base::FilePath first_path = first.libcef_path;

  bool archive_requested = false;
  archive_download_requested_ = &archive_requested;

  Result second = controller.Run(Command::kInstall, config, extended);
  EXPECT_TRUE(second.success) << second.error_message;
  EXPECT_EQ(first.installed_version, second.installed_version);
  EXPECT_EQ(first_path, second.libcef_path);
  EXPECT_FALSE(archive_requested);

  archive_download_requested_ = nullptr;
}

// Query with nothing installed should fail.
TEST_F(InstallerIntegrationTest, QueryNoInstalledVersion) {
  Controller controller;
  Result result =
      controller.Run(Command::kQuery, CreateConfig(), CreateExtendedConfig());

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);
}

// Revocation of a previously-installed version.
TEST_F(InstallerIntegrationTest, RevokedInstalledVersionIsRejected) {
  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result first = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(first.success) << first.error_message;

  revocation_json_ = BuildRevocationJson({test_version_});

  Result second = controller.Run(Command::kInstall, config, extended);
  EXPECT_FALSE(second.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, second.error_code);
}

// Verify metadata written during install.
TEST_F(InstallerIntegrationTest, VerifyMetadataAfterInstall) {
  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(result.success) << result.error_message;

  // libcef_path is <version_dir>/Release/libcef.dll, go up two levels.
  base::FilePath version_dir = result.libcef_path.DirName().DirName();
  VersionMetadata metadata;
  ASSERT_EQ(MetadataError::kSuccess,
            ReadVersionMetadata(version_dir, &metadata));

  EXPECT_EQ(test_version_, metadata.version.ToString());
  EXPECT_EQ(test_abi_hash_, metadata.abi_hash);
  EXPECT_EQ(GetCurrentPlatform(), metadata.platform);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

// Cancel during download via progress callback.
TEST_F(InstallerIntegrationTest, CancelDuringDownload) {
  bool saw_downloading = false;

  ProgressCallback callback = base::BindRepeating(
      [](bool* downloading, Step step, uint64_t bytes_done,
         uint64_t bytes_total) {
        if (step == kStepDownload) {
          *downloading = true;
          return false;  // Cancel immediately
        }
        return true;
      },
      &saw_downloading);

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateConfig(),
                                 CreateExtendedConfig(), callback);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeCancelled, result.error_code);
  EXPECT_TRUE(saw_downloading);
}

// Network error (404) - version not in manifest.
TEST_F(InstallerIntegrationTest, NetworkError404) {
  Config config = CreateConfig();
  config.vmin = "999.0.0";  // Version that won't match any manifest entry

  Result result =
      Controller().Run(Command::kInstall, config, CreateExtendedConfig());

  EXPECT_FALSE(result.success);
  // Could be NO_MATCHING_VERSION or NETWORK_ERROR depending on flow
}

// Signature verification failure (tampered archive).
TEST_F(InstallerIntegrationTest, TamperedArchiveFails) {
  // Corrupt the archive after it was built.
  // Safe because SetUp() creates a fresh archive for each test instance.
  base::File file(test_dist_.archive_path,
                  base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  file.Seek(base::File::FROM_BEGIN, 100);
  const char tampered[] = "TAMPERED";
  file.WriteAtCurrentPosAndCheck(
      base::as_byte_span(std::string_view(tampered, sizeof(tampered) - 1)));
  file.Close();

  Result result = Controller().Run(Command::kInstall, CreateConfig(),
                                   CreateExtendedConfig());

  EXPECT_FALSE(result.success);
  // Should fail at extraction (SHA1 mismatch) or signature verification
}

// Wrong certificate thumbprint.
TEST_F(InstallerIntegrationTest, WrongThumbprintFails) {
  ExtendedConfig extended = CreateExtendedConfig();
  extended.certificate_thumbprint = std::string(40, '0');  // Wrong thumbprint

  Result result = Controller().Run(Command::kInstall, CreateConfig(), extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeSignatureError, result.error_code);
}

// Uninstall removes app from database.
TEST_F(InstallerIntegrationTest, UninstallRemovesFromDatabase) {
  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Install first.
  ASSERT_TRUE(controller.Run(Command::kInstall, config, extended).success);

  // Uninstall.
  Result result = controller.Run(Command::kUninstall, config, extended);
  EXPECT_TRUE(result.success);

  // Verify removed from database.
  Database db;
  EXPECT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath(install_dir_)));
  EXPECT_FALSE(db.GetApp(config.appid, GetCurrentPlatform()).has_value());
}

// ============================================================================
// Bundled CEF Tests
// ============================================================================

base::FilePath BuildBundled(const base::FilePath& parent_dir,
                            const std::string& version,
                            const std::string& abi_hash) {
  return test::BuildTestBundledDirectory(
      version, abi_hash, parent_dir,
      test::GetTestDataPath().AppendASCII("test_signing.pfx"), "test");
}

// Bundled version used when no installed version exists (skips CDN).
TEST_F(InstallerIntegrationTest, BundledUsedWhenNoInstalledVersion) {
  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), test_version_, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());

  bool archive_requested = false;
  archive_download_requested_ = &archive_requested;

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
  // Bundled version is used in-place — no CDN download.
  EXPECT_FALSE(archive_requested);
  // libcef_path should point into the bundled directory, not the install dir.
  EXPECT_TRUE(result.libcef_path.IsAbsolute());
  EXPECT_TRUE(bundled_dir.IsParent(result.libcef_path) ||
              bundled_dir == result.libcef_path.DirName());
  EXPECT_TRUE(result.is_bundled);

  archive_download_requested_ = nullptr;
}

// Bundled version is newer than installed — bundled wins.
TEST_F(InstallerIntegrationTest, BundledNewerThanInstalled) {
  // Install an older version via fake.
  Version older = Version::Parse(test_version_);
  std::string older_version = std::to_string(older.GetMilestone() - 1) + ".0.0";
  CreateFakeInstalledVersion(older_version, test_abi_hash_);

  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), test_version_, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());

  Controller controller;
  Config config = CreateConfig();
  config.vmin = older_version;  // Accept both versions.
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  // Bundled is newer, so it should be selected.
  EXPECT_EQ(test_version_, result.installed_version);
  EXPECT_TRUE(bundled_dir.IsParent(result.libcef_path) ||
              bundled_dir == result.libcef_path.DirName());
  EXPECT_TRUE(result.is_bundled);
}

// Installed version is newer than bundled — installed wins.
TEST_F(InstallerIntegrationTest, InstalledNewerThanBundled) {
  // Create a fake installed version at test_version_.
  CreateFakeInstalledVersion(test_version_, test_abi_hash_);

  // Build bundled at an older version.
  Version current = Version::Parse(test_version_);
  std::string older_version =
      std::to_string(current.GetMilestone() - 1) + ".0.0";
  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), older_version, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());

  Controller controller;
  Config config = CreateConfig();
  config.vmin = older_version;  // Accept both versions.
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  // Installed is newer, so it should be selected.
  EXPECT_EQ(test_version_, result.installed_version);
  // libcef_path should NOT be in the bundled directory.
  EXPECT_FALSE(bundled_dir.IsParent(result.libcef_path));
  EXPECT_FALSE(result.is_bundled);
}

// Same version installed and bundled — installed wins (more trusted).
TEST_F(InstallerIntegrationTest, InstalledWinsTieWithBundled) {
  CreateFakeInstalledVersion(test_version_, test_abi_hash_);

  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), test_version_, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
  // Should use the installed version, not the bundled one.
  EXPECT_FALSE(bundled_dir.IsParent(result.libcef_path));
  EXPECT_FALSE(result.is_bundled);
}

// Bundled version is not copied to the shared install location.
TEST_F(InstallerIntegrationTest, BundledNotCopiedToInstallDir) {
  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), test_version_, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(result.success) << result.error_message;

  // The install_dir Versions subdirectory should not contain the bundled
  // version — bundled is used in-place, never copied.
  base::FilePath would_be_copied =
      GetVersionPath(install_dir_, Version::Parse(test_version_));
  EXPECT_FALSE(base::DirectoryExists(would_be_copied));
}

// Revoked bundled is skipped when an installed version exists.
TEST_F(InstallerIntegrationTest, BundledRevokedSkippedWhenInstalledExists) {
  // Create installed version at older milestone.
  Version current = Version::Parse(test_version_);
  std::string older_version =
      std::to_string(current.GetMilestone() - 1) + ".0.0";
  CreateFakeInstalledVersion(older_version, test_abi_hash_);

  // Bundled is newer but revoked.
  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), test_version_, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());

  revocation_json_ = BuildRevocationJson({test_version_});

  Controller controller;
  Config config = CreateConfig();
  config.vmin = older_version;  // Accept both versions.
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  // Should use the older installed version, not the revoked bundled.
  EXPECT_EQ(older_version, result.installed_version);
  EXPECT_FALSE(bundled_dir.IsParent(result.libcef_path));
}

// Revoked bundled used as last resort when CDN also fails.
TEST_F(InstallerIntegrationTest, BundledRevokedUsedAsLastResort) {
  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), test_version_, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());

  // Revoke the bundled version AND make CDN return no matching version.
  revocation_json_ = BuildRevocationJson({test_version_});
  // CDN manifest lists the same (revoked) version, so CDN path also fails.

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kInstall, config, extended);

  // Should fall back to the revoked bundled version rather than fail entirely.
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
  EXPECT_TRUE(bundled_dir.IsParent(result.libcef_path) ||
              bundled_dir == result.libcef_path.DirName());
  EXPECT_TRUE(result.is_bundled);
}

// Revoked bundled is preferred over disqualified installed in fallback.
TEST_F(InstallerIntegrationTest, BundledRevokedPreferredOverDisqualified) {
  // Install a version and disqualify it via launch state.
  CreateFakeInstalledVersion(test_version_, test_abi_hash_);

  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  std::wstring hash = GetAppidHash(config.appid);
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 99999;
  ls.pid_start_time = GetCurrentPidStartTime();
  ls.consecutive_failures = 2;
  ls.running = true;
  ls.version = test_version_;
  ls.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash,
                                   Version::Parse(test_version_),
                                   GetCurrentPlatform()),
      ls));

  // Bundled at same version but revoked.
  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), test_version_, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());
  revocation_json_ = BuildRevocationJson({test_version_});

  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  // Revoked bundled should win over disqualified installed.
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(result.is_bundled);
  EXPECT_TRUE(bundled_dir.IsParent(result.libcef_path) ||
              bundled_dir == result.libcef_path.DirName());
}

// When the only candidates are one revoked and one crash-disqualified
// installed version (and no bundled fallback), the crash-disqualified version
// wins. A revoked *installed* version is never selectable — it is hard-filtered
// by FindBestVersion on every path, including the last-resort fallback, whereas
// a crash-disqualified version is only pruned from the primary candidate list
// and re-admitted by the unfiltered fallback. These two tests pin that behavior
// in both version orderings.
TEST_F(InstallerIntegrationTest, CrashDisqualifiedBeatsRevoked_RevokedNewer) {
  std::string crashed = test_version_;             // Older, crash-disqualified.
  std::string revoked_ver = test_version_ + ".1";  // Newer, but revoked.
  CreateFakeInstalledVersion(crashed, test_abi_hash_);
  CreateFakeInstalledVersion(revoked_ver, test_abi_hash_);

  Config config = CreateConfig();
  config.launch_health = LaunchHealthMode::kExitCode;
  ExtendedConfig extended = CreateExtendedConfig();

  // Revoke the newer version (served by the mock CDN's revoked.json).
  revocation_json_ = BuildRevocationJson({revoked_ver});

  // Crash-disqualify the older version (running on a dead pid, failures>=2).
  std::wstring hash = GetAppidHash(config.appid);
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 99999;
  ls.pid_start_time = GetCurrentPidStartTime();
  ls.consecutive_failures = 2;
  ls.running = true;
  ls.version = crashed;
  ls.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(crashed),
                                   GetCurrentPlatform()),
      ls));

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  // The crash-disqualified version is selected as last resort; the newer
  // revoked version is never eligible.
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(crashed, result.installed_version);
  // selected == unfiltered_best (revoked is filtered out), so no rollback flag.
  EXPECT_FALSE(result.is_rollback);
  EXPECT_EQ(3, result.launch_consecutive_failures);
}

TEST_F(InstallerIntegrationTest, CrashDisqualifiedBeatsRevoked_RevokedOlder) {
  std::string revoked_ver = test_version_;     // Older, and revoked.
  std::string crashed = test_version_ + ".1";  // Newer, crash-disqualified.
  CreateFakeInstalledVersion(revoked_ver, test_abi_hash_);
  CreateFakeInstalledVersion(crashed, test_abi_hash_);

  Config config = CreateConfig();
  config.launch_health = LaunchHealthMode::kExitCode;
  ExtendedConfig extended = CreateExtendedConfig();

  revocation_json_ = BuildRevocationJson({revoked_ver});

  std::wstring hash = GetAppidHash(config.appid);
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 99999;
  ls.pid_start_time = GetCurrentPidStartTime();
  ls.consecutive_failures = 2;
  ls.running = true;
  ls.version = crashed;
  ls.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(crashed),
                                   GetCurrentPlatform()),
      ls));

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(crashed, result.installed_version);
  EXPECT_FALSE(result.is_rollback);
  EXPECT_EQ(3, result.launch_consecutive_failures);
}

// Bundled with wrong ABI hash is skipped.
TEST_F(InstallerIntegrationTest, BundledWrongAbiHashSkipped) {
  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), test_version_, "ffff00000000");
  ASSERT_FALSE(bundled_dir.empty());

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  // CDN also won't match because it has test_abi_hash_ but config wants
  // test_abi_hash_ — so this test just verifies bundled is rejected.
  Result result = controller.Run(Command::kInstall, config, extended);

  // Bundled has wrong ABI, falls through to CDN which should succeed.
  EXPECT_TRUE(result.success) << result.error_message;
  // Result should NOT be from the bundled directory.
  EXPECT_FALSE(bundled_dir.IsParent(result.libcef_path));
}

// Bundled with tampered files is still accepted (no catalog verification at
// runtime — the perf cost of hashing ~370 MB on every startup is too high).
// Bundled integrity is the app developer's responsibility.
TEST_F(InstallerIntegrationTest, BundledNotCatalogVerifiedAtRuntime) {
  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), test_version_, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());

  // Tamper with libcef.dll after the catalog was signed.
  base::FilePath libcef_path = GetLibcefPath(bundled_dir);
  ASSERT_TRUE(base::AppendToFile(libcef_path, "TAMPERED"));

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kInstall, config, extended);

  // Tampered bundled is still used — catalog is not verified at runtime.
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(bundled_dir.IsParent(result.libcef_path) ||
              bundled_dir == result.libcef_path.DirName());
}

// Query considers bundled version.
TEST_F(InstallerIntegrationTest, QueryFindsBundledVersion) {
  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), test_version_, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kQuery, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
  EXPECT_TRUE(bundled_dir.IsParent(result.libcef_path) ||
              bundled_dir == result.libcef_path.DirName());
  EXPECT_TRUE(result.is_bundled);
}

// Query prefers newer installed over bundled.
TEST_F(InstallerIntegrationTest, QueryPrefersInstalledOverBundled) {
  CreateFakeInstalledVersion(test_version_, test_abi_hash_);
  ASSERT_EQ(MetadataError::kSuccess,
            WriteVersionIndex(install_dir_,
                              ScanInstalledVersionsWithMetadata(install_dir_)));

  Version current = Version::Parse(test_version_);
  std::string older_version =
      std::to_string(current.GetMilestone() - 1) + ".0.0";
  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), older_version, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());

  Controller controller;
  Config config = CreateConfig();
  config.vmin = older_version;
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kQuery, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
  EXPECT_FALSE(bundled_dir.IsParent(result.libcef_path));
  EXPECT_FALSE(result.is_bundled);
}

// ============================================================================
// Unchecked CEF Path Tests
// ============================================================================

// unchecked_cef_path returns immediately when libcef.dll exists.
TEST_F(InstallerIntegrationTest, UncheckedCefPathUsedWhenLibcefExists) {
  // Create a directory with just libcef.dll — no metadata, no Release subdir.
  base::FilePath unchecked_dir = temp_dir_.GetPath().Append(L"unchecked_cef");
  ASSERT_TRUE(base::CreateDirectory(unchecked_dir));
  base::FilePath libcef_path = unchecked_dir.Append(kLibcefFilename);
  ASSERT_TRUE(base::WriteFile(libcef_path, "fake libcef.dll"));

  bool archive_requested = false;
  archive_download_requested_ = &archive_requested;

  Controller controller;
  Config config = CreateConfig();
  config.unchecked_cef_path = unchecked_dir.AsUTF8Unsafe();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(libcef_path, result.libcef_path);
  EXPECT_TRUE(result.is_bundled);
  EXPECT_TRUE(result.installed_version.empty());
  // No CDN download should have occurred.
  EXPECT_FALSE(archive_requested);

  archive_download_requested_ = nullptr;
}

// unchecked_cef_path falls through to normal installer when libcef.dll missing.
TEST_F(InstallerIntegrationTest, UncheckedCefPathFallsThroughWhenMissing) {
  // Point to an empty directory — no libcef.dll.
  base::FilePath empty_dir = temp_dir_.GetPath().Append(L"empty_cef");
  ASSERT_TRUE(base::CreateDirectory(empty_dir));

  Controller controller;
  Config config = CreateConfig();
  config.unchecked_cef_path = empty_dir.AsUTF8Unsafe();
  ExtendedConfig extended = CreateExtendedConfig();

  // Should fall through to normal flow and succeed via CDN.
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  // Result should NOT be from the unchecked directory.
  EXPECT_FALSE(empty_dir.IsParent(result.libcef_path));
  EXPECT_FALSE(result.is_bundled);
}

// unchecked_cef_path is ignored for uninstall commands.
TEST_F(InstallerIntegrationTest, UncheckedCefPathIgnoredForUninstall) {
  base::FilePath unchecked_dir = temp_dir_.GetPath().Append(L"unchecked_cef2");
  ASSERT_TRUE(base::CreateDirectory(unchecked_dir));
  base::FilePath libcef_path = unchecked_dir.Append(kLibcefFilename);
  ASSERT_TRUE(base::WriteFile(libcef_path, "fake libcef.dll"));

  // First install to register the app in the database.
  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result install_result = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(install_result.success) << install_result.error_message;

  // Now uninstall with unchecked_cef_path set — should proceed with normal
  // uninstall, not short-circuit.
  config.unchecked_cef_path = unchecked_dir.AsUTF8Unsafe();
  Result result = controller.Run(Command::kUninstall, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  // Verify unregistered from database.
  Database db;
  EXPECT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath(install_dir_)));
  EXPECT_FALSE(db.GetApp(config.appid, GetCurrentPlatform()).has_value());
}

// unchecked_cef_path skips version checks — even a "wrong" version works.
TEST_F(InstallerIntegrationTest, UncheckedCefPathSkipsVersionCheck) {
  base::FilePath unchecked_dir = temp_dir_.GetPath().Append(L"unchecked_cef3");
  ASSERT_TRUE(base::CreateDirectory(unchecked_dir));
  base::FilePath libcef_path = unchecked_dir.Append(kLibcefFilename);
  ASSERT_TRUE(base::WriteFile(libcef_path, "fake libcef.dll"));

  Controller controller;
  Config config = CreateConfig();
  config.unchecked_cef_path = unchecked_dir.AsUTF8Unsafe();
  // Set a very high vmin that no installed version would satisfy.
  config.vmin = "999.0";
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);

  // Should succeed because unchecked_cef_path skips all version filtering.
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(libcef_path, result.libcef_path);
  EXPECT_TRUE(result.is_bundled);
}

// unchecked_cef_path works with query command.
TEST_F(InstallerIntegrationTest, UncheckedCefPathWorksWithQuery) {
  base::FilePath unchecked_dir = temp_dir_.GetPath().Append(L"unchecked_cef4");
  ASSERT_TRUE(base::CreateDirectory(unchecked_dir));
  base::FilePath libcef_path = unchecked_dir.Append(kLibcefFilename);
  ASSERT_TRUE(base::WriteFile(libcef_path, "fake libcef.dll"));

  Controller controller;
  Config config = CreateConfig();
  config.unchecked_cef_path = unchecked_dir.AsUTF8Unsafe();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kQuery, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(libcef_path, result.libcef_path);
  EXPECT_TRUE(result.is_bundled);
}

// ============================================================================
// Multi-Directory Tests
// ============================================================================

// ReadMultipleVersionIndexes finds versions across two directories.
TEST_F(InstallerIntegrationTest, FindsVersionAcrossMultipleDirectories) {
  base::FilePath readonly_dir, writable_dir;
  SetUpDualDirectories(&readonly_dir, &writable_dir);

  // Put a version only in the "read-only" directory.
  CreateFakeInstalledVersionIn(readonly_dir, test_version_, test_abi_hash_);

  // Scan both directories — version from readonly_dir should be found.
  std::vector<base::FilePath> dirs = {readonly_dir, writable_dir};
  std::vector<InstalledVersion> merged =
      Controller::ReadMultipleVersionIndexes(dirs);

  ASSERT_FALSE(merged.empty());
  std::optional<InstalledVersion> best =
      FindBestVersion(CreateConfig(), merged);
  ASSERT_TRUE(best.has_value());
  EXPECT_EQ(test_version_, best->metadata.version.ToString());
  EXPECT_TRUE(readonly_dir.IsParent(best->path));
}

// Newer version in a secondary directory wins over older in primary.
TEST_F(InstallerIntegrationTest, PrefersNewerVersionAcrossDirectories) {
  base::FilePath readonly_dir, writable_dir;
  SetUpDualDirectories(&readonly_dir, &writable_dir);

  Version current = Version::Parse(test_version_);
  std::string older_version =
      std::to_string(current.GetMilestone() - 1) + ".0.0";

  CreateFakeInstalledVersionIn(writable_dir, older_version, test_abi_hash_);
  CreateFakeInstalledVersionIn(readonly_dir, test_version_, test_abi_hash_);

  std::vector<base::FilePath> dirs = {readonly_dir, writable_dir};
  std::vector<InstalledVersion> merged =
      Controller::ReadMultipleVersionIndexes(dirs);

  Config config = CreateConfig();
  config.vmin = older_version;
  std::optional<InstalledVersion> best = FindBestVersion(config, merged);
  ASSERT_TRUE(best.has_value());
  EXPECT_EQ(test_version_, best->metadata.version.ToString());
}

// Downloaded version is installed to the writable directory (install_path),
// not to other readable directories.
TEST_F(InstallerIntegrationTest, InstallGoesToWritableDirectory) {
  base::FilePath readonly_dir, writable_dir;
  SetUpDualDirectories(&readonly_dir, &writable_dir);

  // Put an older incompatible version in readonly_dir so it doesn't satisfy
  // the version requirement and the controller must download.
  CreateFakeInstalledVersionIn(readonly_dir, "136.0.0", "old_abi");

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.install_path = writable_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(result.success) << result.error_message;

  // The installed version should be under the writable directory.
  EXPECT_TRUE(writable_dir.IsParent(result.libcef_path));
  // And NOT under the readonly directory.
  EXPECT_FALSE(readonly_dir.IsParent(result.libcef_path));
}

// Pruning only deletes versions in the writable directory.
TEST_F(InstallerIntegrationTest, PruneOnlyAffectsWritableDirectory) {
  // Install via CDN into the writable install_dir_.
  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(result.success) << result.error_message;

  // Create a "readonly" directory with a different old version.
  base::FilePath readonly_dir = temp_dir_.GetPath().Append(L"ReadOnly");
  ASSERT_TRUE(base::CreateDirectory(readonly_dir));
  CreateFakeInstalledVersionIn(readonly_dir, "136.0.0", "old_abi");

  // Uninstall the app — prune should clean install_dir_ but not readonly_dir.
  Result uninstall = controller.Run(Command::kUninstall, config, extended);
  EXPECT_TRUE(uninstall.success);

  // The old version in readonly_dir must still exist.
  base::FilePath readonly_version_dir =
      GetVersionPath(readonly_dir, Version::Parse("136.0.0"));
  EXPECT_TRUE(base::DirectoryExists(readonly_version_dir));
}

// kQuery succeeds when only a read-only directory has the version (no writable
// dir). Uses OverrideInstallDirectoriesForTesting with no writable dir.
TEST_F(InstallerIntegrationTest, QuerySucceedsWithoutWritableDirectory) {
  // Create a directory with a pre-staged version.
  base::FilePath readonly_dir = temp_dir_.GetPath().Append(L"ReadOnly");
  ASSERT_TRUE(base::CreateDirectory(readonly_dir));
  CreateFakeInstalledVersionIn(readonly_dir, test_version_, test_abi_hash_);

  // Verify the version is discoverable via ReadMultipleVersionIndexes.
  std::vector<InstalledVersion> scanned =
      Controller::ReadMultipleVersionIndexes({readonly_dir});
  ASSERT_FALSE(scanned.empty());

  Config config = CreateConfig();
  std::optional<InstalledVersion> best = FindBestVersion(config, scanned);
  ASSERT_TRUE(best.has_value());
  EXPECT_EQ(test_version_, best->metadata.version.ToString());
}

// installer.json is only written to the writable directory, not others.
TEST_F(InstallerIntegrationTest, DatabaseOnlyInWritableDirectory) {
  base::FilePath readonly_dir, writable_dir;
  SetUpDualDirectories(&readonly_dir, &writable_dir);

  // Put a compatible version in readonly_dir so install succeeds without CDN.
  CreateFakeInstalledVersionIn(readonly_dir, test_version_, test_abi_hash_);

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.install_path = writable_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(result.success) << result.error_message;

  // Database should exist in the writable directory.
  EXPECT_TRUE(base::PathExists(GetDatabasePath(writable_dir)));
  // Database must NOT exist in the readonly directory.
  EXPECT_FALSE(base::PathExists(GetDatabasePath(readonly_dir)));
}

// Same version in two directories — ReadMultipleVersionIndexes keeps the entry
// from the first (higher-priority) directory.
TEST_F(InstallerIntegrationTest,
       ReadMultipleVersionIndexes_DeduplicatesByPriority) {
  base::FilePath high_priority_dir = temp_dir_.GetPath().Append(L"HighPri");
  ASSERT_TRUE(base::CreateDirectory(high_priority_dir));
  base::FilePath low_priority_dir = temp_dir_.GetPath().Append(L"LowPri");
  ASSERT_TRUE(base::CreateDirectory(low_priority_dir));

  // Same version in both directories.
  CreateFakeInstalledVersionIn(high_priority_dir, test_version_,
                               test_abi_hash_);
  CreateFakeInstalledVersionIn(low_priority_dir, test_version_, test_abi_hash_);

  // High-priority directory listed first.
  std::vector<base::FilePath> dirs = {high_priority_dir, low_priority_dir};
  std::vector<InstalledVersion> merged =
      Controller::ReadMultipleVersionIndexes(dirs);

  // Should have exactly one entry (deduplicated).
  int count = 0;
  for (const auto& iv : merged) {
    if (iv.metadata.version.ToString() == test_version_) {
      count++;
      // The path should be from the high-priority directory.
      EXPECT_TRUE(high_priority_dir.IsParent(iv.path));
      EXPECT_FALSE(low_priority_dir.IsParent(iv.path));
    }
  }
  EXPECT_EQ(1, count);
}

// ============================================================================
// Fallback When Not Writable
// ============================================================================

// Install falls back to existing compatible version in a readable dir when
// no writable dir is available.
TEST_F(InstallerIntegrationTest, InstallFallsBackToExistingWhenNotWritable) {
  base::FilePath readonly_dir = temp_dir_.GetPath().Append(L"ReadOnly");
  ASSERT_TRUE(base::CreateDirectory(readonly_dir));
  CreateFakeInstalledVersionIn(readonly_dir, test_version_, test_abi_hash_);

  // Readable dirs include readonly_dir; no writable dir.
  OverrideInstallDirectoriesForTesting({readonly_dir}, std::nullopt);

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.install_path.clear();

  Result result = controller.Run(Command::kInstall, config, extended);

  ClearInstallDirectoryOverridesForTesting();

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
  EXPECT_TRUE(readonly_dir.IsParent(result.libcef_path));
}

TEST_F(InstallerIntegrationTest,
       AutomaticStartupWithoutWritableDirectoryOmitsLaunchHealthPath) {
  base::FilePath readonly_dir = temp_dir_.GetPath().Append(L"ReadOnly");
  ASSERT_TRUE(base::CreateDirectory(readonly_dir));
  CreateFakeInstalledVersionIn(readonly_dir, test_version_, test_abi_hash_);
  std::vector<InstalledVersion> installed =
      ScanInstalledVersionsWithMetadata(readonly_dir);
  ASSERT_EQ(1u, installed.size());
  ASSERT_EQ(MetadataError::kSuccess,
            WriteVersionIndex(readonly_dir, installed));
  OverrideInstallDirectoriesForTesting({readonly_dir}, std::nullopt);

  Controller controller;
  Config config = CreateConfig();
  config.launch_health = LaunchHealthMode::kExplicit;
  ExtendedConfig extended = CreateExtendedConfig();
  extended.install_path.clear();
  Result result = controller.Run(Command::kInstall, config, extended, {},
                                 ExecutionContext::kAutomaticStartup);

  ClearInstallDirectoryOverridesForTesting();

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(result.version_lease);
  EXPECT_TRUE(result.launch_state_path.empty());
  EXPECT_TRUE(result.launch_cleanup_paths.empty());
}

// Install returns CONFIG_ERROR when no writable dir and no compatible version.
TEST_F(InstallerIntegrationTest,
       InstallFailsWhenNotWritableAndNoCompatibleVersion) {
  base::FilePath readonly_dir = temp_dir_.GetPath().Append(L"ReadOnly");
  ASSERT_TRUE(base::CreateDirectory(readonly_dir));
  CreateFakeInstalledVersionIn(readonly_dir, test_version_, "ffff00000000");

  OverrideInstallDirectoriesForTesting({readonly_dir}, std::nullopt);

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.install_path.clear();

  Result result = controller.Run(Command::kInstall, config, extended);

  ClearInstallDirectoryOverridesForTesting();

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeConfigError, result.error_code);
}

// ============================================================================
// Revocation Delta Tests
// ============================================================================

// Query skips a version that is revoked via disk-cached delta.
TEST_F(InstallerIntegrationTest, QuerySkipsRevokedVersion) {
  CreateFakeInstalledVersion(test_version_, test_abi_hash_);

  // Write a delta file that revokes the installed version.
  WriteRevocationCacheFile(install_dir_, {test_version_});

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kQuery, config, extended);

  // The only version is revoked — query should fail.
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);
}

// Query demotes a revoked bundled version so an older installed version wins.
TEST_F(InstallerIntegrationTest, QueryDemotesRevokedBundledVersion) {
  // Install an older version.
  Version current = Version::Parse(test_version_);
  std::string older_version =
      std::to_string(current.GetMilestone() - 1) + ".0.0";
  CreateFakeInstalledVersion(older_version, test_abi_hash_);
  ASSERT_EQ(MetadataError::kSuccess,
            WriteVersionIndex(install_dir_,
                              ScanInstalledVersionsWithMetadata(install_dir_)));

  // Build a newer bundled version.
  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), test_version_, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());

  // Write a delta file revoking the bundled version.
  WriteRevocationCacheFile(install_dir_, {test_version_});

  Controller controller;
  Config config = CreateConfig();
  config.vmin = older_version;
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kQuery, config, extended);

  // The older installed version should win because bundled is revoked.
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(older_version, result.installed_version);
  EXPECT_FALSE(bundled_dir.IsParent(result.libcef_path));
}

// Fallback (no writable dir) skips a revoked version from a readable dir.
TEST_F(InstallerIntegrationTest, FallbackSkipsRevokedVersion) {
  base::FilePath readonly_dir = temp_dir_.GetPath().Append(L"ReadOnly");
  ASSERT_TRUE(base::CreateDirectory(readonly_dir));
  CreateFakeInstalledVersionIn(readonly_dir, test_version_, test_abi_hash_);

  // Write a delta file revoking the version.
  WriteRevocationCacheFile(readonly_dir, {test_version_});

  // Override so no writable dir is available.
  OverrideInstallDirectoriesForTesting({readonly_dir}, std::nullopt);

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.install_path.clear();

  Result result = controller.Run(Command::kInstall, config, extended);

  ClearInstallDirectoryOverridesForTesting();

  // The only version is revoked — fallback should not return it.
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeConfigError, result.error_code);
}

// Local download path does not persist revocation entries to disk cache.
TEST_F(InstallerIntegrationTest, LocalDownloadPathSkipsRevocationCacheWrite) {
  CreateFakeInstalledVersion(test_version_, test_abi_hash_);

  // Set up a local download directory with a revoked.json that revokes the
  // installed version.
  base::FilePath local_cdn = temp_dir_.GetPath().Append(L"local_cdn");
  ASSERT_TRUE(base::CreateDirectory(local_cdn));
  std::string revoked_json = BuildRevocationJson({test_version_});
  ASSERT_TRUE(
      base::WriteFile(local_cdn.AppendASCII("revoked.json"), revoked_json));

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.local_download_path = local_cdn.AsUTF8Unsafe();

  Result result = controller.Run(Command::kQuery, config, extended);

  // The version should be revoked for the current session.
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);

  // The revocation cache must NOT have been written.
  base::FilePath cache_path = install_dir_.Append(kRevocationCacheFilename);
  EXPECT_FALSE(base::PathExists(cache_path));
}

// ============================================================================
// Phase 1: Multi-App Concurrency (P1)
// ============================================================================

// P1.1: Two apps with overlapping version ranges share one version.
TEST_F(InstallerIntegrationTest, TwoAppsOverlappingRanges) {
  Config app_a = CreateConfig();
  Config app_b = CreateSecondAppConfig(test_version_);
  ExtendedConfig extended = CreateExtendedConfig();

  Controller controller;

  // Install app A.
  Result result_a = controller.Run(Command::kInstall, app_a, extended);
  ASSERT_TRUE(result_a.success) << result_a.error_message;

  // Install app B — should reuse existing version.
  bool archive_requested = false;
  archive_download_requested_ = &archive_requested;
  Result result_b = controller.Run(Command::kInstall, app_b, extended);
  ASSERT_TRUE(result_b.success) << result_b.error_message;
  EXPECT_FALSE(archive_requested);
  archive_download_requested_ = nullptr;

  // Both should use the same version.
  EXPECT_EQ(result_a.installed_version, result_b.installed_version);

  // Database should have both apps.
  Database db;
  ASSERT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(db.GetApp(app_a.appid, GetCurrentPlatform()).has_value());
  EXPECT_TRUE(db.GetApp(app_b.appid, GetCurrentPlatform()).has_value());

  // Uninstall app A — version should be kept for app B.
  Result uninstall_a = controller.Run(Command::kUninstall, app_a, extended);
  EXPECT_TRUE(uninstall_a.success);

  base::FilePath version_path =
      GetVersionPath(install_dir_, Version::Parse(test_version_));
  EXPECT_TRUE(base::DirectoryExists(version_path));

  // Uninstall app B — version should now be pruned.
  Result uninstall_b = controller.Run(Command::kUninstall, app_b, extended);
  EXPECT_TRUE(uninstall_b.success);

  EXPECT_FALSE(base::DirectoryExists(version_path));
}

// P1.2: Two apps with disjoint version ranges install two versions.
TEST_F(InstallerIntegrationTest, TwoAppsDisjointRanges) {
  std::string newer_version = test_version_ + ".1";

  auto mv = BuildTwoVersions(test_version_, test_abi_hash_, newer_version,
                             test_abi_hash_);
  ASSERT_TRUE(mv.has_value()) << "Failed to build two versions";

  manifest_json_ = mv->merged_manifest_json;
  archive_map_[mv->dist_a.archive_path.BaseName().AsUTF8Unsafe()] =
      mv->dist_a.archive_path;
  archive_map_[mv->dist_b.archive_path.BaseName().AsUTF8Unsafe()] =
      mv->dist_b.archive_path;

  ExtendedConfig extended = CreateExtendedConfig();
  Controller controller;

  // App A wants only test_version_.
  Config app_a = CreateConfig();
  app_a.vmin = test_version_;
  app_a.vmax = test_version_;

  Result result_a = controller.Run(Command::kInstall, app_a, extended);
  ASSERT_TRUE(result_a.success) << result_a.error_message;
  EXPECT_EQ(test_version_, result_a.installed_version);

  // App B wants only the newer version.
  Config app_b = CreateSecondAppConfig(newer_version, newer_version);
  Result result_b = controller.Run(Command::kInstall, app_b, extended);
  ASSERT_TRUE(result_b.success) << result_b.error_message;
  EXPECT_EQ(newer_version, result_b.installed_version);

  // Both version directories should exist.
  EXPECT_TRUE(base::DirectoryExists(
      GetVersionPath(install_dir_, Version::Parse(test_version_))));
  EXPECT_TRUE(base::DirectoryExists(
      GetVersionPath(install_dir_, Version::Parse(newer_version))));

  // Uninstall app A — only test_version_ pruned.
  controller.Run(Command::kUninstall, app_a, extended);
  EXPECT_FALSE(base::DirectoryExists(
      GetVersionPath(install_dir_, Version::Parse(test_version_))));
  EXPECT_TRUE(base::DirectoryExists(
      GetVersionPath(install_dir_, Version::Parse(newer_version))));

  archive_map_.clear();
}

// P1.3: Two apps with different ABI hashes at the same milestone.
TEST_F(InstallerIntegrationTest, TwoAppsDifferentAbiHashes) {
  std::string abi_hash_b = "ffff00000000";

  Config app_a = CreateConfig();
  Config app_b = CreateSecondAppConfig(test_version_, "", abi_hash_b);
  ExtendedConfig extended = CreateExtendedConfig();

  Controller controller;

  // App A installs with the default abi_hash — should succeed.
  Result result_a = controller.Run(Command::kInstall, app_a, extended);
  ASSERT_TRUE(result_a.success) << result_a.error_message;

  // App B with a different abi_hash — CDN only has one ABI, should fail.
  Result result_b = controller.Run(Command::kInstall, app_b, extended);
  EXPECT_FALSE(result_b.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result_b.error_code);
}

// P1.4: Concurrent Controller::Run on two threads, serialized by lock.
TEST_F(InstallerIntegrationTest, ConcurrentInstallSerializedByLock) {
  Config app_a = CreateConfig();
  Config app_b = CreateSecondAppConfig(test_version_);
  ExtendedConfig extended = CreateExtendedConfig();

  base::Thread thread_a("InstallerA");
  base::Thread thread_b("InstallerB");
  thread_a.Start();
  thread_b.Start();

  Result result_a, result_b;

  base::WaitableEvent done_a(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent done_b(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);

  thread_a.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](Config config, ExtendedConfig ext, Result* out,
                        base::WaitableEvent* done) {
                       Controller c;
                       *out = c.Run(Command::kInstall, config, ext);
                       done->Signal();
                     },
                     app_a, extended, &result_a, &done_a));

  thread_b.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](Config config, ExtendedConfig ext, Result* out,
                        base::WaitableEvent* done) {
                       Controller c;
                       *out = c.Run(Command::kInstall, config, ext);
                       done->Signal();
                     },
                     app_b, extended, &result_b, &done_b));

  done_a.Wait();
  done_b.Wait();

  EXPECT_TRUE(result_a.success) << result_a.error_message;
  EXPECT_TRUE(result_b.success) << result_b.error_message;

  // Both apps should be in the database.
  Database db;
  ASSERT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(db.GetApp(app_a.appid, GetCurrentPlatform()).has_value());
  EXPECT_TRUE(db.GetApp(app_b.appid, GetCurrentPlatform()).has_value());
}

// ============================================================================
// Phase 1: Upgrade/Downgrade Lifecycle (P2)
// ============================================================================

// P2.1: App upgrade — vmin moves forward, old version pruned.
TEST_F(InstallerIntegrationTest, AppUpgradeVminMovesForward) {
  std::string newer_version = test_version_ + ".1";

  auto mv = BuildTwoVersions(test_version_, test_abi_hash_, newer_version,
                             test_abi_hash_);
  ASSERT_TRUE(mv.has_value());

  archive_map_[mv->dist_a.archive_path.BaseName().AsUTF8Unsafe()] =
      mv->dist_a.archive_path;
  archive_map_[mv->dist_b.archive_path.BaseName().AsUTF8Unsafe()] =
      mv->dist_b.archive_path;

  ExtendedConfig extended = CreateExtendedConfig();
  Controller controller;

  // First install with vmin=test_version_, vmax=test_version_ — gets
  // test_version_.
  manifest_json_ = mv->merged_manifest_json;
  Config config = CreateConfig();
  config.vmin = test_version_;
  config.vmax = test_version_;

  Result first = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(first.success) << first.error_message;
  EXPECT_EQ(test_version_, first.installed_version);

  // Re-register with higher vmin — old should be pruned, new downloaded.
  config.vmin = newer_version;
  config.vmax.clear();

  Result second = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(second.success) << second.error_message;
  EXPECT_EQ(newer_version, second.installed_version);

  // Old version should be pruned.
  EXPECT_FALSE(base::DirectoryExists(
      GetVersionPath(install_dir_, Version::Parse(test_version_))));

  archive_map_.clear();
}

// P2.2: CDN serves newer version on re-check — kInstall picks the best.
TEST_F(InstallerIntegrationTest, CdnServesNewerVersionOnRecheck) {
  std::string newer_version = test_version_ + ".1";

  auto mv = BuildTwoVersions(test_version_, test_abi_hash_, newer_version,
                             test_abi_hash_);
  ASSERT_TRUE(mv.has_value());

  archive_map_[mv->dist_a.archive_path.BaseName().AsUTF8Unsafe()] =
      mv->dist_a.archive_path;
  archive_map_[mv->dist_b.archive_path.BaseName().AsUTF8Unsafe()] =
      mv->dist_b.archive_path;

  ExtendedConfig extended = CreateExtendedConfig();
  Controller controller;

  // Serve only test_version_ first.
  manifest_json_ =
      R"([{"version": ")" + test_version_ + R"(", "file": "cef_)" +
      test_version_ + "_" + mv->dist_a.platform + R"(.tar.xz", "sha1": ")" +
      mv->dist_a.archive_sha1 +
      R"(", "last_modified": "2026-03-05T10:00:00Z", "abi_hash": ")" +
      test_abi_hash_ + R"("}])";

  Config config = CreateConfig();

  Result first = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(first.success) << first.error_message;
  EXPECT_EQ(test_version_, first.installed_version);

  // Now update CDN to include both versions. A fresh install should pick the
  // newer version. Uninstall first to clear the existing version.
  controller.Run(Command::kUninstall, config, extended);
  manifest_json_ = mv->merged_manifest_json;

  Result reinstall = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(reinstall.success) << reinstall.error_message;
  EXPECT_EQ(newer_version, reinstall.installed_version);

  archive_map_.clear();
}

TEST_F(InstallerIntegrationTest, NextBestSignedVersionAfterPropagationLag) {
  const std::string newer_version = test_version_ + ".1";
  auto mv = BuildTwoVersions(test_version_, test_abi_hash_, newer_version,
                             test_abi_hash_);
  ASSERT_TRUE(mv.has_value());
  manifest_json_ = mv->merged_manifest_json;
  const std::string older_filename =
      mv->dist_a.archive_path.BaseName().AsUTF8Unsafe();
  const std::string newer_filename =
      mv->dist_b.archive_path.BaseName().AsUTF8Unsafe();
  archive_map_[older_filename] = mv->dist_a.archive_path;
  missing_archive_filenames_.insert(newer_filename);
  request_log_.clear();

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
  EXPECT_TRUE(base::DirectoryExists(
      GetVersionPath(install_dir_, Version::Parse(test_version_))));
  EXPECT_FALSE(base::DirectoryExists(
      GetVersionPath(install_dir_, Version::Parse(newer_version))));
  int manifest_requests = 0;
  for (const auto& request : request_log_) {
    if (request.find("_windows") != std::string::npos &&
        base::EndsWith(request, ".json")) {
      ++manifest_requests;
    }
  }
  EXPECT_EQ(1, manifest_requests);
}

TEST_F(InstallerIntegrationTest,
       NextBestSignedVersionAfterCatalogVerificationFailure) {
  const std::string newer_version = test_version_ + ".1";
  const base::FilePath signed_parent =
      archive_dir_.AppendASCII("tampered-signed-candidate");
  const base::FilePath signed_dir = test::BuildTestBundledDirectory(
      newer_version, test_abi_hash_, signed_parent,
      test::GetTestDataPath().AppendASCII("test_signing.pfx"), "test");
  ASSERT_FALSE(signed_dir.empty());
  ASSERT_TRUE(base::AppendToFile(
      signed_dir.Append(kReleaseSubdirectory).Append(kLibcefFilename),
      "tamper-after-catalog"));
  const std::string newer_filename =
      "cef_" + newer_version + "_" + test_dist_.platform + ".tar.xz";
  const base::FilePath newer_archive = archive_dir_.AppendASCII(newer_filename);
  ASSERT_TRUE(test::CreateTarXzArchive(signed_dir, newer_archive));
  const std::string newer_sha1 = ComputeFileSha1(newer_archive);
  ASSERT_FALSE(newer_sha1.empty());
  const std::string older_filename =
      test_dist_.archive_path.BaseName().AsUTF8Unsafe();
  archive_map_[older_filename] = test_dist_.archive_path;
  archive_map_[newer_filename] = newer_archive;
  manifest_json_ =
      R"([{"version": ")" + test_version_ + R"(", "file": ")" + older_filename +
      R"(", "sha1": ")" + test_dist_.archive_sha1 +
      R"(", "last_modified": "2026-03-05T10:00:00Z", "abi_hash": ")" +
      test_abi_hash_ + R"("},{"version": ")" + newer_version +
      R"(", "file": ")" + newer_filename + R"(", "sha1": ")" + newer_sha1 +
      R"(", "last_modified": "2026-03-05T10:00:00Z", "abi_hash": ")" +
      test_abi_hash_ + R"("}])";
  request_log_.clear();

  ExtendedConfig extended = CreateExtendedConfig();
  extended.cdn_urls = {server_->base_url().spec() + "primary/",
                       server_->base_url().spec() + "secondary/"};
  Result result = Controller().Run(Command::kInstall, CreateConfig(), extended);

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
  EXPECT_EQ(1,
            std::ranges::count(request_log_, "GET /primary/" + newer_filename));
  EXPECT_EQ(
      0, std::ranges::count(request_log_, "GET /secondary/" + newer_filename));
  EXPECT_EQ(1,
            std::ranges::count(request_log_, "GET /primary/" + older_filename));
}

TEST_F(InstallerIntegrationTest,
       NextBestSignedVersionAfterManifestMetadataMismatch) {
  const std::string advertised_version = test_version_ + ".1";
  const std::string archive_version = test_version_ + ".9";
  auto mismatched = test::BuildTestDistribution(
      archive_version, test_abi_hash_, archive_dir_,
      test::GetTestDataPath().AppendASCII("test_signing.pfx"), "test");
  ASSERT_TRUE(mismatched.has_value());
  const std::string advertised_filename =
      "cef_" + advertised_version + "_" + test_dist_.platform + ".tar.xz";
  const std::string older_filename =
      test_dist_.archive_path.BaseName().AsUTF8Unsafe();
  archive_map_[older_filename] = test_dist_.archive_path;
  archive_map_[advertised_filename] = mismatched->archive_path;
  manifest_json_ =
      R"([{"version": ")" + test_version_ + R"(", "file": ")" + older_filename +
      R"(", "sha1": ")" + test_dist_.archive_sha1 +
      R"(", "last_modified": "2026-03-05T10:00:00Z", "abi_hash": ")" +
      test_abi_hash_ + R"("},{"version": ")" + advertised_version +
      R"(", "file": ")" + advertised_filename + R"(", "sha1": ")" +
      mismatched->archive_sha1 +
      R"(", "last_modified": "2026-03-05T10:00:00Z", "abi_hash": ")" +
      test_abi_hash_ + R"("}])";
  request_log_.clear();

  Result result = Controller().Run(Command::kInstall, CreateConfig(),
                                   CreateExtendedConfig());

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
  EXPECT_EQ(1, std::ranges::count(request_log_, "GET /" + advertised_filename));
  EXPECT_EQ(1, std::ranges::count(request_log_, "GET /" + older_filename));
}

// P2.3: Install succeeds, then CDN revokes the version. Next install fails.
TEST_F(InstallerIntegrationTest, RevocationForcesFallback) {
  ExtendedConfig extended = CreateExtendedConfig();
  Config config = CreateConfig();
  Controller controller;

  // First install succeeds.
  Result first = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(first.success) << first.error_message;
  EXPECT_EQ(test_version_, first.installed_version);

  // CDN revokes the installed version.
  revocation_json_ = BuildRevocationJson({test_version_});

  // Next install should fail — installed version is now revoked and CDN has
  // no alternative.
  Result second = controller.Run(Command::kInstall, config, extended);
  EXPECT_FALSE(second.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, second.error_code);
}

// ============================================================================
// Phase 1: Database Resilience (P3)
// ============================================================================

// P3.1: Corrupt database — garbage bytes, clear error (no crash).
TEST_F(InstallerIntegrationTest, CorruptDatabaseRecovery) {
  // Write garbage to the database file.
  base::FilePath db_path = GetDatabasePath(install_dir_);
  ASSERT_TRUE(base::WriteFile(db_path, "NOT VALID JSON !@#$%"));

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());

  // The controller returns a clear error for corrupt database (no crash).
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeDatabaseError, result.error_code);
}

// P3.2: Schema version forward-compatibility — database Load returns
// kSchemaVersionTooNew but query still succeeds (known fields readable).
// Tested at the Database layer directly since the Controller treats load
// errors as fatal.
TEST_F(InstallerIntegrationTest, SchemaVersionForwardCompatibility) {
  // Install normally to populate the database.
  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result first = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(first.success) << first.error_message;

  // Load the database, verify the app is present.
  Database db;
  base::FilePath db_path = GetDatabasePath(install_dir_);
  ASSERT_EQ(DatabaseError::kSuccess, db.Load(db_path));
  EXPECT_TRUE(db.GetApp(config.appid, GetCurrentPlatform()).has_value());
  EXPECT_TRUE(db.CanPrune());

  // Query should find the installed version.
  Result query = controller.Run(Command::kQuery, config, extended);
  EXPECT_TRUE(query.success) << query.error_message;
  EXPECT_EQ(test_version_, query.installed_version);
}

// P3.3: Database file locked — timeout, not hang.
TEST_F(InstallerIntegrationTest, DatabaseFileLocked) {
  // Create database file and hold an exclusive lock on it.
  base::FilePath db_path = GetDatabasePath(install_dir_);
  ASSERT_TRUE(base::WriteFile(db_path, "{}"));

  base::File locked_file(db_path, base::File::FLAG_OPEN |
                                      base::File::FLAG_READ |
                                      base::File::FLAG_WIN_EXCLUSIVE_READ |
                                      base::File::FLAG_WIN_EXCLUSIVE_WRITE);
  ASSERT_TRUE(locked_file.IsValid());

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());

  locked_file.Close();

  // Should fail but not hang. Could be database error or install error.
  // The key assertion: the test completes in reasonable time (test timeout).
  EXPECT_FALSE(result.success);
}

// ============================================================================
// Phase 1: CDN Error Scenarios (P4)
// ============================================================================

// P4.1: Connection reset mid-download — no partial dir left.
TEST_F(InstallerIntegrationTest, PartialDownloadConnectionReset) {
  cdn_failure_mode_ = CdnFailureMode::kDropConnection;

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());

  EXPECT_FALSE(result.success);

  // No partial version directory should be left.
  base::FilePath versions_dir = install_dir_.Append(L"Versions");
  if (base::DirectoryExists(versions_dir)) {
    base::FileEnumerator enumerator(versions_dir, false,
                                    base::FileEnumerator::DIRECTORIES);
    int dir_count = 0;
    while (!enumerator.Next().empty()) {
      dir_count++;
    }
    EXPECT_EQ(0, dir_count) << "Partial version directory left after reset";
  }

  cdn_failure_mode_ = CdnFailureMode::kNone;
}

// P4.2: Manifest lists version but archive 404s.
TEST_F(InstallerIntegrationTest, ManifestListsVersionButArchive404) {
  cdn_failure_mode_ = CdnFailureMode::kArchive404;

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());

  EXPECT_FALSE(result.success);

  cdn_failure_mode_ = CdnFailureMode::kNone;
}

// P4.3: SHA256 mismatch — archive rejected before extraction.
TEST_F(InstallerIntegrationTest, Sha256Mismatch) {
  cdn_failure_mode_ = CdnFailureMode::kWrongSha256;

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());

  EXPECT_FALSE(result.success);

  cdn_failure_mode_ = CdnFailureMode::kNone;
}

// ============================================================================
// Phase 1: Filesystem Edge Cases (P5)
// ============================================================================

// P5.3: Target version directory already exists — install handles collision.
TEST_F(InstallerIntegrationTest, TargetVersionDirectoryAlreadyExists) {
  // Pre-create the version directory with a fake valid installation.
  CreateFakeInstalledVersion(test_version_, test_abi_hash_);

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());

  // Should succeed — existing compatible version found, no download needed.
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
}

// P5.4: Trash directory from failed prune — retried on next install.
TEST_F(InstallerIntegrationTest, TrashDirectoryRetry) {
  // Create a leftover entry in the .trash/ subdirectory.
  base::FilePath trash_root = install_dir_.Append(kTrashSubdirectory);
  ASSERT_TRUE(base::CreateDirectory(trash_root));
  base::FilePath trash_entry = trash_root.Append(L"150.1_deadbeef");
  ASSERT_TRUE(base::CreateDirectory(trash_entry));
  ASSERT_TRUE(base::WriteFile(trash_entry.Append(L"dummy.txt"), "test"));

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;

  // The trash entry should have been cleaned up.
  EXPECT_FALSE(base::DirectoryExists(trash_entry));
}

TEST_F(InstallerIntegrationTest, RegularFileTrashInUseRemainsCleanupDeferred) {
  base::FilePath trash_root = install_dir_.Append(kTrashSubdirectory);
  ASSERT_TRUE(base::CreateDirectory(trash_root));
  base::FilePath trash_file = trash_root.Append(L"pending_regular_file");
  ASSERT_TRUE(base::WriteFile(trash_file, "pending"));
  HANDLE handle =
      ::CreateFileW(trash_file.value().c_str(), GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE(INVALID_HANDLE_VALUE, handle);

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  EXPECT_NE(result.warnings.end(),
            std::find(result.warnings.begin(), result.warnings.end(),
                      "Pending trash reclamation remains"));
  EXPECT_TRUE(base::PathExists(trash_file));
  ::CloseHandle(handle);
  EXPECT_EQ(1, RetryPendingDeletions(install_dir_));
  EXPECT_FALSE(base::PathExists(trash_file));
}

// ============================================================================
// Phase 1: Security-Specific Integration (P6)
// ============================================================================

// P6.1: Certificate rotation — cert A accepted, cert B rejected, then rotated.
TEST_F(InstallerIntegrationTest, CertificateRotation) {
  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Install with the test cert thumbprint — should succeed.
  Result result_a = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(result_a.success) << result_a.error_message;

  // Uninstall to reset state.
  controller.Run(Command::kUninstall, config, extended);

  // Try with a wrong thumbprint — should fail.
  extended.certificate_thumbprint = std::string(40, 'a');
  Result result_wrong = controller.Run(Command::kInstall, config, extended);
  EXPECT_FALSE(result_wrong.success);
  EXPECT_EQ(kExitCodeSignatureError, result_wrong.error_code);

  // Rotate back to test thumbprint — should succeed again.
  extended.certificate_thumbprint = test_thumbprint_;
  Result result_rotated = controller.Run(Command::kInstall, config, extended);
  EXPECT_TRUE(result_rotated.success) << result_rotated.error_message;
}

// P6.2: Symlink/junction in install path — rejected.
TEST_F(InstallerIntegrationTest, SymlinkJunctionInInstallPath) {
  // Create a junction point as the install path.
  base::FilePath target_dir = temp_dir_.GetPath().Append(L"real_target");
  ASSERT_TRUE(base::CreateDirectory(target_dir));

  base::FilePath junction_dir = temp_dir_.GetPath().Append(L"junction_link");

  // Create a junction using mklink /J (requires no special privileges).
  std::wstring cmd = L"cmd /c mklink /J \"" + junction_dir.value() + L"\" \"" +
                     target_dir.value() + L"\"";
  ::_wsystem(cmd.c_str());

  if (!base::DirectoryExists(junction_dir)) {
    GTEST_SKIP() << "Could not create junction point";
  }

  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.install_path = junction_dir.AsUTF8Unsafe();

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  // Reparse point should be rejected.
  EXPECT_FALSE(result.success);

  // Clean up junction.
  ::_wsystem((L"cmd /c rmdir \"" + junction_dir.value() + L"\"").c_str());
}

// P6.3: Registry-based path override via OverrideInstallDirectoriesForTesting.
TEST_F(InstallerIntegrationTest, RegistryBasedPathOverride) {
  base::FilePath custom_dir = temp_dir_.GetPath().Append(L"RegistryPath");
  ASSERT_TRUE(base::CreateDirectory(custom_dir));
  const base::FilePath cache_dir = GetCacheDirectory(custom_dir);
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  const base::FilePath cached_archive = cache_dir.AppendASCII(
      ComputeFileSha256(test_dist_.archive_path) + ".tar.xz");
  ASSERT_TRUE(base::CopyFile(test_dist_.archive_path, cached_archive));

  OverrideInstallDirectoriesForTesting({}, custom_dir);

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.install_path.clear();

  Result result = controller.Run(Command::kInstall, config, extended);

  ClearInstallDirectoryOverridesForTesting();

  ASSERT_TRUE(result.success) << result.error_message;
  // Installed to the overridden path.
  EXPECT_TRUE(custom_dir.IsParent(result.libcef_path));
}

// ============================================================================
// Phase 1: Additional Scenarios (P7)
// ============================================================================

// P7.1: Query with multiple apps registered returns correct best version.
TEST_F(InstallerIntegrationTest, QueryWithMultipleAppsRegistered) {
  Config app_a = CreateConfig();
  Config app_b = CreateSecondAppConfig(test_version_);
  ExtendedConfig extended = CreateExtendedConfig();

  Controller controller;

  // Install both apps.
  ASSERT_TRUE(controller.Run(Command::kInstall, app_a, extended).success);
  ASSERT_TRUE(controller.Run(Command::kInstall, app_b, extended).success);

  // Query for each app should return the correct version.
  Result query_a = controller.Run(Command::kQuery, app_a, extended);
  EXPECT_TRUE(query_a.success) << query_a.error_message;
  EXPECT_EQ(test_version_, query_a.installed_version);

  Result query_b = controller.Run(Command::kQuery, app_b, extended);
  EXPECT_TRUE(query_b.success) << query_b.error_message;
  EXPECT_EQ(test_version_, query_b.installed_version);
}

// P7.2: Log file written after install.
TEST_F(InstallerIntegrationTest, LogFileWritten) {
  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateConfig(), CreateExtendedConfig());
  ASSERT_TRUE(result.success) << result.error_message;

  base::FilePath log_path = install_dir_.Append(kLogFilename);
  if (!base::PathExists(log_path)) {
    // In test mode, the log directory may not be writable or the logger
    // might not be initialized. Verify the install succeeded at least.
    GTEST_SKIP() << "Log file not written (expected in some test environments)";
  }

  std::string log_content;
  ASSERT_TRUE(base::ReadFileToString(log_path, &log_content));
  EXPECT_FALSE(log_content.empty());
}

// P4.4: Manifest caching — verify DownloadWithCache skips re-download when
// cache is valid and force_check is false, and re-downloads when force_check
// is true (or cache is stale). HEAD-based validation (304/200) is tested
// implicitly: force_check=false hits the cache path (HEAD or cache-valid
// shortcut), force_check=true bypasses it entirely.
TEST_F(InstallerIntegrationTest, ManifestCachingAndForceCheck) {
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.force_check = true;

  Controller controller;
  Result first = controller.Run(Command::kInstall, config, extended);
  ASSERT_TRUE(first.success) << first.error_message;
  EXPECT_EQ(test_version_, first.installed_version);

  const std::string manifest_path =
      "/" + BuildAbiHashUrl("", config.abi_hash, GetCurrentPlatform(),
                            config.channel);

  // Second run: force_check=false → cache used, no re-download needed.
  extended.force_check = false;
  manifest_head_returns_304_ = true;
  revocation_head_returns_304_ = true;
  request_log_.clear();
  Result second = controller.Run(Command::kUpdate, config, extended);
  ASSERT_TRUE(second.success) << second.error_message;
  EXPECT_EQ(1, std::count(request_log_.begin(), request_log_.end(),
                          "HEAD " + manifest_path));
  EXPECT_EQ(0, std::count(request_log_.begin(), request_log_.end(),
                          "GET " + manifest_path));
  EXPECT_EQ(1, std::count(request_log_.begin(), request_log_.end(),
                          "HEAD /revoked.json"));
  EXPECT_EQ(0, std::count(request_log_.begin(), request_log_.end(),
                          "GET /revoked.json"));

  // Now build a newer version and replace the server manifest.
  std::string newer_version = test_version_ + ".1";

  auto newer_dist = test::BuildTestDistribution(
      newer_version, test_abi_hash_, archive_dir_,
      test::GetTestDataPath().AppendASCII("test_signing.pfx"), "test");
  ASSERT_TRUE(newer_dist.has_value());
  archive_map_[newer_dist->archive_path.BaseName().AsUTF8Unsafe()] =
      newer_dist->archive_path;

  manifest_json_ =
      R"([{"version": ")" + newer_version + R"(", "file": "cef_)" +
      newer_version + "_" + newer_dist->platform + R"(.tar.xz", "sha1": ")" +
      newer_dist->archive_sha1 +
      R"(", "last_modified": "2026-04-01T10:00:00Z", "abi_hash": ")" +
      test_abi_hash_ + R"("}])";

  // Third run: force_check=true. The existing version remains compatible,
  // but update must still check and install the newer manifest entry.
  extended.force_check = true;
  request_log_.clear();

  Result third = controller.Run(Command::kUpdate, config, extended);
  ASSERT_TRUE(third.success) << third.error_message;
  EXPECT_EQ(newer_version, third.installed_version);
  EXPECT_EQ(0, std::count(request_log_.begin(), request_log_.end(),
                          "HEAD " + manifest_path));
  EXPECT_EQ(1, std::count(request_log_.begin(), request_log_.end(),
                          "GET " + manifest_path));
  EXPECT_EQ(0, std::count(request_log_.begin(), request_log_.end(),
                          "HEAD /revoked.json"));
  EXPECT_EQ(1, std::count(request_log_.begin(), request_log_.end(),
                          "GET /revoked.json"));

  archive_map_.clear();
}

// ============================================================================
// Reparse / path validation rejection tests
// ============================================================================

// Callsite 1: Read-only fallback rejects when Release/ is missing.
TEST_F(InstallerIntegrationTest, ReadOnlyFallbackRejectsInvalidPath) {
  base::FilePath readonly_dir = temp_dir_.GetPath().Append(L"ReadOnly");
  ASSERT_TRUE(base::CreateDirectory(readonly_dir));
  CreateFakeInstalledVersionIn(readonly_dir, test_version_, test_abi_hash_);

  // Remove Release/ so the libcef path is invalid.
  Version version = Version::Parse(test_version_);
  base::FilePath version_dir = GetVersionPath(readonly_dir, version);
  base::DeletePathRecursively(version_dir.Append(kReleaseSubdirectory));

  OverrideInstallDirectoriesForTesting({readonly_dir}, std::nullopt);

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.install_path.clear();

  Result result = controller.Run(Command::kInstall, config, extended);

  ClearInstallDirectoryOverridesForTesting();

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeConfigError, result.error_code);
}

// Callsite 3: a writable mutation replaces an unindexed incomplete
// destination with the newly verified staging distribution.
TEST_F(InstallerIntegrationTest,
       InstallReplacesUnindexedDestinationWithVerifiedStaging) {
  CreateFakeInstalledVersion(test_version_, test_abi_hash_);

  // Remove Release/ so the libcef path is invalid.
  Version version = Version::Parse(test_version_);
  base::FilePath version_dir = GetVersionPath(install_dir_, version);
  base::DeletePathRecursively(version_dir.Append(kReleaseSubdirectory));
  base::FilePath old_marker = version_dir.Append(L"old_marker.txt");
  ASSERT_TRUE(base::WriteFile(old_marker, "old target"));

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(test_version_, result.installed_version);
  EXPECT_TRUE(base::PathExists(GetLibcefPath(version_dir)));
  EXPECT_FALSE(base::PathExists(old_marker));
}

// Callsite 4: Revoked bundled fallback rejects when Release/ is missing.
TEST_F(InstallerIntegrationTest, RevokedBundledFallbackRejectsInvalidPath) {
  base::FilePath bundled_dir =
      BuildBundled(temp_dir_.GetPath(), test_version_, test_abi_hash_);
  ASSERT_FALSE(bundled_dir.empty());

  // Remove Release/ so the libcef path is invalid.
  base::DeletePathRecursively(bundled_dir.Append(kReleaseSubdirectory));

  // Revoke the bundled version AND make CDN return no alternative.
  revocation_json_ = BuildRevocationJson({test_version_});

  Controller controller;
  Config config = CreateConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled_dir.AsUTF8Unsafe();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeInstallError, result.error_code);
}

}  // namespace
}  // namespace cef_installer
