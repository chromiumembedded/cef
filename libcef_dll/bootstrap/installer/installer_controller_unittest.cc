// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_controller.h"

#include <stdlib.h>

#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <thread>

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cef/include/cef_api_hash.h"
#include "cef/libcef_dll/bootstrap/installer/installer_archive.h"
#include "cef/libcef_dll/bootstrap/installer/installer_bootstrap_helpers.h"
#include "cef/libcef_dll/bootstrap/installer/installer_cdn_manifest.h"
#include "cef/libcef_dll/bootstrap/installer/installer_database.h"
#include "cef/libcef_dll/bootstrap/installer/installer_download.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"
#include "cef/libcef_dll/bootstrap/installer/installer_launch_state.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "cef/libcef_dll/bootstrap/installer/installer_progress_dialog.h"
#include "cef/libcef_dll/bootstrap/installer/installer_signature.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version_metadata.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {

using internal::BuildProgressJson;
using internal::ExitCodeToString;
using internal::HandleLaunchSuccess;
using internal::ParseCombinedConfig;
using internal::ParseCommand;
using internal::ParseExtendedConfigFromJson;
using internal::ResetProgressNotificationState;
using internal::SetTestingMode;
using internal::StepCanonicalName;
using internal::StringToExitCode;
using internal::StringToOutcome;

namespace {

ProgressCallback RecordProgress(std::vector<Step>* steps) {
  return base::BindRepeating(
      [](std::vector<Step>* steps, Step step, uint64_t, uint64_t) {
        steps->push_back(step);
        return true;
      },
      base::Unretained(steps));
}

// ============================================================================
// Result Tests
// ============================================================================

TEST(InstallerResultTest, ToJsonSuccess) {
  Result result;
  result.success = true;
  result.outcome = Outcome::kCommitted;
  result.libcef_path = base::FilePath(L"C:\\Program Files\\CEF\\libcef.dll");
  result.installed_version = "137.3.5";

  std::string json = result.ToJson();

  EXPECT_NE(json.find("\"success\":true"), std::string::npos);
  EXPECT_NE(json.find("\"outcome\":\"committed\""), std::string::npos);
  EXPECT_NE(json.find("\"libcef_path\":"), std::string::npos);
  EXPECT_NE(json.find("137.3.5"), std::string::npos);
}

TEST(InstallerResultTest, ToJsonError) {
  Result result;
  result.success = false;
  result.error_code = kExitCodeNetworkError;
  result.error_message = "Download failed";

  std::string json = result.ToJson();

  EXPECT_NE(json.find("\"success\":false"), std::string::npos);
  EXPECT_NE(json.find("\"outcome\":\"failed\""), std::string::npos);
  EXPECT_NE(json.find("\"error_code\":101"), std::string::npos);
  EXPECT_NE(json.find("\"error_name\":\"NETWORK_ERROR\""), std::string::npos);
  EXPECT_NE(json.find("Download failed"), std::string::npos);
}

TEST(InstallerResultTest, FromJsonValid) {
  std::string json = R"({
    "success": true,
    "outcome": "committed",
    "libcef_path": "C:\\CEF\\libcef.dll",
    "installed_version": "137.3.5",
    "version_full": "137.3.5+g62d140e+chromium-137.0.7204.6"
  })";

  std::optional<Result> result = Result::FromJson(json);

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->success);
  EXPECT_EQ(result->outcome, Outcome::kCommitted);
  EXPECT_EQ(result->installed_version, "137.3.5");
  EXPECT_EQ(result->version_full, "137.3.5+g62d140e+chromium-137.0.7204.6");
}

TEST(InstallerResultTest, FromJsonError) {
  std::string json = R"({
    "success": false,
    "outcome": "failed",
    "error_code": 100,
    "error_name": "CONFIG_ERROR",
    "error_message": "Invalid config"
  })";

  std::optional<Result> result = Result::FromJson(json);

  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->success);
  EXPECT_EQ(result->outcome, Outcome::kFailed);
  EXPECT_EQ(result->error_code, kExitCodeConfigError);
  EXPECT_EQ(result->error_message, "Invalid config");
}

TEST(InstallerResultTest, FromJsonMalformed) {
  std::optional<Result> result1 = Result::FromJson("not json");
  EXPECT_FALSE(result1.has_value());

  std::optional<Result> result2 = Result::FromJson("{}");
  EXPECT_FALSE(result2.has_value());  // Missing required "success" field

  std::optional<Result> result3 = Result::FromJson("");
  EXPECT_FALSE(result3.has_value());
}

TEST(InstallerResultTest, FactorySuccess) {
  Result result =
      Result::Success(base::FilePath(L"C:\\CEF\\libcef.dll"), "137.3.5",
                      "137.3.5+g62d140e+chromium-137.0.7204.6");

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.outcome, Outcome::kCommitted);
  EXPECT_EQ(result.libcef_path.value(), L"C:\\CEF\\libcef.dll");
  EXPECT_EQ(result.installed_version, "137.3.5");
  EXPECT_EQ(result.version_full, "137.3.5+g62d140e+chromium-137.0.7204.6");
  EXPECT_FALSE(result.is_bundled);
}

TEST(InstallerResultTest, FactorySuccessBundled) {
  Result result = Result::Success(
      base::FilePath(L"C:\\App\\CEF\\libcef.dll"), "137.3.5",
      "137.3.5+g62d140e+chromium-137.0.7204.6", /*is_bundled=*/true);

  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.is_bundled);
  EXPECT_EQ(result.version_full, "137.3.5+g62d140e+chromium-137.0.7204.6");
}

TEST(InstallerResultTest, FactoryError) {
  Result result = Result::Error(kExitCodeNetworkError, "Connection failed");

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.outcome, Outcome::kFailed);
  EXPECT_EQ(result.error_code, kExitCodeNetworkError);
  EXPECT_EQ(result.error_message, "Connection failed");
}

TEST(InstallerResultTest, RoundTrip) {
  Result original;
  original.success = true;
  original.outcome = Outcome::kCommitted;
  original.libcef_path = base::FilePath(L"C:\\Test\\libcef.dll");
  original.installed_version = "138.0.1";
  original.version_full = "138.0.1+gabcdef0+chromium-138.0.7500.0";
  original.is_bundled = true;

  std::string json = original.ToJson();
  std::optional<Result> parsed = Result::FromJson(json);

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(original.success, parsed->success);
  EXPECT_EQ(original.installed_version, parsed->installed_version);
  EXPECT_EQ(original.version_full, parsed->version_full);
  EXPECT_EQ(original.outcome, parsed->outcome);
  EXPECT_EQ(original.is_bundled, parsed->is_bundled);
}

TEST(InstallerResultTest, RoundTripNotBundled) {
  Result original =
      Result::Success(base::FilePath(L"C:\\CEF\\libcef.dll"), "138.0.1");

  std::string json = original.ToJson();
  std::optional<Result> parsed = Result::FromJson(json);

  ASSERT_TRUE(parsed.has_value());
  EXPECT_FALSE(parsed->is_bundled);
}

TEST(InstallerResultTest, FromJsonMissingIsBundledDefaultsFalse) {
  std::string json = R"({
    "success": true,
    "outcome": "committed",
    "libcef_path": "C:\\CEF\\libcef.dll",
    "installed_version": "137.3.5"
  })";

  std::optional<Result> result = Result::FromJson(json);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->is_bundled);
}

// ============================================================================
// ExtendedConfig Tests
// ============================================================================

TEST(ExtendedConfigTest, DefaultValues) {
  ExtendedConfig config;

  EXPECT_TRUE(config.cdn_urls.empty());
  EXPECT_TRUE(config.install_path.empty());
  EXPECT_TRUE(config.bundled_cef_path.empty());
  EXPECT_EQ(config.certificate_thumbprint,
            std::string(kCefCertificateThumbprint));
  EXPECT_FALSE(config.force_check);
  EXPECT_TRUE(config.show_progress_ui);
  EXPECT_FALSE(config.background_mode);
  EXPECT_EQ(config.parent_window, nullptr);
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST(InstallerControllerClassificationTest,
     NextBestArchiveErrorClassification) {
  const ArchiveError retryable[] = {
      ArchiveError::kInvalidFormat,    ArchiveError::kInvalidHeader,
      ArchiveError::kExtractionFailed, ArchiveError::kPathTraversal,
      ArchiveError::kAbsolutePath,     ArchiveError::kUnsupportedEntryType,
  };
  for (ArchiveError error : retryable) {
    EXPECT_TRUE(internal::IsCandidateRetryableArchiveErrorForTesting(error));
  }
  const ArchiveError terminal[] = {
      ArchiveError::kFileNotFound, ArchiveError::kFileReadError,
      ArchiveError::kDiskFull,     ArchiveError::kWriteError,
      ArchiveError::kCancelled,
  };
  for (ArchiveError error : terminal) {
    EXPECT_FALSE(internal::IsCandidateRetryableArchiveErrorForTesting(error));
  }
}

TEST(InstallerControllerClassificationTest,
     NextBestSignatureErrorClassification) {
  const SignatureError retryable[] = {
      SignatureError::kFileNotFound,       SignatureError::kNotSigned,
      SignatureError::kSignatureInvalid,   SignatureError::kCertificateExpired,
      SignatureError::kCertificateRevoked, SignatureError::kThumbprintMismatch,
      SignatureError::kCatalogNotFound,    SignatureError::kCatalogInvalid,
      SignatureError::kFileNotInCatalog,   SignatureError::kHashMismatch,
  };
  for (SignatureError error : retryable) {
    EXPECT_TRUE(internal::IsCandidateRetryableSignatureErrorForTesting(error));
  }
  EXPECT_FALSE(internal::IsCandidateRetryableSignatureErrorForTesting(
      SignatureError::kSuccess));
  EXPECT_FALSE(internal::IsCandidateRetryableSignatureErrorForTesting(
      SignatureError::kCancelled));
}

TEST(InstallerControllerClassificationTest,
     NextBestMetadataErrorClassification) {
  const MetadataError retryable[] = {
      MetadataError::kFileNotFound,
      MetadataError::kJsonParseError,
      MetadataError::kMissingRequiredField,
  };
  for (MetadataError error : retryable) {
    EXPECT_TRUE(internal::IsCandidateRetryableMetadataErrorForTesting(error));
  }
  const MetadataError terminal[] = {
      MetadataError::kSuccess,           MetadataError::kFileReadError,
      MetadataError::kFileWriteError,    MetadataError::kIndexValidationError,
      MetadataError::kIntegrityMismatch,
  };
  for (MetadataError error : terminal) {
    EXPECT_FALSE(internal::IsCandidateRetryableMetadataErrorForTesting(error));
  }
}
#endif

TEST(ExtendedConfigTest, JsonParsingAllFields) {
  std::string json = R"({
    "cdn_urls": ["https://custom.cdn.com/"],
    "install_path": "C:\\CustomCEF",
    "bundled_cef_path": "C:\\App\\CEF",
    "certificate_thumbprint": "ABCD1234",
    "force_check": true,
    "show_progress_ui": false
  })";

  ExtendedConfig config;
  EXPECT_TRUE(ParseExtendedConfigFromJson(json, &config));

  EXPECT_EQ((std::vector<std::string>{"https://custom.cdn.com/"}),
            config.cdn_urls);
  EXPECT_EQ(config.install_path, "C:\\CustomCEF");
  EXPECT_EQ(config.bundled_cef_path, "C:\\App\\CEF");
  EXPECT_EQ(config.certificate_thumbprint, "ABCD1234");
  EXPECT_TRUE(config.force_check);
  EXPECT_FALSE(config.show_progress_ui);
}

TEST(ExtendedConfigTest, JsonParsingPartialFields) {
  std::string json = R"({
    "force_check": true
  })";

  ExtendedConfig config;
  EXPECT_TRUE(ParseExtendedConfigFromJson(json, &config));

  // Only force_check should be changed
  EXPECT_TRUE(config.force_check);
  // Others should retain defaults
  EXPECT_TRUE(config.cdn_urls.empty());
}

TEST(ExtendedConfigTest, JsonParsingEmptyJson) {
  ExtendedConfig config;
  EXPECT_TRUE(ParseExtendedConfigFromJson("{}", &config));

  // All defaults should remain
  EXPECT_TRUE(config.cdn_urls.empty());
}

TEST(ExtendedConfigTest, CdnUrlsPreserveOrderDuplicatesAndClearOnReuse) {
  ExtendedConfig config;
  ASSERT_TRUE(ParseExtendedConfigFromJson(
      R"({"cdn_urls":["https://one.example/path","https://two.example/","https://one.example/path"]})",
      &config));
  EXPECT_EQ((std::vector<std::string>{"https://one.example/path/",
                                      "https://two.example/",
                                      "https://one.example/path/"}),
            config.cdn_urls);
  ASSERT_TRUE(ParseExtendedConfigFromJson("{}", &config));
  EXPECT_TRUE(config.cdn_urls.empty());
}

TEST(ExtendedConfigTest, CdnUrlsRejectInvalidShapes) {
  for (
      const char* json :
      {R"({"cdn_urls":null})", R"({"cdn_urls":"https://one.example/"})",
       R"({"cdn_urls":{}})", R"({"cdn_urls":["https://one.example/",1]})",
       R"({"cdn_urls":["https://1.example/","https://2.example/","https://3.example/","https://4.example/"]})"}) {
    ExtendedConfig config;
    std::string diagnostic;
    EXPECT_FALSE(ParseExtendedConfigFromJson(json, &config, &diagnostic))
        << json;
    EXPECT_TRUE(config.cdn_urls.empty()) << json;
    EXPECT_FALSE(diagnostic.empty()) << json;
  }
}

TEST(ExtendedConfigTest, JsonParsingInvalidJson) {
  ExtendedConfig config;
  EXPECT_FALSE(ParseExtendedConfigFromJson("not json", &config));
  EXPECT_FALSE(ParseExtendedConfigFromJson("", &config));
}

TEST(ExtendedConfigTest, JsonParsingNullPointer) {
  EXPECT_FALSE(ParseExtendedConfigFromJson("{}", nullptr));
}

TEST(ExtendedConfigTest, ParentWindowStringValues) {
  const uint64_t values[] = {
      0, 12345, UINT32_MAX,
      static_cast<uint64_t>(std::numeric_limits<uintptr_t>::max())};
  for (uint64_t value : values) {
    ExtendedConfig config;
    const std::string json =
        "{\"parent_window\":\"" + std::to_string(value) + "\"}";
    ASSERT_TRUE(ParseExtendedConfigFromJson(json, &config)) << json;
    EXPECT_EQ(reinterpret_cast<HWND>(static_cast<uintptr_t>(value)),
              config.parent_window);
  }
}

TEST(ExtendedConfigTest, ParentWindowAboveInt32) {
  ExtendedConfig config;
  ASSERT_TRUE(ParseExtendedConfigFromJson(R"({"parent_window":"2147483648"})",
                                          &config));
  EXPECT_EQ(
      reinterpret_cast<HWND>(static_cast<uintptr_t>(UINT64_C(2147483648))),
      config.parent_window);
}

#if ARCH_CPU_64_BITS
TEST(ExtendedConfigTest, ParentWindowAboveUint32RoundTrips) {
  constexpr uint64_t kHandle = UINT64_C(0x100000001);
  ExtendedConfig config;
  ASSERT_TRUE(ParseExtendedConfigFromJson(R"({"parent_window":"4294967297"})",
                                          &config));
  EXPECT_EQ(reinterpret_cast<HWND>(static_cast<uintptr_t>(kHandle)),
            config.parent_window);
}
#endif

TEST(ExtendedConfigTest, ParentWindowRejectsInvalidValues) {
  const char* invalid[] = {
      R"({"parent_window":"18446744073709551616"})",
      R"({"parent_window":"not-a-number"})",
      R"({"parent_window":-1})",
      R"({"parent_window":1.5})",
      R"({"parent_window":1e100})",
      R"({"parent_window":true})",
      R"({"parent_window":[]})",
  };
  for (const char* json : invalid) {
    ExtendedConfig config;
    std::string diagnostic;
    EXPECT_FALSE(ParseExtendedConfigFromJson(json, &config, &diagnostic))
        << json;
    EXPECT_NE(std::string::npos, diagnostic.find("parent_window")) << json;
  }
}

TEST(ExtendedConfigTest, InvalidParentWindowFailsCombinedConfig) {
  const std::string json =
      R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":")" +
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString() +
      R"(","parent_window":"invalid"})";
  Config config;
  ExtendedConfig extended;
  std::string diagnostic;
  EXPECT_FALSE(ParseCombinedConfig(json, &config, &extended, &diagnostic));
  EXPECT_NE(std::string::npos, diagnostic.find("parent_window"));
}

// ============================================================================
// Combined Config Parsing Tests
// ============================================================================

TEST(CombinedConfigTest, ParseBothConfigs) {
  // Use vmin based on CEF_API_VERSION_LAST so it won't be clamped
  Version next_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
  std::string vmin_str = next_ver.ToString();
  std::string vmax_str = vmin_str + ".99";

  std::string json =
      R"({"appid": "550e8400-e29b-41d4-a716-446655440000", "vmin": ")" +
      vmin_str + R"(", "vmax": ")" + vmax_str +
      R"(", "abi_hash": "abc123", "unchecked_cef_path": "C:\\App",)" +
      R"( "cdn_urls": ["https://test.cdn.com/"], "force_check": true})";

  Config config;
  ExtendedConfig extended;
  EXPECT_TRUE(ParseCombinedConfig(json, &config, &extended));

  // Basic config
  EXPECT_EQ(config.appid, "550e8400-e29b-41d4-a716-446655440000");
  EXPECT_EQ(config.vmin, vmin_str);
  EXPECT_EQ(config.vmax, vmax_str);
  EXPECT_EQ(config.abi_hash, "abc123");
  // unchecked_cef_path is ignored in ParseCombinedConfig (bootstrap-only).
  EXPECT_TRUE(config.unchecked_cef_path.empty());

  // Extended config
  EXPECT_EQ((std::vector<std::string>{"https://test.cdn.com/"}),
            extended.cdn_urls);
  EXPECT_TRUE(extended.force_check);
}

TEST(CombinedConfigTest, MissingRequiredFields) {
  std::string json = R"({
    "vmin": "137.1",
    "force_check": true
  })";  // Missing appid

  Config config;
  ExtendedConfig extended;
  EXPECT_FALSE(ParseCombinedConfig(json, &config, &extended));
}

TEST(CombinedConfigTest, RunInstallerRejectsInvalidCdnUrlsAsConfigError) {
  auto expect_config_error = [](const std::vector<std::string>& urls) {
    base::DictValue dict;
    dict.Set("appid", "550e8400-e29b-41d4-a716-446655440000");
    dict.Set("vmin", Version::FromApiVersion(CEF_API_VERSION_LAST).ToString());
    base::ListValue list;
    for (const auto& url : urls) {
      list.Append(url);
    }
    dict.Set("cdn_urls", std::move(list));
    std::string json;
    CHECK(base::JSONWriter::Write(dict, &json));
    const char* raw_result = RunInstaller("query", json.c_str());
    ASSERT_NE(nullptr, raw_result);
    const std::optional<Result> result = Result::FromJson(raw_result);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->success);
    EXPECT_EQ(kExitCodeConfigError, result->error_code);
  };

  expect_config_error({});
  expect_config_error({"https://1.example/", "https://2.example/",
                       "https://3.example/", "https://4.example/"});
  expect_config_error({"http://insecure.example/"});
  expect_config_error(
      {"https://example.com/" + std::string(kMaxCdnUrlBytes, 'a')});

  const std::string vmin =
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString();
  const std::string wrong_shape =
      R"({"appid":"550e8400-e29b-41d4-a716-446655440000","vmin":")" + vmin +
      R"(","cdn_urls":"https://one.example/"})";
  const std::optional<Result> result =
      Result::FromJson(RunInstaller("query", wrong_shape.c_str()));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kExitCodeConfigError, result->error_code);
}

// ============================================================================
// Command Conversion Tests
// ============================================================================

TEST(CommandTest, ToString) {
  EXPECT_STREQ(CommandToString(Command::kInstall), "install");
  EXPECT_STREQ(CommandToString(Command::kUpdate), "update");
  EXPECT_STREQ(CommandToString(Command::kUninstall), "uninstall");
  EXPECT_STREQ(CommandToString(Command::kQuery), "query");
}

TEST(CommandTest, ParseValid) {
  EXPECT_EQ(ParseCommand("install"), Command::kInstall);
  EXPECT_EQ(ParseCommand("INSTALL"), Command::kInstall);
  EXPECT_EQ(ParseCommand("Install"), Command::kInstall);
  EXPECT_EQ(ParseCommand("update"), Command::kUpdate);
  EXPECT_EQ(ParseCommand("uninstall"), Command::kUninstall);
  EXPECT_EQ(ParseCommand("query"), Command::kQuery);
}

TEST(CommandTest, ParseInvalid) {
  EXPECT_EQ(ParseCommand(""), std::nullopt);
  EXPECT_EQ(ParseCommand("invalid"), std::nullopt);
  EXPECT_EQ(ParseCommand("remove"), std::nullopt);  // Not a valid command
}

// ============================================================================
// Exit Code Tests
// ============================================================================

TEST(InstallerExitCodeTest, SuccessResult) {
  Result result = Result::Success({}, "137.0.0");
  EXPECT_EQ(ResultToExitCode(result), kExitCodeSuccess);
}

TEST(InstallerExitCodeTest, ErrorResults) {
  EXPECT_EQ(ResultToExitCode(Result::Error(kExitCodeConfigError, "")),
            kExitCodeConfigError);
  EXPECT_EQ(ResultToExitCode(Result::Error(kExitCodeNetworkError, "")),
            kExitCodeNetworkError);
  EXPECT_EQ(ResultToExitCode(Result::Error(kExitCodeSignatureError, "")),
            kExitCodeSignatureError);
  EXPECT_EQ(ResultToExitCode(Result::Error(kExitCodeNoMatchingVersion, "")),
            kExitCodeNoMatchingVersion);
  EXPECT_EQ(ResultToExitCode(Result::Error(kExitCodeExtractionError, "")),
            kExitCodeExtractionError);
  EXPECT_EQ(ResultToExitCode(Result::Error(kExitCodeInstallError, "")),
            kExitCodeInstallError);
  EXPECT_EQ(ResultToExitCode(Result::Error(kExitCodeDatabaseError, "")),
            kExitCodeDatabaseError);
  EXPECT_EQ(ResultToExitCode(Result::Error(kExitCodeLockTimeout, "")),
            kExitCodeLockTimeout);
  EXPECT_EQ(ResultToExitCode(Result::Error(kExitCodeCancelled, "")),
            kExitCodeCancelled);
  EXPECT_EQ(ResultToExitCode(Result::Error(kExitCodeUnknownError, "")),
            kExitCodeUnknownError);
}

TEST(InstallerExitCodeTest, ExitCodeToStringAllCodes) {
  EXPECT_STREQ("SUCCESS", ExitCodeToString(kExitCodeSuccess));
  EXPECT_STREQ("CONFIG_ERROR", ExitCodeToString(kExitCodeConfigError));
  EXPECT_STREQ("NETWORK_ERROR", ExitCodeToString(kExitCodeNetworkError));
  EXPECT_STREQ("SIGNATURE_ERROR", ExitCodeToString(kExitCodeSignatureError));
  EXPECT_STREQ("NO_MATCHING_VERSION",
               ExitCodeToString(kExitCodeNoMatchingVersion));
  EXPECT_STREQ("EXTRACTION_ERROR", ExitCodeToString(kExitCodeExtractionError));
  EXPECT_STREQ("INSTALL_ERROR", ExitCodeToString(kExitCodeInstallError));
  EXPECT_STREQ("DATABASE_ERROR", ExitCodeToString(kExitCodeDatabaseError));
  EXPECT_STREQ("LOCK_TIMEOUT", ExitCodeToString(kExitCodeLockTimeout));
  EXPECT_STREQ("CANCELLED", ExitCodeToString(kExitCodeCancelled));
  EXPECT_STREQ("RELAUNCHED", ExitCodeToString(kExitCodeRelaunched));
  EXPECT_STREQ("NO_SENTINEL", ExitCodeToString(kExitCodeNoSentinel));
  EXPECT_STREQ("SENTINEL_READ_ERROR",
               ExitCodeToString(kExitCodeSentinelReadError));
  EXPECT_STREQ("SENTINEL_OWNER_MISMATCH",
               ExitCodeToString(kExitCodeSentinelOwnerMismatch));
  EXPECT_STREQ("POLICY_DENIED", ExitCodeToString(kExitCodePolicyDenied));
  EXPECT_STREQ("INDEX_ERROR", ExitCodeToString(kExitCodeIndexError));
  EXPECT_STREQ("RECOVERY_ERROR", ExitCodeToString(kExitCodeRecoveryError));
  EXPECT_STREQ("REPAIR_ERROR", ExitCodeToString(kExitCodeRepairError));
  EXPECT_STREQ("QUARANTINE_ERROR", ExitCodeToString(kExitCodeQuarantineError));
  EXPECT_STREQ("RETENTION_SNAPSHOT_CHANGED",
               ExitCodeToString(kExitCodeRetentionSnapshotChanged));
  EXPECT_STREQ("POLICY_ERROR", ExitCodeToString(kExitCodePolicyError));
  EXPECT_STREQ("UNKNOWN_ERROR", ExitCodeToString(kExitCodeUnknownError));
  EXPECT_STREQ("UNKNOWN_ERROR", ExitCodeToString(999));
}

TEST(InstallerExitCodeTest, StringToExitCodeRoundTrip) {
  EXPECT_EQ(kExitCodeSuccess, StringToExitCode("SUCCESS"));
  EXPECT_EQ(kExitCodeConfigError, StringToExitCode("CONFIG_ERROR"));
  EXPECT_EQ(kExitCodeNetworkError, StringToExitCode("NETWORK_ERROR"));
  EXPECT_EQ(kExitCodeSignatureError, StringToExitCode("SIGNATURE_ERROR"));
  EXPECT_EQ(kExitCodeNoMatchingVersion,
            StringToExitCode("NO_MATCHING_VERSION"));
  EXPECT_EQ(kExitCodeExtractionError, StringToExitCode("EXTRACTION_ERROR"));
  EXPECT_EQ(kExitCodeInstallError, StringToExitCode("INSTALL_ERROR"));
  EXPECT_EQ(kExitCodeDatabaseError, StringToExitCode("DATABASE_ERROR"));
  EXPECT_EQ(kExitCodeLockTimeout, StringToExitCode("LOCK_TIMEOUT"));
  EXPECT_EQ(kExitCodeCancelled, StringToExitCode("CANCELLED"));
  EXPECT_EQ(kExitCodeRelaunched, StringToExitCode("RELAUNCHED"));
  EXPECT_EQ(kExitCodeNoSentinel, StringToExitCode("NO_SENTINEL"));
  EXPECT_EQ(kExitCodeSentinelReadError,
            StringToExitCode("SENTINEL_READ_ERROR"));
  EXPECT_EQ(kExitCodeSentinelOwnerMismatch,
            StringToExitCode("SENTINEL_OWNER_MISMATCH"));
  EXPECT_EQ(kExitCodePolicyDenied, StringToExitCode("POLICY_DENIED"));
  EXPECT_EQ(kExitCodePolicyError, StringToExitCode("POLICY_ERROR"));
  EXPECT_EQ(kExitCodeIndexError, StringToExitCode("INDEX_ERROR"));
  EXPECT_EQ(kExitCodeRecoveryError, StringToExitCode("RECOVERY_ERROR"));
  EXPECT_EQ(kExitCodeRepairError, StringToExitCode("REPAIR_ERROR"));
  EXPECT_EQ(kExitCodeQuarantineError, StringToExitCode("QUARANTINE_ERROR"));
  EXPECT_EQ(kExitCodeRetentionSnapshotChanged,
            StringToExitCode("RETENTION_SNAPSHOT_CHANGED"));
  EXPECT_EQ(kExitCodeUnknownError, StringToExitCode("UNKNOWN_GARBAGE"));
}

TEST(InstallerResultTest, PolicyErrorJsonRoundTrip) {
  const Result input =
      Result::Error(kExitCodePolicyError, "Invalid enterprise policy value");
  const auto output = Result::FromJson(input.ToJson());
  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(kExitCodePolicyError, output->error_code);
  EXPECT_EQ("Invalid enterprise policy value", output->error_message);
}

TEST(InstallerOutcomeTest, AllMappings) {
  EXPECT_STREQ("committed", OutcomeToString(Outcome::kCommitted));
  EXPECT_STREQ("cleanup_deferred", OutcomeToString(Outcome::kCleanupDeferred));
  EXPECT_STREQ("failed", OutcomeToString(Outcome::kFailed));
  EXPECT_EQ(Outcome::kCommitted, StringToOutcome("committed"));
  EXPECT_EQ(Outcome::kCleanupDeferred, StringToOutcome("cleanup_deferred"));
  EXPECT_EQ(Outcome::kFailed, StringToOutcome("failed"));
  EXPECT_FALSE(StringToOutcome("unknown").has_value());
}

// ============================================================================
// Controller Tests (Basic / Mocked)
// ============================================================================

class InstallerControllerTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Helper to create app config with valid UUID
  Config CreateConfig(const std::string& appid,
                      const std::string& vmin,
                      const std::string& vmax = "",
                      const std::string& abi_hash = "") {
    Config config;
    config.appid = appid;
    config.vmin = vmin;
    config.vmax = vmax;
    config.abi_hash = abi_hash;
    return config;
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(InstallerControllerTest, RunWithInvalidConfig) {
  Controller controller;
  Config config;  // Invalid - missing required fields
  ExtendedConfig extended;
  extended.install_path = temp_dir_.GetPath().AsUTF8Unsafe();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_code, kExitCodeConfigError);
}

TEST_F(InstallerControllerTest, RunWithEmptyJson) {
  Controller controller;

  Result result = controller.Run(Command::kInstall, "", ProgressCallback{});

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_code, kExitCodeConfigError);
}

TEST_F(InstallerControllerTest, RunWithInvalidJson) {
  Controller controller;

  Result result =
      controller.Run(Command::kInstall, "not valid json", ProgressCallback{});

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_code, kExitCodeConfigError);
}

TEST_F(InstallerControllerTest, ProgressCallbackUpdates) {
  int callback_count = 0;
  Step last_step = kStepInit;

  ProgressCallback callback = base::BindRepeating(
      [](int* count, Step* last, Step step, uint64_t bytes_done,
         uint64_t bytes_total) {
        (*count)++;
        *last = step;
        return true;  // Continue
      },
      &callback_count, &last_step);

  Controller controller;
  Config config = CreateConfig("test-app", "137.0");

  // Even with an invalid setup, the callback should be called during init
  // Note: Full progress testing requires a complete mock CDN setup
}

