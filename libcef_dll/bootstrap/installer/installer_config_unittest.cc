// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_config.h"

#include <windows.h>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/test/mock_log.h"
#include "cef/include/cef_api_hash.h"
#include "cef/libcef_dll/bootstrap/installer/installer_policy.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

class InstallerConfigTest : public testing::Test {};

ConfigError ParseConfigWithCdnUrls(const std::vector<std::string>& urls,
                                   Config* config,
                                   std::string* diagnostic = nullptr) {
  base::DictValue dict;
  dict.Set("appid", "12345678-1234-1234-1234-123456789abc");
  dict.Set("vmin", Version::FromApiVersion(CEF_API_VERSION_LAST).ToString());
  base::ListValue list;
  for (const auto& url : urls) {
    list.Append(url);
  }
  dict.Set("cdn_urls", std::move(list));
  std::string json;
  CHECK(base::JSONWriter::Write(dict, &json));
  return ParseConfigFromJson(json, config, {}, diagnostic);
}

TEST_F(InstallerConfigTest, ParseValidJson) {
  // Use vmin based on CEF_API_VERSION_LAST so it won't be clamped
  Version next_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
  std::string vmin_str = next_ver.ToString();
  std::string json =
      R"({"appid": "12345678-1234-1234-1234-123456789abc", "vmin": ")" +
      vmin_str + R"(", "vmax": ")" + vmin_str +
      R"(.99", "abi_hash": "abc123def4567890"})";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_EQ("12345678-1234-1234-1234-123456789abc", config.appid);
  EXPECT_EQ(vmin_str, config.vmin);
  EXPECT_EQ(vmin_str + ".99", config.vmax);
  EXPECT_EQ("abc123def4567890", config.abi_hash);
}

TEST_F(InstallerConfigTest, ParseOptionalVmax) {
  // Use vmin based on CEF_API_VERSION_LAST so it won't be clamped
  Version next_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
  std::string vmin_str = next_ver.ToString();
  std::string json =
      R"({"appid": "12345678-1234-1234-1234-123456789abc", "vmin": ")" +
      vmin_str + R"("})";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_EQ(vmin_str, config.vmin);
  EXPECT_TRUE(config.vmax.empty());
}

TEST_F(InstallerConfigTest, CdnUrlsPreserveOrderDuplicatesAndNormalize) {
  Config config;
  ASSERT_EQ(ConfigError::kSuccess,
            ParseConfigWithCdnUrls(
                {"https://one.example/prefix", "https://two.example/",
                 "https://one.example/prefix"},
                &config));
  EXPECT_EQ((std::vector<std::string>{"https://one.example/prefix/",
                                      "https://two.example/",
                                      "https://one.example/prefix/"}),
            config.cdn_urls);
}

TEST_F(InstallerConfigTest, CdnUrlsRejectShapeAndCount) {
  const std::string vmin =
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString();
  const std::string prefix =
      R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":")" + vmin +
      R"(","cdn_urls":)";
  for (
      const char* value :
      {"null", R"("https://one.example/")", "{}", "[]",
       R"(["https://one.example/",1])",
       R"(["https://1.example/","https://2.example/","https://3.example/","https://4.example/"])"}) {
    Config config;
    std::string diagnostic;
    EXPECT_EQ(
        ConfigError::kInvalidFieldValue,
        ParseConfigFromJson(prefix + value + "}", &config, {}, &diagnostic))
        << value;
    EXPECT_TRUE(config.cdn_urls.empty()) << value;
    EXPECT_FALSE(diagnostic.empty()) << value;
  }
}

TEST_F(InstallerConfigTest, CdnUrlsRejectInvalidPublicUrls) {
  const std::vector<std::string> invalid = {
      "",
      "http://example.com/",
      "https://",
      "https://user@example.com/",
      "https://example.com/?q=1",
      "https://example.com/#fragment",
      "https://example.com/../bad",
      "https://example.com\\bad",
      "https://example.com/line\n",
      "https://example.com/" + std::string(kMaxCdnUrlBytes, 'a')};
  for (const auto& url : invalid) {
    Config config;
    std::string diagnostic;
    EXPECT_EQ(ConfigError::kInvalidFieldValue,
              ParseConfigWithCdnUrls({url}, &config, &diagnostic))
        << url;
    EXPECT_TRUE(config.cdn_urls.empty()) << url;
    EXPECT_FALSE(diagnostic.empty()) << url;
  }
}