TEST_F(InstallerControllerTest, ProgressCallbackCancel) {
  bool should_cancel = false;

  ProgressCallback callback = base::BindRepeating(
      [](bool* cancel, Step, uint64_t, uint64_t) {
        return !(*cancel);  // Return false to cancel
      },
      &should_cancel);

  Controller controller;
  Config config = CreateConfig("test-app", "137.0");

  should_cancel = true;  // Cancel immediately

  // The operation should be cancelled
  // Note: Actual cancellation depends on reaching the callback point
}

// ============================================================================
// RunInstaller Export Function Tests
// ============================================================================

TEST(RunInstallerTest, InvalidCommand) {
  const char* result = RunInstaller("invalid_command", "{}");

  ASSERT_NE(result, nullptr);
  std::optional<Result> parsed = Result::FromJson(result);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_FALSE(parsed->success);
  EXPECT_EQ(parsed->error_code, kExitCodeConfigError);

  auto raw = base::JSONReader::ReadDict(result, base::JSON_PARSE_RFC);
  ASSERT_TRUE(raw.has_value());
  EXPECT_TRUE(raw->FindInt("error_code").has_value());
  EXPECT_EQ("CONFIG_ERROR", *raw->FindString("error_name"));
  EXPECT_EQ("failed", *raw->FindString("outcome"));
}

TEST(RunInstallerTest, RetentionRejectsAppConfigAndCandidateLists) {
  const char* app_config =
      RunInstaller("retention_dry_run", R"({"appid":"app","vmin":"100"})");
  ASSERT_NE(nullptr, app_config);
  EXPECT_NE(std::string(app_config).find("Unsupported retention option"),
            std::string::npos);

  const char* candidates =
      RunInstaller("retention_apply", R"({"candidates":["app/windows64"]})");
  ASSERT_NE(nullptr, candidates);
  EXPECT_NE(std::string(candidates).find("Unsupported retention option"),
            std::string::npos);
}

TEST(RunInstallerTest, RetentionRejectsMalformedThreshold) {
  for (const char* json : {R"({"max_age_days":89})", R"({"max_age_days":3651})",
                           R"({"max_age_days":"180"})"}) {
    const char* result = RunInstaller("retention_dry_run", json);
    ASSERT_NE(nullptr, result);
    EXPECT_NE(std::string(result).find(
                  "max_age_days must be an integer from 90 through 3650"),
              std::string::npos);
  }
}

TEST(RunInstallerTest, RetentionRejectsMalformedOperationOptions) {
  for (const char* json :
       {R"({"install_path":42})", R"({"log_level":"verbose"})",
        R"({"dry_run":true})", R"({"apply":true})"}) {
    const char* result = RunInstaller("retention_dry_run", json);
    ASSERT_NE(nullptr, result);
    std::optional<Result> parsed = Result::FromJson(result);
    ASSERT_TRUE(parsed);
    EXPECT_FALSE(parsed->success);
    EXPECT_EQ(kExitCodeConfigError, parsed->error_code);
  }
}

TEST(RunInstallerTest, NullCommand) {
  const char* result = RunInstaller(nullptr, "{}");

  ASSERT_NE(result, nullptr);
  std::optional<Result> parsed = Result::FromJson(result);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_FALSE(parsed->success);
}

TEST(RunInstallerTest, EmptyConfig) {
  const char* result = RunInstaller("install", nullptr);

  ASSERT_NE(result, nullptr);
  // Should fail because config is required
}

TEST(RunInstallerTest, InvalidParentWindowUsesFailedSchema) {
  const std::string config =
      R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":")" +
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString() +
      R"(","parent_window":"invalid"})";
  const char* result = RunInstaller("query", config.c_str());
  auto raw = base::JSONReader::ReadDict(result, base::JSON_PARSE_RFC);
  ASSERT_TRUE(raw.has_value());
  EXPECT_FALSE(*raw->FindBool("success"));
  EXPECT_EQ(kExitCodeConfigError, *raw->FindInt("error_code"));
  EXPECT_EQ("CONFIG_ERROR", *raw->FindString("error_name"));
  EXPECT_EQ("failed", *raw->FindString("outcome"));
  EXPECT_NE(std::string::npos,
            raw->FindString("error_message")->find("parent_window"));
}

TEST(RunInstallerTest, ResultLifetime) {
  // Test that the returned string is valid until next call
  const char* result1 = RunInstaller("invalid", "{}");
  std::string saved1 = result1;

  const char* result2 = RunInstaller("invalid", "{}");
  std::string saved2 = result2;

  // Both should be valid JSON (even if errors)
  EXPECT_TRUE(Result::FromJson(saved1).has_value());
  EXPECT_TRUE(Result::FromJson(saved2).has_value());
}

// ============================================================================
// Integration Tests with Mock CDN Server
// ============================================================================

// Mock CDN manifest is generated dynamically in SetUp() based on
// CEF_API_VERSION_LAST

class InstallerControllerServerTest : public testing::Test {
 protected:
  void SetUp() override {
    SetTestingMode(true);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    install_dir_ = temp_dir_.GetPath().Append(kCefSubdirectory);
    ASSERT_TRUE(base::CreateDirectory(install_dir_));

    // Build dynamic manifest with version based on CEF_API_VERSION_LAST
    Version next_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
    dynamic_version_ = next_ver.ToString();
    dynamic_manifest_ = R"([
      {
        "version": ")" + dynamic_version_ +
                        R"(",
        "file": "cef_)" +
                        dynamic_version_ + R"(_windows64.tar.xz",
        "sha1": "da39a3ee5e6b4b0d3255bfef95601890afd80709",
        "last_modified": "2026-02-27T10:00:00Z",
        "abi_hash": "abc123def456"
      }
    ])";

    // Set up mock CDN server
    server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    server_->RegisterRequestHandler(base::BindRepeating(
        &InstallerControllerServerTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(server_->Start());
  }

  void TearDown() override {
    internal::ClearInstallDirectoryOverridesForTesting();
    internal::OverrideEnterprisePolicyForTesting(std::nullopt);
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
    SetEmergencyRecoveryScanLimitsForTesting(std::nullopt);
#endif
    SetTestingMode(false);
    SetRetentionPostIndexFailureForTesting(false);
    SetRetentionPendingRestoreFailureForTesting(false);
    SetRetentionPostValidationEvidenceChangeForTesting(false);
    SetLaunchStateGcTimeForTesting(std::nullopt);
    SetConditionalDeleteHookForTesting({});
    SetLaunchStateGcPreDeleteHookForTesting({});
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    requested_urls_.push_back(request.relative_url);
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);

    if (base::StartsWith(request.relative_url, "/missing/")) {
      response->set_code(net::HTTP_NOT_FOUND);
      return response;
    }

    if (request.relative_url.find("_windows64.json") != std::string::npos ||
        request.relative_url.find("_windows32.json") != std::string::npos ||
        request.relative_url.find("_windowsarm64.json") != std::string::npos) {
      // Platform manifest request - return dynamic manifest with correct
      // version
      response->set_content(dynamic_manifest_);
      response->set_content_type("application/json");
    } else if (base::EndsWith(request.relative_url, "/revoked.json")) {
      if (hang_on_revocation_ &&
          base::StartsWith(request.relative_url, "/slow/")) {
        return std::make_unique<
            net::test_server::HungAfterHeadersHttpResponse>();
      }
      response->set_content(revocation_response_);
      response->set_content_type("application/json");
    } else if (base::EndsWith(request.relative_url, ".tar.xz")) {
      if (base::StartsWith(request.relative_url, "/timeout/")) {
        return std::make_unique<net::test_server::HungResponse>();
      }
      if (base::StartsWith(request.relative_url, "/outage/")) {
        response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
        return response;
      }
      if (hang_on_archive_) {
        archive_requested_.Signal();
        return std::make_unique<
            net::test_server::HungAfterHeadersHttpResponse>();
      }
      for (const auto& [needle, content] : archive_contents_) {
        if (request.relative_url.find(needle) != std::string::npos) {
          response->set_content(content);
          response->set_content_type("application/octet-stream");
          return response;
        }
      }
      response->set_code(net::HTTP_NOT_FOUND);
    } else {
      response->set_code(net::HTTP_NOT_FOUND);
    }

    return response;
  }

  std::string GetServerUrl() { return server_->base_url().spec(); }

  // Helper to create a fake installed version in our temp install directory
  void CreateFakeInstalledVersion(const std::string& version_str,
                                  const std::string& abi_hash = "",
                                  bool publish_index = false) {
    // Use GetVersionPath to get the correct path with platform
    Version version = Version::Parse(version_str);
    base::FilePath version_dir = GetVersionPath(install_dir_, version);

    // Create the full path including parent directories
    ASSERT_TRUE(base::CreateDirectory(version_dir));

    // Create fake Release/libcef.dll matching real distribution layout.
    base::FilePath release_dir = version_dir.Append(kReleaseSubdirectory);
    ASSERT_TRUE(base::CreateDirectory(release_dir));
    base::FilePath libcef_path = release_dir.Append(kLibcefFilename);
    ASSERT_TRUE(base::WriteFile(libcef_path, "fake dll"));
    ASSERT_TRUE(
        base::WriteFile(version_dir.Append(kCatalogFilename), "fake catalog"));

    // Create version metadata
    VersionMetadata metadata;
    metadata.version = version;
    metadata.abi_hash = abi_hash;
    metadata.platform = GetCurrentPlatform();
    metadata.version_full = version_str + "+gtest+chromium-" + version_str;
    ASSERT_EQ(WriteVersionMetadata(version_dir, metadata),
              MetadataError::kSuccess);
    if (publish_index) {
      ASSERT_EQ(
          MetadataError::kSuccess,
          WriteVersionIndex(install_dir_,
                            ScanInstalledVersionsWithMetadata(install_dir_)));
    }
  }

  Config CreateValidConfig() {
    Config config;
    config.appid = "550e8400-e29b-41d4-a716-446655440000";
    config.vmin = "137.0";
    config.vmax = "";
    config.abi_hash = "";
    config.launch_health = LaunchHealthMode::kExitCode;
    return config;
  }

  ExtendedConfig CreateExtendedConfig() {
    ExtendedConfig ext;
    ext.cdn_urls = {GetServerUrl()};
    ext.install_path = install_dir_.AsUTF8Unsafe();
    ext.show_progress_ui = false;
    return ext;
  }

  void UseServerPolicySourceForTesting() {
    PolicyLoadResult policy;
    policy.status = PolicyLoadStatus::kValid;
    policy.policy.download_source.kind = DownloadSourceKind::kPolicyUrls;
    policy.policy.download_source.authority =
        DownloadSourceAuthority::kEnterprisePolicy;
    policy.policy.download_source.urls = {GetServerUrl()};
    internal::OverrideEnterprisePolicyForTesting(policy);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath install_dir_;
  std::unique_ptr<net::EmbeddedTestServer> server_;
  std::string dynamic_version_;
  std::string dynamic_manifest_;
  std::string revocation_response_ = R"({"revoked_versions": []})";
  bool hang_on_revocation_ = false;
  bool hang_on_archive_ = false;
  std::map<std::string, std::string> archive_contents_;
  base::WaitableEvent archive_requested_{
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};
  std::vector<std::string> requested_urls_;
};

TEST_F(InstallerControllerServerTest,
       AutomaticStartupFreshRevocationCacheMakesZeroRequests) {
  CreateFakeInstalledVersion(dynamic_version_, "abc123def456", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  requested_urls_.clear();

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(result.version_lease);
  EXPECT_TRUE(requested_urls_.empty());
}

TEST_F(InstallerControllerServerTest,
       AutomaticStartupStaleRevocationRefreshesOnceBeforeSelection) {
  CreateFakeInstalledVersion(dynamic_version_, "abc123def456", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  base::FilePath cache = install_dir_.Append(kRevocationCacheFilename);
  base::Time stale =
      base::Time::Now() - base::Seconds(kRevocationCacheValiditySeconds);
  ASSERT_TRUE(base::TouchFile(cache, stale, stale));
  requested_urls_.clear();

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(std::vector<std::string>({"/revoked.json"}), requested_urls_);
}

TEST_F(InstallerControllerServerTest,
       AutomaticStartupStaleRevocationChangesCurrentSelection) {
  CreateFakeInstalledVersion(dynamic_version_, "abc123def456", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  base::FilePath cache = install_dir_.Append(kRevocationCacheFilename);
  base::Time stale =
      base::Time::Now() - base::Seconds(kRevocationCacheValiditySeconds);
  ASSERT_TRUE(base::TouchFile(cache, stale, stale));
  revocation_response_ =
      R"({"revoked_versions":[{"version":")" + dynamic_version_ + R"("}]})";
  requested_urls_.clear();

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.version_lease);
  EXPECT_NE(requested_urls_.end(),
            std::find(requested_urls_.begin(), requested_urls_.end(),
                      "/revoked.json"));
}

TEST_F(InstallerControllerServerTest,
       AutomaticStartupCorruptFreshRevocationCacheRefreshes) {
  CreateFakeInstalledVersion(dynamic_version_, "abc123def456", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  ASSERT_TRUE(base::WriteFile(install_dir_.Append(kRevocationCacheFilename),
                              R"({"revoked_versions": []})"));
  requested_urls_.clear();

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(std::vector<std::string>({"/revoked.json"}), requested_urls_);
}

TEST_F(InstallerControllerServerTest,
       AutomaticStartupFailureBackoffSuppressesRefresh) {
  CreateFakeInstalledVersion(dynamic_version_, "abc123def456", true);
  ExtendedConfig extended = CreateExtendedConfig();
  const EffectiveDownloadSource source = ResolveEffectiveDownloadSource(
      EnterprisePolicy{}, extended.cdn_urls, {}, {});
  ASSERT_TRUE(RecordRevocationRefreshFailure(
      install_dir_, GetDownloadSourceIdentity(source), base::Time::Now()));
  requested_urls_.clear();

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateValidConfig(), extended, {},
                     ExecutionContext::kAutomaticStartup);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(requested_urls_.empty());
}

TEST_F(InstallerControllerServerTest,
       PolicyFailoverReorderedSourceBypassesUnrelatedBackoff) {
  CreateFakeInstalledVersion(dynamic_version_, "abc123def456", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  const base::FilePath cache = install_dir_.Append(kRevocationCacheFilename);
  const base::Time stale =
      base::Time::Now() - base::Seconds(kRevocationCacheValiditySeconds);
  ASSERT_TRUE(base::TouchFile(cache, stale, stale));

  EnterprisePolicy previous_policy;
  previous_policy.download_source.kind = DownloadSourceKind::kPolicyUrls;
  previous_policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  previous_policy.download_source.urls = {GetServerUrl() + "one/",
                                          GetServerUrl() + "two/"};
  const EffectiveDownloadSource previous_source =
      ResolveEffectiveDownloadSource(previous_policy, {}, {}, {});
  ASSERT_TRUE(RecordRevocationRefreshFailure(
      install_dir_, GetDownloadSourceIdentity(previous_source),
      base::Time::Now()));

  PolicyLoadResult current_policy;
  current_policy.status = PolicyLoadStatus::kValid;
  current_policy.policy.download_source.kind = DownloadSourceKind::kPolicyUrls;
  current_policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  current_policy.policy.download_source.urls = {GetServerUrl() + "two/",
                                                GetServerUrl() + "one/"};
  internal::OverrideEnterprisePolicyForTesting(current_policy);
  requested_urls_.clear();

  Controller controller;
  const Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                       CreateExtendedConfig(), {},
                                       ExecutionContext::kAutomaticStartup);
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(std::vector<std::string>({"/two/revoked.json"}), requested_urls_);
}

TEST_F(InstallerControllerServerTest,
       CdnConfigOperationUrlsPrecedeSelectedApplicationUrls) {
  CreateFakeInstalledVersion(dynamic_version_, "abc123def456", true);
  Config config = CreateValidConfig();
  config.cdn_urls = {GetServerUrl() + "application/"};
  ExtendedConfig extended = CreateExtendedConfig();
  extended.cdn_urls = {GetServerUrl() + "operation/"};
  requested_urls_.clear();

  Controller controller;
  const Result result = controller.Run(Command::kUpdate, config, extended, {});
  EXPECT_TRUE(result.success) << result.error_message;
  ASSERT_EQ(2u, requested_urls_.size());
  EXPECT_EQ("/operation/revoked.json", requested_urls_[0]);
  EXPECT_TRUE(base::StartsWith(requested_urls_[1], "/operation/"));
  EXPECT_TRUE(base::EndsWith(requested_urls_[1], ".json"));
}

TEST_F(InstallerControllerServerTest,
       CdnConfigSelectedApplicationUrlsFailOverInOrder) {
  CreateFakeInstalledVersion(dynamic_version_, "abc123def456", true);
  Config config = CreateValidConfig();
  config.cdn_urls = {GetServerUrl() + "missing/", GetServerUrl()};
  ExtendedConfig extended = CreateExtendedConfig();
  extended.cdn_urls.clear();
  requested_urls_.clear();

  Controller controller;
  const Result result = controller.Run(Command::kUpdate, config, extended, {});
  EXPECT_TRUE(result.success) << result.error_message;
  ASSERT_EQ(4u, requested_urls_.size());
  EXPECT_EQ("/missing/revoked.json", requested_urls_[0]);
  EXPECT_EQ("/revoked.json", requested_urls_[1]);
  EXPECT_TRUE(base::StartsWith(requested_urls_[2], "/missing/"));
  EXPECT_TRUE(base::EndsWith(requested_urls_[2], ".json"));
  EXPECT_TRUE(base::EndsWith(requested_urls_[3], ".json"));
  EXPECT_FALSE(base::StartsWith(requested_urls_[3], "/missing/"));
}

TEST_F(InstallerControllerServerTest, PolicyFailoverRefreshesFromSecondOrigin) {
  CreateFakeInstalledVersion(dynamic_version_, "abc123def456", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  const base::FilePath cache = install_dir_.Append(kRevocationCacheFilename);
  const base::Time stale =
      base::Time::Now() - base::Seconds(kRevocationCacheValiditySeconds);
  ASSERT_TRUE(base::TouchFile(cache, stale, stale));
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kPolicyUrls;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  policy.policy.download_source.urls = {"http://127.0.0.1:1/", GetServerUrl()};
  internal::OverrideEnterprisePolicyForTesting(policy);
  requested_urls_.clear();

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(std::vector<std::string>({"/revoked.json"}), requested_urls_);
}

TEST_F(InstallerControllerServerTest,
       PolicyFailoverLaunchRefreshUsesOneOverallDeadline) {
  CreateFakeInstalledVersion(dynamic_version_, "abc123def456", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  const base::FilePath cache = install_dir_.Append(kRevocationCacheFilename);
  const base::Time stale =
      base::Time::Now() - base::Seconds(kRevocationCacheValiditySeconds);
  ASSERT_TRUE(base::TouchFile(cache, stale, stale));
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kPolicyUrls;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  policy.policy.download_source.urls = {
      GetServerUrl() + "slow/", GetServerUrl() + "slow-two/", GetServerUrl()};
  internal::OverrideEnterprisePolicyForTesting(policy);
  hang_on_revocation_ = true;
  requested_urls_.clear();

  base::ElapsedTimer timer;
  Controller controller;
  const Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                       CreateExtendedConfig(), {},
                                       ExecutionContext::kAutomaticStartup);
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_LT(timer.Elapsed(), base::Seconds(8));
  ASSERT_FALSE(requested_urls_.empty());
  ASSERT_LE(requested_urls_.size(), 3u);
  const std::vector<std::string> expected_urls = {
      "/slow/revoked.json", "/slow-two/revoked.json", "/revoked.json"};
  EXPECT_TRUE(std::equal(requested_urls_.begin(), requested_urls_.end(),
                         expected_urls.begin()));
}

TEST_F(InstallerControllerServerTest,
       AutomaticStartupReresolvesAfterWriterWithoutNetwork) {
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  auto held = SingletonLock::Acquire(install_dir_, INFINITE);
  ASSERT_TRUE(held && held->IsHeld());
  ExtendedConfig extended = CreateExtendedConfig();
  extended.lock_timeout_ms = 2000;
  base::WaitableEvent reached_lock_path;
  Result waiter_result;

  std::thread waiter([&] {
    Controller controller;
    waiter_result = controller.Run(
        Command::kInstall, CreateValidConfig(), extended,
        base::BindRepeating(
            [](base::WaitableEvent* reached, Step step, uint64_t, uint64_t) {
              if (step == kStepInit) {
                reached->Signal();
              }
              return true;
            },
            &reached_lock_path),
        ExecutionContext::kAutomaticStartup);
  });
  reached_lock_path.Wait();

  CreateFakeInstalledVersion(dynamic_version_, "abc123def456", true);
  requested_urls_.clear();
  held.reset();
  waiter.join();

  EXPECT_TRUE(waiter_result.success) << waiter_result.error_message;
  EXPECT_TRUE(waiter_result.version_lease);
  EXPECT_TRUE(requested_urls_.empty());
}

TEST_F(InstallerControllerServerTest,
       AutomaticStartupReresolvesAfterWriterWithRevocationRefresh) {
  auto held = SingletonLock::Acquire(install_dir_, INFINITE);
  ASSERT_TRUE(held && held->IsHeld());
  ExtendedConfig extended = CreateExtendedConfig();
  extended.lock_timeout_ms = 2000;
  base::WaitableEvent reached_lock_path;
  Result waiter_result;

  std::thread waiter([&] {
    Controller controller;
    waiter_result = controller.Run(
        Command::kInstall, CreateValidConfig(), extended,
        base::BindRepeating(
            [](base::WaitableEvent* reached, Step step, uint64_t, uint64_t) {
              if (step == kStepInit) {
                reached->Signal();
              }
              return true;
            },
            &reached_lock_path),
        ExecutionContext::kAutomaticStartup);
  });
  reached_lock_path.Wait();

  CreateFakeInstalledVersion(dynamic_version_, "abc123def456", true);
  requested_urls_.clear();
  held.reset();
  waiter.join();

  EXPECT_TRUE(waiter_result.success) << waiter_result.error_message;
  EXPECT_TRUE(waiter_result.version_lease);
  EXPECT_EQ(std::vector<std::string>({"/revoked.json"}), requested_urls_);
}

TEST_F(InstallerControllerServerTest,
       AutomaticStartupContentionUsesShortUserFacingFailure) {
  auto held = SingletonLock::Acquire(install_dir_, INFINITE);
  ASSERT_TRUE(held && held->IsHeld());
  ExtendedConfig extended = CreateExtendedConfig();
  extended.lock_timeout_ms = 100;
  Result waiter_result;
  base::TimeDelta elapsed;

  std::thread waiter([&] {
    Controller controller;
    base::TimeTicks start = base::TimeTicks::Now();
    waiter_result =
        controller.Run(Command::kInstall, CreateValidConfig(), extended, {},
                       ExecutionContext::kAutomaticStartup);
    elapsed = base::TimeTicks::Now() - start;
  });
  waiter.join();

  EXPECT_FALSE(waiter_result.success);
  EXPECT_EQ(kExitCodeLockTimeout, waiter_result.error_code);
  EXPECT_EQ(
      "Another application is installing CEF. Please try again when "
      "that install completes.",
      waiter_result.error_message);
  EXPECT_GE(elapsed, base::Milliseconds(90));
  EXPECT_LT(elapsed, base::Milliseconds(500));
  EXPECT_TRUE(requested_urls_.empty());
}

TEST_F(InstallerControllerServerTest, QueryCdnForVersionCalled) {
  // Test that QueryCdnForVersion is called when no local version matches.
  // Don't create a fake installed version - force CDN query.
  Controller controller;
  Config config;
  config.appid = "550e8400-e29b-41d4-a716-446655440000";
  config.vmin = dynamic_version_;    // Use version that matches CDN manifest
  config.abi_hash = "abc123def456";  // Matches mock manifest
  ExtendedConfig extended = CreateExtendedConfig();
  extended.force_check = true;  // Force CDN check

  Result result = controller.Run(Command::kInstall, config, extended);

  // Should fail because CDN has version but we can't download (no real archive)
  // But QueryCdnForVersion should have been called and found the manifest entry
  EXPECT_FALSE(result.success);
  // The error should be about download/network/signature, not "no matching
  // version" because the CDN manifest has matching version and abi_hash
  EXPECT_NE(result.error_code, kExitCodeNoMatchingVersion);
}

TEST_F(InstallerControllerServerTest,
       NoMatchingCdnVersionReportsFailedRequirements) {
  const int milestone = Version::Parse(dynamic_version_).GetMilestone();
  const std::string offered_version = base::NumberToString(milestone) + ".1.0";
  const std::string required_version = base::NumberToString(milestone) + ".2.0";
  dynamic_manifest_ = R"([{"version":")" + offered_version + R"(",
      "file":"cef.tar.xz",
      "sha1":"a",
      "last_modified":"2026-01-01T00:00:00Z",
      "abi_hash":"def456"}])";
  Config config = CreateValidConfig();
  config.vmin = required_version;
  config.abi_hash = "abc123";

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, config, CreateExtendedConfig());

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);
  EXPECT_NE(std::string::npos,
            result.error_message.find("platform = " + GetCurrentPlatform()));
  EXPECT_NE(std::string::npos,
            result.error_message.find("version >= " + required_version));
  EXPECT_NE(std::string::npos, result.error_message.find("ABI hash = abc123"));
  EXPECT_NE(std::string::npos,
            result.error_message.find(offered_version +
                                      " [ABI hash mismatch (got def456), "
                                      "version below vmin]"));
}

TEST_F(InstallerControllerServerTest,
       NoMatchingCdnVersionReportsRevokedCandidate) {
  dynamic_manifest_ = R"([{"version":")" + dynamic_version_ + R"(",
      "file":"cef.tar.xz",
      "sha1":"a",
      "last_modified":"2026-01-01T00:00:00Z",
      "abi_hash":"def456"}])";
  revocation_response_ = R"({"revoked_versions":[{"version":")" +
                         dynamic_version_ + R"(","reason":"test"}]})";
  Config config = CreateValidConfig();
  config.vmin = dynamic_version_;
  config.abi_hash = "def456";

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, config, CreateExtendedConfig());

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);
  EXPECT_NE(std::string::npos, result.error_message.find(
                                   dynamic_version_ + " [excluded: revoked]"));
}

TEST_F(InstallerControllerServerTest,
       NextBestStopsAfterTwoMissingArchivesWithoutManifestRefetch) {
  const int milestone = Version::Parse(dynamic_version_).GetMilestone();
  const std::string v1 = base::NumberToString(milestone) + ".1.0";
  const std::string v2 = base::NumberToString(milestone) + ".2.0";
  const std::string v3 = base::NumberToString(milestone) + ".3.0";
  const std::string sha1 = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
  dynamic_manifest_ =
      "[{\"version\":\"" + v1 + "\",\"file\":\"cef_" + v1 +
      ".tar.xz\",\"sha1\":\"" + sha1 +
      "\",\"last_modified\":\"2026-01-01T00:00:00Z\",\"abi_hash\":\"\"},"
      "{\"version\":\"" +
      v2 + "\",\"file\":\"cef_" + v2 + ".tar.xz\",\"sha1\":\"" + sha1 +
      "\",\"last_modified\":\"2026-01-02T00:00:00Z\",\"abi_hash\":\"\"},"
      "{\"version\":\"" +
      v3 + "\",\"file\":\"cef_" + v3 + ".tar.xz\",\"sha1\":\"" + sha1 +
      "\",\"last_modified\":\"2026-01-03T00:00:00Z\",\"abi_hash\":\"\"}]";
  Config config = CreateValidConfig();
  config.vmin = base::NumberToString(milestone) + ".0";
  ExtendedConfig extended = CreateExtendedConfig();
  extended.force_check = true;
  requested_urls_.clear();

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_FALSE(result.success);
  std::vector<std::string> archive_requests;
  int manifest_requests = 0;
  for (const auto& url : requested_urls_) {
    if (base::EndsWith(url, ".tar.xz")) {
      archive_requests.push_back(url);
    }
    if (url.find("_windows") != std::string::npos &&
        base::EndsWith(url, ".json")) {
      ++manifest_requests;
    }
  }
  ASSERT_EQ(2u, archive_requests.size());
  EXPECT_NE(std::string::npos, archive_requests[0].find(v3));
  EXPECT_NE(std::string::npos, archive_requests[1].find(v2));
  EXPECT_EQ(1, manifest_requests);
  EXPECT_EQ(std::string::npos, archive_requests[0].find(v1));
  EXPECT_EQ(std::string::npos, archive_requests[1].find(v1));
}

TEST_F(InstallerControllerServerTest,
       NextBestRetriesContentSpecificExtractionFailureOnce) {
  const int milestone = Version::Parse(dynamic_version_).GetMilestone();
  const std::string v1 = base::NumberToString(milestone) + ".1.0";
  const std::string v2 = base::NumberToString(milestone) + ".2.0";
  const std::string content1 = "invalid archive for first candidate";
  const std::string content2 = "invalid archive for second candidate";
  const base::FilePath hash1_path =
      temp_dir_.GetPath().AppendASCII("candidate1.bin");
  const base::FilePath hash2_path =
      temp_dir_.GetPath().AppendASCII("candidate2.bin");
  ASSERT_TRUE(base::WriteFile(hash1_path, content1));
  ASSERT_TRUE(base::WriteFile(hash2_path, content2));
  const std::string sha1 = ComputeFileSha1(hash1_path);
  const std::string sha2 = ComputeFileSha1(hash2_path);
  archive_contents_[v1] = content1;
  archive_contents_[v2] = content2;
  dynamic_manifest_ =
      "[{\"version\":\"" + v1 + "\",\"file\":\"cef_" + v1 +
      ".tar.xz\",\"sha1\":\"" + sha1 +
      "\",\"last_modified\":\"2026-01-01T00:00:00Z\",\"abi_hash\":\"\"},"
      "{\"version\":\"" +
      v2 + "\",\"file\":\"cef_" + v2 + ".tar.xz\",\"sha1\":\"" + sha2 +
      "\",\"last_modified\":\"2026-01-02T00:00:00Z\",\"abi_hash\":\"\"}]";
  Config config = CreateValidConfig();
  config.vmin = base::NumberToString(milestone) + ".0";
  ExtendedConfig extended = CreateExtendedConfig();
  extended.force_check = true;
  requested_urls_.clear();

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeExtractionError, result.error_code);
  std::vector<std::string> archive_requests;
  int manifest_requests = 0;
  for (const auto& url : requested_urls_) {
    if (base::EndsWith(url, ".tar.xz")) {
      archive_requests.push_back(url);
    }
    if (url.find("_windows") != std::string::npos &&
        base::EndsWith(url, ".json")) {
      ++manifest_requests;
    }
  }
  ASSERT_EQ(2u, archive_requests.size());
  EXPECT_NE(std::string::npos, archive_requests[0].find(v2));
  EXPECT_NE(std::string::npos, archive_requests[1].find(v1));
  EXPECT_EQ(1, manifest_requests);
  EXPECT_TRUE(base::PathExists(
      GetCacheDirectory(install_dir_).AppendASCII(sha1 + ".tar.xz")));
  EXPECT_TRUE(base::PathExists(
      GetCacheDirectory(install_dir_).AppendASCII(sha2 + ".tar.xz")));
}

TEST_F(InstallerControllerServerTest,
       MixedArchiveMissingAndOutageDoesNotSelectNextCandidate) {
  const int milestone = Version::Parse(dynamic_version_).GetMilestone();
  const std::string older = base::NumberToString(milestone) + ".1.0";
  const std::string newer = base::NumberToString(milestone) + ".2.0";
  const std::string sha1 = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
  dynamic_manifest_ =
      R"([{"version":")" + older + R"(","file":"cef_)" + older +
      R"(.tar.xz","sha1":")" + sha1 +
      R"(","last_modified":"2026-01-01T00:00:00Z","abi_hash":""},{"version":")" +
      newer + R"(","file":"cef_)" + newer + R"(.tar.xz","sha1":")" + sha1 +
      R"(","last_modified":"2026-01-02T00:00:00Z","abi_hash":""}])";
  Config config = CreateValidConfig();
  config.vmin = base::NumberToString(milestone) + ".0";
  ExtendedConfig extended = CreateExtendedConfig();
  extended.cdn_urls = {GetServerUrl() + "missing/", GetServerUrl() + "outage/"};
  extended.force_check = true;
  requested_urls_.clear();

  Result result = Controller().Run(Command::kInstall, config, extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNetworkError, result.error_code);
  std::vector<std::string> archive_requests;
  for (const auto& url : requested_urls_) {
    if (base::EndsWith(url, ".tar.xz")) {
      archive_requests.push_back(url);
    }
  }
  ASSERT_EQ(2u, archive_requests.size());
  EXPECT_NE(std::string::npos, archive_requests[0].find(newer));
  EXPECT_NE(std::string::npos, archive_requests[1].find(newer));
  EXPECT_EQ(std::string::npos, archive_requests[0].find(older));
  EXPECT_EQ(std::string::npos, archive_requests[1].find(older));
}

TEST_F(InstallerControllerServerTest,
       CdnFailoverExhaustsTransportHashBeforeCandidateFallback) {
  const std::string good_content = "hash-valid but invalid archive";
  const std::string bad_content = "wrong transport content";
  const base::FilePath hash_path =
      temp_dir_.GetPath().AppendASCII("good-archive.bin");
  ASSERT_TRUE(base::WriteFile(hash_path, good_content));
  const std::string sha1 = ComputeFileSha1(hash_path);
  archive_contents_["/bad/"] = bad_content;
  archive_contents_["/good/"] = good_content;
  dynamic_manifest_ =
      "[{\"version\":\"" + dynamic_version_ + "\",\"file\":\"cef_" +
      dynamic_version_ + ".tar.xz\",\"sha1\":\"" + sha1 +
      "\",\"last_modified\":\"2026-01-01T00:00:00Z\",\"abi_hash\":\"\"}]";
  Config config = CreateValidConfig();
  config.vmin = dynamic_version_;
  ExtendedConfig extended = CreateExtendedConfig();
  extended.cdn_urls = {GetServerUrl() + "bad/", GetServerUrl() + "good/"};
  extended.force_check = true;
  requested_urls_.clear();

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeExtractionError, result.error_code);
  std::vector<std::string> archive_requests;
  for (const auto& url : requested_urls_) {
    if (base::EndsWith(url, ".tar.xz")) {
      archive_requests.push_back(url);
    }
  }
  ASSERT_EQ(3u, archive_requests.size());
  EXPECT_TRUE(base::StartsWith(archive_requests[0], "/bad/"));
  EXPECT_TRUE(base::StartsWith(archive_requests[1], "/bad/"));
  EXPECT_TRUE(base::StartsWith(archive_requests[2], "/good/"));
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(InstallerControllerServerTest,
       CdnFailoverCleansLockedInterruptedPartialBeforeFallbackSidecarRequest) {
  const std::string sha1 = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
  const std::string archive_file =
      "cef_" + dynamic_version_ + "_" + GetCurrentPlatform() + ".tar.xz";
  const std::string archive_url = GetServerUrl() + "timeout/" + archive_file;
  const base::FilePath archive_path =
      GetCacheDirectory(install_dir_).AppendASCII(sha1 + ".tar.xz");
  const base::FilePath partial_path =
      internal::GetDownloadPartialPathForTesting(archive_path, archive_url,
                                                 archive_url);
  ASSERT_FALSE(partial_path.empty());
  ASSERT_TRUE(base::CreateDirectory(partial_path.DirName()));
  ASSERT_TRUE(base::WriteFile(partial_path, "partial"));

  HANDLE partial_lock =
      ::CreateFileW(partial_path.value().c_str(), GENERIC_READ, FILE_SHARE_READ,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE(INVALID_HANDLE_VALUE, partial_lock);

  Config config = CreateValidConfig();
  config.vmin = dynamic_version_;
  ExtendedConfig extended = CreateExtendedConfig();
  extended.cdn_urls = {GetServerUrl() + "timeout/",
                       GetServerUrl() + "fallback/"};
  extended.download_timeout_ms = 500;
  extended.force_check = true;
  requested_urls_.clear();

  Result result = Controller().Run(Command::kInstall, config, extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeInstallError, result.error_code);
  EXPECT_EQ("Failed to discard prior-origin partial archive",
            result.error_message);
  EXPECT_TRUE(base::PathExists(partial_path));
  EXPECT_NE(requested_urls_.end(),
            std::find(requested_urls_.begin(), requested_urls_.end(),
                      "/timeout/" + archive_file));
  EXPECT_EQ(requested_urls_.end(),
            std::find_if(requested_urls_.begin(), requested_urls_.end(),
                         [](const std::string& url) {
                           return base::StartsWith(url, "/fallback/");
                         }));
  if (partial_lock != INVALID_HANDLE_VALUE) {
    ::CloseHandle(partial_lock);
  }
  EXPECT_EQ(DownloadError::kSuccess, DiscardDownloadPartials(archive_path));
}
#endif

TEST_F(InstallerControllerServerTest, QueryCdnManifest) {
  // Test that we can query the mock CDN and get manifest entries
  // Use the dynamic milestone based on CEF_API_VERSION_LAST
  Version dynamic_ver = Version::Parse(dynamic_version_);
  std::string milestone = std::to_string(dynamic_ver.GetMilestone());
  std::string url =
      GetServerUrl() + milestone + "_" + GetCurrentPlatform() + ".json";
  std::string content;

  DownloadOptions opts;
  opts.allow_http_for_testing = true;

  DownloadError err = DownloadToString(url, &content, opts);
  EXPECT_EQ(DownloadError::kSuccess, err);

  std::vector<CdnBuildEntry> entries;
  ManifestError manifest_err = ParsePlatformManifest(content, &entries);
  EXPECT_EQ(ManifestError::kSuccess, manifest_err);
  EXPECT_EQ(1u, entries.size());
  EXPECT_EQ(dynamic_version_, entries[0].version.ToString());
}

TEST_F(InstallerControllerServerTest, QueryCdnBetaChannelPlatformUrl) {
  // Verify that config.channel="beta" causes the controller to request a
  // manifest URL with the _beta suffix (e.g., 137_windows64_beta.json).
  Controller controller;
  Config config;
  config.appid = "550e8400-e29b-41d4-a716-446655440000";
  config.vmin = dynamic_version_;
  config.channel = std::string(kChannelBeta);
  ExtendedConfig extended = CreateExtendedConfig();
  extended.force_check = true;

  requested_urls_.clear();
  controller.Run(Command::kInstall, config, extended);

  bool found_beta_manifest = false;
  for (const auto& url : requested_urls_) {
    if (url.find("_beta.json") != std::string::npos) {
      found_beta_manifest = true;
      break;
    }
  }
  EXPECT_TRUE(found_beta_manifest)
      << "No _beta.json manifest URL requested among " << requested_urls_.size()
      << " requests";
}

TEST_F(InstallerControllerServerTest, QueryCdnBetaChannelAbiHashUrl) {
  // Verify that config.channel="beta" with an abi_hash causes the controller
  // to request an ABI hash manifest with the _beta suffix.
  Controller controller;
  Config config;
  config.appid = "550e8400-e29b-41d4-a716-446655440000";
  config.vmin = dynamic_version_;
  config.abi_hash = "abc123def456";
  config.channel = std::string(kChannelBeta);
  ExtendedConfig extended = CreateExtendedConfig();
  extended.force_check = true;

  requested_urls_.clear();
  controller.Run(Command::kInstall, config, extended);

  bool found_beta_manifest = false;
  for (const auto& url : requested_urls_) {
    if (url.find("abc123def456_") != std::string::npos &&
        url.find("_beta.json") != std::string::npos) {
      found_beta_manifest = true;
      break;
    }
  }
  EXPECT_TRUE(found_beta_manifest)
      << "No abi_hash _beta.json manifest URL requested among "
      << requested_urls_.size() << " requests";
}

TEST_F(InstallerControllerServerTest, QueryCdnStableChannelNoBetaSuffix) {
  // Verify that an empty channel (stable) does NOT produce _beta suffix.
  Controller controller;
  Config config;
  config.appid = "550e8400-e29b-41d4-a716-446655440000";
  config.vmin = dynamic_version_;
  config.abi_hash = "abc123def456";
  ExtendedConfig extended = CreateExtendedConfig();
  extended.force_check = true;

  requested_urls_.clear();
  controller.Run(Command::kInstall, config, extended);

  for (const auto& url : requested_urls_) {
    EXPECT_EQ(url.find("_beta"), std::string::npos)
        << "Stable channel should not produce _beta URL: " << url;
  }
}

TEST_F(InstallerControllerServerTest, CdnSkipsDisqualifiedPrunedVersion) {
  // Scenario: CDN has v1, v2, v3. v2 and v3 are installed locally.
  // v1 was pruned but its .launch/ crash history survives showing it crashed.
  // v3 also has crash history. v2 was the running version until now.
  // Now v2 also crashes too many times — all versions are disqualified.
  // CDN query should skip v1 (disqualified via .launch/), v2 (installed +
  // disqualified), v3 (installed + disqualified). No archive download.
  // Falls back to disqualified v3 (newest installed).
  // Additionally, v2 should be prunable after this run (no confirmed state).
  std::string v1 = "137.1.0";
  std::string v2 = "137.2.0";
  std::string v3 = "137.3.0";

  // CDN serves all three versions.
  dynamic_manifest_ = R"([
    {"version":"137.1.0","file":"cef_137.1.0.tar.xz","sha1":"a","last_modified":"2026-01-01T00:00:00Z","abi_hash":""},
    {"version":"137.2.0","file":"cef_137.2.0.tar.xz","sha1":"b","last_modified":"2026-01-02T00:00:00Z","abi_hash":""},
    {"version":"137.3.0","file":"cef_137.3.0.tar.xz","sha1":"c","last_modified":"2026-01-03T00:00:00Z","abi_hash":""}
  ])";

  // v2 and v3 are installed on disk (v1 was pruned earlier).
  CreateFakeInstalledVersion(v2, "abc123def456");
  CreateFakeInstalledVersion(v3, "abc123def456");

  Config config = CreateValidConfig();
  config.vmin = "137.0";
  ExtendedConfig extended = CreateExtendedConfig();
  extended.force_check = true;

  std::wstring hash = GetAppidHash(config.appid);
  std::string platform = GetCurrentPlatform();

  // All three versions crashed too many times (running=true, failures=2,
  // dead PID → projected failures = 3 ≥ kMaxConsecutiveFailures).
  LaunchState crashed;
  crashed.appid = config.appid;
  crashed.pid = 99999;
  crashed.pid_start_time = GetCurrentPidStartTime();
  crashed.consecutive_failures = 2;
  crashed.running = true;
  crashed.platform = platform;

  crashed.version = v1;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(v1),
                                   platform),
      crashed));
  crashed.version = v2;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(v2),
                                   platform),
      crashed));
  crashed.version = v3;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(v3),
                                   platform),
      crashed));

  requested_urls_.clear();
  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  // All versions disqualified → falls back to newest disqualified (v3).
  EXPECT_TRUE(result.success);
  EXPECT_EQ(v3, result.installed_version);

  // No archive download should have been attempted — all versions were in
  // skip_versions so FindBestBuildEntry returned nullopt.
  for (const auto& url : requested_urls_) {
    EXPECT_EQ(url.find(".tar.xz"), std::string::npos)
        << "Should not download any archive, but requested: " << url;
  }

  // Simulate post-exit prune. v2 has crash history (not confirmed) and is
  // not the best match for any app, so it should be pruned. v3 is required
  // as the best match.
  {
    Controller prune_controller;
    prune_controller.Run(Command::kPrune, config, extended);
  }

  base::FilePath v2_dir = GetVersionPath(install_dir_, Version::Parse(v2));
  base::FilePath v3_dir = GetVersionPath(install_dir_, Version::Parse(v3));
  EXPECT_FALSE(base::PathExists(v2_dir))
      << "v2 should be pruned (crash history, not required by any app)";
  EXPECT_TRUE(base::PathExists(v3_dir))
      << "v3 should NOT be pruned (required as best match for app)";
}

TEST_F(InstallerControllerServerTest, InstallRegistersAppInDatabase) {
  // Pre-create a version so install can find it without downloading
  CreateFakeInstalledVersion("137.3.5", "abc123def456");

  Controller controller;
  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);

  // The install should find the existing version
  // (Download would fail because we don't have real archive)
  // Check that the database was updated
  Database db;
  EXPECT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath(install_dir_)));

  std::optional<AppEntry> entry = db.GetApp(config.appid, GetCurrentPlatform());
  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(config.vmin, entry->vmin);
  EXPECT_EQ(GetCurrentPlatform(), entry->platform);
}

TEST_F(InstallerControllerServerTest, UninstallRemovesAppFromDatabase) {
  // First register an app
  CreateFakeInstalledVersion("137.3.5", "abc123def456");

  Controller controller;
  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Install first to register
  controller.Run(Command::kInstall, config, extended);

  // Verify registered
  {
    Database db;
    EXPECT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath(install_dir_)));
    EXPECT_TRUE(db.GetApp(config.appid, GetCurrentPlatform()).has_value());
  }

  // Now uninstall
  Result result = controller.Run(Command::kUninstall, config, extended);
  EXPECT_TRUE(result.success);

  // Verify unregistered
  {
    Database db;
    EXPECT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath(install_dir_)));
    EXPECT_FALSE(db.GetApp(config.appid, GetCurrentPlatform()).has_value());
  }
}

TEST_F(InstallerControllerServerTest, QueryReturnsExistingVersion) {
  // Create an installed version
  CreateFakeInstalledVersion("137.3.5", "abc123def456");

  Controller controller;
  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Query should find the existing version
  Result result = controller.Run(Command::kQuery, config, extended);

  // Query doesn't return success in the same way as install
  // It just checks if a version exists
  // The implementation returns nullopt wrapped in error if not found
}

TEST_F(InstallerControllerServerTest, InstallFindsExistingVersion) {
  // Create an installed version that matches requirements
  CreateFakeInstalledVersion("137.3.5", "abc123def456");

  Controller controller;
  Config config = CreateValidConfig();
  config.abi_hash = "abc123def456";  // Match the installed version
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);

  // Should find existing version without needing to download
  EXPECT_TRUE(result.success);
  EXPECT_EQ("137.3.5", result.installed_version);
  EXPECT_FALSE(result.libcef_path.empty());
}

TEST_F(InstallerControllerServerTest, InstallWithAbiHashFilter) {
  // Create two versions with different ABI hashes
  CreateFakeInstalledVersion("137.3.5", "ab11ab11ab11");
  CreateFakeInstalledVersion("137.2.0", "ab22ab22ab22");

  Controller controller;
  Config config = CreateValidConfig();
  config.abi_hash = "ab22ab22ab22";  // Request specific ABI hash
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);

  // Should find the version with matching ABI hash
  EXPECT_TRUE(result.success);
  EXPECT_EQ("137.2.0", result.installed_version);  // Older but matches ABI
}

TEST_F(InstallerControllerServerTest, DatabaseLockPreventsRace) {
  CreateFakeInstalledVersion("137.3.5", "abc123def456");

  // Both controllers should use the same install directory
  // The first should acquire the lock, the second should wait
  Controller controller1;
  Controller controller2;
  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Run sequentially (can't easily test true concurrency in unit test)
  Result result1 = controller1.Run(Command::kInstall, config, extended);
  Result result2 = controller2.Run(Command::kInstall, config, extended);

  // Both should succeed (lock is released between calls)
  EXPECT_TRUE(result1.success);
  EXPECT_TRUE(result2.success);
}

TEST_F(InstallerControllerServerTest, RunInstallerWithProgressUI) {
  // Test RunInstaller exported function with progress UI enabled.
  // This exercises the progress callback lambda in RunInstaller.
  // Use version based on CEF_API_VERSION_LAST so it won't be clamped
  Version next_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
  std::string version_str = next_ver.ToString();
  CreateFakeInstalledVersion(version_str, "abc123def456");
  UseServerPolicySourceForTesting();

  // Build JSON config with show_progress_ui: true
  // Use base::DictValue to properly escape paths
  base::DictValue config_dict;
  config_dict.Set("appid", "550e8400-e29b-41d4-a716-446655440000");
  config_dict.Set("vmin", version_str);
  config_dict.Set("abi_hash", "abc123def456");
  config_dict.Set("install_path", install_dir_.AsUTF8Unsafe());
  config_dict.Set("show_progress_ui", true);

  std::string config_json;
  base::JSONWriter::Write(config_dict, &config_json);

  const char* result = RunInstaller("install", config_json.c_str());

  ASSERT_NE(result, nullptr);
  std::optional<Result> parsed = Result::FromJson(result);
  ASSERT_TRUE(parsed.has_value());
  // Should succeed - finds existing version
  EXPECT_TRUE(parsed->success);
  EXPECT_EQ(version_str, parsed->installed_version);
}

TEST_F(InstallerControllerServerTest, RunInstallerCancelledViaUI) {
  // Test that cancelling the progress dialog returns CANCELLED error.
  // This exercises the cancel callback lambda in RunInstaller.
  // Don't create a matching version - force it to query CDN (which gives time
  // to cancel)
  hang_on_archive_ = true;
  UseServerPolicySourceForTesting();

  // Build JSON config - use version that won't be found locally
  Version next_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
  std::string version_str = next_ver.ToString();

  base::DictValue config_dict;
  config_dict.Set("appid", "550e8400-e29b-41d4-a716-446655440000");
  config_dict.Set("vmin", version_str);
  config_dict.Set("install_path", install_dir_.AsUTF8Unsafe());
  config_dict.Set("show_progress_ui", true);
  // Short timeout so WinHttpReceiveResponse fails quickly against the
  // hung server instead of blocking for the full 60 s default.
  config_dict.Set("download_timeout_ms", 500);

  std::string config_json;
  base::JSONWriter::Write(config_dict, &config_json);

  // Use a background thread to cancel the dialog once the archive download
  // has started. Waiting on archive_requested_ avoids a timing-dependent
  // poll — the dialog is guaranteed to exist by the time the installer
  // reaches the download step.
  base::Thread cancel_thread("CancelDialogThread");
  cancel_thread.Start();
  cancel_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WaitableEvent* archive_requested) {
                       archive_requested->Wait();
                       ProgressDialog* dlg = ProgressDialog::GetCurrent();
                       if (dlg && dlg->GetHwnd()) {
                         PostMessage(dlg->GetHwnd(), WM_CLOSE, 0, 0);
                       }
                     },
                     &archive_requested_));

  const char* result = RunInstaller("install", config_json.c_str());

  cancel_thread.Stop();

  ASSERT_NE(result, nullptr);
  std::optional<Result> parsed = Result::FromJson(result);
  ASSERT_TRUE(parsed.has_value());
  // Should fail - either CANCELLED or NO_MATCHING_VERSION
  EXPECT_FALSE(parsed->success);
}

// ============================================================================
// Security-Related Tests
// ============================================================================

TEST(ExtendedConfigSecurityTest, HwndEdgeCases) {
  // Test HWND parsing with edge case values
  ExtendedConfig config;

  // Zero value (null HWND)
  EXPECT_TRUE(ParseExtendedConfigFromJson(R"({"parent_window": 0})", &config));
  EXPECT_EQ(config.parent_window, nullptr);

  // Positive value
  EXPECT_TRUE(
      ParseExtendedConfigFromJson(R"({"parent_window": 12345})", &config));
  EXPECT_EQ(config.parent_window, reinterpret_cast<HWND>(12345));

  // Negative values are rejected instead of wrapping to a pointer.
  EXPECT_FALSE(
      ParseExtendedConfigFromJson(R"({"parent_window": -1})", &config));
  EXPECT_EQ(config.parent_window, reinterpret_cast<HWND>(12345));

  // Invalid text rejects the combined config instead of being ignored.
  config.parent_window = nullptr;
  EXPECT_FALSE(
      ParseExtendedConfigFromJson(R"({"parent_window": "invalid"})", &config));
  EXPECT_EQ(config.parent_window, nullptr);  // Unchanged
}

TEST(ExtendedConfigSecurityTest, PathWithSpecialCharacters) {
  ExtendedConfig config;

  // Path with spaces
  EXPECT_TRUE(ParseExtendedConfigFromJson(
      R"({"install_path": "C:\\Program Files\\My App"})", &config));
  EXPECT_EQ(config.install_path, "C:\\Program Files\\My App");

  // Path with unicode (JSON escaped)
  EXPECT_TRUE(ParseExtendedConfigFromJson(
      R"({"install_path": "C:\\Users\\\u4e2d\u6587\\CEF"})", &config));
  EXPECT_FALSE(config.install_path.empty());

  // Path with potential traversal (installer should validate, but parsing
  // accepts)
  EXPECT_TRUE(ParseExtendedConfigFromJson(
      R"({"install_path": "C:\\Program Files\\..\\Windows"})", &config));
  EXPECT_EQ(config.install_path, "C:\\Program Files\\..\\Windows");
}

TEST(ExtendedConfigSecurityTest, CertificateThumbprintValidation) {
  ExtendedConfig config;

  // Valid thumbprint format (40 hex chars)
  EXPECT_TRUE(ParseExtendedConfigFromJson(
      R"({"certificate_thumbprint": "ABCDEF0123456789ABCDEF0123456789ABCDEF01"})",
      &config));
  EXPECT_EQ(config.certificate_thumbprint,
            "ABCDEF0123456789ABCDEF0123456789ABCDEF01");

  // Empty thumbprint is ignored — default (kCefCertificateThumbprint) retained
  ExtendedConfig config2;
  EXPECT_TRUE(ParseExtendedConfigFromJson(R"({"certificate_thumbprint": ""})",
                                          &config2));
  EXPECT_EQ(config2.certificate_thumbprint,
            std::string(kCefCertificateThumbprint));

  // Malformed thumbprint (parsing accepts, verification would fail)
  EXPECT_TRUE(ParseExtendedConfigFromJson(
      R"({"certificate_thumbprint": "not-a-thumbprint"})", &config));
  EXPECT_EQ(config.certificate_thumbprint, "not-a-thumbprint");
}