TEST_F(InstallerConfigTest, CdnUrlsAbsentAndInvalidClearReusedObject) {
  Config config;
  ASSERT_EQ(ConfigError::kSuccess,
            ParseConfigWithCdnUrls({"https://one.example/"}, &config));
  ASSERT_FALSE(config.cdn_urls.empty());

  const std::string vmin =
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString();
  ASSERT_EQ(ConfigError::kSuccess,
            ParseConfigFromJson(
                R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":")" +
                    vmin + R"("})",
                &config));
  EXPECT_TRUE(config.cdn_urls.empty());

  ASSERT_EQ(ConfigError::kInvalidFieldValue,
            ParseConfigWithCdnUrls({}, &config));
  EXPECT_TRUE(config.cdn_urls.empty());
}

TEST_F(InstallerConfigTest, LaunchHealthDefaultsOff) {
  const std::string vmin =
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString();
  const std::string json =
      R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":")" + vmin +
      R"("})";

  Config config;
  ASSERT_EQ(ConfigError::kSuccess, ParseConfigFromJson(json, &config));
  EXPECT_EQ(LaunchHealthMode::kOff, config.launch_health);
}

TEST_F(InstallerConfigTest, ParsesLaunchHealthModes) {
  const std::string vmin =
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString();
  for (const auto& [value, expected] :
       std::to_array<std::pair<const char*, LaunchHealthMode>>({
           {"off", LaunchHealthMode::kOff},
           {"explicit", LaunchHealthMode::kExplicit},
           {"exit_code", LaunchHealthMode::kExitCode},
       })) {
    const std::string json =
        R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":")" + vmin +
        R"(","launch_health":")" + value + R"("})";
    Config config;
    ASSERT_EQ(ConfigError::kSuccess, ParseConfigFromJson(json, &config))
        << value;
    EXPECT_EQ(expected, config.launch_health) << value;
  }
}

TEST_F(InstallerConfigTest, RejectsInvalidLaunchHealth) {
  const std::string vmin =
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString();
  for (const char* value :
       {"null", "true", R"("Explicit")", R"("")", R"(" exit_code ")"}) {
    const std::string json =
        R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":")" + vmin +
        R"(","launch_health":)" + value + "}";
    Config config;
    std::string diagnostic;
    EXPECT_EQ(ConfigError::kInvalidFieldValue,
              ParseConfigFromJson(json, &config, {}, &diagnostic))
        << value;
    EXPECT_FALSE(diagnostic.empty()) << value;
  }
}

TEST_F(InstallerConfigTest, ParseVminClampedToApiVersion) {
  // vmin lower than bootstrap API version should be clamped up
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "100.0"
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kSuccess, error);

  // vmin should be clamped to the bootstrap's API version
  constexpr int bootstrap_api_version = (CEF_API_VERSION < CEF_API_VERSION_LAST)
                                            ? CEF_API_VERSION
                                            : CEF_API_VERSION_LAST;
  Version expected_vmin = Version::FromApiVersion(bootstrap_api_version);
  EXPECT_EQ(expected_vmin.ToString(), config.vmin);
}

TEST_F(InstallerConfigTest, VersionRangeValidAfterClamping) {
  constexpr int bootstrap_api_version = (CEF_API_VERSION < CEF_API_VERSION_LAST)
                                            ? CEF_API_VERSION
                                            : CEF_API_VERSION_LAST;
  const std::string effective =
      Version::FromApiVersion(bootstrap_api_version).ToString();
  const std::string json =
      R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":"1.0","vmax":")" +
      effective + R"(.99"})";
  Config config;
  EXPECT_EQ(ConfigError::kSuccess, ParseConfigFromJson(json, &config));
  EXPECT_EQ(effective, config.vmin);
}