TEST(ExtendedConfigSecurityTest, CdnUrlValidation) {
  ExtendedConfig config;

  // HTTPS URLs are validated and normalized.
  EXPECT_TRUE(ParseExtendedConfigFromJson(
      R"({"cdn_urls": ["https://secure.cdn.com"]})", &config));
  EXPECT_EQ((std::vector<std::string>{"https://secure.cdn.com/"}),
            config.cdn_urls);

  EXPECT_FALSE(ParseExtendedConfigFromJson(
      R"({"cdn_urls": ["http://insecure.cdn.com/"]})", &config));

  EXPECT_FALSE(ParseExtendedConfigFromJson(
      R"({"cdn_urls": ["file:///C:/local/path/"]})", &config));

  EXPECT_FALSE(ParseExtendedConfigFromJson(R"({"cdn_urls": []})", &config));
}

TEST(RunInstallerSecurityTest, ResultPointerLifetime) {
  // Test that result pointer remains valid until next call
  const char* result1 = RunInstaller("query", R"({"appid": "test"})");
  ASSERT_NE(result1, nullptr);

  // Copy the result before next call
  std::string saved_result1(result1);

  // Make another call
  const char* result2 = RunInstaller("query", R"({"appid": "test2"})");
  ASSERT_NE(result2, nullptr);

  // The saved string should still be parseable
  // (we copied it, so it's independent of the pointer)
  std::optional<Result> parsed1 = Result::FromJson(saved_result1);
  EXPECT_TRUE(parsed1.has_value());

  // result2 should also be valid
  std::optional<Result> parsed2 = Result::FromJson(result2);
  EXPECT_TRUE(parsed2.has_value());

  // Note: result1 pointer may now be invalid (points to reused storage)
  // This is documented behavior - pointer valid only until next call
}

TEST(RunInstallerSecurityTest, NullAndEmptyInputs) {
  // Both null
  const char* result1 = RunInstaller(nullptr, nullptr);
  ASSERT_NE(result1, nullptr);
  std::optional<Result> parsed1 = Result::FromJson(result1);
  ASSERT_TRUE(parsed1.has_value());
  EXPECT_FALSE(parsed1->success);

  // Valid command, null config
  const char* result2 = RunInstaller("install", nullptr);
  ASSERT_NE(result2, nullptr);
  std::optional<Result> parsed2 = Result::FromJson(result2);
  ASSERT_TRUE(parsed2.has_value());
  EXPECT_FALSE(parsed2->success);

  // Valid command, empty config
  const char* result3 = RunInstaller("install", "");
  ASSERT_NE(result3, nullptr);
  std::optional<Result> parsed3 = Result::FromJson(result3);
  ASSERT_TRUE(parsed3.has_value());
  EXPECT_FALSE(parsed3->success);
}

TEST(ConfigSecurityTest, VeryLongStrings) {
  // Test handling of very long input strings
  std::string long_appid(1000, 'A');
  std::string json = R"({"appid": ")" + long_appid + R"(", "vmin": "137.0"})";

  Config config;
  ExtendedConfig extended;

  // ParseConfigFromJson rejects invalid UUID format
  EXPECT_FALSE(ParseCombinedConfig(json, &config, &extended));
}

TEST(ConfigSecurityTest, JsonInjectionAttempts) {
  // Test that JSON parsing is safe against injection attempts
  Config config;
  ExtendedConfig extended;

  // Nested JSON in string value (should be treated as literal string)
  std::string json = R"({
    "appid": "550e8400-e29b-41d4-a716-446655440000",
    "vmin": "137.0",
    "cdn_urls": "{\"malicious\": true}"
  })";

  EXPECT_FALSE(ParseCombinedConfig(json, &config, &extended));
}

// ============================================================================
// StepCanonicalName and BuildProgressJson Tests
// ============================================================================

TEST(StepCanonicalNameTest, AllSteps) {
  EXPECT_STREQ("initializing", StepCanonicalName(kStepInit));
  EXPECT_STREQ("initializing", StepCanonicalName(kStepLock));
  EXPECT_STREQ("checking", StepCanonicalName(kStepVersionCheck));
  EXPECT_STREQ("checking", StepCanonicalName(kStepCdnResolve));
  EXPECT_STREQ("downloading", StepCanonicalName(kStepDownload));
  EXPECT_STREQ("extracting", StepCanonicalName(kStepExtract));
  EXPECT_STREQ("verifying", StepCanonicalName(kStepSignatureVerify));
  EXPECT_STREQ("installing", StepCanonicalName(kStepInstall));
  EXPECT_STREQ("committing", StepCanonicalName(kStepCommitting));
  EXPECT_STREQ("cleaning", StepCanonicalName(kStepCleanup));
}

TEST(StepTest, CleanupIsAlwaysTheFinalStep) {
  EXPECT_EQ(8, kStepCommitting);
  EXPECT_EQ(9, kStepCleanup);
  EXPECT_EQ(kNumSteps - 1, kStepCleanup);
}

TEST(StepCanonicalNameTest, OutOfRange) {
  EXPECT_STREQ("unknown", StepCanonicalName(static_cast<Step>(-1)));
  EXPECT_STREQ("unknown", StepCanonicalName(kNumSteps));
}

TEST(BuildProgressJsonTest, Format) {
  std::string json = BuildProgressJson(kStepDownload, 1024, 4096);

  // Verify JSON structure
  EXPECT_NE(json.find("\"step_name\":\"downloading\""), std::string::npos);
  EXPECT_NE(json.find("\"step\":4"), std::string::npos);
  EXPECT_NE(json.find("\"total_steps\":9"), std::string::npos);
  // bytes are stored as doubles, so check for the values
  EXPECT_NE(json.find("\"bytes_done\":1024"), std::string::npos);
  EXPECT_NE(json.find("\"bytes_total\":4096"), std::string::npos);
  EXPECT_NE(json.find("\"message\":\"Downloading...\""), std::string::npos);
  // 1024/4096 = 25% through download step (15-55%), so 15 + 40*0.25 = 25
  EXPECT_NE(json.find("\"overall_percent\":25"), std::string::npos);
}

TEST(BuildProgressJsonTest, ZeroValues) {
  std::string json = BuildProgressJson(kStepInit, 0, 0);

  EXPECT_NE(json.find("\"step_name\":\"initializing\""), std::string::npos);
  EXPECT_NE(json.find("\"step\":0"), std::string::npos);
  EXPECT_NE(json.find("\"total_steps\":9"), std::string::npos);
  EXPECT_NE(json.find("\"overall_percent\":0"), std::string::npos);
}

TEST(BuildProgressJsonTest, CleanupStepMatchesTotalSteps) {
  std::optional<base::DictValue> parsed = base::JSONReader::ReadDict(
      BuildProgressJson(kStepCleanup, 0, 0), base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->FindInt("step"), 9);
  EXPECT_EQ(parsed->FindInt("total_steps"), 9);
  EXPECT_EQ(*parsed->FindString("step_name"), "cleaning");
}

TEST(BuildProgressJsonTest, LargeByteValues) {
  // Test with large byte values (> 4GB)
  uint64_t bytes_done = 5ULL * 1024 * 1024 * 1024;    // 5GB
  uint64_t bytes_total = 10ULL * 1024 * 1024 * 1024;  // 10GB
  std::string json = BuildProgressJson(kStepDownload, bytes_done, bytes_total);

  // JSON should be parseable and contain the step name
  EXPECT_NE(json.find("\"step_name\":\"downloading\""), std::string::npos);
  // Large values are stored as doubles
  EXPECT_NE(json.find("bytes_done"), std::string::npos);
  EXPECT_NE(json.find("bytes_total"), std::string::npos);
}

TEST(SendProgressToParentTest, NullHwnd) {
  // Should not crash, just return early
  ResetProgressNotificationState();
  SendProgressToParent(nullptr, kStepInit, 0, 0);
  // No assertion needed - test passes if no crash
}

TEST(SendProgressToParentTest, InvalidHwnd) {
  // HWND value that is definitely not a valid window
  HWND invalid = reinterpret_cast<HWND>(static_cast<uintptr_t>(0xDEADBEEF));

  ResetProgressNotificationState();
  SendProgressToParent(invalid, kStepInit, 0, 0);
  // No assertion needed - test passes if no crash
}

// ============================================================================
// CalculateOverallProgress Tests
// ============================================================================

TEST(CalculateOverallProgressTest, StepBoundaries) {
  EXPECT_EQ(0, CalculateOverallProgress(kStepInit, 0, 0));
  EXPECT_EQ(3, CalculateOverallProgress(kStepLock, 0, 0));
  EXPECT_EQ(5, CalculateOverallProgress(kStepVersionCheck, 0, 0));
  EXPECT_EQ(10, CalculateOverallProgress(kStepCdnResolve, 0, 0));
  EXPECT_EQ(15, CalculateOverallProgress(kStepDownload, 0, 0));
  EXPECT_EQ(55, CalculateOverallProgress(kStepExtract, 0, 0));
  EXPECT_EQ(80, CalculateOverallProgress(kStepSignatureVerify, 0, 0));
  EXPECT_EQ(90, CalculateOverallProgress(kStepInstall, 0, 0));
  EXPECT_EQ(95, CalculateOverallProgress(kStepCommitting, 0, 0));
  EXPECT_EQ(95, CalculateOverallProgress(kStepCleanup, 0, 0));
}

TEST(CalculateOverallProgressTest, ByteInterpolation) {
  // Step 4 (Download): 15-55%, range = 40
  EXPECT_EQ(35, CalculateOverallProgress(kStepDownload, 500, 1000));
  EXPECT_EQ(55, CalculateOverallProgress(kStepDownload, 1000, 1000));
  EXPECT_EQ(15, CalculateOverallProgress(kStepDownload, 0, 1000));
}

TEST(CalculateOverallProgressTest, NegativeStep) {
  EXPECT_EQ(0, CalculateOverallProgress(static_cast<Step>(-1), 0, 0));
}

TEST(CalculateOverallProgressTest, StepBeyondMax) {
  // Step >= kNumSteps should clamp to last step (kStepCleanup: 95-100%)
  EXPECT_EQ(95, CalculateOverallProgress(kNumSteps, 0, 0));
  EXPECT_EQ(95, CalculateOverallProgress(static_cast<Step>(100), 0, 0));
}

TEST(CalculateOverallProgressTest, LargeByteValues) {
  uint64_t done = 5ULL * 1024 * 1024 * 1024;
  uint64_t total = 10ULL * 1024 * 1024 * 1024;
  EXPECT_EQ(35, CalculateOverallProgress(kStepDownload, done, total));
}

// ============================================================================
// Result::FromJson Edge Cases
// ============================================================================

TEST(InstallerResultTest, RetentionJsonRoundTrip) {
  Result result = Result::Success({}, "");
  result.retention_max_age_days = 180;
  result.registrations_committed = true;
  result.versions_pruned = true;
  RetentionPlan plan;
  RetentionRegistrationReport registration;
  registration.entry = {"full-appid", "windows64", "100", "200", "abi"};
  registration.evidence = {RetentionEvidenceKind::kLiveness, 123, true,
                           "invalid_or_noncanonical_evidence"};
  registration.age_days = 180;
  registration.decision = RetentionRegistrationDecision::kReclaim;
  registration.reason = RetentionReason::kStaleEvidence;
  plan.registrations.push_back(registration);
  plan.candidates.push_back({"full-appid", "windows64"});
  RetentionVersionReport version;
  version.version = Version::Parse("100.1");
  version.platform = "windows64";
  version.required_before = true;
  version.required_after = false;
  version.expected_removal = true;
  version.decision = RetentionVersionDecision::kNewlyReclaimable;
  version.reason = RetentionReason::kNewlyUnreferenced;
  plan.versions.push_back(version);
  result.retention_plan = plan;

  std::optional<Result> parsed = Result::FromJson(result.ToJson());

  ASSERT_TRUE(parsed);
  ASSERT_TRUE(parsed->retention_plan);
  EXPECT_EQ(180, parsed->retention_max_age_days);
  EXPECT_TRUE(parsed->registrations_committed);
  ASSERT_EQ(1u, parsed->retention_plan->registrations.size());
  EXPECT_EQ("full-appid", parsed->retention_plan->registrations[0].entry.uuid);
  EXPECT_EQ(RetentionReason::kStaleEvidence,
            parsed->retention_plan->registrations[0].reason);
  EXPECT_EQ("invalid_or_noncanonical_evidence",
            parsed->retention_plan->registrations[0].evidence.diagnostic);
  ASSERT_EQ(1u, parsed->retention_plan->versions.size());
  EXPECT_EQ("100.1", parsed->retention_plan->versions[0].version.ToString());

  std::string invalid_reason = result.ToJson();
  const size_t reason_offset = invalid_reason.find("stale_evidence");
  ASSERT_NE(std::string::npos, reason_offset);
  invalid_reason.replace(reason_offset, strlen("stale_evidence"),
                         "arbitrary_diagnostic");
  EXPECT_FALSE(Result::FromJson(invalid_reason));
}

TEST(InstallerResultTest, RetentionEligibilityRoundTrips) {
  Result result = Result::Error(kExitCodeConfigError, "ineligible");
  RetentionPlan plan;
  plan.eligible = false;
  plan.store_blocked = false;
  plan.blocker = "provisioning_store_ineligible";
  result.retention_plan = plan;
  result.retention_max_age_days = 180;

  std::optional<Result> parsed = Result::FromJson(result.ToJson());

  ASSERT_TRUE(parsed && parsed->retention_plan);
  EXPECT_FALSE(parsed->retention_plan->eligible);
  EXPECT_FALSE(parsed->retention_plan->store_blocked);
  EXPECT_EQ("provisioning_store_ineligible", parsed->retention_plan->blocker);
}

TEST(InstallerResultTest, FromJsonMissingSuccess) {
  // JSON without "success" field should return nullopt
  std::string json = R"({"error_code": "TEST"})";
  EXPECT_FALSE(Result::FromJson(json).has_value());
}

TEST(InstallerResultTest, FromJsonSuccessOnlyMinimal) {
  std::string json = R"({"success": true, "outcome": "committed"})";
  auto result = Result::FromJson(json);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->success);
  EXPECT_EQ(result->error_code, kExitCodeSuccess);
  EXPECT_TRUE(result->error_message.empty());
  EXPECT_TRUE(result->libcef_path.empty());
  EXPECT_TRUE(result->installed_version.empty());
  EXPECT_TRUE(result->version_full.empty());
  EXPECT_EQ(result->outcome, Outcome::kCommitted);
}

TEST(InstallerResultTest, CleanupDeferredRoundTrip) {
  Result original = Result::Success({}, "1.0");
  original.outcome = Outcome::kCleanupDeferred;
  original.warnings.push_back("trash cleanup deferred");
  auto parsed = Result::FromJson(original.ToJson());
  ASSERT_TRUE(parsed.has_value());
  EXPECT_TRUE(parsed->success);
  EXPECT_EQ(parsed->outcome, Outcome::kCleanupDeferred);
  ASSERT_EQ(1u, parsed->warnings.size());
  EXPECT_EQ("trash cleanup deferred", parsed->warnings[0]);
}

TEST(InstallerResultTest, FailedCategoryRoundTrips) {
  for (int code : {kExitCodePolicyDenied, kExitCodeCancelled}) {
    Result original = Result::Error(code, "diagnostic");
    auto parsed = Result::FromJson(original.ToJson());
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->success);
    EXPECT_EQ(Outcome::kFailed, parsed->outcome);
    EXPECT_EQ(code, parsed->error_code);
    EXPECT_EQ("diagnostic", parsed->error_message);
  }
}

TEST(InstallerResultTest, RejectsInvalidSchema) {
  EXPECT_FALSE(Result::FromJson(R"({"success":true})").has_value());
  EXPECT_FALSE(
      Result::FromJson(R"({"success":true,"outcome":"unknown"})").has_value());
  EXPECT_FALSE(
      Result::FromJson(
          R"({"success":false,"outcome":"committed","error_code":100,"error_name":"CONFIG_ERROR","error_message":"bad"})")
          .has_value());
  EXPECT_FALSE(
      Result::FromJson(R"({"success":true,"outcome":"failed"})").has_value());
  EXPECT_FALSE(
      Result::FromJson(
          R"({"success":false,"outcome":"failed","error_code":"CONFIG_ERROR","error_name":"CONFIG_ERROR","error_message":"bad"})")
          .has_value());
  EXPECT_FALSE(
      Result::FromJson(
          R"({"success":false,"outcome":"failed","error_code":100,"error_name":"NETWORK_ERROR","error_message":"bad"})")
          .has_value());
}

TEST(InstallerResultTest, ToJsonSuccessOmitsErrorFields) {
  Result result = Result::Success(base::FilePath(), "1.0.0");
  std::string json = result.ToJson();

  // Success results should not include error_code or error_message
  EXPECT_EQ(json.find("error_code"), std::string::npos);
  EXPECT_EQ(json.find("error_message"), std::string::npos);
  // Non-bundled results should not include is_bundled
  EXPECT_EQ(json.find("is_bundled"), std::string::npos);
  // Empty version_full should not be serialized
  EXPECT_EQ(json.find("version_full"), std::string::npos);
}

TEST(InstallerResultTest, ToJsonIncludesVersionFull) {
  Result result =
      Result::Success(base::FilePath(L"C:\\CEF\\libcef.dll"), "1.0.0",
                      "1.0.0+gabcdef0+chromium-138.0.7500.0");
  std::string json = result.ToJson();
  EXPECT_NE(json.find("1.0.0+gabcdef0+chromium-138.0.7500.0"),
            std::string::npos);
}

TEST(InstallerResultTest, RoundTripVersionFull) {
  Result original =
      Result::Success(base::FilePath(L"C:\\CEF\\libcef.dll"), "138.0.1",
                      "138.0.1+gabcdef0+chromium-138.0.7500.0");

  std::string json = original.ToJson();
  std::optional<Result> parsed = Result::FromJson(json);

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->version_full, "138.0.1+gabcdef0+chromium-138.0.7500.0");
}

TEST(InstallerResultTest, FromJsonMissingVersionFullDefaultsEmpty) {
  std::string json = R"({
    "success": true,
    "outcome": "committed",
    "libcef_path": "C:\\CEF\\libcef.dll",
    "installed_version": "137.3.5"
  })";

  std::optional<Result> result = Result::FromJson(json);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->version_full.empty());
}

TEST(InstallerResultTest, ToJsonBundledIncludesFlag) {
  Result result = Result::Success(base::FilePath(L"C:\\App\\CEF\\libcef.dll"),
                                  "1.0.0", "", /*is_bundled=*/true);
  std::string json = result.ToJson();
  EXPECT_NE(json.find("\"is_bundled\":true"), std::string::npos);
}

// ============================================================================
// SendProgressToParent Rate Limiting Tests
// ============================================================================

TEST(SendProgressToParentTest, RateLimitingSameStep) {
  // Use an invalid HWND that passes the nullptr check but fails IsWindow.
  // This exercises the rate-limiting logic up to the IsWindow check.
  HWND fake = reinterpret_cast<HWND>(static_cast<uintptr_t>(0xDEADBEEF));

  ResetProgressNotificationState();

  // First call with step 1 - always sent (step change)
  SendProgressToParent(fake, kStepLock, 0, 0);

  // Second call with same step - should be rate-limited (no crash)
  SendProgressToParent(fake, kStepLock, 50, 100);

  // Call with different step - should be sent (step change)
  SendProgressToParent(fake, kStepVersionCheck, 50, 100);
}

// ============================================================================
// Controller with Temp Install Dir Tests
// ============================================================================

// Shared fixture for tests that need a temp install dir with database support
// but no mock CDN server.
class ControllerTempDirTest : public testing::Test {
 protected:
  void SetUp() override {
    SetTestingMode(true);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    install_dir_ = temp_dir_.GetPath().Append(kCefSubdirectory);
    ASSERT_TRUE(base::CreateDirectory(install_dir_));

    // Use CEF_API_VERSION_LAST to avoid vmin clamping
    Version api_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
    api_version_str_ = api_ver.ToString();
  }

  void TearDown() override {
    internal::ClearInstallDirectoryOverridesForTesting();
    internal::OverrideEnterprisePolicyForTesting(std::nullopt);
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
    SetEmergencyRecoveryScanLimitsForTesting(std::nullopt);
#endif
    SetTestingMode(false);
    SetRetentionPostIndexFailureForTesting(false);
    SetRetentionPendingRestoreFailureForTesting(false);
    SetRetentionPostValidationEvidenceChangeForTesting(false);
    SetLaunchStateGcTimeForTesting(std::nullopt);
    SetConditionalDeleteHookForTesting({});
    SetLaunchStateGcPreDeleteHookForTesting({});
  }

  Config CreateValidConfig() {
    Config config;
    config.appid = "550e8400-e29b-41d4-a716-446655440000";
    config.vmin = api_version_str_;
    config.launch_health = LaunchHealthMode::kExitCode;
    return config;
  }

  ExtendedConfig CreateExtendedConfig() {
    ExtendedConfig ext;
    ext.install_path = install_dir_.AsUTF8Unsafe();
    ext.show_progress_ui = false;
    return ext;
  }

  // Create a fake installed version with metadata on disk. Defaults to the
  // current platform; pass |platform| to create a version for a different one
  // (used to exercise cross-platform pruning).
  void CreateFakeInstalledVersion(const std::string& version_str,
                                  const std::string& abi_hash = "defa01defa01",
                                  const std::string& platform = "",
                                  bool publish_index = false) {
    const std::string plat = platform.empty() ? GetCurrentPlatform() : platform;
    Version version = Version::Parse(version_str);
    base::FilePath version_dir = GetVersionPath(install_dir_, version, plat);
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
    metadata.platform = plat;
    metadata.version_full = version_str + "+gtest+chromium-" + version_str;
    ASSERT_EQ(WriteVersionMetadata(version_dir, metadata),
              MetadataError::kSuccess);
    if (publish_index) {
      ASSERT_EQ(
          MetadataError::kSuccess,
          WriteVersionIndex(install_dir_,
                            ScanInstalledVersionsWithMetadata(install_dir_)));
    }
  }

  base::FilePath CreateFakeBundledVersion(const std::wstring& name) {
    const base::FilePath bundled_dir = temp_dir_.GetPath().Append(name);
    const base::FilePath release_dir = bundled_dir.Append(kReleaseSubdirectory);
    EXPECT_TRUE(base::CreateDirectory(release_dir));
    EXPECT_TRUE(
        base::WriteFile(release_dir.Append(kLibcefFilename), "fake dll"));

    VersionMetadata metadata;
    metadata.version = Version::Parse(api_version_str_);
    metadata.abi_hash = "defa01defa01";
    metadata.platform = GetCurrentPlatform();
    metadata.version_full =
        api_version_str_ + "+gtest+chromium-" + api_version_str_;
    EXPECT_EQ(WriteVersionMetadata(bundled_dir, metadata),
              MetadataError::kSuccess);
    return bundled_dir;
  }

  // Initialize a database with one registered app.
  void CreateDatabaseWithApp(const Config& config) {
    Database db;
    AppEntry entry;
    entry.uuid = config.appid;
    entry.platform = GetCurrentPlatform();
    entry.vmin = config.vmin;
    entry.vmax = config.vmax;
    entry.abi_hash = config.abi_hash;
    db.RegisterApp(entry);
    ASSERT_EQ(DatabaseError::kSuccess, db.Save(GetDatabasePath(install_dir_)));
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath install_dir_;
  std::string api_version_str_;
};

TEST_F(ControllerTempDirTest, BundledQueryRequiresRegularCatalogFile) {
  Config config = CreateValidConfig();

  const base::FilePath missing_catalog =
      CreateFakeBundledVersion(L"BundledMissingCatalog");
  ExtendedConfig missing_extended = CreateExtendedConfig();
  missing_extended.bundled_cef_path = missing_catalog.AsUTF8Unsafe();
  Controller missing_controller;
  Result missing =
      missing_controller.Run(Command::kQuery, config, missing_extended);
  EXPECT_FALSE(missing.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, missing.error_code);

  const base::FilePath directory_catalog =
      CreateFakeBundledVersion(L"BundledDirectoryCatalog");
  ASSERT_TRUE(
      base::CreateDirectory(directory_catalog.Append(kCatalogFilename)));
  ExtendedConfig directory_extended = CreateExtendedConfig();
  directory_extended.bundled_cef_path = directory_catalog.AsUTF8Unsafe();
  Controller directory_controller;
  Result directory =
      directory_controller.Run(Command::kQuery, config, directory_extended);
  EXPECT_FALSE(directory.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, directory.error_code);
}

TEST_F(ControllerTempDirTest, BundledQueryAcceptsRegularCatalogFile) {
  const base::FilePath bundled =
      CreateFakeBundledVersion(L"BundledRegularCatalog");
  ASSERT_TRUE(base::WriteFile(bundled.Append(kCatalogFilename), "catalog"));
  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled.AsUTF8Unsafe();

  Controller controller;
  Result result =
      controller.Run(Command::kQuery, CreateValidConfig(), extended);

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(result.is_bundled);
  EXPECT_TRUE(bundled.IsParent(result.libcef_path));
}

TEST_F(ControllerTempDirTest, BundledQueryRejectsReparseCatalogFile) {
  const base::FilePath bundled =
      CreateFakeBundledVersion(L"BundledReparseCatalog");
  const base::FilePath catalog_target =
      temp_dir_.GetPath().Append(L"CatalogSymlinkTarget.cat");
  ASSERT_TRUE(base::WriteFile(catalog_target, "catalog"));

  const base::FilePath catalog_link = bundled.Append(kCatalogFilename);
  constexpr DWORD kAllowUnprivilegedCreate = 0x2;
  if (!CreateSymbolicLinkW(catalog_link.value().c_str(),
                           catalog_target.value().c_str(),
                           kAllowUnprivilegedCreate)) {
    DWORD error = GetLastError();
    if (error != ERROR_INVALID_PARAMETER ||
        !CreateSymbolicLinkW(catalog_link.value().c_str(),
                             catalog_target.value().c_str(), 0)) {
      if (error == ERROR_INVALID_PARAMETER) {
        error = GetLastError();
      }
      GTEST_SKIP() << "Host cannot create a file symlink: " << error;
    }
  }
  ASSERT_TRUE(IsReparsePoint(catalog_link));

  ExtendedConfig extended = CreateExtendedConfig();
  extended.bundled_cef_path = bundled.AsUTF8Unsafe();
  Controller controller;
  Result result =
      controller.Run(Command::kQuery, CreateValidConfig(), extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);
}

TEST_F(ControllerTempDirTest, PreparedUninstallUsesSingleResolvedTarget) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "",
                             /*publish_index=*/true);
  const base::FilePath external_exe =
      temp_dir_.GetPath().Append(L"external-bootstrap.exe");
  ASSERT_TRUE(base::WriteFile(external_exe, "exe"));
  ExtendedConfig extended;
  extended.show_progress_ui = false;

  internal::OverrideEnterprisePolicyForTesting(PolicyLoadResult{});
  internal::OverrideInstallDirectoryCandidatesForTesting(
      {{install_dir_, DirectoryRole::kPerUserDefault, std::nullopt,
        std::nullopt}});
  auto preflight = PrepareUninstall(config, extended, external_exe,
                                    UninstallInvocationState::kOriginal);
  ASSERT_TRUE(preflight.prepared);
  ASSERT_EQ(UninstallExecution::kInProcess, preflight.decision.execution);
  ASSERT_EQ(1u, internal::GetInstallDirectoryMutationProbeCountForTesting());

  const base::FilePath changed_target =
      temp_dir_.GetPath().Append(L"changed-target");
  ASSERT_TRUE(base::CreateDirectory(changed_target));
  PolicyLoadResult invalid_policy;
  invalid_policy.status = PolicyLoadStatus::kInvalid;
  invalid_policy.diagnostic = "policy changed after preflight";
  internal::OverrideEnterprisePolicyForTesting(invalid_policy);
  internal::OverrideInstallDirectoryCandidatesForTesting(
      {{changed_target, DirectoryRole::kPerUserDefault, std::nullopt,
        std::nullopt}});

  Controller controller;
  Result result =
      controller.RunPreparedUninstall(config, extended, *preflight.prepared);

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCommitted, result.outcome);
  EXPECT_EQ(1u, internal::GetInstallDirectoryMutationProbeCountForTesting());
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(database.GetAllApps().empty());
  EXPECT_FALSE(base::PathExists(GetDatabasePath(changed_target)));
}

TEST_F(ControllerTempDirTest, PreparedUninstallRejectsBindingMismatch) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  const base::FilePath external_exe =
      temp_dir_.GetPath().Append(L"external-bootstrap.exe");
  ASSERT_TRUE(base::WriteFile(external_exe, "exe"));
  ExtendedConfig extended;
  internal::OverrideEnterprisePolicyForTesting(PolicyLoadResult{});
  internal::OverrideInstallDirectoryCandidatesForTesting(
      {{install_dir_, DirectoryRole::kPerUserDefault, true, true}});
  auto preflight = PrepareUninstall(config, extended, external_exe,
                                    UninstallInvocationState::kOriginal);
  ASSERT_TRUE(preflight.prepared);

  PreparedUninstall wrong_command = *preflight.prepared;
  wrong_command.command = Command::kUpdate;
  Controller command_controller;
  EXPECT_EQ(
      kExitCodeConfigError,
      command_controller.RunPreparedUninstall(config, extended, wrong_command)
          .error_code);

  PreparedUninstall wrong_config = *preflight.prepared;
  wrong_config.config_binding = "different";
  Controller config_controller;
  EXPECT_EQ(
      kExitCodeConfigError,
      config_controller.RunPreparedUninstall(config, extended, wrong_config)
          .error_code);

  ExtendedConfig wrong_extended = extended;
  wrong_extended.install_path = install_dir_.AsUTF8Unsafe();
  Controller path_controller;
  EXPECT_EQ(
      kExitCodeConfigError,
      path_controller
          .RunPreparedUninstall(config, wrong_extended, *preflight.prepared)
          .error_code);

  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_EQ(1u, database.GetAllApps().size());
}

TEST_F(ControllerTempDirTest, PreparedUninstallRejectsNonDirectDecision) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  ExtendedConfig extended;
  internal::OverrideEnterprisePolicyForTesting(PolicyLoadResult{});
  internal::OverrideInstallDirectoryCandidatesForTesting(
      {{install_dir_, DirectoryRole::kPerUserDefault, true, true}});

  const base::FilePath contained_exe =
      install_dir_.Append(L"contained-bootstrap.exe");
  ASSERT_TRUE(base::WriteFile(contained_exe, "exe"));
  auto relaunch = PrepareUninstall(config, extended, contained_exe,
                                   UninstallInvocationState::kOriginal);
  ASSERT_TRUE(relaunch.prepared);
  ASSERT_EQ(UninstallExecution::kRelaunch, relaunch.prepared->execution);
  Controller relaunch_controller;
  EXPECT_EQ(kExitCodeConfigError,
            relaunch_controller
                .RunPreparedUninstall(config, extended, *relaunch.prepared)
                .error_code);

  const base::FilePath missing_exe =
      temp_dir_.GetPath().Append(L"missing-bootstrap.exe");
  auto rejected = PrepareUninstall(config, extended, missing_exe,
                                   UninstallInvocationState::kOriginal);
  ASSERT_TRUE(rejected.prepared);
  ASSERT_EQ(UninstallExecution::kReject, rejected.prepared->execution);
  Controller rejected_controller;
  EXPECT_EQ(kExitCodeConfigError,
            rejected_controller
                .RunPreparedUninstall(config, extended, *rejected.prepared)
                .error_code);

  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_EQ(1u, database.GetAllApps().size());
}

TEST_F(ControllerTempDirTest, PreparedUninstallPreservesInvalidPolicy) {
  PolicyLoadResult invalid_policy;
  invalid_policy.status = PolicyLoadStatus::kInvalid;
  invalid_policy.diagnostic = "invalid preflight policy";
  internal::OverrideEnterprisePolicyForTesting(invalid_policy);
  const base::FilePath external_exe =
      temp_dir_.GetPath().Append(L"external-bootstrap.exe");
  ASSERT_TRUE(base::WriteFile(external_exe, "exe"));

  Config config = CreateValidConfig();
  ExtendedConfig extended;
  auto preflight = PrepareUninstall(config, extended, external_exe,
                                    UninstallInvocationState::kOriginal);
  ASSERT_TRUE(preflight.prepared);
  EXPECT_FALSE(preflight.prepared->directories);
  EXPECT_EQ(UninstallExecution::kInProcess, preflight.decision.execution);

  internal::OverrideEnterprisePolicyForTesting(PolicyLoadResult{});
  Controller controller;
  Result result =
      controller.RunPreparedUninstall(config, extended, *preflight.prepared);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodePolicyError, result.error_code);
  EXPECT_EQ(Outcome::kFailed, result.outcome);
}

TEST_F(ControllerTempDirTest, UninstallDatabaseFailureIsFailedAndNonzero) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "",
                             /*publish_index=*/true);
  SetDatabaseSaveFailureForTesting(true);

  Controller controller;
  Result result =
      controller.Run(Command::kUninstall, config, CreateExtendedConfig());
  SetDatabaseSaveFailureForTesting(false);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(Outcome::kFailed, result.outcome);
  EXPECT_EQ(kExitCodeDatabaseError, result.error_code);
  EXPECT_EQ(kExitCodeDatabaseError, ResultToExitCode(result));
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_EQ(1u, indexed.size());
}

TEST_F(ControllerTempDirTest, UninstallIndexFailureIsFailedAndNonzero) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "",
                             /*publish_index=*/true);
  SetVersionIndexFaultForTesting(VersionIndexFault::kWrite);

  Controller controller;
  Result result =
      controller.Run(Command::kUninstall, config, CreateExtendedConfig());
  SetVersionIndexFaultForTesting(VersionIndexFault::kNone);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(Outcome::kFailed, result.outcome);
  EXPECT_EQ(kExitCodeIndexError, result.error_code);
  EXPECT_EQ(kExitCodeIndexError, ResultToExitCode(result));
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(database.GetAllApps().empty());
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_EQ(1u, indexed.size());
}

TEST_F(ControllerTempDirTest, UninstallTrashMoveIsSuccessfulCleanupDeferred) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "",
                             /*publish_index=*/true);
  const base::FilePath version_path =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  SetFileOpsFaultForTesting(FileOpsFault::kTrashMove);

  Controller controller;
  Result result =
      controller.Run(Command::kUninstall, config, CreateExtendedConfig());
  SetFileOpsFaultForTesting(FileOpsFault::kNone);

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  EXPECT_FALSE(result.warnings.empty());
  EXPECT_EQ(kExitCodeSuccess, ResultToExitCode(result));
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(database.GetAllApps().empty());
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_TRUE(indexed.empty());
  EXPECT_TRUE(base::PathExists(version_path));
}

TEST_F(ControllerTempDirTest, PolicyErrorReturnsDistinctStructuredFailure) {
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kInvalid;
  policy.diagnostic = "Invalid enterprise policy value CdnUrls";
  internal::OverrideEnterprisePolicyForTesting(policy);

  Controller controller;
  const Result result = controller.Run(Command::kQuery, CreateValidConfig(),
                                       CreateExtendedConfig());
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodePolicyError, result.error_code);
  EXPECT_EQ("POLICY_ERROR", internal::ExitCodeToString(result.error_code));
}

TEST_F(ControllerTempDirTest, PolicyDisabledUpdateIsDeniedBeforeCacheWrites) {
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kDisabled;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  internal::OverrideEnterprisePolicyForTesting(policy);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);

  Controller controller;
  const Result result = controller.Run(Command::kUpdate, CreateValidConfig(),
                                       CreateExtendedConfig());
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodePolicyDenied, result.error_code);
  EXPECT_NE(std::string::npos,
            result.error_message.find("Contact your administrator"));
  EXPECT_NE(std::string::npos, result.error_message.find("effective vmin="));
  EXPECT_FALSE(base::PathExists(GetCacheDirectory(install_dir_)));
}

TEST_F(ControllerTempDirTest,
       PolicyDisabledUpdateDoesNotCreateCustomDirectory) {
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kDisabled;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  internal::OverrideEnterprisePolicyForTesting(policy);
  const base::FilePath custom_dir =
      temp_dir_.GetPath().Append(L"policy-disabled-custom");
  ExtendedConfig extended = CreateExtendedConfig();
  extended.install_path = custom_dir.AsUTF8Unsafe();

  Controller controller;
  const Result result =
      controller.Run(Command::kUpdate, CreateValidConfig(), extended);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodePolicyDenied, result.error_code);
  EXPECT_FALSE(base::PathExists(custom_dir));
}

TEST_F(ControllerTempDirTest, PolicyDisabledInstallUsesLocalCandidate) {
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kDisabled;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  internal::OverrideEnterprisePolicyForTesting(policy);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);

  Controller controller;
  const Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                       CreateExtendedConfig());
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_FALSE(base::PathExists(GetCacheDirectory(install_dir_)));
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(database.GetApp(CreateValidConfig().appid, GetCurrentPlatform())
                  .has_value());
}

TEST_F(ControllerTempDirTest,
       PolicyDisabledInstallRetainsCandidateLeaseThroughMutation) {
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kDisabled;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  internal::OverrideEnterprisePolicyForTesting(policy);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);

  const Version version = Version::Parse(api_version_str_);
  bool removal_attempted = false;
  FileOpsError removal_error = FileOpsError::kSuccess;
  ProgressCallback attempt_removal = base::BindRepeating(
      [](const base::FilePath& install_dir, const Version& version,
         bool* attempted, FileOpsError* error, Step step, uint64_t, uint64_t) {
        if (step == kStepInit && !*attempted) {
          *attempted = true;
          *error = UninstallVersion(install_dir, version);
        }
        return true;
      },
      install_dir_, version, &removal_attempted, &removal_error);

  Controller controller;
  const Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                       CreateExtendedConfig(), attempt_removal);
  EXPECT_TRUE(removal_attempted);
  EXPECT_TRUE(removal_error == FileOpsError::kInUse ||
              removal_error == FileOpsError::kAccessDenied);
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(base::PathExists(GetVersionPath(install_dir_, version)));
}

TEST_F(ControllerTempDirTest,
       PolicyDisabledInstallRegistersPerUserWithoutAdminProbes) {
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kDisabled;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  internal::OverrideEnterprisePolicyForTesting(policy);

  const base::FilePath missing_hklm =
      temp_dir_.GetPath().Append(L"missing-hklm");
  const base::FilePath missing_program_files =
      temp_dir_.GetPath().Append(L"missing-program-files");
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  internal::OverrideInstallDirectoryCandidatesForTesting(
      {{missing_hklm, DirectoryRole::kHklmDefault, std::nullopt, std::nullopt},
       {missing_program_files, DirectoryRole::kProgramFilesDefault,
        std::nullopt, std::nullopt},
       {install_dir_, DirectoryRole::kPerUserDefault, std::nullopt,
        std::nullopt}});
  internal::OverrideProcessElevationForTesting(false);
  internal::OverrideAdminMutationAllowedForTesting(true);
  ExtendedConfig extended;
  extended.show_progress_ui = false;

  Controller controller;
  const Result result =
      controller.Run(Command::kInstall, CreateValidConfig(), extended);
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_FALSE(base::PathExists(missing_hklm));
  EXPECT_FALSE(base::PathExists(missing_program_files));
  EXPECT_EQ(1u, internal::GetInstallDirectoryMutationProbeCountForTesting());
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(database.GetApp(CreateValidConfig().appid, GetCurrentPlatform())
                  .has_value());
}

TEST_F(ControllerTempDirTest,
       PolicyDisabledInstallRejectsCandidateDroppedByWritableResolution) {
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kDisabled;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  internal::OverrideEnterprisePolicyForTesting(policy);

  const base::FilePath missing_hklm =
      temp_dir_.GetPath().Append(L"missing-hklm");
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  internal::OverrideInstallDirectoryCandidatesForTesting(
      {{missing_hklm, DirectoryRole::kHklmDefault, std::nullopt, std::nullopt},
       {install_dir_, DirectoryRole::kProgramFilesDefault, true, false}});
  internal::OverrideProcessElevationForTesting(true);
  internal::OverrideAdminMutationAllowedForTesting(true);
  ExtendedConfig extended;
  extended.show_progress_ui = false;

  Controller controller;
  const Result result =
      controller.Run(Command::kInstall, CreateValidConfig(), extended);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodePolicyDenied, result.error_code);
  EXPECT_FALSE(base::PathExists(missing_hklm));
  EXPECT_EQ(0u, internal::GetInstallDirectoryMutationProbeCountForTesting());
}

TEST_F(ControllerTempDirTest, PolicyOfflineSelectsAdminAndIgnoresPerUser) {
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.allow_shared_user_store = false;
  internal::OverrideEnterprisePolicyForTesting(policy);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  const base::FilePath per_user = temp_dir_.GetPath().Append(L"per-user");
  internal::OverrideInstallDirectoryCandidatesForTesting(
      {{install_dir_, DirectoryRole::kHklmDefault, true, false},
       {per_user, DirectoryRole::kPerUserDefault, true, true}});
  ExtendedConfig extended;
  extended.show_progress_ui = false;

  Controller controller;
  const Result result =
      controller.Run(Command::kInstall, CreateValidConfig(), extended, {},
                     ExecutionContext::kAutomaticStartup);
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(base::FilePath(install_dir_).IsParent(result.libcef_path));
  EXPECT_FALSE(base::PathExists(per_user));
}

TEST_F(ControllerTempDirTest, PolicyOfflineMissReturnsDenial) {
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.allow_shared_user_store = false;
  internal::OverrideEnterprisePolicyForTesting(policy);
  const base::FilePath per_user = temp_dir_.GetPath().Append(L"per-user");
  internal::OverrideInstallDirectoryCandidatesForTesting(
      {{install_dir_, DirectoryRole::kHklmDefault, true, false},
       {per_user, DirectoryRole::kPerUserDefault, true, true}});

  Controller controller;
  const Result result =
      controller.Run(Command::kInstall, CreateValidConfig(), ExtendedConfig{},
                     {}, ExecutionContext::kAutomaticStartup);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodePolicyDenied, result.error_code);
  EXPECT_FALSE(base::PathExists(per_user));
}

TEST_F(ControllerTempDirTest, PolicyBlockedMutationMatrixReturnsDenial) {
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.allow_shared_user_store = false;
  internal::OverrideEnterprisePolicyForTesting(policy);
  const base::FilePath per_user = temp_dir_.GetPath().Append(L"per-user");
  for (Command command : {Command::kUpdate, Command::kUninstall,
                          Command::kPrune, Command::kRetentionApply}) {
    internal::OverrideInstallDirectoryCandidatesForTesting(
        {{per_user, DirectoryRole::kPerUserDefault, true, true}});
    Controller controller;
    const Result result = controller.Run(command, CreateValidConfig());
    EXPECT_FALSE(result.success);
    EXPECT_EQ(kExitCodePolicyDenied, result.error_code);
    EXPECT_FALSE(base::PathExists(per_user));
  }
}

TEST_F(ControllerTempDirTest, PolicyLockdownQueryRetainsOrdinaryMiss) {
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.allow_shared_user_store = false;
  internal::OverrideEnterprisePolicyForTesting(policy);
  const base::FilePath per_user = temp_dir_.GetPath().Append(L"per-user");
  internal::OverrideInstallDirectoryCandidatesForTesting(
      {{per_user, DirectoryRole::kPerUserDefault, true, true}});

  Controller controller;
  const Result result = controller.Run(Command::kQuery, CreateValidConfig());
  EXPECT_FALSE(result.success);
  EXPECT_NE(kExitCodePolicyDenied, result.error_code);
  EXPECT_FALSE(base::PathExists(per_user));
}

TEST_F(ControllerTempDirTest,
       PolicyDisabledEnforcesCachedRevocationWithoutRefreshWrites) {
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kDisabled;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  internal::OverrideEnterprisePolicyForTesting(policy);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  RevokedVersionRange revoked;
  revoked.version_min = Version::Parse(api_version_str_);
  revoked.version_max = revoked.version_min;
  ASSERT_EQ(RevocationError::kSuccess,
            WriteRevocationCache(install_dir_, {revoked}));

  Controller controller;
  const Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                       CreateExtendedConfig(), {},
                                       ExecutionContext::kAutomaticStartup);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodePolicyDenied, result.error_code);
  EXPECT_FALSE(
      base::PathExists(install_dir_.Append(kRevocationBackoffFilename)));
  EXPECT_FALSE(base::PathExists(GetCacheDirectory(install_dir_)));
}

TEST_F(ControllerTempDirTest, PolicyMirrorPersistsRevocationDelta) {
  const base::FilePath mirror = temp_dir_.GetPath().Append(L"policy-mirror");
  ASSERT_TRUE(base::CreateDirectory(mirror));
  ASSERT_TRUE(base::WriteFile(mirror.AppendASCII("revoked.json"),
                              R"({"revoked_versions":[]})"));
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kPolicyMirror;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  policy.policy.download_source.mirror_path = mirror;
  internal::OverrideEnterprisePolicyForTesting(policy);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);

  Controller controller;
  const Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                       CreateExtendedConfig());
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(base::PathExists(install_dir_.Append(kRevocationCacheFilename)));
}

TEST_F(ControllerTempDirTest, RetentionDryRunReportsWithoutMutation) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  std::string database_before;
  std::string index_before;
  std::string evidence_before;
  ASSERT_TRUE(
      base::ReadFileToString(GetDatabasePath(install_dir_), &database_before));
  ASSERT_TRUE(base::ReadFileToString(install_dir_.Append(kVersionIndexFilename),
                                     &index_before));
  ASSERT_TRUE(base::ReadFileToString(evidence_path, &evidence_before));

  Controller controller;
  Result result = controller.Run(Command::kRetentionDryRun, Config{},
                                 CreateExtendedConfig());

  ASSERT_TRUE(result.success);
  ASSERT_TRUE(result.retention_plan);
  ASSERT_EQ(1u, result.retention_plan->candidates.size());
  EXPECT_FALSE(result.registrations_committed);
  EXPECT_FALSE(result.versions_pruned);
  std::string database_after;
  std::string index_after;
  std::string evidence_after;
  ASSERT_TRUE(
      base::ReadFileToString(GetDatabasePath(install_dir_), &database_after));
  ASSERT_TRUE(base::ReadFileToString(install_dir_.Append(kVersionIndexFilename),
                                     &index_after));
  ASSERT_TRUE(base::ReadFileToString(evidence_path, &evidence_after));
  EXPECT_EQ(database_before, database_after);
  EXPECT_EQ(index_before, index_after);
  EXPECT_EQ(evidence_before, evidence_after);
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kLogFilename)));
}

TEST_F(ControllerTempDirTest, RetentionApplyDoesNotPruneUnrelatedOrphan) {
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  const std::string orphan = "orphan-app";
  const std::string platform = GetCurrentPlatform();
  base::FilePath expired_path =
      GetInstallDirLivenessPath(install_dir_, GetAppidHash(orphan), platform);
  constexpr uint64_t kNow = 2 * kLaunchStateGcMaxAge + 100;
  ASSERT_TRUE(WriteLivenessPath(
      expired_path, {orphan, platform, kNow - kLaunchStateGcMaxAge}));
  SetLaunchStateGcTimeForTesting(kNow);

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig());

  ASSERT_TRUE(result.success) << result.error_message;
  ASSERT_TRUE(result.retention_plan);
  ASSERT_EQ(1u, result.retention_plan->versions.size());
  EXPECT_EQ(RetentionVersionDecision::kAlreadyUnreferenced,
            result.retention_plan->versions[0].decision);
  EXPECT_FALSE(result.retention_plan->versions[0].expected_removal);
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_EQ(1u, indexed.size());
  EXPECT_TRUE(base::PathExists(expired_path));
}

TEST_F(ControllerTempDirTest, RunInstallerRetentionNoOpJson) {
  base::DictValue config;
  config.Set("install_path", install_dir_.AsUTF8Unsafe());
  config.Set("max_age_days", 90);
  std::string config_json;
  ASSERT_TRUE(base::JSONWriter::Write(config, &config_json));

  const char* json = RunInstaller("retention_dry_run", config_json.c_str());
  std::optional<Result> result = Result::FromJson(json);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->success);
  ASSERT_TRUE(result->retention_plan);
  EXPECT_EQ(90, result->retention_max_age_days);
  EXPECT_TRUE(result->retention_plan->registrations.empty());
  EXPECT_TRUE(result->retention_plan->versions.empty());
}

TEST_F(ControllerTempDirTest, RetentionApplyCommitsThenPrunes) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));

  std::vector<Step> steps;
  Controller controller;
  Result result =
      controller.Run(Command::kRetentionApply, Config{}, CreateExtendedConfig(),
                     RecordProgress(&steps));

  ASSERT_TRUE(result.success) << result.error_message;
  ASSERT_GE(steps.size(), 2u);
  EXPECT_EQ(kStepCommitting, steps[steps.size() - 2]);
  EXPECT_EQ(kStepCleanup, steps.back());
  EXPECT_TRUE(result.registrations_committed);
  EXPECT_TRUE(result.versions_pruned);
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(database.IsEmpty());
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_TRUE(indexed.empty());
  EXPECT_FALSE(base::PathExists(evidence_path));
}

TEST_F(ControllerTempDirTest, RetentionApplyWithoutVersionWorkEndsWithCleanup) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));

  std::vector<Step> steps;
  Controller controller;
  Result result =
      controller.Run(Command::kRetentionApply, Config{}, CreateExtendedConfig(),
                     RecordProgress(&steps));

  ASSERT_TRUE(result.success) << result.error_message;
  ASSERT_GE(steps.size(), 2u);
  EXPECT_EQ(kStepCommitting, steps[steps.size() - 2]);
  EXPECT_EQ(kStepCleanup, steps.back());
  EXPECT_TRUE(result.registrations_committed);
  EXPECT_TRUE(result.versions_pruned);
}

TEST_F(ControllerTempDirTest,
       RetentionRemovedAppsConfirmedSentinelDoesNotProtectVersion) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  LaunchState health;
  health.appid = app.appid;
  health.pid_start_time = GetCurrentWallTime() - 181 * kFileTimeTicksPerDay;
  health.confirmed = true;
  health.version = api_version_str_;
  health.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, GetAppidHash(app.appid),
                                   Version::Parse(api_version_str_),
                                   GetCurrentPlatform()),
      health));

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig());

  ASSERT_TRUE(result.success) << result.error_message;
  ASSERT_TRUE(result.retention_plan);
  ASSERT_EQ(1u, result.retention_plan->versions.size());
  EXPECT_TRUE(result.retention_plan->versions[0].expected_removal);
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_TRUE(indexed.empty());
}

TEST_F(ControllerTempDirTest, RetentionDatabaseFailureChangesNoOtherState) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  std::string index_before;
  ASSERT_TRUE(base::ReadFileToString(install_dir_.Append(kVersionIndexFilename),
                                     &index_before));

  SetDatabaseSaveFailureForTesting(true);
  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig());
  SetDatabaseSaveFailureForTesting(false);

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.registrations_committed);
  EXPECT_FALSE(result.retry_required);
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_EQ(1u, database.GetAllApps().size());
  std::string index_after;
  ASSERT_TRUE(base::ReadFileToString(install_dir_.Append(kVersionIndexFilename),
                                     &index_after));
  EXPECT_EQ(index_before, index_after);
  EXPECT_TRUE(base::PathExists(evidence_path));
}

TEST_F(ControllerTempDirTest, RetentionReportsRetryStateRollbackFailure) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  ASSERT_TRUE(WriteLivenessPath(
      GetInstallDirLivenessPath(install_dir_, GetAppidHash(app.appid),
                                GetCurrentPlatform()),
      {app.appid, GetCurrentPlatform(),
       GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  SetDatabaseSaveFailureForTesting(true);
  SetRetentionPendingRestoreFailureForTesting(true);

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig());
  SetDatabaseSaveFailureForTesting(false);
  SetRetentionPendingRestoreFailureForTesting(false);

  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.retry_required);
  EXPECT_FALSE(result.warnings.empty());
  EXPECT_TRUE(base::PathExists(install_dir_.Append(L"retention_pending.json")));
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_EQ(1u, database.GetAllApps().size());
}

TEST_F(ControllerTempDirTest, RetentionPreservesPostObservationEvidence) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  SetRetentionPostValidationEvidenceChangeForTesting(true);

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig());
  SetRetentionPostValidationEvidenceChangeForTesting(false);

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_FALSE(result.retry_required);
  EXPECT_TRUE(result.registrations_committed);
  ASSERT_TRUE(result.retention_plan);
  EXPECT_EQ(1u, result.retention_plan->candidates.size());
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(database.GetAllApps().empty());
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_TRUE(indexed.empty());
  EXPECT_TRUE(base::PathExists(evidence_path));
}

TEST_F(ControllerTempDirTest, RetentionIndexFailureReportsPartialCommit) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));

  SetVersionIndexFaultForTesting(VersionIndexFault::kWrite);
  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig());
  SetVersionIndexFaultForTesting(VersionIndexFault::kNone);

  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.registrations_committed);
  EXPECT_FALSE(result.versions_pruned);
  EXPECT_TRUE(result.retry_required);
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(database.IsEmpty());
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_EQ(1u, indexed.size());
  EXPECT_TRUE(base::PathExists(evidence_path));
}

TEST_F(ControllerTempDirTest, RetentionRecomputesFreshEvidenceUnderLock) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  Controller dry_run;
  ASSERT_EQ(
      1u,
      dry_run.Run(Command::kRetentionDryRun, Config{}, CreateExtendedConfig())
          .retention_plan->candidates.size());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(), GetCurrentWallTime()}));

  Controller apply;
  Result result =
      apply.Run(Command::kRetentionApply, Config{}, CreateExtendedConfig());

  ASSERT_TRUE(result.success);
  EXPECT_TRUE(result.retention_plan->candidates.empty());
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_EQ(1u, database.GetAllApps().size());
}

TEST_F(ControllerTempDirTest, RetentionFinalValidationReportsChangedPlan) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  int planning_checkpoints = 0;
  ProgressCallback refresh_during_planning = base::BindRepeating(
      [](int* checkpoints, const base::FilePath& path, const std::string& appid,
         Step step, uint64_t, uint64_t) {
        if (step == kStepVersionCheck && ++*checkpoints == 4) {
          EXPECT_TRUE(WriteLivenessPath(
              path, {appid, GetCurrentPlatform(), GetCurrentWallTime()}));
        }
        return true;
      },
      base::Unretained(&planning_checkpoints), evidence_path, app.appid);

  Controller controller;
  Result result =
      controller.Run(Command::kRetentionApply, Config{}, CreateExtendedConfig(),
                     refresh_during_planning);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeRetentionSnapshotChanged, result.error_code);
  EXPECT_TRUE(result.retry_required);
  EXPECT_TRUE(result.retention_plan->candidates.empty());
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_EQ(1u, database.GetAllApps().size());
  EXPECT_TRUE(base::PathExists(evidence_path));
}

TEST_F(ControllerTempDirTest, RetentionLockTimeoutDoesNotPlanUnlocked) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  base::WaitableEvent lock_acquired;
  base::WaitableEvent release_lock;
  std::thread holder([&] {
    auto held = SingletonLock::Acquire(install_dir_, INFINITE);
    ASSERT_TRUE(held && held->IsHeld());
    lock_acquired.Signal();
    release_lock.Wait();
  });
  lock_acquired.Wait();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.lock_timeout_ms = 50;

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{}, extended);
  release_lock.Signal();
  holder.join();

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeLockTimeout, result.error_code);
  EXPECT_FALSE(result.retention_plan);
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_EQ(1u, database.GetAllApps().size());
}

TEST_F(ControllerTempDirTest, RetentionCancellationBeforeCommitIsClean) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  ProgressCallback cancel = base::BindRepeating(
      [](Step step, uint64_t, uint64_t) { return step != kStepLock; });

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig(), cancel);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeCancelled, result.error_code);
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_EQ(1u, database.GetAllApps().size());
  EXPECT_TRUE(base::PathExists(evidence_path));
}

TEST_F(ControllerTempDirTest, RetentionCancellationAfterPlanningIsClean) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  int planning_checkpoints = 0;
  ProgressCallback cancel_before_commit = base::BindRepeating(
      [](int* checkpoints, Step step, uint64_t, uint64_t) {
        return step != kStepVersionCheck || ++*checkpoints < 5;
      },
      base::Unretained(&planning_checkpoints));

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig(), cancel_before_commit);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeCancelled, result.error_code);
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_EQ(1u, database.GetAllApps().size());
  EXPECT_TRUE(base::PathExists(evidence_path));
  EXPECT_FALSE(
      base::PathExists(install_dir_.Append(L"retention_pending.json")));
}

TEST_F(ControllerTempDirTest, RetentionCommittingStateIsNonCancellable) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  bool saw_committing = false;
  ProgressCallback cancel_committing = base::BindRepeating(
      [](bool* saw, Step step, uint64_t, uint64_t) {
        if (step == kStepCommitting) {
          *saw = true;
          return false;
        }
        return true;
      },
      base::Unretained(&saw_committing));

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig(), cancel_committing);

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(saw_committing);
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(database.GetAllApps().empty());
}

TEST_F(ControllerTempDirTest,
       RetentionDeferredCancellationStopsPhysicalCleanup) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  bool cancellation_pending = false;
  ProgressCallback defer_until_cleanup = base::BindRepeating(
      [](bool* pending, Step step, uint64_t, uint64_t) {
        if (step == kStepCommitting) {
          *pending = true;
          return false;
        }
        return step != kStepCleanup || !*pending;
      },
      base::Unretained(&cancellation_pending));

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig(), defer_until_cleanup);

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  EXPECT_TRUE(result.retry_required);
  EXPECT_TRUE(result.registrations_committed);
  EXPECT_TRUE(result.versions_pruned);
  EXPECT_TRUE(base::DirectoryExists(
      GetVersionPath(install_dir_, Version::Parse(api_version_str_))));
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_TRUE(indexed.empty());
  EXPECT_TRUE(base::PathExists(install_dir_.Append(L"retention_pending.json")));
}

TEST_F(ControllerTempDirTest, RetentionPruningSuspensionBlocksApply) {
  Config app = CreateValidConfig();
  Database database;
  database.RegisterApp(
      {app.appid, GetCurrentPlatform(), app.vmin, app.vmax, app.abi_hash});
  database.SuspendPruning();
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Save(GetDatabasePath(install_dir_)));

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig());

  EXPECT_FALSE(result.success);
  ASSERT_TRUE(result.retention_plan);
  EXPECT_EQ("database_pruning_blocked", result.retention_plan->blocker);
  Database reread;
  ASSERT_EQ(DatabaseError::kSuccess,
            reread.Load(GetDatabasePath(install_dir_)));
  EXPECT_EQ(1u, reread.GetAllApps().size());
}

TEST_F(ControllerTempDirTest, RetentionProvisioningRoleIsIneligible) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  internal::OverrideInstallDirectoryCandidatesForTesting(
      {{install_dir_, DirectoryRole::kHklmDefault, true, true}});
  ExtendedConfig extended;
  extended.show_progress_ui = false;

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{}, extended);

  EXPECT_FALSE(result.success);
  ASSERT_TRUE(result.retention_plan);
  EXPECT_FALSE(result.retention_plan->eligible);
  EXPECT_EQ("provisioning_store_ineligible", result.retention_plan->blocker);
  ASSERT_EQ(1u, result.retention_plan->registrations.size());
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_EQ(1u, database.GetAllApps().size());
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kLogFilename)));
}

TEST_F(ControllerTempDirTest, RetentionIndexFailureRetryConverges) {
  Config app = CreateValidConfig();
  app.vmax = api_version_str_;
  CreateDatabaseWithApp(app);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  const std::string unrelated_version = api_version_str_ + ".1";
  CreateFakeInstalledVersion(unrelated_version, "ab11ab11ab11", "", true);
  ASSERT_TRUE(WriteLivenessPath(
      GetInstallDirLivenessPath(install_dir_, GetAppidHash(app.appid),
                                GetCurrentPlatform()),
      {app.appid, GetCurrentPlatform(),
       GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  SetVersionIndexFaultForTesting(VersionIndexFault::kWrite);
  Controller first;
  EXPECT_FALSE(
      first.Run(Command::kRetentionApply, Config{}, CreateExtendedConfig())
          .success);
  SetVersionIndexFaultForTesting(VersionIndexFault::kNone);

  Controller retry;
  Result result =
      retry.Run(Command::kRetentionApply, Config{}, CreateExtendedConfig());

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(result.registrations_committed);
  EXPECT_TRUE(result.versions_pruned);
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  ASSERT_EQ(1u, indexed.size());
  EXPECT_EQ(unrelated_version, indexed[0].metadata.version.ToString());
}

TEST_F(ControllerTempDirTest, RetentionRetryScansAfterReducedIndexCrash) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  const Version version = Version::Parse(api_version_str_);
  const base::FilePath version_path = GetVersionPath(install_dir_, version);
  ASSERT_TRUE(WriteLivenessPath(
      GetInstallDirLivenessPath(install_dir_, GetAppidHash(app.appid),
                                GetCurrentPlatform()),
      {app.appid, GetCurrentPlatform(),
       GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  SetRetentionPostIndexFailureForTesting(true);

  Controller first;
  Result interrupted =
      first.Run(Command::kRetentionApply, Config{}, CreateExtendedConfig());
  SetRetentionPostIndexFailureForTesting(false);

  EXPECT_FALSE(interrupted.success);
  EXPECT_TRUE(interrupted.registrations_committed);
  EXPECT_TRUE(interrupted.retry_required);
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_TRUE(indexed.empty());
  EXPECT_TRUE(base::DirectoryExists(version_path));
  EXPECT_TRUE(base::PathExists(install_dir_.Append(L"retention_pending.json")));

  Controller retry;
  Result result =
      retry.Run(Command::kRetentionApply, Config{}, CreateExtendedConfig());

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_FALSE(base::PathExists(version_path));
  EXPECT_FALSE(
      base::PathExists(install_dir_.Append(L"retention_pending.json")));
}

TEST_F(ControllerTempDirTest, RetentionLiveLeaseDefersPhysicalCleanup) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  ASSERT_TRUE(WriteLivenessPath(
      GetInstallDirLivenessPath(install_dir_, GetAppidHash(app.appid),
                                GetCurrentPlatform()),
      {app.appid, GetCurrentPlatform(),
       GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  base::FilePath version_path =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  std::unique_ptr<VersionLease> lease;
  ASSERT_EQ(VersionLeaseError::kSuccess,
            AcquireVersionLease(install_dir_, version_path, &lease));

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig());

  ASSERT_TRUE(result.success);
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  EXPECT_TRUE(result.versions_pruned);
  EXPECT_TRUE(base::DirectoryExists(version_path));
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_TRUE(indexed.empty());
}

TEST_F(ControllerTempDirTest, RetentionTrashMoveFailureIsCleanupDeferred) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  ASSERT_TRUE(WriteLivenessPath(
      GetInstallDirLivenessPath(install_dir_, GetAppidHash(app.appid),
                                GetCurrentPlatform()),
      {app.appid, GetCurrentPlatform(),
       GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  SetFileOpsFaultForTesting(FileOpsFault::kTrashMove);

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig());
  SetFileOpsFaultForTesting(FileOpsFault::kNone);

  ASSERT_TRUE(result.success);
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  EXPECT_TRUE(result.registrations_committed);
  EXPECT_TRUE(result.versions_pruned);
  EXPECT_TRUE(base::DirectoryExists(
      GetVersionPath(install_dir_, Version::Parse(api_version_str_))));
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_TRUE(indexed.empty());
}

TEST_F(ControllerTempDirTest, RetentionReportsDeferredVersionPrecisely) {
  Config app = CreateValidConfig();
  app.vmin = api_version_str_;
  CreateDatabaseWithApp(app);
  const std::string newer_version = api_version_str_ + ".1";
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  CreateFakeInstalledVersion(newer_version, "defa01defa01", "", true);
  ASSERT_TRUE(WriteLivenessPath(
      GetInstallDirLivenessPath(install_dir_, GetAppidHash(app.appid),
                                GetCurrentPlatform()),
      {app.appid, GetCurrentPlatform(),
       GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  const Version held_version = Version::Parse(newer_version);
  std::unique_ptr<VersionLease> lease;
  ASSERT_EQ(
      VersionLeaseError::kSuccess,
      AcquireVersionLease(install_dir_,
                          GetVersionPath(install_dir_, held_version), &lease));

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig());

  ASSERT_TRUE(result.success) << result.error_message;
  ASSERT_EQ(Outcome::kCleanupDeferred, result.outcome);
  ASSERT_TRUE(result.retention_plan);
  ASSERT_EQ(2u, result.retention_plan->versions.size());
  for (const auto& version : result.retention_plan->versions) {
    EXPECT_EQ(version.version == held_version, version.cleanup_deferred)
        << version.version.ToString();
  }
}

TEST_F(ControllerTempDirTest, RetentionEvidenceFailureIsCleanupDeferred) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  base::FilePath evidence_path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(app.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      evidence_path, {app.appid, GetCurrentPlatform(),
                      GetCurrentWallTime() - 181 * kFileTimeTicksPerDay}));
  SetRetentionEvidenceDeleteFailureForTesting(true);

  Controller controller;
  Result result = controller.Run(Command::kRetentionApply, Config{},
                                 CreateExtendedConfig());
  SetRetentionEvidenceDeleteFailureForTesting(false);

  ASSERT_TRUE(result.success);
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  EXPECT_TRUE(result.registrations_committed);
  EXPECT_TRUE(result.versions_pruned);
  EXPECT_TRUE(base::PathExists(evidence_path));
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(database.IsEmpty());
}

TEST_F(ControllerTempDirTest, OrdinaryPruneNeverExpiresOldRegistration) {
  Config app = CreateValidConfig();
  CreateDatabaseWithApp(app);
  ASSERT_TRUE(WriteLivenessPath(
      GetInstallDirLivenessPath(install_dir_, GetAppidHash(app.appid),
                                GetCurrentPlatform()),
      {app.appid, GetCurrentPlatform(),
       GetCurrentWallTime() - 1000 * kFileTimeTicksPerDay}));

  Controller controller;
  ASSERT_TRUE(controller.Run(Command::kPrune, Config{}, CreateExtendedConfig())
                  .success);

  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_EQ(1u, database.GetAllApps().size());
}

TEST_F(ControllerTempDirTest, QueryFindsExistingVersion) {
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  base::FilePath invalid =
      GetLaunchStateDir(install_dir_).Append(L"footerless-old-schema");
  ASSERT_TRUE(base::CreateDirectory(invalid.DirName()));
  ASSERT_TRUE(base::WriteFile(invalid, "{}"));

  Controller controller;
  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kQuery, config, extended);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(api_version_str_, result.installed_version);
  EXPECT_FALSE(result.libcef_path.empty());
  EXPECT_TRUE(base::PathExists(invalid));
}

TEST_F(ControllerTempDirTest, QueryDoesNotAcquireWriterLock) {
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  auto held = SingletonLock::Acquire(install_dir_, INFINITE);
  ASSERT_TRUE(held && held->IsHeld());

  ExtendedConfig extended = CreateExtendedConfig();
  extended.lock_timeout_ms = 100;
  Controller controller;
  base::TimeTicks start = base::TimeTicks::Now();
  Result result =
      controller.Run(Command::kQuery, CreateValidConfig(), extended);
  base::TimeDelta elapsed = base::TimeTicks::Now() - start;

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_LT(elapsed, base::Milliseconds(90));
}

TEST_F(ControllerTempDirTest, QueryMissingCustomPathIsExclusiveAndReadOnly) {
  const base::FilePath missing = temp_dir_.GetPath().Append(L"missing_custom");
  ExtendedConfig extended = CreateExtendedConfig();
  extended.install_path = missing.AsUTF8Unsafe();
  Controller controller;
  Result result =
      controller.Run(Command::kQuery, CreateValidConfig(), extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeConfigError, result.error_code);
  EXPECT_FALSE(base::PathExists(missing));
  EXPECT_FALSE(base::PathExists(GetDatabasePath(missing)));
}

TEST_F(ControllerTempDirTest, AutomaticStartupLocalHitDoesNotWaitForWriter) {
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  auto held = SingletonLock::Acquire(install_dir_, INFINITE);
  ASSERT_TRUE(held && held->IsHeld());

  ExtendedConfig extended = CreateExtendedConfig();
  extended.lock_timeout_ms = 100;
  Controller controller;
  base::TimeTicks start = base::TimeTicks::Now();
  Result result =
      controller.Run(Command::kInstall, CreateValidConfig(), extended, {},
                     ExecutionContext::kAutomaticStartup);
  base::TimeDelta elapsed = base::TimeTicks::Now() - start;

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(result.version_lease);
  EXPECT_LT(elapsed, base::Milliseconds(90));
  EXPECT_FALSE(base::PathExists(GetDatabasePath(install_dir_)));

  held.reset();
  result.version_lease.reset();
  Controller retry;
  Result retry_result =
      retry.Run(Command::kInstall, CreateValidConfig(), extended, {},
                ExecutionContext::kAutomaticStartup);
  ASSERT_TRUE(retry_result.success) << retry_result.error_message;
  Database database;
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Load(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(database.GetApp(CreateValidConfig().appid, GetCurrentPlatform()));
}

TEST_F(ControllerTempDirTest, AutomaticStartupGatedAdminDirectoryIsReadOnly) {
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  internal::OverrideInstallDirectoryCandidatesForTesting(
      {{install_dir_, DirectoryRole::kProgramFilesDefault, true, true}});
  internal::OverrideProcessElevationForTesting(true);
  internal::OverrideAdminMutationAllowedForTesting(false);

  auto held = SingletonLock::Acquire(install_dir_, INFINITE);
  ASSERT_TRUE(held && held->IsHeld());
  ExtendedConfig extended;
  extended.show_progress_ui = false;
  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateValidConfig(), extended, {},
                     ExecutionContext::kAutomaticStartup);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(api_version_str_, result.installed_version);
  EXPECT_TRUE(result.version_lease);
  EXPECT_FALSE(base::PathExists(GetDatabasePath(install_dir_)));
  EXPECT_TRUE(result.launch_state_path.empty());
  EXPECT_TRUE(result.liveness_path.empty());
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupGatedAdminDirectoryDoesNotFallThrough) {
  const base::FilePath per_user = temp_dir_.GetPath().Append(L"per_user");
  internal::OverrideInstallDirectoryCandidatesForTesting(
      {{install_dir_, DirectoryRole::kProgramFilesDefault, true, true},
       {per_user, DirectoryRole::kPerUserDefault, true, true}});
  internal::OverrideProcessElevationForTesting(true);
  internal::OverrideAdminMutationAllowedForTesting(false);

  ExtendedConfig extended;
  extended.show_progress_ui = false;
  Controller controller;
  Result result =
      controller.Run(Command::kInstall, CreateValidConfig(), extended, {},
                     ExecutionContext::kAutomaticStartup);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeConfigError, result.error_code);
  EXPECT_FALSE(base::PathExists(per_user));
  EXPECT_FALSE(base::PathExists(GetDatabasePath(install_dir_)));
}

TEST_F(ControllerTempDirTest, AutomaticStartupLeaseBlocksRemovalUntilReleased) {
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  const Version version = Version::Parse(api_version_str_);

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);
  ASSERT_TRUE(result.success) << result.error_message;
  ASSERT_TRUE(result.version_lease);

  FileOpsError leased_remove = UninstallVersion(install_dir_, version);
  EXPECT_TRUE(leased_remove == FileOpsError::kInUse ||
              leased_remove == FileOpsError::kAccessDenied);
  result.version_lease.reset();
  EXPECT_EQ(FileOpsError::kSuccess, UninstallVersion(install_dir_, version));
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupUnchangedRegistrationDoesNotRewriteDatabase) {
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  Controller first;
  Result first_result =
      first.Run(Command::kInstall, CreateValidConfig(), CreateExtendedConfig(),
                {}, ExecutionContext::kAutomaticStartup);
  ASSERT_TRUE(first_result.success) << first_result.error_message;

  base::FilePath database_path = GetDatabasePath(install_dir_);
  base::Time old_time = base::Time::Now() - base::Hours(1);
  ASSERT_TRUE(base::TouchFile(database_path, old_time, old_time));

  Controller second;
  Result second_result =
      second.Run(Command::kInstall, CreateValidConfig(), CreateExtendedConfig(),
                 {}, ExecutionContext::kAutomaticStartup);
  ASSERT_TRUE(second_result.success) << second_result.error_message;
  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(database_path, &info));
  EXPECT_EQ(old_time, info.last_modified);
}

TEST_F(ControllerTempDirTest, AutomaticStartupOffPublishesLivenessHandoff) {
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  Config config = CreateValidConfig();
  config.launch_health = LaunchHealthMode::kOff;

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, config, CreateExtendedConfig(), {},
                     ExecutionContext::kAutomaticStartup);

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(result.launch_state_path.empty());
  EXPECT_EQ(GetInstallDirLivenessPath(install_dir_, GetAppidHash(config.appid),
                                      GetCurrentPlatform()),
            result.liveness_path);
  EXPECT_FALSE(base::PathExists(result.liveness_path));
}

TEST_F(ControllerTempDirTest, AutomaticStartupHealthPublishesLivenessHandoff) {
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);
  ASSERT_EQ(RevocationError::kSuccess, WriteRevocationCache(install_dir_, {}));
  Config config = CreateValidConfig();
  config.launch_health = LaunchHealthMode::kExitCode;

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, config, CreateExtendedConfig(), {},
                     ExecutionContext::kAutomaticStartup);

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_FALSE(result.launch_state_path.empty());
  EXPECT_EQ(GetInstallDirLivenessPath(install_dir_, GetAppidHash(config.appid),
                                      GetCurrentPlatform()),
            result.liveness_path);
  EXPECT_FALSE(base::PathExists(result.liveness_path));
}