TEST_F(InstallerConfigTest, VersionRangeEmptyAfterClamping) {
  constexpr int bootstrap_api_version = (CEF_API_VERSION < CEF_API_VERSION_LAST)
                                            ? CEF_API_VERSION
                                            : CEF_API_VERSION_LAST;
  const std::string effective =
      Version::FromApiVersion(bootstrap_api_version).ToString();
  const std::string json =
      R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":"1.0","vmax":"2.0"})";
  Config config;
  std::string diagnostic;
  EXPECT_EQ(ConfigError::kInvalidVersionRange,
            ParseConfigFromJson(json, &config, {}, &diagnostic));
  EXPECT_NE(std::string::npos, diagnostic.find("Configured vmin 1.0"));
  EXPECT_NE(std::string::npos, diagnostic.find("effective vmin " + effective));
  EXPECT_NE(std::string::npos,
            diagnostic.find("bootstrap API version " +
                            std::to_string(bootstrap_api_version)));
  EXPECT_NE(std::string::npos, diagnostic.find("configured vmax 2.0"));
}

TEST_F(InstallerConfigTest, VersionRangeInvalidAsConfigured) {
  const std::string json =
      R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":"999.0","vmax":"998.0"})";
  Config config;
  std::string diagnostic;
  EXPECT_EQ(ConfigError::kInvalidVersionRange,
            ParseConfigFromJson(json, &config, {}, &diagnostic));
  EXPECT_EQ("Configured vmin 999.0 exceeds configured vmax 998.0", diagnostic);
}

TEST_F(InstallerConfigTest, VersionRangeEqualAfterClamping) {
  constexpr int bootstrap_api_version = (CEF_API_VERSION < CEF_API_VERSION_LAST)
                                            ? CEF_API_VERSION
                                            : CEF_API_VERSION_LAST;
  const std::string effective =
      Version::FromApiVersion(bootstrap_api_version).ToString();
  const std::string json =
      R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":"1.0","vmax":")" +
      effective + R"("})";
  Config config;
  EXPECT_EQ(ConfigError::kSuccess, ParseConfigFromJson(json, &config));
  EXPECT_EQ(effective, config.vmax);
}

TEST_F(InstallerConfigTest, ParseInvalidJson) {
  const char* json = "not valid json {{{";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kJsonParseError, error);
}

TEST_F(InstallerConfigTest, ParseMissingAppid) {
  const char* json = R"({
    "vmin": "137.1",
    "vmax": "137.99"
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kMissingRequiredField, error);
}

TEST_F(InstallerConfigTest, ParseMissingVmin) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmax": "137.99"
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kMissingRequiredField, error);
}

TEST_F(InstallerConfigTest, ParseInvalidUuid) {
  const char* json = R"({
    "appid": "not-a-valid-uuid",
    "vmin": "137.1"
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kInvalidFieldValue, error);
}

TEST_F(InstallerConfigTest, ParseUnknownFieldsPreserved) {
  // Unknown fields warn but remain non-fatal for forward compatibility.
  // Use vmin based on CEF_API_VERSION_LAST so it won't be clamped
  Version next_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
  std::string vmin_str = next_ver.ToString();
  std::string json =
      R"({"appid": "12345678-1234-1234-1234-123456789abc", "vmin": ")" +
      vmin_str + R"(", "unknown_field": "some value", "another_unknown": 42})";

  base::test::MockLog log;
  EXPECT_CALL(log, Log(::logging::LOGGING_WARNING, testing::_, testing::_,
                       testing::_, testing::HasSubstr("unknown_field")))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(log, Log(::logging::LOGGING_WARNING, testing::_, testing::_,
                       testing::_, testing::HasSubstr("another_unknown")))
      .WillOnce(testing::Return(true));
  log.StartCapturingLogs();

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_EQ("12345678-1234-1234-1234-123456789abc", config.appid);
  EXPECT_EQ(vmin_str, config.vmin);
}