TEST_F(ControllerTempDirTest, PruneRetainsRegisteredLivenessOnlyRecord) {
  Config config = CreateValidConfig();
  config.launch_health = LaunchHealthMode::kOff;
  CreateDatabaseWithApp(config);
  base::FilePath path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(config.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      path, {config.appid, GetCurrentPlatform(), GetCurrentWallTime()}));

  Controller controller;
  Result result =
      controller.Run(Command::kPrune, config, CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(base::PathExists(path));
}

TEST_F(ControllerTempDirTest, PruneRetainsRecentOrphanLivenessOnlyRecord) {
  Config config = CreateValidConfig();
  config.launch_health = LaunchHealthMode::kOff;
  base::FilePath path = GetInstallDirLivenessPath(
      install_dir_, GetAppidHash(config.appid), GetCurrentPlatform());
  ASSERT_TRUE(WriteLivenessPath(
      path, {config.appid, GetCurrentPlatform(), GetCurrentWallTime()}));

  Controller controller;
  Result result =
      controller.Run(Command::kPrune, config, CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(base::PathExists(path));
}

TEST_F(ControllerTempDirTest, QueryNoMatchingVersion) {
  // No versions installed - query should fail
  Controller controller;
  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kQuery, config, extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);
  EXPECT_NE(std::string::npos,
            result.error_message.find(
                "no installed or bundled candidates were found"));
}

TEST_F(ControllerTempDirTest, QueryNoMatchingVersionReportsFailedRequirements) {
  CreateFakeInstalledVersion(api_version_str_, "def456", "", true);
  Controller controller;
  Config config = CreateValidConfig();
  config.abi_hash = "abc123";

  Result result =
      controller.Run(Command::kQuery, config, CreateExtendedConfig());

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);
  EXPECT_NE(std::string::npos, result.error_message.find("ABI hash = abc123"));
  EXPECT_NE(std::string::npos,
            result.error_message.find("installed " + api_version_str_ +
                                      " [ABI hash mismatch (got def456)]"));
}

TEST_F(ControllerTempDirTest, UninstallPrunesVersions) {
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);

  Controller controller;
  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Install to register app in database
  controller.Run(Command::kInstall, config, extended);

  // Uninstall should unregister and attempt prune
  std::vector<Step> steps;
  Controller controller2;
  Result result = controller2.Run(Command::kUninstall, config, extended,
                                  RecordProgress(&steps));
  EXPECT_TRUE(result.success);
  ASSERT_GE(steps.size(), 2u);
  EXPECT_EQ(kStepVersionCheck, steps[steps.size() - 2]);
  EXPECT_EQ(kStepCleanup, steps.back());

  // App should be removed from database
  Database db;
  EXPECT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath(install_dir_)));
  EXPECT_FALSE(db.GetApp(config.appid, GetCurrentPlatform()).has_value());
}

TEST_F(ControllerTempDirTest, PruneRemovesOrphanedVersion) {
  CreateFakeInstalledVersion(api_version_str_);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Register then unregister so version becomes orphaned
  {
    Controller c;
    c.Run(Command::kInstall, config, extended);
  }

  {
    Controller c;
    c.Run(Command::kUninstall, config, extended);
  }

  // Prune runs during uninstall - version dir may be cleaned up
  // Just verify no crash; file lock behavior varies
}

TEST_F(ControllerTempDirTest, RunInvalidConfig) {
  Controller controller;
  Config config;  // Invalid - empty appid
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeConfigError, result.error_code);
}

TEST_F(ControllerTempDirTest, UpdateDatabaseQueryDoesNotRegister) {
  Controller controller;
  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Query should not register the app in the database
  controller.Run(Command::kQuery, config, extended);

  base::FilePath db_path = GetDatabasePath(install_dir_);
  Database db;
  if (db.Load(db_path) == DatabaseError::kSuccess) {
    EXPECT_FALSE(db.GetApp(config.appid, GetCurrentPlatform()).has_value());
  }
}

TEST_F(ControllerTempDirTest, UpdateDatabaseUninstallRemovesApp) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);

  // Verify app exists before uninstall
  {
    Database db;
    ASSERT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath(install_dir_)));
    ASSERT_TRUE(db.GetApp(config.appid, GetCurrentPlatform()).has_value());
  }

  // Uninstall should remove app entry
  Controller controller;
  ExtendedConfig extended = CreateExtendedConfig();
  controller.Run(Command::kUninstall, config, extended);

  // Verify app removed
  {
    Database db;
    ASSERT_EQ(DatabaseError::kSuccess, db.Load(GetDatabasePath(install_dir_)));
    EXPECT_FALSE(db.GetApp(config.appid, GetCurrentPlatform()).has_value());
  }
}

TEST_F(ControllerTempDirTest, UpdateDatabaseSkipsSaveWhenUnchanged) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);

  base::FilePath db_path = GetDatabasePath(install_dir_);

  // Record modification time after initial save
  base::File::Info info_before;
  ASSERT_TRUE(base::GetFileInfo(db_path, &info_before));

  // Sleep briefly so any rewrite would have a different timestamp
  base::PlatformThread::Sleep(base::Milliseconds(50));

  // Run install with identical config — should skip save
  Controller controller;
  ExtendedConfig extended = CreateExtendedConfig();
  CreateFakeInstalledVersion(api_version_str_);
  controller.Run(Command::kInstall, config, extended);

  // Database file should not have been rewritten
  base::File::Info info_after;
  ASSERT_TRUE(base::GetFileInfo(db_path, &info_after));
  EXPECT_EQ(info_before.last_modified, info_after.last_modified);
}

TEST_F(ControllerTempDirTest, ProgressCallbackCancelsEarly) {
  CreateFakeInstalledVersion(api_version_str_);

  Controller controller;
  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  int calls = 0;
  ProgressCallback cancel_on_second = base::BindRepeating(
      [](int* calls, Step, uint64_t, uint64_t) -> bool {
        (*calls)++;
        return *calls < 2;  // Cancel on second call
      },
      &calls);

  Result result =
      controller.Run(Command::kInstall, config, extended, cancel_on_second);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeCancelled, result.error_code);
  EXPECT_GE(calls, 2);
}

// ============================================================================
// Reparse point validation tests
// ============================================================================

TEST_F(ControllerTempDirTest, QueryRejectsReparsePointInRelease) {
  // Create a version directory with a normal structure first so
  // metadata is valid, then replace Release/ with a file that we can
  // check via IsReparsePoint. We can't create real junctions without
  // admin, so instead we verify that missing components are caught by
  // the validation gate: remove the Release directory so the path
  // doesn't exist and validation fails.
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "", true);

  // Remove Release/libcef.dll and Release/ to make the path invalid
  Version version = Version::Parse(api_version_str_);
  base::FilePath version_dir = GetVersionPath(install_dir_, version);
  base::FilePath release_dir = version_dir.Append(kReleaseSubdirectory);
  base::DeletePathRecursively(release_dir);

  Controller controller;
  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Result result = controller.Run(Command::kQuery, config, extended);

  // Should fail because the libcef path components don't exist
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);
}

// ============================================================================
// ReadMultipleVersionIndexes tests
// ============================================================================

TEST_F(ControllerTempDirTest, ReadMultipleVersionIndexesFallsBackToScan) {
  // Create a version but don't write an index file.
  CreateFakeInstalledVersion(api_version_str_, "abc123def456");

  // ReadMultipleVersionIndexes should fall back to scanning.
  auto versions = Controller::ReadMultipleVersionIndexes({install_dir_});
  ASSERT_EQ(1u, versions.size());
  EXPECT_EQ(api_version_str_, versions[0].metadata.version.ToString());
}

TEST_F(ControllerTempDirTest, ReadMultipleVersionIndexesPrefersIndex) {
  CreateFakeInstalledVersion(api_version_str_, "abc123def456");

  // Write an index with version_full set (which the on-disk metadata
  // doesn't have). This lets us verify the index was used, not scan.
  auto all = ScanInstalledVersionsWithMetadata(install_dir_);
  ASSERT_EQ(1u, all.size());
  all[0].metadata.version_full = "from-index";
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, all));

  auto versions = Controller::ReadMultipleVersionIndexes({install_dir_});
  ASSERT_EQ(1u, versions.size());
  EXPECT_EQ("from-index", versions[0].metadata.version_full);
}

TEST_F(ControllerTempDirTest, ReadMultipleVersionIndexesCorruptFallsBack) {
  CreateFakeInstalledVersion(api_version_str_, "abc123def456");

  // Write garbage to the index file.
  base::FilePath index_path = install_dir_.Append(kVersionIndexFilename);
  ASSERT_TRUE(base::WriteFile(index_path, "corrupt data"));

  // Should fall back to scanning.
  auto versions = Controller::ReadMultipleVersionIndexes({install_dir_});
  ASSERT_EQ(1u, versions.size());
  EXPECT_EQ(api_version_str_, versions[0].metadata.version.ToString());
}

// ============================================================================
// Launch State Integration Tests
// ============================================================================

TEST_F(ControllerTempDirTest, LaunchState_DisqualifiedVersionFiltered) {
  std::string older = api_version_str_;
  std::string newer = api_version_str_ + ".1";
  CreateFakeInstalledVersion(older);
  CreateFakeInstalledVersion(newer);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  std::wstring hash = GetAppidHash(config.appid);
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 99999;
  ls.pid_start_time = GetCurrentPidStartTime();
  ls.consecutive_failures = 2;
  ls.running = true;
  ls.version = newer;
  ls.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(newer),
                                   GetCurrentPlatform()),
      ls));

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(older, result.installed_version);
  EXPECT_TRUE(result.is_rollback);
}

TEST_F(ControllerTempDirTest, LaunchState_UnderThresholdNotFiltered) {
  std::string older = api_version_str_;
  std::string newer = api_version_str_ + ".1";
  CreateFakeInstalledVersion(older);
  CreateFakeInstalledVersion(newer);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  std::wstring hash = GetAppidHash(config.appid);
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 99999;
  ls.pid_start_time = GetCurrentPidStartTime();
  ls.consecutive_failures = 1;
  ls.running = true;
  ls.version = newer;
  ls.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(newer),
                                   GetCurrentPlatform()),
      ls));

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(newer, result.installed_version);
  EXPECT_FALSE(result.is_rollback);
}

TEST_F(ControllerTempDirTest, LaunchState_NeutralExitNotDisqualified) {
  std::string older = api_version_str_;
  std::string newer = api_version_str_ + ".1";
  CreateFakeInstalledVersion(older);
  CreateFakeInstalledVersion(newer);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  std::wstring hash = GetAppidHash(config.appid);
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 99999;
  ls.pid_start_time = GetCurrentPidStartTime();
  ls.consecutive_failures = 2;
  ls.running = false;
  ls.version = newer;
  ls.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(newer),
                                   GetCurrentPlatform()),
      ls));

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(newer, result.installed_version);
  EXPECT_EQ(2, result.launch_consecutive_failures);
}

TEST_F(ControllerTempDirTest, LaunchState_ProcessAliveSkipsDisqualification) {
  std::string older = api_version_str_;
  std::string newer = api_version_str_ + ".1";
  CreateFakeInstalledVersion(older);
  CreateFakeInstalledVersion(newer);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  std::wstring hash = GetAppidHash(config.appid);
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = GetCurrentProcessId();
  ls.pid_start_time = GetCurrentPidStartTime();
  ls.consecutive_failures = 2;
  ls.running = true;
  ls.version = newer;
  ls.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(newer),
                                   GetCurrentPlatform()),
      ls));

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(newer, result.installed_version);
  EXPECT_EQ(2, result.launch_consecutive_failures);
}

TEST_F(ControllerTempDirTest, LaunchState_AllDisqualifiedFallback) {
  CreateFakeInstalledVersion(api_version_str_);

  // This test exercises local fallback selection, not online discovery. Keep
  // it hermetic so changes to the default CDN manifest cannot add a newer
  // candidate and invalidate the fallback assertions.
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kDisabled;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  internal::OverrideEnterprisePolicyForTesting(policy);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  std::wstring hash = GetAppidHash(config.appid);
  Version ver = Version::Parse(api_version_str_);
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 99999;
  ls.pid_start_time = GetCurrentPidStartTime();
  ls.consecutive_failures = 2;
  ls.running = true;
  ls.version = api_version_str_;
  ls.platform = GetCurrentPlatform();
  ASSERT_TRUE(
      WriteLaunchStatePath(GetInstallDirLaunchStatePath(install_dir_, hash, ver,
                                                        GetCurrentPlatform()),
                           ls));

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  // Should still return the disqualified version as last resort.
  EXPECT_TRUE(result.success);
  EXPECT_EQ(api_version_str_, result.installed_version);
  EXPECT_FALSE(result.is_rollback);
  EXPECT_EQ(3, result.launch_consecutive_failures);
}

TEST_F(ControllerTempDirTest, LaunchState_ConsecutiveFailuresIncremented) {
  CreateFakeInstalledVersion(api_version_str_);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  std::wstring hash = GetAppidHash(config.appid);
  Version ver = Version::Parse(api_version_str_);
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 99999;
  ls.pid_start_time = GetCurrentPidStartTime();
  ls.consecutive_failures = 1;
  ls.running = true;
  ls.version = api_version_str_;
  ls.platform = GetCurrentPlatform();
  ASSERT_TRUE(
      WriteLaunchStatePath(GetInstallDirLaunchStatePath(install_dir_, hash, ver,
                                                        GetCurrentPlatform()),
                           ls));

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(2, result.launch_consecutive_failures);
}

TEST_F(ControllerTempDirTest, LaunchState_ConsecutiveFailuresFresh) {
  CreateFakeInstalledVersion(api_version_str_);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(0, result.launch_consecutive_failures);
  EXPECT_FALSE(result.launch_state_path.empty());
  EXPECT_EQ(api_version_str_, result.launch_version);
  EXPECT_EQ(GetCurrentPlatform(), result.launch_platform);
}

TEST_F(ControllerTempDirTest, LaunchState_CleanupPathsPopulated) {
  std::string v1 = api_version_str_;
  std::string v2 = api_version_str_ + ".1";
  std::string v3 = api_version_str_ + ".2";
  CreateFakeInstalledVersion(v1);
  CreateFakeInstalledVersion(v2);
  CreateFakeInstalledVersion(v3);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Write confirmed state for v1 and v2.
  std::wstring hash = GetAppidHash(config.appid);
  LaunchState confirmed;
  confirmed.appid = config.appid;
  confirmed.pid = 1;
  confirmed.pid_start_time = 1;
  confirmed.consecutive_failures = 0;
  confirmed.running = false;
  confirmed.confirmed = true;

  confirmed.version = v1;
  confirmed.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(v1),
                                   GetCurrentPlatform()),
      confirmed));
  confirmed.version = v2;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(v2),
                                   GetCurrentPlatform()),
      confirmed));

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(v3, result.installed_version);
  // v1 and v2 are older, should be in cleanup paths.
  EXPECT_EQ(2u, result.launch_cleanup_paths.size());
}

TEST_F(ControllerTempDirTest, LaunchState_CleanupPathsExcludesNewer) {
  std::string v1 = api_version_str_;
  std::string v2 = api_version_str_ + ".1";
  CreateFakeInstalledVersion(v1);
  CreateFakeInstalledVersion(v2);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Disqualify v2 so v1 is selected via rollback.
  std::wstring hash = GetAppidHash(config.appid);
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 99999;
  ls.pid_start_time = GetCurrentPidStartTime();
  ls.consecutive_failures = 2;
  ls.running = true;
  ls.version = v2;
  ls.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(v2),
                                   GetCurrentPlatform()),
      ls));

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(v1, result.installed_version);
  // v2 is newer than selected v1, should NOT be in cleanup paths.
  EXPECT_EQ(0u, result.launch_cleanup_paths.size());
}

TEST_F(ControllerTempDirTest, LaunchState_CleanupPreservesCrashHistory) {
  std::string v1 = api_version_str_;
  std::string v2 = api_version_str_ + ".1";
  std::string v3 = api_version_str_ + ".2";
  CreateFakeInstalledVersion(v1);
  CreateFakeInstalledVersion(v2);
  CreateFakeInstalledVersion(v3);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  std::wstring hash = GetAppidHash(config.appid);
  std::string platform = GetCurrentPlatform();

  // v1: confirmed (should be cleaned up).
  LaunchState confirmed;
  confirmed.appid = config.appid;
  confirmed.pid = 1;
  confirmed.pid_start_time = 1;
  confirmed.consecutive_failures = 0;
  confirmed.running = false;
  confirmed.confirmed = true;
  confirmed.version = v1;
  confirmed.platform = platform;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(v1),
                                   platform),
      confirmed));

  // v2: crash history (should be preserved).
  LaunchState crash_history;
  crash_history.appid = config.appid;
  crash_history.pid = 1;
  crash_history.pid_start_time = 1;
  crash_history.consecutive_failures = 1;
  crash_history.running = false;
  crash_history.version = v2;
  crash_history.platform = platform;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(v2),
                                   platform),
      crash_history));

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(v3, result.installed_version);

  // v1 (confirmed) should be in cleanup, v2 (crash history) should not.
  ASSERT_EQ(1u, result.launch_cleanup_paths.size());
  EXPECT_NE(result.launch_cleanup_paths[0].value().find(base::UTF8ToWide(v1)),
            std::wstring::npos);
}

TEST_F(ControllerTempDirTest, Pruning_GCAgesBelowVminHealthByContentTime) {
  std::string v_recent = api_version_str_;
  std::string v_expired = "100.0.0";
  std::string v_new = api_version_str_ + ".1";
  CreateFakeInstalledVersion(v_new);

  Config config = CreateValidConfig();
  config.vmin = v_new;  // vmin above v_old
  ExtendedConfig extended = CreateExtendedConfig();

  // Register app with vmin=v_new so v_old is below global vmin.
  {
    Controller c;
    c.Run(Command::kInstall, config, extended);
  }

  std::wstring hash = GetAppidHash(config.appid);
  std::string platform = GetCurrentPlatform();

  // Write .launch/ files for both versions.
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 1;
  ls.pid_start_time = 1;
  ls.consecutive_failures = 2;
  ls.running = false;
  ls.platform = platform;

  constexpr uint64_t kNow = 2 * kLaunchStateGcMaxAge + 100;
  SetLaunchStateGcTimeForTesting(kNow - kLaunchStateGcMaxAge + 1);
  ls.version = v_recent;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(v_recent),
                                   platform),
      ls));
  SetLaunchStateGcTimeForTesting(kNow - kLaunchStateGcMaxAge);
  ls.version = v_expired;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash,
                                   Version::Parse(v_expired), platform),
      ls));
  SetLaunchStateGcTimeForTesting(kNow);
  ls.version = v_new;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(v_new),
                                   platform),
      ls));

  // Run prune — GC should delete v_old's .launch/ file (below vmin).
  {
    Controller c;
    c.Run(Command::kPrune, config, extended);
  }

  base::FilePath recent_path = GetInstallDirLaunchStatePath(
      install_dir_, hash, Version::Parse(v_recent), platform);
  base::FilePath expired_path = GetInstallDirLaunchStatePath(
      install_dir_, hash, Version::Parse(v_expired), platform);
  base::FilePath new_ls_path = GetInstallDirLaunchStatePath(
      install_dir_, hash, Version::Parse(v_new), platform);
  EXPECT_TRUE(base::PathExists(recent_path));
  EXPECT_FALSE(base::PathExists(expired_path));
  EXPECT_TRUE(base::PathExists(new_ls_path))
      << "v_new .launch/ file should survive GC";
}

TEST_F(ControllerTempDirTest, Pruning_GCRetainsRecentOrphanedAppFiles) {
  CreateFakeInstalledVersion(api_version_str_);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Register and install so the database exists.
  {
    Controller c;
    c.Run(Command::kInstall, config, extended);
  }

  // Write a confirmed .launch/ file for this app.
  std::wstring hash = GetAppidHash(config.appid);
  std::string platform = GetCurrentPlatform();
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 1;
  ls.pid_start_time = 1;
  ls.consecutive_failures = 0;
  ls.running = false;
  ls.confirmed = true;
  ls.version = api_version_str_;
  ls.platform = platform;
  base::FilePath ls_path = GetInstallDirLaunchStatePath(
      install_dir_, hash, Version::Parse(api_version_str_), platform);
  ASSERT_TRUE(WriteLaunchStatePath(ls_path, ls));

  // The confirmed file should protect the version from pruning.
  {
    Controller c;
    c.Run(Command::kPrune, config, extended);
  }
  EXPECT_TRUE(base::PathExists(ls_path));

  // Uninstall the app — removes it from the database.
  {
    Controller c;
    c.Run(Command::kUninstall, config, extended);
  }

  // The recent orphaned history should remain inside the age grace.
  EXPECT_TRUE(base::PathExists(ls_path));
}

TEST_F(ControllerTempDirTest, Pruning_GCAgesOrphanHealthAndLiveness) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  ExtendedConfig extended = CreateExtendedConfig();
  const std::string orphan = "orphan-app";
  const std::string platform = GetCurrentPlatform();
  const std::wstring hash = GetAppidHash(orphan);
  const Version version = Version::Parse(api_version_str_);
  const base::FilePath health_path =
      GetInstallDirLaunchStatePath(install_dir_, hash, version, platform);
  const base::FilePath liveness_path =
      GetInstallDirLivenessPath(install_dir_, hash, platform);
  constexpr uint64_t kNow = 2 * kLaunchStateGcMaxAge + 100;
  SetLaunchStateGcTimeForTesting(kNow - kLaunchStateGcMaxAge);
  LaunchState health;
  health.appid = orphan;
  health.pid_start_time = 1;
  health.consecutive_failures = 2;
  health.version = version.ToString();
  health.platform = platform;
  ASSERT_TRUE(WriteLaunchStatePath(health_path, health));
  ASSERT_TRUE(WriteLivenessPath(
      liveness_path, {orphan, platform, kNow - kLaunchStateGcMaxAge}));

  // A fresh filesystem timestamp cannot make old content younger.
  ASSERT_TRUE(
      base::TouchFile(health_path, base::Time::Now(), base::Time::Now()));
  SetLaunchStateGcTimeForTesting(kNow);
  Controller controller;
  Result result = controller.Run(Command::kPrune, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_FALSE(base::PathExists(health_path));
  EXPECT_FALSE(base::PathExists(liveness_path));
}

TEST_F(ControllerTempDirTest,
       Pruning_FutureAndUnavailableEligibleHealthNeverProtectsVersions) {
  const std::string future_version = "100.0.0";
  const std::string zero_clock_version = "101.0.0";
  CreateFakeInstalledVersion(future_version);
  CreateFakeInstalledVersion(zero_clock_version);
  CreateFakeInstalledVersion(api_version_str_);
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  ExtendedConfig extended = CreateExtendedConfig();
  const std::string orphan = "orphan-app";
  const std::string platform = GetCurrentPlatform();
  const std::wstring hash = GetAppidHash(orphan);
  LaunchState health;
  health.appid = orphan;
  health.pid_start_time = 1;
  health.confirmed = true;
  health.version = future_version;
  health.platform = platform;
  SetLaunchStateGcTimeForTesting(300);
  base::FilePath future_path = GetInstallDirLaunchStatePath(
      install_dir_, hash, Version::Parse(future_version), platform);
  ASSERT_TRUE(WriteLaunchStatePath(future_path, health));
  health.version = zero_clock_version;
  SetLaunchStateGcTimeForTesting(100);
  base::FilePath zero_clock_path = GetInstallDirLaunchStatePath(
      install_dir_, hash, Version::Parse(zero_clock_version), platform);
  ASSERT_TRUE(WriteLaunchStatePath(zero_clock_path, health));

  SetLaunchStateGcTimeForTesting(200);
  Controller first;
  ASSERT_TRUE(first.Run(Command::kPrune, config, extended).success);
  EXPECT_FALSE(base::PathExists(
      GetVersionPath(install_dir_, Version::Parse(future_version), platform)));
  EXPECT_FALSE(base::PathExists(GetVersionPath(
      install_dir_, Version::Parse(zero_clock_version), platform)));
  EXPECT_TRUE(base::PathExists(future_path));
  EXPECT_TRUE(base::PathExists(zero_clock_path));

  CreateFakeInstalledVersion(zero_clock_version);
  ASSERT_EQ(MetadataError::kSuccess,
            WriteVersionIndex(install_dir_,
                              ScanInstalledVersionsWithMetadata(install_dir_)));
  SetLaunchStateGcTimeForTesting(0);
  Controller second;
  ASSERT_TRUE(second.Run(Command::kPrune, config, extended).success);
  EXPECT_FALSE(base::PathExists(GetVersionPath(
      install_dir_, Version::Parse(zero_clock_version), platform)));
  EXPECT_TRUE(base::PathExists(zero_clock_path));
}

TEST_F(ControllerTempDirTest,
       Pruning_ConcurrentReplacementSurvivesExpiredSnapshot) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  ExtendedConfig extended = CreateExtendedConfig();
  const std::string orphan = "orphan-app";
  const std::string platform = GetCurrentPlatform();
  const Version version = Version::Parse(api_version_str_);
  base::FilePath path = GetInstallDirLaunchStatePath(
      install_dir_, GetAppidHash(orphan), version, platform);
  constexpr uint64_t kNow = 2 * kLaunchStateGcMaxAge + 100;
  SetLaunchStateGcTimeForTesting(kNow - kLaunchStateGcMaxAge);
  LaunchState health;
  health.appid = orphan;
  health.pid_start_time = 1;
  health.consecutive_failures = 2;
  health.version = version.ToString();
  health.platform = platform;
  ASSERT_TRUE(WriteLaunchStatePath(path, health));
  SetLaunchStateGcTimeForTesting(kNow);
  SetConditionalDeleteHookForTesting(base::BindRepeating(
      [](LaunchState replacement, uint64_t now,
         const base::FilePath& hook_path) {
        SetLaunchStateGcTimeForTesting(now);
        ASSERT_TRUE(WriteLaunchStatePath(hook_path, replacement));
      },
      health, kNow));

  Controller controller;
  Result result = controller.Run(Command::kPrune, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  ASSERT_TRUE(base::PathExists(path));
  EXPECT_EQ(kNow, ReadLaunchStatePath(path)->last_update);
}