TEST_F(InstallerConfigTest, ParseCombinedConfigFieldsWithoutWarnings) {
  const std::string version =
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString();
  base::DictValue dict;
  dict.Set(config_fields::kAppIdField, "12345678-1234-1234-1234-123456789abc");
  dict.Set(config_fields::kVminField, version);
  dict.Set(config_fields::kVmaxField, version);
  dict.Set(config_fields::kAbiHashField, "abc123def4567890");
  dict.Set(config_fields::kChannelField, "beta");
  base::ListValue cdn_urls;
  cdn_urls.Append("https://example.test/cef/");
  dict.Set(config_fields::kCdnUrlsField, std::move(cdn_urls));
  dict.Set(config_fields::kLaunchHealthField, "explicit");
  dict.Set(config_fields::kEnableExplicitModesField, true);
  dict.Set(config_fields::kUncheckedCefPathField, "C:\\unchecked");
  dict.Set(config_fields::kBundledCefPathField, "C:\\bundled");
  dict.Set(config_fields::kInstallPathField, "C:\\install");
  dict.Set(config_fields::kCertificateThumbprintField, "thumbprint");
  dict.Set(config_fields::kForceCheckField, true);
  dict.Set(config_fields::kShowProgressUiField, false);
  dict.Set(config_fields::kParentWindowField, "0");
  dict.Set(config_fields::kLocalDownloadPathField, "C:\\mirror");
  dict.Set(config_fields::kLogLevelField, "warning");
  dict.Set(config_fields::kDownloadTimeoutMsField, 1000);
  dict.Set(config_fields::kMaxAgeDaysField, 90);
  std::string json;
  ASSERT_TRUE(base::JSONWriter::Write(dict, &json));

  base::test::MockLog log;
  EXPECT_CALL(log, Log(::logging::LOGGING_WARNING, testing::_, testing::_,
                       testing::_, testing::_))
      .Times(0);
  log.StartCapturingLogs();

  ConfigParseOptions options;
  options.allow_enable_explicit_modes = true;
  options.allow_unchecked_cef_path = true;
  options.allow_bundled_cef_path = true;
  options.allow_install_path = true;
  Config config;
  EXPECT_EQ(ConfigError::kSuccess, ParseConfigFromJson(json, &config, options));
}

// ============================================================================
// Channel Tests
// ============================================================================

TEST_F(InstallerConfigTest, ParseChannelBeta) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "channel": "beta"
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_EQ("beta", config.channel);
}

TEST_F(InstallerConfigTest, ParseChannelEmptyDefault) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1"
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_TRUE(config.channel.empty());  // Empty = stable
}

TEST_F(InstallerConfigTest, ParseChannelInvalid) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "channel": "canary"
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kInvalidFieldValue, error);
}

TEST_F(InstallerConfigTest, ParseChannelStableExplicitInvalid) {
  // "stable" is not a valid explicit value - use empty string instead
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "channel": "stable"
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kInvalidFieldValue, error);
}

// ============================================================================
// enable_explicit_modes Tests
// ============================================================================

TEST_F(InstallerConfigTest, ParseEnableExplicitModesTrue) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "enable_explicit_modes": true
  })";

  Config config;
  ConfigError error =
      ParseConfigFromJson(json, &config, {.allow_enable_explicit_modes = true});

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_TRUE(config.enable_explicit_modes);
}

TEST_F(InstallerConfigTest, ParseEnableExplicitModesFalse) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "enable_explicit_modes": false
  })";

  Config config;
  ConfigError error =
      ParseConfigFromJson(json, &config, {.allow_enable_explicit_modes = true});

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_FALSE(config.enable_explicit_modes);
}

TEST_F(InstallerConfigTest, ParseEnableExplicitModesDefault) {
  // Absent field defaults to false even when allowed
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1"
  })";

  Config config;
  ConfigError error =
      ParseConfigFromJson(json, &config, {.allow_enable_explicit_modes = true});

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_FALSE(config.enable_explicit_modes);
}

TEST_F(InstallerConfigTest, ParseEnableExplicitModesIgnoredByDefault) {
  // When allow_enable_explicit_modes is false (default), the field is ignored
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "enable_explicit_modes": true
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_FALSE(config.enable_explicit_modes);
}

// ============================================================================
// unchecked_cef_path Tests
// ============================================================================

TEST_F(InstallerConfigTest, ParseUncheckedCefPath) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "unchecked_cef_path": "."
  })";

  Config config;
  ConfigError error =
      ParseConfigFromJson(json, &config, {.allow_unchecked_cef_path = true});

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_EQ(".", config.unchecked_cef_path);
}

TEST_F(InstallerConfigTest, ParseUncheckedCefPathAbsolute) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "unchecked_cef_path": "C:\\App\\CEF"
  })";

  Config config;
  ConfigError error =
      ParseConfigFromJson(json, &config, {.allow_unchecked_cef_path = true});

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_EQ("C:\\App\\CEF", config.unchecked_cef_path);
}

TEST_F(InstallerConfigTest, ParseUncheckedCefPathEmptyIgnored) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "unchecked_cef_path": ""
  })";

  Config config;
  ConfigError error =
      ParseConfigFromJson(json, &config, {.allow_unchecked_cef_path = true});

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_TRUE(config.unchecked_cef_path.empty());
}

TEST_F(InstallerConfigTest, ParseUncheckedCefPathAbsentDefault) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1"
  })";

  Config config;
  ConfigError error =
      ParseConfigFromJson(json, &config, {.allow_unchecked_cef_path = true});

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_TRUE(config.unchecked_cef_path.empty());
}

TEST_F(InstallerConfigTest, ParseUncheckedCefPathIgnoredByDefault) {
  // When allow_unchecked_cef_path is false (default), the field is ignored
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "unchecked_cef_path": "C:\\App\\CEF"
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_TRUE(config.unchecked_cef_path.empty());
}

// ============================================================================
// bundled_cef_path Tests
// ============================================================================

TEST_F(InstallerConfigTest, ParseBundledCefPath) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "bundled_cef_path": "bundled_cef"
  })";

  Config config;
  ConfigError error =
      ParseConfigFromJson(json, &config, {.allow_bundled_cef_path = true});

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_EQ("bundled_cef", config.bundled_cef_path);
}

TEST_F(InstallerConfigTest, ParseBundledCefPathAbsolute) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "bundled_cef_path": "C:\\App\\BundledCEF"
  })";

  Config config;
  ConfigError error =
      ParseConfigFromJson(json, &config, {.allow_bundled_cef_path = true});

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_EQ("C:\\App\\BundledCEF", config.bundled_cef_path);
}

TEST_F(InstallerConfigTest, ParseBundledCefPathEmptyIgnored) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "bundled_cef_path": ""
  })";

  Config config;
  ConfigError error =
      ParseConfigFromJson(json, &config, {.allow_bundled_cef_path = true});

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_TRUE(config.bundled_cef_path.empty());
}

TEST_F(InstallerConfigTest, ParseBundledCefPathIgnoredByDefault) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "bundled_cef_path": "C:\\App\\BundledCEF"
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_TRUE(config.bundled_cef_path.empty());
}

// ============================================================================
// ConfigErrorToString Tests
// ============================================================================

TEST_F(InstallerConfigTest, ConfigErrorToString) {
  EXPECT_STREQ("Success", ConfigErrorToString(ConfigError::kSuccess));
  EXPECT_STREQ("Malformed JSON",
               ConfigErrorToString(ConfigError::kJsonParseError));
  EXPECT_STREQ("Required field missing",
               ConfigErrorToString(ConfigError::kMissingRequiredField));
  EXPECT_STREQ("Invalid field value",
               ConfigErrorToString(ConfigError::kInvalidFieldValue));
  EXPECT_STREQ("Embedded resource not found",
               ConfigErrorToString(ConfigError::kResourceNotFound));
}

TEST_F(InstallerConfigTest, ParseInstallPathAllowedForClientResource) {
  const std::string vmin =
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString();
  const std::string json =
      R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":")" + vmin +
      R"(","install_path":"cef\\private"})";

  Config config;
  EXPECT_EQ(ConfigError::kSuccess,
            ParseConfigFromJson(json, &config, {.allow_install_path = true}));
  EXPECT_EQ("cef\\private", config.install_path);
}