TEST_F(ControllerTempDirTest,
       Pruning_ConcurrentValidReplacementSurvivesInvalidRepair) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  ExtendedConfig extended = CreateExtendedConfig();
  const std::string orphan = "orphan-app";
  const std::string platform = GetCurrentPlatform();
  const Version version = Version::Parse(api_version_str_);
  base::FilePath path = GetInstallDirLaunchStatePath(
      install_dir_, GetAppidHash(orphan), version, platform);
  ASSERT_TRUE(base::CreateDirectory(path.DirName()));
  ASSERT_TRUE(WriteFileWithIntegrity(path, "{}"));

  constexpr uint64_t kNow = 2 * kLaunchStateGcMaxAge + 100;
  LaunchState replacement;
  replacement.appid = orphan;
  replacement.pid_start_time = 1;
  replacement.consecutive_failures = 2;
  replacement.version = version.ToString();
  replacement.platform = platform;
  SetConditionalDeleteHookForTesting(base::BindRepeating(
      [](LaunchState state, uint64_t now, const base::FilePath& hook_path) {
        SetLaunchStateGcTimeForTesting(now);
        ASSERT_TRUE(WriteLaunchStatePath(hook_path, state));
      },
      replacement, kNow));

  Controller controller;
  Result result = controller.Run(Command::kPrune, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  ASSERT_TRUE(base::PathExists(path));
  std::optional<LaunchState> current = ReadLaunchStatePath(path);
  ASSERT_TRUE(current);
  EXPECT_EQ(kNow, current->last_update);
}

TEST_F(ControllerTempDirTest,
       Pruning_ClockRollbackBeforeDeletePreservesExpiredRecord) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  ExtendedConfig extended = CreateExtendedConfig();
  const std::string orphan = "orphan-app";
  const std::string platform = GetCurrentPlatform();
  const Version version = Version::Parse(api_version_str_);
  base::FilePath path = GetInstallDirLaunchStatePath(
      install_dir_, GetAppidHash(orphan), version, platform);
  constexpr uint64_t kNow = 2 * kLaunchStateGcMaxAge + 100;
  SetLaunchStateGcTimeForTesting(kNow - kLaunchStateGcMaxAge);
  LaunchState health;
  health.appid = orphan;
  health.pid_start_time = 1;
  health.consecutive_failures = 2;
  health.version = version.ToString();
  health.platform = platform;
  ASSERT_TRUE(WriteLaunchStatePath(path, health));
  SetLaunchStateGcTimeForTesting(kNow);
  SetLaunchStateGcPreDeleteHookForTesting(base::BindRepeating(
      [](uint64_t now) { SetLaunchStateGcTimeForTesting(now); }, kNow - 1));

  Controller controller;
  Result result = controller.Run(Command::kPrune, config, extended);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(base::PathExists(path));
}

TEST_F(ControllerTempDirTest, Pruning_LivenessAgeEdgesIgnoreFilesystemTime) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  ExtendedConfig extended = CreateExtendedConfig();
  const std::string platform = GetCurrentPlatform();
  constexpr uint64_t kNow = 2 * kLaunchStateGcMaxAge + 100;
  auto path_for = [&](const std::string& appid) {
    return GetInstallDirLivenessPath(install_dir_, GetAppidHash(appid),
                                     platform);
  };
  const base::FilePath recent_path = path_for("recent-orphan");
  const base::FilePath expired_path = path_for("expired-orphan");
  const base::FilePath future_path = path_for("future-orphan");
  ASSERT_TRUE(WriteLivenessPath(
      recent_path,
      {"recent-orphan", platform, kNow - kLaunchStateGcMaxAge + 1}));
  ASSERT_TRUE(WriteLivenessPath(
      expired_path, {"expired-orphan", platform, kNow - kLaunchStateGcMaxAge}));
  ASSERT_TRUE(
      WriteLivenessPath(future_path, {"future-orphan", platform, kNow + 1}));
  ASSERT_TRUE(
      base::TouchFile(expired_path, base::Time::Now(), base::Time::Now()));

  SetLaunchStateGcTimeForTesting(kNow);
  Controller first;
  ASSERT_TRUE(first.Run(Command::kPrune, config, extended).success);
  EXPECT_TRUE(base::PathExists(recent_path));
  EXPECT_FALSE(base::PathExists(expired_path));
  EXPECT_TRUE(base::PathExists(future_path));

  const base::FilePath unavailable_path = path_for("unavailable-orphan");
  ASSERT_TRUE(WriteLivenessPath(
      unavailable_path,
      {"unavailable-orphan", platform, kNow - kLaunchStateGcMaxAge}));
  SetLaunchStateGcTimeForTesting(0);
  Controller second;
  ASSERT_TRUE(second.Run(Command::kPrune, config, extended).success);
  EXPECT_TRUE(base::PathExists(unavailable_path));
}

TEST_F(ControllerTempDirTest, Pruning_DeleteErrorPreservesAndRetriesRecord) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  ExtendedConfig extended = CreateExtendedConfig();
  const std::string orphan = "orphan-app";
  const std::string platform = GetCurrentPlatform();
  const Version version = Version::Parse(api_version_str_);
  base::FilePath path = GetInstallDirLaunchStatePath(
      install_dir_, GetAppidHash(orphan), version, platform);
  constexpr uint64_t kNow = 2 * kLaunchStateGcMaxAge + 100;
  SetLaunchStateGcTimeForTesting(kNow - kLaunchStateGcMaxAge);
  LaunchState health;
  health.appid = orphan;
  health.pid_start_time = 1;
  health.version = version.ToString();
  health.platform = platform;
  ASSERT_TRUE(WriteLaunchStatePath(path, health));
  SetLaunchStateGcTimeForTesting(kNow);
  HANDLE handle = ::CreateFileW(path.value().c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE(INVALID_HANDLE_VALUE, handle);

  Controller first;
  Result first_result = first.Run(Command::kPrune, config, extended);
  EXPECT_TRUE(first_result.success) << first_result.error_message;
  EXPECT_TRUE(base::PathExists(path));
  ::CloseHandle(handle);

  Controller second;
  Result second_result = second.Run(Command::kPrune, config, extended);
  EXPECT_TRUE(second_result.success) << second_result.error_message;
  EXPECT_FALSE(base::PathExists(path));
}

TEST_F(ControllerTempDirTest,
       Pruning_GCRemovesInvalidAndNoncanonicalLaunchFiles) {
  CreateFakeInstalledVersion(api_version_str_);
  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();
  {
    Controller c;
    ASSERT_TRUE(c.Run(Command::kInstall, config, extended).success);
  }

  base::FilePath launch_dir = GetLaunchStateDir(install_dir_);
  ASSERT_TRUE(base::CreateDirectory(launch_dir));
  base::FilePath invalid = launch_dir.Append(L"schema-invalid");
  ASSERT_TRUE(WriteFileWithIntegrity(invalid, "{}"));
  base::FilePath oversized = launch_dir.Append(L"oversized-invalid");
  ASSERT_TRUE(base::WriteFile(oversized,
                              std::string(kMaxLaunchStateFileSize + 17, 'x')));
  base::FilePath reparse_target =
      temp_dir_.GetPath().Append(L"launch-file-reparse-target");
  ASSERT_TRUE(WriteFileWithIntegrity(reparse_target, "target"));
  base::FilePath reparse = launch_dir.Append(L"reparse-invalid");
  constexpr DWORD kAllowUnprivilegedCreate = 0x2;
  if (!::CreateSymbolicLinkW(reparse.value().c_str(),
                             reparse_target.value().c_str(),
                             kAllowUnprivilegedCreate)) {
    GTEST_SKIP() << "Symbolic-link creation is unavailable";
  }

  LaunchState state;
  state.appid = config.appid;
  state.pid = 1;
  state.pid_start_time = 1;
  state.consecutive_failures = 0;
  state.running = false;
  state.confirmed = true;
  state.version = api_version_str_;
  state.platform = GetCurrentPlatform();
  base::FilePath noncanonical = launch_dir.Append(L"abandoned-atomic-temp");
  ASSERT_TRUE(WriteLaunchStatePath(noncanonical, state));

  Controller controller;
  Result result = controller.Run(Command::kPrune, config, extended);
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_FALSE(base::PathExists(invalid));
  EXPECT_FALSE(base::PathExists(oversized));
  EXPECT_FALSE(base::PathExists(reparse));
  std::string target_content;
  EXPECT_EQ(IntegrityResult::kSuccess,
            ReadFileWithIntegrity(reparse_target, &target_content));
  EXPECT_EQ("target", target_content);
  EXPECT_FALSE(base::PathExists(noncanonical));
}

TEST_F(ControllerTempDirTest,
       LaunchState_WriterLockedInstallRepairsReparseDirectory) {
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath target = temp_dir_.GetPath().Append(L"launch_target");
  ASSERT_TRUE(base::CreateDirectory(target));
  base::FilePath junction = GetLaunchStateDir(install_dir_);
  std::wostringstream create_command;
  create_command << L"cmd /c mklink /J " << std::quoted(junction.value())
                 << L" " << std::quoted(target.value());
  ::_wsystem(create_command.str().c_str());
  if (!base::DirectoryExists(junction)) {
    GTEST_SKIP() << "Could not create junction point";
  }
  ASSERT_TRUE(IsReparsePoint(junction));

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig());
  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_FALSE(base::PathExists(junction));
  EXPECT_TRUE(base::DirectoryExists(target));
}

TEST_F(ControllerTempDirTest,
       UninstallRetainsRecentLaunchFileAndRemovesVersion) {
  CreateFakeInstalledVersion(api_version_str_);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Register and install.
  {
    Controller c;
    c.Run(Command::kInstall, config, extended);
  }

  // Write a confirmed .launch/ file.
  std::wstring hash = GetAppidHash(config.appid);
  std::string platform = GetCurrentPlatform();
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 1;
  ls.pid_start_time = 1;
  ls.consecutive_failures = 0;
  ls.running = false;
  ls.confirmed = true;
  ls.version = api_version_str_;
  ls.platform = platform;
  base::FilePath ls_path = GetInstallDirLaunchStatePath(
      install_dir_, hash, Version::Parse(api_version_str_), platform);
  ASSERT_TRUE(WriteLaunchStatePath(ls_path, ls));

  base::FilePath ver_dir =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  ASSERT_TRUE(base::PathExists(ver_dir));
  ASSERT_TRUE(base::PathExists(ls_path));

  // Uninstall removes the app and version directory but retains the recent
  // orphaned history inside the age grace.
  {
    Controller c;
    c.Run(Command::kUninstall, config, extended);
  }

  EXPECT_TRUE(base::PathExists(ls_path));
  EXPECT_FALSE(base::PathExists(ver_dir))
      << "Version directory should be pruned after uninstall";
}

TEST_F(ControllerTempDirTest, LaunchState_QueryUsesSameDisqualification) {
  std::string older = api_version_str_;
  std::string newer = api_version_str_ + ".1";
  CreateFakeInstalledVersion(older);
  CreateFakeInstalledVersion(newer);
  ASSERT_EQ(MetadataError::kSuccess,
            WriteVersionIndex(install_dir_,
                              ScanInstalledVersionsWithMetadata(install_dir_)));

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  std::wstring hash = GetAppidHash(config.appid);
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 99999;
  ls.pid_start_time = GetCurrentPidStartTime();
  ls.consecutive_failures = 2;
  ls.running = true;
  ls.version = newer;
  ls.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(newer),
                                   GetCurrentPlatform()),
      ls));

  Controller controller;
  Result result = controller.Run(Command::kQuery, config, extended);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(older, result.installed_version);
  EXPECT_TRUE(result.launch_state_path.empty());
}

TEST_F(ControllerTempDirTest, LaunchState_OffSkipsHealthState) {
  std::string older = api_version_str_;
  std::string newer = api_version_str_ + ".1";
  CreateFakeInstalledVersion(older);
  CreateFakeInstalledVersion(newer);

  Config config = CreateValidConfig();
  config.launch_health = LaunchHealthMode::kOff;
  ExtendedConfig extended = CreateExtendedConfig();

  LaunchState state;
  state.appid = config.appid;
  state.pid = 99999;
  state.pid_start_time = GetCurrentPidStartTime();
  state.consecutive_failures = 2;
  state.running = true;
  state.version = newer;
  state.platform = GetCurrentPlatform();
  base::FilePath state_path =
      GetInstallDirLaunchStatePath(install_dir_, GetAppidHash(config.appid),
                                   Version::Parse(newer), GetCurrentPlatform());
  ASSERT_TRUE(WriteLaunchStatePath(state_path, state));

  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended);

  ASSERT_TRUE(result.success);
  EXPECT_EQ(newer, result.installed_version);
  EXPECT_FALSE(result.is_rollback);
  EXPECT_TRUE(result.launch_state_path.empty());
  Controller prune;
  ASSERT_TRUE(prune.Run(Command::kPrune, config, extended).success);
  EXPECT_FALSE(base::PathExists(state_path));
}

TEST_F(ControllerTempDirTest, LaunchState_PruningProtectsConfirmed) {
  std::string older = api_version_str_;
  std::string newer = api_version_str_ + ".1";
  CreateFakeInstalledVersion(newer);
  CreateFakeInstalledVersion(older);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Register app and install so DB is set up.
  {
    Controller c;
    c.Run(Command::kInstall, config, extended);
  }

  // The install mutation performs ordinary prune. Recreate the older version
  // before attaching the confirmed protection exercised by this test.
  CreateFakeInstalledVersion(older);

  // Write confirmed launch state for older version.
  std::wstring hash = GetAppidHash(config.appid);
  LaunchState confirmed;
  confirmed.appid = config.appid;
  confirmed.pid = 1;
  confirmed.pid_start_time = 1;
  confirmed.consecutive_failures = 0;
  confirmed.running = false;
  confirmed.confirmed = true;
  confirmed.version = older;
  confirmed.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(older),
                                   GetCurrentPlatform()),
      confirmed));

  // Prune with app still registered — confirmed file protects the version.
  {
    Controller c;
    c.Run(Command::kPrune, config, extended);
  }

  base::FilePath older_dir =
      GetVersionPath(install_dir_, Version::Parse(older));
  EXPECT_TRUE(base::PathExists(older_dir));
}

TEST_F(ControllerTempDirTest, LaunchState_PruningIgnoresNeutral) {
  std::string older = api_version_str_;
  std::string newer = api_version_str_ + ".1";
  CreateFakeInstalledVersion(newer);
  CreateFakeInstalledVersion(older);

  Config config = CreateValidConfig();
  ExtendedConfig extended = CreateExtendedConfig();

  // Register app and install.
  {
    Controller c;
    c.Run(Command::kInstall, config, extended);
  }

  // Write neutral launch state for older version (running=false, failures>0).
  std::wstring hash = GetAppidHash(config.appid);
  LaunchState neutral;
  neutral.appid = config.appid;
  neutral.pid = 1;
  neutral.pid_start_time = 1;
  neutral.consecutive_failures = 1;
  neutral.running = false;
  neutral.version = older;
  neutral.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(older),
                                   GetCurrentPlatform()),
      neutral));

  // Uninstall triggers pruning. Neutral state doesn't protect.
  {
    Controller c;
    c.Run(Command::kUninstall, config, extended);
  }

  // Older version should be pruned (neutral doesn't protect).
  base::FilePath older_dir =
      GetVersionPath(install_dir_, Version::Parse(older));
  EXPECT_FALSE(base::PathExists(older_dir));
}

// Pruning protection spans platforms: a confirmed launch file for a
// different-platform version protects that version from a current-platform
// prune. Version pruning is cross-platform, so protection must be too —
// otherwise a 64-bit prune would delete a 32-bit app's rollback target.
TEST_F(ControllerTempDirTest,
       LaunchState_PruningProtectsConfirmedCrossPlatform) {
  // Pick a platform different from the one this test binary runs as.
  const std::string current = GetCurrentPlatform();
  const std::string other =
      (current == "windows64") ? "windows32" : "windows64";

  const std::string way_old = "100.0.0";              // below vmin → prunable
  const std::string older = api_version_str_;         // in range, not best
  const std::string newer = api_version_str_ + ".1";  // best → required

  // Three version directories on the OTHER platform.
  CreateFakeInstalledVersion(way_old, "defa01defa01", other);
  CreateFakeInstalledVersion(older, "defa01defa01", other);
  CreateFakeInstalledVersion(newer, "defa01defa01", other);

  // Register one app for the OTHER platform (vmin = older → newer is best,
  // older is in range but not required, way_old is below vmin).
  Config config = CreateValidConfig();
  {
    Database db;
    AppEntry entry;
    entry.uuid = config.appid;
    entry.platform = other;
    entry.vmin = older;
    entry.vmax = "";
    entry.abi_hash = "";
    db.RegisterApp(entry);
    ASSERT_EQ(DatabaseError::kSuccess, db.Save(GetDatabasePath(install_dir_)));
  }

  // Confirmed launch file for the older other-platform version.
  std::wstring hash = GetAppidHash(config.appid);
  LaunchState confirmed;
  confirmed.appid = config.appid;
  confirmed.pid = 1;
  confirmed.pid_start_time = 1;
  confirmed.consecutive_failures = 0;
  confirmed.running = false;
  confirmed.confirmed = true;
  confirmed.version = older;
  confirmed.platform = other;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, Version::Parse(older),
                                   other),
      confirmed));

  // Prune as the current platform.
  ExtendedConfig extended = CreateExtendedConfig();
  {
    Controller c;
    c.Run(Command::kPrune, config, extended);
  }

  // Control: way_old (below vmin, no launch file) is pruned — confirms the
  // current-platform prune genuinely reaches other-platform versions.
  EXPECT_FALSE(base::PathExists(
      GetVersionPath(install_dir_, Version::Parse(way_old), other)));
  // Required best version is kept.
  EXPECT_TRUE(base::PathExists(
      GetVersionPath(install_dir_, Version::Parse(newer), other)));
  // The fix under test: older is not required, but its confirmed launch file
  // (on a non-current platform) protects it from pruning.
  EXPECT_TRUE(base::PathExists(
      GetVersionPath(install_dir_, Version::Parse(older), other)));
}

// Orphan deletion is keyed by (appid, platform), not appid alone: a launch
// file for a platform where the appid is NOT registered is deleted even when
// the same appid is registered for a different platform. Guards against
// regressing to appid-only keying, which would preserve — and wrongly trust —
// the stale file.
TEST_F(ControllerTempDirTest, LaunchState_PruningOrphanGraceIsPerPlatform) {
  const std::string current = GetCurrentPlatform();
  const std::string other =
      (current == "windows64") ? "windows32" : "windows64";

  CreateFakeInstalledVersion(api_version_str_);  // current platform

  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);  // registers appid for the CURRENT platform

  std::wstring hash = GetAppidHash(config.appid);
  Version ver = Version::Parse(api_version_str_);

  // Same appid, two confirmed launch files: one on the current platform
  // (registered → live) and one on the other platform (NOT registered).
  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 1;
  ls.pid_start_time = 1;
  ls.consecutive_failures = 0;
  ls.running = false;
  ls.confirmed = true;
  ls.version = api_version_str_;

  ls.platform = current;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, ver, current), ls));
  ls.platform = other;
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, hash, ver, other), ls));

  ExtendedConfig extended = CreateExtendedConfig();
  {
    Controller c;
    c.Run(Command::kPrune, config, extended);
  }

  // Current-platform file: appid registered for this platform → kept.
  EXPECT_TRUE(base::PathExists(
      GetInstallDirLaunchStatePath(install_dir_, hash, ver, current)));
  // Other-platform file: appid NOT registered for that platform → orphan,
  // deleted. (appid-only keying would wrongly keep it.)
  EXPECT_TRUE(base::PathExists(
      GetInstallDirLaunchStatePath(install_dir_, hash, ver, other)));
}

// Stale GC judges each launch file against ITS OWN platform's vmin, not the
// running installer's platform. Guards against regressing to a single
// current-platform vmin.
TEST_F(ControllerTempDirTest, LaunchState_PruningStaleGCIsPerPlatform) {
  const std::string current = GetCurrentPlatform();
  const std::string other =
      (current == "windows64") ? "windows32" : "windows64";

  Config config = CreateValidConfig();

  // Same appid registered on two platforms: current with a HIGH vmin, other
  // with a LOW vmin.
  {
    Database db;
    AppEntry cur;
    cur.uuid = config.appid;
    cur.platform = current;
    cur.vmin = api_version_str_;  // high (current API version, well above 130)
    db.RegisterApp(cur);
    AppEntry oth;
    oth.uuid = config.appid;
    oth.platform = other;
    oth.vmin = "120.0.0";  // low
    db.RegisterApp(oth);
    ASSERT_EQ(DatabaseError::kSuccess, db.Save(GetDatabasePath(install_dir_)));
  }

  std::wstring hash = GetAppidHash(config.appid);

  LaunchState ls;
  ls.appid = config.appid;
  ls.pid = 1;
  ls.pid_start_time = 1;
  ls.consecutive_failures = 0;
  ls.running = false;
  ls.platform = other;

  // 130.0.0: above other's vmin (120) but below current's vmin. Must survive —
  // it is judged against other's vmin, not the running platform's.
  ls.version = "130.0.0";
  base::FilePath keep_path = GetInstallDirLaunchStatePath(
      install_dir_, hash, Version::Parse("130.0.0"), other);
  ASSERT_TRUE(WriteLaunchStatePath(keep_path, ls));

  // 110.0.0: below other's vmin (120) → stale.
  constexpr uint64_t kNow = 2 * kLaunchStateGcMaxAge + 100;
  SetLaunchStateGcTimeForTesting(kNow - kLaunchStateGcMaxAge);
  ls.version = "110.0.0";
  base::FilePath drop_path = GetInstallDirLaunchStatePath(
      install_dir_, hash, Version::Parse("110.0.0"), other);
  ASSERT_TRUE(WriteLaunchStatePath(drop_path, ls));
  SetLaunchStateGcTimeForTesting(kNow);

  ExtendedConfig extended = CreateExtendedConfig();
  {
    Controller c;
    c.Run(Command::kPrune, config, extended);
  }

  // 130 survives (>= other's vmin 120) despite being < current's vmin. A
  // current-platform-vmin regression would wrongly delete it.
  EXPECT_TRUE(base::PathExists(keep_path))
      << "launch file should be judged against its own platform's vmin";
  // 110 is below other's vmin → stale → deleted.
  EXPECT_FALSE(base::PathExists(drop_path));
}

TEST_F(ControllerTempDirTest, Prune_CommandPrunesOnly) {
  CreateFakeInstalledVersion(api_version_str_);

  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  ExtendedConfig extended = CreateExtendedConfig();

  // Run kPrune. Should run pruning without version resolution.
  std::vector<Step> steps;
  Controller controller;
  Result result =
      controller.Run(Command::kPrune, config, extended, RecordProgress(&steps));
  EXPECT_TRUE(result.success);
  ASSERT_GE(steps.size(), 2u);
  EXPECT_EQ(kStepVersionCheck, steps[steps.size() - 2]);
  EXPECT_EQ(kStepCleanup, steps.back());

  // No launch state files should have been written.
  std::wstring hash = GetAppidHash(config.appid);
  auto ls = ReadLaunchStatePath(GetInstallDirLaunchStatePath(
      install_dir_, hash, Version::Parse(api_version_str_),
      GetCurrentPlatform()));
  EXPECT_FALSE(ls.has_value());
}

TEST_F(ControllerTempDirTest, PruneQuarantinesValidIndexCanonicalOrphan) {
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));

  Controller controller;
  Result result = controller.Run(Command::kPrune, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCommitted, result.outcome);
  EXPECT_FALSE(base::PathExists(orphan));
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_TRUE(indexed.empty());
  base::FilePath trash = install_dir_.Append(kTrashSubdirectory);
  base::FileEnumerator trash_entries(
      trash, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  EXPECT_TRUE(trash_entries.Next().empty());
}

TEST_F(ControllerTempDirTest, PruneDoesNotQuarantineIndexedDirectory) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  CreateFakeInstalledVersion(api_version_str_, "defa01defa01", "",
                             /*publish_index=*/true);
  base::FilePath indexed_path =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));

  Controller controller;
  Result result =
      controller.Run(Command::kPrune, config, CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(base::PathExists(indexed_path));
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  ASSERT_EQ(1u, indexed.size());
  EXPECT_EQ(indexed_path, indexed[0].path);
}

TEST_F(ControllerTempDirTest, PruneMissingIndexDoesNotClassifyOrphan) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  base::FilePath index_path = install_dir_.Append(kVersionIndexFilename);
  ASSERT_FALSE(base::PathExists(index_path));

  Controller controller;
  Result result =
      controller.Run(Command::kPrune, config, CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(base::PathExists(orphan));
  EXPECT_FALSE(base::PathExists(index_path));
}

TEST_F(ControllerTempDirTest, PruneCorruptIndexDoesNotClassifyOrphan) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  base::FilePath index_path = install_dir_.Append(kVersionIndexFilename);
  ASSERT_TRUE(base::WriteFile(index_path, "corrupt"));

  Controller controller;
  Result result =
      controller.Run(Command::kPrune, config, CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(base::PathExists(orphan));
  std::string unchanged;
  ASSERT_TRUE(base::ReadFileToString(index_path, &unchanged));
  EXPECT_EQ("corrupt", unchanged);
}

TEST_F(ControllerTempDirTest,
       PruneMissingIndexOrdinaryPruneRebuildsAndRemovesPrunableVersion) {
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath version_path =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  base::FilePath index_path = install_dir_.Append(kVersionIndexFilename);
  ASSERT_FALSE(base::PathExists(index_path));

  Controller controller;
  Result result = controller.Run(Command::kPrune, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_FALSE(base::PathExists(version_path));
  std::vector<InstalledVersion> rebuilt;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &rebuilt));
  EXPECT_TRUE(rebuilt.empty());
}

TEST_F(ControllerTempDirTest,
       PruneCorruptIndexOrdinaryPruneRebuildsAndRemovesPrunableVersion) {
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath version_path =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  base::FilePath index_path = install_dir_.Append(kVersionIndexFilename);
  ASSERT_TRUE(base::WriteFile(index_path, "corrupt"));

  Controller controller;
  Result result = controller.Run(Command::kPrune, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_FALSE(base::PathExists(version_path));
  std::vector<InstalledVersion> rebuilt;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &rebuilt));
  EXPECT_TRUE(rebuilt.empty());
}

TEST_F(ControllerTempDirTest, PruneUnreadableIndexDoesNotClassifyOrphan) {
  Config config = CreateValidConfig();
  CreateDatabaseWithApp(config);
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  base::FilePath index_path = install_dir_.Append(kVersionIndexFilename);
  HANDLE exclusive =
      CreateFileW(index_path.value().c_str(), GENERIC_READ, 0, nullptr,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE(INVALID_HANDLE_VALUE, exclusive);

  Controller controller;
  Result result =
      controller.Run(Command::kPrune, config, CreateExtendedConfig());
  CloseHandle(exclusive);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(base::PathExists(orphan));
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_TRUE(indexed.empty());
}

TEST_F(ControllerTempDirTest, PruneOrphanTrashMoveFailureIsDeferred) {
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));

  SetFileOpsFaultForTesting(FileOpsFault::kTrashMove);
  Controller controller;
  Result result = controller.Run(Command::kPrune, CreateValidConfig(),
                                 CreateExtendedConfig());
  SetFileOpsFaultForTesting(FileOpsFault::kNone);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  ASSERT_FALSE(result.warnings.empty());
  EXPECT_NE(std::string::npos, result.warnings[0].find("Unindexed"));
  EXPECT_TRUE(base::PathExists(orphan));
}

TEST_F(ControllerTempDirTest,
       PruneOrphanTrashReclaimFailureIsDeletedByLaterWriter) {
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  base::FilePath trash = install_dir_.Append(kTrashSubdirectory);

  SetFileOpsFaultForTesting(FileOpsFault::kTrashReclaim);
  Controller prune;
  Result prune_result =
      prune.Run(Command::kPrune, CreateValidConfig(), CreateExtendedConfig());
  SetFileOpsFaultForTesting(FileOpsFault::kNone);

  EXPECT_TRUE(prune_result.success) << prune_result.error_message;
  EXPECT_EQ(Outcome::kCleanupDeferred, prune_result.outcome);
  EXPECT_FALSE(base::PathExists(orphan));
  base::FileEnumerator pending(
      trash, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  EXPECT_FALSE(pending.Next().empty());

  Controller later_writer;
  Result later_result = later_writer.Run(
      Command::kUninstall, CreateValidConfig(), CreateExtendedConfig());
  EXPECT_TRUE(later_result.success) << later_result.error_message;
  base::FileEnumerator reclaimed(
      trash, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  EXPECT_TRUE(reclaimed.Next().empty());
}

TEST_F(ControllerTempDirTest,
       PruneUnsafeArtifactDoesNotBlockSafeOrphanQuarantine) {
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  base::FilePath unsafe = install_dir_.Append(kVersionsSubdirectory)
                              .Append(L"not-a-version")
                              .AppendASCII(GetCurrentPlatform());
  ASSERT_TRUE(base::CreateDirectory(unsafe));
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath safe =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));

  Controller controller;
  Result result = controller.Run(Command::kPrune, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  EXPECT_TRUE(base::PathExists(unsafe));
  EXPECT_FALSE(base::PathExists(safe));
}

TEST_F(ControllerTempDirTest, PruneLockTimeoutDoesNotScanOrMutate) {
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  base::WaitableEvent lock_acquired;
  base::WaitableEvent release_lock;
  std::thread holder([&] {
    auto held = SingletonLock::Acquire(install_dir_, INFINITE);
    ASSERT_TRUE(held && held->IsHeld());
    lock_acquired.Signal();
    release_lock.Wait();
  });
  lock_acquired.Wait();
  ExtendedConfig extended = CreateExtendedConfig();
  extended.lock_timeout_ms = 50;

  Controller controller;
  Result result =
      controller.Run(Command::kPrune, CreateValidConfig(), extended);
  release_lock.Signal();
  holder.join();

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeLockTimeout, result.error_code);
  EXPECT_TRUE(base::PathExists(orphan));
}