TEST_F(InstallerConfigTest, ParseInstallPathEmptyAndMissingClearValue) {
  const std::string vmin =
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString();
  const std::string prefix =
      R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":")" + vmin;
  Config config;
  ASSERT_EQ(ConfigError::kSuccess,
            ParseConfigFromJson(prefix + R"(","install_path":"first"})",
                                &config, {.allow_install_path = true}));
  ASSERT_EQ("first", config.install_path);
  EXPECT_EQ(ConfigError::kSuccess,
            ParseConfigFromJson(prefix + R"(","install_path":""})", &config,
                                {.allow_install_path = true}));
  EXPECT_TRUE(config.install_path.empty());
  config.install_path = "stale";
  EXPECT_EQ(ConfigError::kSuccess,
            ParseConfigFromJson(prefix + R"("})", &config,
                                {.allow_install_path = true}));
  EXPECT_TRUE(config.install_path.empty());
}

TEST_F(InstallerConfigTest, ParseInstallPathWrongTypeRejected) {
  const std::string vmin =
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString();
  const std::string json =
      R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":")" + vmin +
      R"(","install_path":7})";
  Config config;
  std::string diagnostic;
  EXPECT_EQ(ConfigError::kInvalidFieldValue,
            ParseConfigFromJson(json, &config, {.allow_install_path = true},
                                &diagnostic));
  EXPECT_EQ("install_path must be a string", diagnostic);
  EXPECT_TRUE(config.install_path.empty());
}

TEST_F(InstallerConfigTest, ParseInstallPathDisallowedAndReuseClearsValue) {
  const std::string vmin =
      Version::FromApiVersion(CEF_API_VERSION_LAST).ToString();
  const std::string json =
      R"({"appid":"12345678-1234-1234-1234-123456789abc","vmin":")" + vmin +
      R"(","install_path":"ignored"})";
  Config config;
  ASSERT_EQ(ConfigError::kSuccess,
            ParseConfigFromJson(json, &config, {.allow_install_path = true}));
  ASSERT_EQ("ignored", config.install_path);
  EXPECT_EQ(ConfigError::kSuccess, ParseConfigFromJson(json, &config));
  EXPECT_TRUE(config.install_path.empty());
}

// ============================================================================
// ConfigToJson Tests
// ============================================================================

TEST_F(InstallerConfigTest, ConfigToJson_AllFields) {
  // Use vmin based on CEF_API_VERSION_LAST so it won't be clamped
  Version next_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
  std::string vmin_str = next_ver.ToString();

  Config config;
  config.appid = "12345678-1234-1234-1234-123456789abc";
  config.vmin = vmin_str;
  config.vmax = vmin_str + ".99";
  config.abi_hash = "abc123def4567890";
  config.channel = "beta";
  config.cdn_urls = {"https://one.example/", "https://one.example/"};
  config.launch_health = LaunchHealthMode::kExplicit;
  config.enable_explicit_modes = true;
  config.unchecked_cef_path = "C:\\App\\CEF";
  config.bundled_cef_path = "C:\\App\\BundledCEF";
  config.install_path = "C:\\App\\PrivateStore";

  std::string json = ConfigToJson(config);
  EXPECT_EQ(std::string::npos, json.find("install_path"));

  // Round-trip: parse the JSON back and verify all fields
  Config parsed;
  EXPECT_EQ(ConfigError::kSuccess,
            ParseConfigFromJson(json, &parsed,
                                {.allow_enable_explicit_modes = true,
                                 .allow_unchecked_cef_path = true,
                                 .allow_bundled_cef_path = true}));
  EXPECT_EQ(config.appid, parsed.appid);
  EXPECT_EQ(config.vmin, parsed.vmin);
  EXPECT_EQ(config.vmax, parsed.vmax);
  EXPECT_EQ(config.abi_hash, parsed.abi_hash);
  EXPECT_EQ(config.channel, parsed.channel);
  EXPECT_EQ(config.cdn_urls, parsed.cdn_urls);
  EXPECT_EQ(config.launch_health, parsed.launch_health);
  EXPECT_EQ(config.enable_explicit_modes, parsed.enable_explicit_modes);
  EXPECT_EQ(config.unchecked_cef_path, parsed.unchecked_cef_path);
  EXPECT_EQ(config.bundled_cef_path, parsed.bundled_cef_path);
}

TEST_F(InstallerConfigTest, ConfigToJson_RequiredFieldsOnly) {
  // Use vmin based on CEF_API_VERSION_LAST so it won't be clamped
  Version next_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
  std::string vmin_str = next_ver.ToString();

  Config config;
  config.appid = "12345678-1234-1234-1234-123456789abc";
  config.vmin = vmin_str;

  std::string json = ConfigToJson(config);

  Config parsed;
  EXPECT_EQ(ConfigError::kSuccess, ParseConfigFromJson(json, &parsed));
  EXPECT_EQ(config.appid, parsed.appid);
  EXPECT_EQ(config.vmin, parsed.vmin);
  EXPECT_TRUE(parsed.vmax.empty());
  EXPECT_TRUE(parsed.abi_hash.empty());
  EXPECT_TRUE(parsed.channel.empty());
  EXPECT_TRUE(parsed.cdn_urls.empty());
  EXPECT_EQ(LaunchHealthMode::kOff, parsed.launch_health);
  EXPECT_FALSE(parsed.enable_explicit_modes);
  EXPECT_TRUE(parsed.unchecked_cef_path.empty());
  EXPECT_TRUE(parsed.bundled_cef_path.empty());
}

// ============================================================================
// ReadConfigFromResource Tests
// ============================================================================

TEST_F(InstallerConfigTest, ReadConfigFromResource_NullConfig) {
  // Passing nullptr should return an error, not crash
  EXPECT_EQ(ConfigError::kMissingRequiredField,
            ReadConfigFromResource(GetModuleHandle(nullptr), nullptr));
}

TEST_F(InstallerConfigTest, ReadConfigFromResource_ResourceNotFound) {
  Config config;
  // Current module has no CEF_INSTALLER_CONFIG resource
  EXPECT_EQ(ConfigError::kResourceNotFound,
            ReadConfigFromResource(GetModuleHandle(nullptr), &config));
}

TEST_F(InstallerConfigTest, ParseEmptyVmin) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": ""
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);
  EXPECT_EQ(ConfigError::kMissingRequiredField, error);
}

TEST_F(InstallerConfigTest, ParseInvalidVmin) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "not-a-version"
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);
  EXPECT_EQ(ConfigError::kInvalidFieldValue, error);
}

TEST_F(InstallerConfigTest, ParseInvalidVmax) {
  const char* json = R"({
    "appid": "12345678-1234-1234-1234-123456789abc",
    "vmin": "137.1",
    "vmax": "not-a-version"
  })";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);
  EXPECT_EQ(ConfigError::kInvalidFieldValue, error);
}

TEST_F(InstallerConfigTest, ParseNullConfig) {
  ConfigError error = ParseConfigFromJson("{}", nullptr);
  EXPECT_EQ(ConfigError::kMissingRequiredField, error);
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(InstallerConfigTest, ParseCertificateThumbprint) {
  Version next_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
  std::string vmin_str = next_ver.ToString();
  std::string json =
      R"({"appid": "12345678-1234-1234-1234-123456789abc", "vmin": ")" +
      vmin_str +
      R"(", "certificate_thumbprint": "136B16359FD1209E8255DB538F3F02C8E8D8BB55"})";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_EQ("136B16359FD1209E8255DB538F3F02C8E8D8BB55",
            config.certificate_thumbprint);
}

TEST_F(InstallerConfigTest, ParseCertificateThumbprintAbsent) {
  Version next_ver = Version::FromApiVersion(CEF_API_VERSION_LAST);
  std::string vmin_str = next_ver.ToString();
  std::string json =
      R"({"appid": "12345678-1234-1234-1234-123456789abc", "vmin": ")" +
      vmin_str + R"("})";

  Config config;
  ConfigError error = ParseConfigFromJson(json, &config);

  EXPECT_EQ(ConfigError::kSuccess, error);
  EXPECT_TRUE(config.certificate_thumbprint.empty());
}
#endif

}  // namespace
}  // namespace cef_installer