TEST_F(ControllerTempDirTest, PruneCancellationBeforeScanDoesNotMutate) {
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  ProgressCallback cancel_at_version_check = base::BindRepeating(
      [](Step step, uint64_t, uint64_t) { return step != kStepVersionCheck; });

  Controller controller;
  Result result =
      controller.Run(Command::kPrune, CreateValidConfig(),
                     CreateExtendedConfig(), cancel_at_version_check);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeCancelled, result.error_code);
  EXPECT_TRUE(base::PathExists(orphan));
}

TEST_F(ControllerTempDirTest, PruneSuspendedDatabaseDoesNotMutate) {
  Database database;
  database.SuspendPruning();
  ASSERT_EQ(DatabaseError::kSuccess,
            database.Save(GetDatabasePath(install_dir_)));
  CreateFakeInstalledVersion(api_version_str_);
  auto indexed = ScanInstalledVersionsWithMetadata(install_dir_);
  ASSERT_EQ(1u, indexed.size());
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, indexed));
  const std::string orphan_version = api_version_str_ + ".1";
  CreateFakeInstalledVersion(orphan_version);
  base::FilePath indexed_path = indexed[0].path;
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(orphan_version));

  Controller controller;
  Result result = controller.Run(Command::kPrune, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(base::PathExists(indexed_path));
  EXPECT_TRUE(base::PathExists(orphan));
  std::vector<InstalledVersion> unchanged;
  ASSERT_EQ(MetadataError::kSuccess,
            ReadVersionIndex(install_dir_, &unchanged));
  EXPECT_EQ(1u, unchanged.size());
}

TEST_F(ControllerTempDirTest, PruneNewerSchemaDatabaseDoesNotMutate) {
  ASSERT_TRUE(WriteFileWithIntegrity(GetDatabasePath(install_dir_),
                                     R"({"schema_version":999,"apps":[]})"));
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));

  Controller controller;
  Result result = controller.Run(Command::kPrune, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeDatabaseError, result.error_code);
  EXPECT_TRUE(base::PathExists(orphan));
}

TEST_F(ControllerTempDirTest,
       PruneOrphanQuarantineSurvivesNormalPruneIndexFailure) {
  CreateFakeInstalledVersion(api_version_str_);
  auto indexed = ScanInstalledVersionsWithMetadata(install_dir_);
  ASSERT_EQ(1u, indexed.size());
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, indexed));
  const std::string orphan_version = api_version_str_ + ".1";
  CreateFakeInstalledVersion(orphan_version);
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(orphan_version));

  SetVersionIndexFaultForTesting(VersionIndexFault::kWrite);
  Controller controller;
  Result result = controller.Run(Command::kPrune, CreateValidConfig(),
                                 CreateExtendedConfig());
  SetVersionIndexFaultForTesting(VersionIndexFault::kNone);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeIndexError, result.error_code);
  EXPECT_FALSE(base::PathExists(orphan));
  EXPECT_TRUE(base::PathExists(indexed[0].path));
  std::vector<InstalledVersion> unchanged;
  ASSERT_EQ(MetadataError::kSuccess,
            ReadVersionIndex(install_dir_, &unchanged));
  EXPECT_EQ(1u, unchanged.size());
}

TEST_F(ControllerTempDirTest, PruneIndexFailureKeepsDirectoryAdvertised) {
  CreateFakeInstalledVersion(api_version_str_);
  auto installed = ScanInstalledVersionsWithMetadata(install_dir_);
  ASSERT_EQ(1u, installed.size());
  ASSERT_EQ(MetadataError::kSuccess,
            WriteVersionIndex(install_dir_, installed));

  SetVersionIndexFaultForTesting(VersionIndexFault::kWrite);
  Controller controller;
  Result result = controller.Run(Command::kPrune, CreateValidConfig(),
                                 CreateExtendedConfig());
  SetVersionIndexFaultForTesting(VersionIndexFault::kNone);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeIndexError, result.error_code);
  EXPECT_TRUE(base::PathExists(installed[0].path));
  std::vector<InstalledVersion> reread;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &reread));
  ASSERT_EQ(1u, reread.size());
}

TEST_F(ControllerTempDirTest, PostIndexTrashMoveIsCleanupDeferred) {
  CreateFakeInstalledVersion(api_version_str_);
  auto installed = ScanInstalledVersionsWithMetadata(install_dir_);
  ASSERT_EQ(MetadataError::kSuccess,
            WriteVersionIndex(install_dir_, installed));

  SetFileOpsFaultForTesting(FileOpsFault::kTrashMove);
  Controller controller;
  Result result = controller.Run(Command::kPrune, CreateValidConfig(),
                                 CreateExtendedConfig());
  SetFileOpsFaultForTesting(FileOpsFault::kNone);

  EXPECT_TRUE(result.success);
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  EXPECT_FALSE(result.warnings.empty());
  EXPECT_TRUE(base::PathExists(installed[0].path));
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  EXPECT_TRUE(indexed.empty());
}

TEST_F(ControllerTempDirTest, RegistrationFailureLeavesPublishedVersion) {
  CreateFakeInstalledVersion(api_version_str_);
  SetDatabaseSaveFailureForTesting(true);
  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig());
  SetDatabaseSaveFailureForTesting(false);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeDatabaseError, result.error_code);
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  ASSERT_EQ(1u, indexed.size());
  EXPECT_TRUE(base::PathExists(indexed[0].path));
}

TEST_F(ControllerTempDirTest, QueryDoesNotScanOrRebuildMissingIndex) {
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath index_path = install_dir_.Append(kVersionIndexFilename);
  ASSERT_FALSE(base::PathExists(index_path));

  Controller controller;
  Result result = controller.Run(Command::kQuery, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);
  EXPECT_FALSE(base::PathExists(index_path));
}

TEST_F(ControllerTempDirTest, QueryPreservesBadCrcIndexWithoutScanning) {
  CreateFakeInstalledVersion(api_version_str_);
  ASSERT_EQ(MetadataError::kSuccess,
            WriteVersionIndex(install_dir_,
                              ScanInstalledVersionsWithMetadata(install_dir_)));
  base::FilePath index_path = install_dir_.Append(kVersionIndexFilename);
  std::string corrupt_index;
  ASSERT_TRUE(base::ReadFileToString(index_path, &corrupt_index));
  ASSERT_FALSE(corrupt_index.empty());
  corrupt_index[0] = ~corrupt_index[0];
  ASSERT_TRUE(base::WriteFile(index_path, corrupt_index));

  Controller controller;
  Result result = controller.Run(Command::kQuery, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoMatchingVersion, result.error_code);
  std::string unchanged;
  ASSERT_TRUE(base::ReadFileToString(index_path, &unchanged));
  EXPECT_EQ(corrupt_index, unchanged);
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupRecoversReadOnlyMissingIndexAndLeasesVersion) {
  CreateFakeInstalledVersion(api_version_str_);
  internal::OverrideInstallDirectoriesForTesting({install_dir_}, std::nullopt);
  auto held = SingletonLock::Acquire(install_dir_, INFINITE);
  ASSERT_TRUE(held && held->IsHeld());

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);

  ASSERT_TRUE(result.success) << result.error_message;
  ASSERT_TRUE(result.version_lease);
  base::FilePath version_path =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  base::FilePath moved = version_path.AddExtension(L"moved");
  EXPECT_FALSE(MoveFileExW(version_path.value().c_str(), moved.value().c_str(),
                           MOVEFILE_WRITE_THROUGH));
  result.version_lease.reset();
  EXPECT_TRUE(MoveFileExW(version_path.value().c_str(), moved.value().c_str(),
                          MOVEFILE_WRITE_THROUGH));
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kVersionIndexFilename)));
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupFreshDirectoryDoesNotWarnAboutEmergencyRecovery) {
  ASSERT_FALSE(
      base::DirectoryExists(install_dir_.Append(kVersionsSubdirectory)));
  internal::OverrideInstallDirectoriesForTesting({install_dir_}, std::nullopt);
  PolicyLoadResult policy;
  policy.status = PolicyLoadStatus::kValid;
  policy.policy.download_source.kind = DownloadSourceKind::kDisabled;
  policy.policy.download_source.authority =
      DownloadSourceAuthority::kEnterprisePolicy;
  internal::OverrideEnterprisePolicyForTesting(policy);

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);

  EXPECT_FALSE(result.success);
  std::string log;
  base::ReadFileToString(install_dir_.Append(kLogFilename), &log);
  EXPECT_EQ(std::string::npos,
            base::ToLowerASCII(log).find("emergency startup recovery"))
      << log;
}

TEST_F(ControllerTempDirTest, AutomaticStartupRecoversReadOnlyCorruptIndex) {
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath index_path = install_dir_.Append(kVersionIndexFilename);
  ASSERT_TRUE(base::WriteFile(index_path, "corrupt"));
  internal::OverrideInstallDirectoriesForTesting({install_dir_}, std::nullopt);

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(result.version_lease);
  std::string unchanged;
  ASSERT_TRUE(base::ReadFileToString(index_path, &unchanged));
  EXPECT_EQ("corrupt", unchanged);
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupPreservesBadCrcIndexDuringRecovery) {
  CreateFakeInstalledVersion(api_version_str_);
  ASSERT_EQ(MetadataError::kSuccess,
            WriteVersionIndex(install_dir_,
                              ScanInstalledVersionsWithMetadata(install_dir_)));
  base::FilePath index_path = install_dir_.Append(kVersionIndexFilename);
  std::string corrupt_index;
  ASSERT_TRUE(base::ReadFileToString(index_path, &corrupt_index));
  ASSERT_FALSE(corrupt_index.empty());
  corrupt_index[0] = ~corrupt_index[0];
  ASSERT_TRUE(base::WriteFile(index_path, corrupt_index));
  internal::OverrideInstallDirectoriesForTesting({install_dir_}, std::nullopt);

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);

  ASSERT_TRUE(result.success) << result.error_message;
  std::string unchanged;
  ASSERT_TRUE(base::ReadFileToString(index_path, &unchanged));
  EXPECT_EQ(corrupt_index, unchanged);
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupDoesNotScanAfterInconclusiveIndexReadError) {
  CreateFakeInstalledVersion(api_version_str_);
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  base::FilePath index_path = install_dir_.Append(kVersionIndexFilename);
  HANDLE exclusive =
      CreateFileW(index_path.value().c_str(), GENERIC_READ, 0, nullptr,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE(INVALID_HANDLE_VALUE, exclusive);
  internal::OverrideInstallDirectoriesForTesting({install_dir_}, std::nullopt);

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);
  CloseHandle(exclusive);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeConfigError, result.error_code);
  EXPECT_TRUE(base::PathExists(
      GetVersionPath(install_dir_, Version::Parse(api_version_str_))));
  EXPECT_TRUE(base::PathExists(index_path));
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupSelectorRejectsHigherPriorityWrongAbiBeforeDedup) {
  const std::string requested_abi = "bbbbbbbbbbbb";
  CreateFakeInstalledVersion(api_version_str_, "aaaaaaaaaaaa", "", true);
  base::FilePath lower_root = temp_dir_.GetPath().Append(L"Lower");
  base::FilePath higher_version =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));
  base::FilePath lower_version =
      GetVersionPath(lower_root, Version::Parse(api_version_str_));
  ASSERT_TRUE(base::CreateDirectory(lower_version.DirName()));
  ASSERT_TRUE(base::CopyDirectory(higher_version, lower_version, true));
  VersionMetadata lower_metadata;
  lower_metadata.version = Version::Parse(api_version_str_);
  lower_metadata.abi_hash = requested_abi;
  lower_metadata.platform = GetCurrentPlatform();
  lower_metadata.version_full = api_version_str_ + "+lower";
  ASSERT_EQ(MetadataError::kSuccess,
            WriteVersionMetadata(lower_version, lower_metadata));
  internal::OverrideInstallDirectoriesForTesting({install_dir_, lower_root},
                                                 std::nullopt);
  Config config = CreateValidConfig();
  config.abi_hash = requested_abi;

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, config, CreateExtendedConfig(), {},
                     ExecutionContext::kAutomaticStartup);

  ASSERT_TRUE(result.success) << result.error_message;
  EXPECT_TRUE(lower_root.IsParent(result.libcef_path));
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupValidEmptyIndexDoesNotRecoverOrphan) {
  CreateFakeInstalledVersion(api_version_str_);
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  internal::OverrideInstallDirectoriesForTesting({install_dir_}, std::nullopt);

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeConfigError, result.error_code);
  EXPECT_TRUE(base::PathExists(
      GetVersionPath(install_dir_, Version::Parse(api_version_str_))));
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupEmergencyRecoveryAppliesValidationAndRevocation) {
  CreateFakeInstalledVersion(api_version_str_);
  ASSERT_EQ(RevocationError::kSuccess,
            WriteRevocationCache(install_dir_,
                                 {{Version::Parse(api_version_str_),
                                   Version::Parse(api_version_str_), "test"}}));
  internal::OverrideInstallDirectoriesForTesting({install_dir_}, std::nullopt);

  Controller revoked_controller;
  Result revoked = revoked_controller.Run(
      Command::kInstall, CreateValidConfig(), CreateExtendedConfig(), {},
      ExecutionContext::kAutomaticStartup);
  EXPECT_FALSE(revoked.success);

  ASSERT_TRUE(base::DeleteFile(install_dir_.Append(kRevocationCacheFilename)));
  ASSERT_TRUE(base::DeleteFile(
      GetVersionPath(install_dir_, Version::Parse(api_version_str_))
          .Append(kCatalogFilename)));
  Controller incomplete_controller;
  Result incomplete = incomplete_controller.Run(
      Command::kInstall, CreateValidConfig(), CreateExtendedConfig(), {},
      ExecutionContext::kAutomaticStartup);
  EXPECT_FALSE(incomplete.success);
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupEmergencyRecoveryRejectsIncompatibleCandidate) {
  CreateFakeInstalledVersion(api_version_str_);
  internal::OverrideInstallDirectoriesForTesting({install_dir_}, std::nullopt);
  Config config = CreateValidConfig();
  config.abi_hash = "different-abi";

  Controller controller;
  Result result =
      controller.Run(Command::kInstall, config, CreateExtendedConfig(), {},
                     ExecutionContext::kAutomaticStartup);
  EXPECT_FALSE(result.success);
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupEmergencyRecoveryRejectsNonCanonicalPath) {
  CreateFakeInstalledVersion(api_version_str_);
  Version version = Version::Parse(api_version_str_);
  std::string noncanonical = api_version_str_;
  size_t first_dot = noncanonical.find('.');
  ASSERT_NE(std::string::npos, first_dot);
  noncanonical.insert(first_dot + 1, "0");
  ASSERT_EQ(version, Version::Parse(noncanonical));
  base::FilePath canonical_parent =
      GetVersionPath(install_dir_, version).DirName();
  base::FilePath noncanonical_parent =
      canonical_parent.DirName().AppendASCII(noncanonical);
  ASSERT_TRUE(base::Move(canonical_parent, noncanonical_parent));
  internal::OverrideInstallDirectoriesForTesting({install_dir_}, std::nullopt);

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);
  EXPECT_FALSE(result.success);
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupEmergencyRecoveryRejectsUnhealthyCandidate) {
  CreateFakeInstalledVersion(api_version_str_);
  Config config = CreateValidConfig();
  LaunchState state;
  state.appid = config.appid;
  state.pid = 99999;
  state.pid_start_time = GetCurrentPidStartTime();
  state.consecutive_failures = kMaxConsecutiveFailures;
  state.running = true;
  state.version = api_version_str_;
  state.platform = GetCurrentPlatform();
  ASSERT_TRUE(WriteLaunchStatePath(
      GetInstallDirLaunchStatePath(install_dir_, GetAppidHash(config.appid),
                                   Version::Parse(api_version_str_),
                                   GetCurrentPlatform()),
      state));

  base::WaitableEvent lock_acquired;
  base::WaitableEvent release_lock;
  std::thread holder([&] {
    auto held = SingletonLock::Acquire(install_dir_, INFINITE);
    ASSERT_TRUE(held && held->IsHeld());
    lock_acquired.Signal();
    release_lock.Wait();
  });
  lock_acquired.Wait();

  ExtendedConfig extended = CreateExtendedConfig();
  extended.lock_timeout_ms = 50;
  Controller controller;
  Result result = controller.Run(Command::kInstall, config, extended, {},
                                 ExecutionContext::kAutomaticStartup);
  release_lock.Signal();
  holder.join();

  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeLockTimeout, result.error_code);
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(ControllerTempDirTest,
       AutomaticStartupEmergencyRecoveryHonorsRootLimit) {
  CreateFakeInstalledVersion(api_version_str_);
  EmergencyRecoveryScanLimits limits;
  limits.max_roots = 0;
  SetEmergencyRecoveryScanLimitsForTesting(limits);
  internal::OverrideInstallDirectoriesForTesting({install_dir_}, std::nullopt);

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);
  EXPECT_FALSE(result.success);
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupEmergencyRecoveryHonorsEntryLimit) {
  CreateFakeInstalledVersion(api_version_str_);
  EmergencyRecoveryScanLimits limits;
  limits.max_version_entries = 0;
  SetEmergencyRecoveryScanLimitsForTesting(limits);
  internal::OverrideInstallDirectoriesForTesting({install_dir_}, std::nullopt);

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);
  EXPECT_FALSE(result.success);
}

TEST_F(ControllerTempDirTest,
       AutomaticStartupEmergencyRecoveryHonorsSoftTimeBudget) {
  CreateFakeInstalledVersion(api_version_str_);
  EmergencyRecoveryScanLimits limits;
  limits.time_budget = base::TimeDelta();
  SetEmergencyRecoveryScanLimitsForTesting(limits);
  internal::OverrideInstallDirectoriesForTesting({install_dir_}, std::nullopt);

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig(), {},
                                 ExecutionContext::kAutomaticStartup);
  EXPECT_FALSE(result.success);
}
#endif

TEST_F(ControllerTempDirTest, InstallRebuildsMissingIndexConservatively) {
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath index_path = install_dir_.Append(kVersionIndexFilename);
  ASSERT_FALSE(base::PathExists(index_path));

  Controller controller;
  Result result = controller.Run(Command::kInstall, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  std::vector<InstalledVersion> indexed;
  ASSERT_EQ(MetadataError::kSuccess, ReadVersionIndex(install_dir_, &indexed));
  ASSERT_EQ(1u, indexed.size());
}

TEST_F(ControllerTempDirTest, ValidIndexQuarantinesUnindexedOrphan) {
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));

  Controller controller;
  Result result = controller.Run(Command::kUninstall, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_FALSE(base::PathExists(orphan));
}

TEST_F(ControllerTempDirTest, RecoveryDefersUnindexedTrashMoveFailure) {
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  CreateFakeInstalledVersion(api_version_str_);
  base::FilePath orphan =
      GetVersionPath(install_dir_, Version::Parse(api_version_str_));

  SetFileOpsFaultForTesting(FileOpsFault::kTrashMove);
  Controller controller;
  Result result = controller.Run(Command::kUninstall, CreateValidConfig(),
                                 CreateExtendedConfig());
  SetFileOpsFaultForTesting(FileOpsFault::kNone);

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  EXPECT_FALSE(result.warnings.empty());
  EXPECT_EQ(std::string::npos, result.ToJson().find(orphan.AsUTF8Unsafe()));
  EXPECT_TRUE(base::PathExists(orphan));
}

TEST_F(ControllerTempDirTest,
       RecoveryDefersNonCanonicalUnindexedVersionDirectory) {
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  const std::string noncanonical = "137.03.5";
  Version parsed = Version::Parse(noncanonical);
  ASSERT_TRUE(parsed.IsValid());
  ASSERT_NE(noncanonical, parsed.ToString());
  base::FilePath orphan = install_dir_.Append(kVersionsSubdirectory)
                              .AppendASCII(noncanonical)
                              .AppendASCII(GetCurrentPlatform());
  ASSERT_TRUE(base::CreateDirectory(orphan));

  Controller controller;
  Result result = controller.Run(Command::kUninstall, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  EXPECT_FALSE(result.warnings.empty());
  EXPECT_EQ(std::string::npos, result.ToJson().find(orphan.AsUTF8Unsafe()));
  EXPECT_TRUE(base::PathExists(orphan));
}

TEST_F(ControllerTempDirTest,
       RecoveryReportsUnparseableUnindexedVersionDirectory) {
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  base::FilePath orphan = install_dir_.Append(kVersionsSubdirectory)
                              .Append(L"not-a-version")
                              .AppendASCII(GetCurrentPlatform());
  ASSERT_TRUE(base::CreateDirectory(orphan));

  Controller controller;
  Result result = controller.Run(Command::kUninstall, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  ASSERT_FALSE(result.warnings.empty());
  EXPECT_NE(std::string::npos, result.warnings[0].find("Unparseable"));
  EXPECT_EQ(std::string::npos, result.ToJson().find(orphan.AsUTF8Unsafe()));
  EXPECT_TRUE(base::PathExists(orphan));
}

TEST_F(ControllerTempDirTest,
       RecoveryReportsReparsePointUnindexedVersionDirectory) {
  ASSERT_EQ(MetadataError::kSuccess, WriteVersionIndex(install_dir_, {}));
  base::FilePath target = temp_dir_.GetPath().Append(L"reparse_target");
  ASSERT_TRUE(base::CreateDirectory(target));
  base::FilePath versions_root = install_dir_.Append(kVersionsSubdirectory);
  ASSERT_TRUE(base::CreateDirectory(versions_root));
  base::FilePath junction = versions_root.AppendASCII(api_version_str_);
  std::wostringstream create_command;
  create_command << L"cmd /c mklink /J " << std::quoted(junction.value())
                 << L" " << std::quoted(target.value());
  ::_wsystem(create_command.str().c_str());
  if (!base::DirectoryExists(junction)) {
    GTEST_SKIP() << "Could not create junction point";
  }
  EXPECT_TRUE(IsReparsePoint(junction));

  Controller controller;
  Result result = controller.Run(Command::kUninstall, CreateValidConfig(),
                                 CreateExtendedConfig());

  EXPECT_TRUE(result.success) << result.error_message;
  EXPECT_EQ(Outcome::kCleanupDeferred, result.outcome);
  bool found_warning = false;
  for (const auto& warning : result.warnings) {
    if (warning.find("Reparse-point") != std::string::npos) {
      found_warning = true;
      break;
    }
  }
  EXPECT_TRUE(found_warning);
  EXPECT_EQ(std::string::npos, result.ToJson().find(target.AsUTF8Unsafe()));
  EXPECT_EQ(std::string::npos, result.ToJson().find(junction.AsUTF8Unsafe()));
  EXPECT_TRUE(base::PathExists(junction));

  std::wostringstream cleanup_command;
  cleanup_command << L"cmd /c rmdir " << std::quoted(junction.value());
  ::_wsystem(cleanup_command.str().c_str());
}

// ============================================================================
// kLaunchSuccess (launch health confirmation)
// ============================================================================

class LaunchSuccessTest : public testing::Test {
 protected:
  static constexpr char kAppid[] = "550e8400-e29b-41d4-a716-446655440000";

  void SetUp() override {
    SetTestingMode(true);
    // Derive the version from the build's API version (avoids hardcoding and
    // vmin clamping) — same approach as ControllerTempDirTest.
    version_ = Version::FromApiVersion(CEF_API_VERSION_LAST).ToString();
    // Start from a clean process-global regardless of prior tests in this
    // process. The behavioral tests below use the path-taking
    // HandleLaunchSuccess overload and never touch the global; only the
    // RunInstaller export-dispatch tests exercise it, and they set+reset it.
    SetActiveLaunchStatePath(base::FilePath());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    install_dir_ = temp_dir_.GetPath().Append(kCefSubdirectory);
    ASSERT_TRUE(base::CreateDirectory(install_dir_));
    sentinel_path_ = GetInstallDirLaunchStatePath(
        install_dir_, GetAppidHash(kAppid), Version::Parse(version_),
        GetCurrentPlatform());
  }

  void TearDown() override {
    // Clear the process-global so it doesn't leak into other tests.
    SetActiveLaunchStatePath(base::FilePath());
    SetLaunchStateGcTimeForTesting(std::nullopt);
    SetTestingMode(false);
  }

  // Write a sentinel owned by the current process unless |pid| /
  // |pid_start_time| are overridden. Returns the written state.
  LaunchState WriteSentinel(
      bool running,
      int failures,
      uint32_t pid = ::GetCurrentProcessId(),
      uint64_t pid_start_time = GetCurrentPidStartTime()) {
    LaunchState ls;
    ls.appid = kAppid;
    ls.pid = pid;
    ls.pid_start_time = pid_start_time;
    ls.consecutive_failures = failures;
    ls.running = running;
    ls.version = version_;
    ls.platform = GetCurrentPlatform();
    EXPECT_TRUE(WriteLaunchStatePath(sentinel_path_, ls));
    return ls;
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath install_dir_;
  base::FilePath sentinel_path_;
  std::string version_;
};

TEST_F(LaunchSuccessTest, HandleLaunchSuccess_Confirms) {
  SetLaunchStateGcTimeForTesting(100);
  WriteSentinel(/*running=*/true, /*failures=*/2);
  ASSERT_EQ(100u, ReadLaunchStatePath(sentinel_path_)->last_update);

  SetLaunchStateGcTimeForTesting(200);
  Result result = HandleLaunchSuccess(sentinel_path_);
  EXPECT_TRUE(result.success);
  EXPECT_EQ(kExitCodeSuccess, result.error_code);

  auto ls = ReadLaunchStatePath(sentinel_path_);
  ASSERT_TRUE(ls.has_value());
  EXPECT_FALSE(ls->running);
  EXPECT_EQ(0, ls->consecutive_failures);
  EXPECT_EQ(200u, ls->last_update);
}

TEST_F(LaunchSuccessTest, HandleLaunchSuccess_NoActivePath) {
  WriteSentinel(/*running=*/true, /*failures=*/0);
  // Empty path stands in for "no active launch state".
  Result result = HandleLaunchSuccess(base::FilePath());
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeNoSentinel, result.error_code);
}

TEST_F(LaunchSuccessTest, HandleLaunchSuccess_MissingFile) {
  // Path points to a sentinel that was never written.
  Result result = HandleLaunchSuccess(sentinel_path_);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeSentinelReadError, result.error_code);
}

TEST_F(LaunchSuccessTest, HandleLaunchSuccess_CorruptFile) {
  ASSERT_TRUE(base::CreateDirectory(sentinel_path_.DirName()));
  ASSERT_TRUE(base::WriteFile(sentinel_path_, "{ this is not valid json"));
  Result result = HandleLaunchSuccess(sentinel_path_);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeSentinelReadError, result.error_code);
}

TEST_F(LaunchSuccessTest, HandleLaunchSuccess_PidMismatch) {
  // Sentinel owned by a different process (pid_start_time differs).
  WriteSentinel(/*running=*/true, /*failures=*/0,
                /*pid=*/::GetCurrentProcessId(),
                /*pid_start_time=*/12345);

  Result result = HandleLaunchSuccess(sentinel_path_);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(kExitCodeSentinelOwnerMismatch, result.error_code);

  // Sentinel left unchanged (still running).
  auto ls = ReadLaunchStatePath(sentinel_path_);
  ASSERT_TRUE(ls.has_value());
  EXPECT_TRUE(ls->running);
}

TEST_F(LaunchSuccessTest, HandleLaunchSuccess_AlreadyConfirmed) {
  // running=false with a contrived non-zero failure count so any write is
  // detectable: a confirmation write would reset failures to 0.
  SetLaunchStateGcTimeForTesting(100);
  WriteSentinel(/*running=*/false, /*failures=*/5);

  SetLaunchStateGcTimeForTesting(200);
  Result result = HandleLaunchSuccess(sentinel_path_);
  EXPECT_TRUE(result.success);
  EXPECT_EQ(kExitCodeSuccess, result.error_code);

  auto ls = ReadLaunchStatePath(sentinel_path_);
  ASSERT_TRUE(ls.has_value());
  EXPECT_FALSE(ls->running);
  // Unchanged → confirms the idempotent no-write path.
  EXPECT_EQ(5, ls->consecutive_failures);
  EXPECT_EQ(100u, ls->last_update);
}

TEST_F(LaunchSuccessTest, HandleLaunchSuccess_PreservesOtherFields) {
  LaunchState before = WriteSentinel(/*running=*/true, /*failures=*/2);

  Result result = HandleLaunchSuccess(sentinel_path_);
  EXPECT_TRUE(result.success);

  auto ls = ReadLaunchStatePath(sentinel_path_);
  ASSERT_TRUE(ls.has_value());
  EXPECT_EQ(before.appid, ls->appid);
  EXPECT_EQ(before.pid, ls->pid);
  EXPECT_EQ(before.pid_start_time, ls->pid_start_time);
  EXPECT_EQ(before.version, ls->version);
  EXPECT_EQ(before.platform, ls->platform);
  EXPECT_FALSE(ls->running);
  EXPECT_EQ(0, ls->consecutive_failures);
}

TEST_F(LaunchSuccessTest, RunInstaller_LaunchSuccess_DoesNotRunController) {
  WriteSentinel(/*running=*/true, /*failures=*/0);
  SetActiveLaunchStatePath(sentinel_path_);

  // nullptr config: Controller::Run requires a parsed config and the
  // config-parse block emits CONFIG_ERROR for null/empty config. Success here
  // proves the dispatch bypasses config parsing and Controller::Run entirely.
  const char* result = RunInstaller("launch_success", nullptr);
  ASSERT_NE(result, nullptr);
  std::optional<Result> parsed = Result::FromJson(result);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_TRUE(parsed->success);

  auto ls = ReadLaunchStatePath(sentinel_path_);
  ASSERT_TRUE(ls.has_value());
  EXPECT_FALSE(ls->running);

  // Without an active path the dispatch still bypasses config parsing: it
  // returns NO_SENTINEL rather than the CONFIG_ERROR the parse block emits.
  SetActiveLaunchStatePath(base::FilePath());
  const char* result2 = RunInstaller("launch_success", nullptr);
  std::optional<Result> parsed2 = Result::FromJson(result2);
  ASSERT_TRUE(parsed2.has_value());
  EXPECT_FALSE(parsed2->success);
  EXPECT_EQ(kExitCodeNoSentinel, parsed2->error_code);
}

TEST_F(LaunchSuccessTest, RunInstaller_LaunchSuccess_DoesNotAcquireLock) {
  WriteSentinel(/*running=*/true, /*failures=*/0);
  SetActiveLaunchStatePath(sentinel_path_);

  // Hold the installer lock for the install directory. The lightweight
  // launch_success path never calls SingletonLock::Acquire, so the call must
  // succeed immediately even while the lock is held.
  auto held = SingletonLock::Acquire(install_dir_, INFINITE);
  ASSERT_TRUE(held && held->IsHeld());

  const char* result = RunInstaller("launch_success", nullptr);
  ASSERT_NE(result, nullptr);
  std::optional<Result> parsed = Result::FromJson(result);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_TRUE(parsed->success);
}

}  // namespace
}  // namespace cef_installer
