// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_policy.h"

#include <windows.h>

#include <algorithm>
#include <cstring>

#include "base/scoped_environment_variable_override.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

PolicyValue Dword(uint32_t value) {
  PolicyValue policy_value;
  policy_value.type = PolicyValueType::kDword;
  policy_value.dword_value = value;
  return policy_value;
}

PolicyValue String(const wchar_t* value) {
  PolicyValue policy_value;
  policy_value.type = PolicyValueType::kString;
  policy_value.string_value = value;
  return policy_value;
}

PolicyValue MultiString(std::vector<std::wstring> values) {
  PolicyValue policy_value;
  policy_value.type = PolicyValueType::kMultiString;
  policy_value.multi_string_value = std::move(values);
  return policy_value;
}

std::vector<uint8_t> RawWideBytes(const std::wstring& value) {
  std::vector<uint8_t> bytes(value.size() * sizeof(wchar_t));
  std::memcpy(bytes.data(), value.data(), bytes.size());
  return bytes;
}

TEST(InstallerPolicyTest, ValidationAbsentUsesDefaults) {
  const auto result =
      ValidateEnterprisePolicySnapshot(EnterprisePolicySnapshot{});
  EXPECT_TRUE(result.valid());
  EXPECT_EQ(PolicyLoadStatus::kValid, result.status);
  EXPECT_TRUE(result.policy.allow_shared_user_store);
  EXPECT_EQ(DownloadSourceKind::kDefaultOrApplication,
            result.policy.download_source.kind);
}

TEST(InstallerPolicyTest, ValidationBooleanDomains) {
  EnterprisePolicySnapshot snapshot;
  snapshot.allow_shared_user_store = Dword(0);
  snapshot.disable_downloads = Dword(1);
  auto result = ValidateEnterprisePolicySnapshot(snapshot);
  ASSERT_TRUE(result.valid());
  EXPECT_FALSE(result.policy.allow_shared_user_store);
  EXPECT_EQ(DownloadSourceKind::kDisabled, result.policy.download_source.kind);

  snapshot.allow_shared_user_store = Dword(2);
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
  snapshot.allow_shared_user_store = Dword(1);
  snapshot.disable_downloads = Dword(42);
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
}

TEST(InstallerPolicyTest, ValidationRejectsWrongTypes) {
  EnterprisePolicySnapshot snapshot;
  snapshot.allow_shared_user_store = String(L"1");
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
  snapshot = {};
  snapshot.cdn_urls = String(L"https://one.example/");
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
  snapshot = {};
  snapshot.download_path = MultiString({L"C:\\mirror"});
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
}

TEST(InstallerPolicyTest, ValidationPolicyUrlsPreserveOrderAndNormalize) {
  EnterprisePolicySnapshot snapshot;
  snapshot.cdn_urls =
      MultiString({L"https://one.example/prefix", L"https://two.example/"});
  const auto result = ValidateEnterprisePolicySnapshot(snapshot);
  ASSERT_TRUE(result.valid()) << result.diagnostic;
  ASSERT_EQ(2u, result.policy.download_source.urls.size());
  EXPECT_EQ("https://one.example/prefix/",
            result.policy.download_source.urls[0]);
  EXPECT_EQ("https://two.example/", result.policy.download_source.urls[1]);
  EXPECT_EQ(DownloadSourceAuthority::kEnterprisePolicy,
            result.policy.download_source.authority);
}

TEST(InstallerPolicyTest, ValidationRejectsInvalidPolicyUrls) {
  const std::vector<std::wstring> invalid = {L"",
                                             L"http://example.com/",
                                             L"https://",
                                             L"https://user@example.com/",
                                             L"https://example.com/?q=1",
                                             L"https://example.com/#fragment",
                                             L"https://example.com/../bad",
                                             L"https://example.com\\bad"};
  for (const auto& url : invalid) {
    EnterprisePolicySnapshot snapshot;
    snapshot.cdn_urls = MultiString({url});
    EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid())
        << base::WideToUTF8(url);
  }
}

TEST(InstallerPolicyTest, ValidationBoundsPolicyUrls) {
  EnterprisePolicySnapshot snapshot;
  snapshot.cdn_urls = MultiString({});
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
  snapshot.cdn_urls =
      MultiString({L"https://1.example/", L"https://2.example/",
                   L"https://3.example/", L"https://4.example/"});
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
  snapshot.cdn_urls = MultiString(
      {L"https://example.com/" + std::wstring(kMaxCdnUrlBytes, L'a')});
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
}

TEST(InstallerPolicyTest, ValidationMirrorPath) {
  EnterprisePolicySnapshot snapshot;
  snapshot.download_path = String(L"C:\\policy-mirror");
  auto result = ValidateEnterprisePolicySnapshot(snapshot);
  ASSERT_TRUE(result.valid()) << result.diagnostic;
  EXPECT_EQ(base::FilePath(L"C:\\policy-mirror"),
            result.policy.download_source.mirror_path);
  EXPECT_TRUE(result.policy.download_source.persist_revocations());

  snapshot.download_path = String(L"relative\\mirror");
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
  snapshot.download_path = String(L"C:\\mirror\\..\\other");
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
  snapshot.download_path = String(L"");
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
}

TEST(InstallerPolicyTest, ValidationRejectsEverySourceConflict) {
  EnterprisePolicySnapshot snapshot;
  snapshot.cdn_urls = MultiString({L"https://example.com/"});
  snapshot.download_path = String(L"C:\\mirror");
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
  snapshot.download_path.reset();
  snapshot.disable_downloads = Dword(1);
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
  snapshot.cdn_urls.reset();
  snapshot.download_path = String(L"C:\\mirror");
  EXPECT_FALSE(ValidateEnterprisePolicySnapshot(snapshot).valid());
}

TEST(InstallerPolicyTest, EffectivePolicyPrecedesApplicationSource) {
  EnterprisePolicy policy;
  policy.download_source.kind = DownloadSourceKind::kPolicyUrls;
  policy.download_source.authority = DownloadSourceAuthority::kEnterprisePolicy;
  policy.download_source.urls = {"https://policy.example/"};
  const auto effective = ResolveEffectiveDownloadSource(
      policy, {"https://operation.example/"}, {"https://application.example/"},
      base::FilePath(L"C:\\operation"));
  EXPECT_EQ(policy.download_source.urls, effective.urls);
  EXPECT_TRUE(effective.mirror_path.empty());
}

TEST(InstallerPolicyTest, EffectiveOperationMirrorPrecedesUrlSources) {
  const auto effective = ResolveEffectiveDownloadSource(
      EnterprisePolicy{}, {"https://operation.example/"},
      {"https://application.example/"}, base::FilePath(L"C:\\operation"));
  ASSERT_EQ(1u, effective.urls.size());
  EXPECT_EQ(kDefaultCdnBaseUrl, effective.urls[0]);
  EXPECT_EQ(base::FilePath(L"C:\\operation"), effective.mirror_path);
  EXPECT_FALSE(effective.persist_revocations());
}

TEST(InstallerPolicyTest, EffectiveUrlAndDefaultPrecedence) {
  auto effective = ResolveEffectiveDownloadSource(
      EnterprisePolicy{}, {"https://operation.example/"},
      {"https://application.example/"}, {});
  EXPECT_EQ((std::vector<std::string>{"https://operation.example/"}),
            effective.urls);

  effective = ResolveEffectiveDownloadSource(
      EnterprisePolicy{}, {}, {"https://application.example/"}, {});
  EXPECT_EQ((std::vector<std::string>{"https://application.example/"}),
            effective.urls);

  effective = ResolveEffectiveDownloadSource(EnterprisePolicy{}, {}, {}, {});
  EXPECT_EQ((std::vector<std::string>{std::string(kDefaultCdnBaseUrl)}),
            effective.urls);
}

TEST(InstallerPolicyTest, EffectiveIdentityIncludesOrderAndKind) {
  EffectiveDownloadSource first;
  first.kind = DownloadSourceKind::kPolicyUrls;
  first.urls = {"https://one.example/", "https://two.example/"};
  EffectiveDownloadSource reordered = first;
  std::reverse(reordered.urls.begin(), reordered.urls.end());
  EffectiveDownloadSource mirror;
  mirror.kind = DownloadSourceKind::kPolicyMirror;
  mirror.mirror_path = base::FilePath(L"C:\\mirror");
  EXPECT_NE(GetDownloadSourceIdentity(first),
            GetDownloadSourceIdentity(reordered));
  EXPECT_NE(GetDownloadSourceIdentity(first),
            GetDownloadSourceIdentity(mirror));
  EXPECT_EQ(64u, GetDownloadSourceIdentity(first).size());
}

TEST(InstallerPolicyTest, ApplicationIdentityUsesCompleteNormalizedOrder) {
  std::vector<std::string> normalized_a;
  std::vector<std::string> normalized_b;
  ASSERT_TRUE(ValidateAndNormalizeCdnUrls(
      {"https://one.example", "https://two.example/"}, &normalized_a));
  ASSERT_TRUE(ValidateAndNormalizeCdnUrls(
      {"https://one.example/", "https://two.example"}, &normalized_b));

  EffectiveDownloadSource first;
  first.urls = normalized_a;
  EffectiveDownloadSource equivalent = first;
  equivalent.urls = normalized_b;
  EXPECT_EQ(GetDownloadSourceIdentity(first),
            GetDownloadSourceIdentity(equivalent));

  EffectiveDownloadSource reordered = first;
  std::reverse(reordered.urls.begin(), reordered.urls.end());
  EXPECT_NE(GetDownloadSourceIdentity(first),
            GetDownloadSourceIdentity(reordered));

  EffectiveDownloadSource duplicate = first;
  duplicate.urls.push_back(duplicate.urls.front());
  EXPECT_NE(GetDownloadSourceIdentity(first),
            GetDownloadSourceIdentity(duplicate));
}

TEST(InstallerPolicyTest, RegistryTestSeamRejectsRealPolicyKey) {
  internal::OverrideEnterprisePolicyForTesting(std::nullopt);
  base::ScopedEnvironmentVariableOverride environment(
      "CEF_INSTALLER_POLICY_TEST_KEY", "SOFTWARE\\Policies\\CEF");
  const PolicyLoadResult result = LoadEnterprisePolicy();
  EXPECT_EQ(PolicyLoadStatus::kInvalid, result.status);
  EXPECT_NE(std::string::npos, result.diagnostic.find("isolated test root"));
}

TEST(InstallerPolicyTest, RegistryTestSeamRejectsUnknownHive) {
  internal::OverrideEnterprisePolicyForTesting(std::nullopt);
  base::ScopedEnvironmentVariableOverride key_environment(
      "CEF_INSTALLER_POLICY_TEST_KEY",
      "SOFTWARE\\CEF\\InstallerPolicyTests\\unknown-hive");
  base::ScopedEnvironmentVariableOverride hive_environment(
      "CEF_INSTALLER_POLICY_TEST_HIVE", "HKCR");
  const PolicyLoadResult result = LoadEnterprisePolicy();
  EXPECT_EQ(PolicyLoadStatus::kInvalid, result.status);
  EXPECT_NE(std::string::npos, result.diagnostic.find("test policy hive"));
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST(InstallerPolicyTest, RegistryRejectsEmbeddedNullInString) {
  std::wstring raw = L"C:\\mirror";
  raw.push_back(L'\0');
  raw.append(L"trailing");
  raw.push_back(L'\0');
  PolicyValue output;
  std::string error;
  EXPECT_FALSE(internal::ParsePolicyRegistryValueForTesting(
      PolicyValueType::kString, REG_SZ, RawWideBytes(raw), &output, &error));
}

TEST(InstallerPolicyTest, RegistryRejectsMalformedMultiStringTermination) {
  std::wstring missing_double_null = L"https://one.example/";
  missing_double_null.push_back(L'\0');
  PolicyValue output;
  std::string error;
  EXPECT_FALSE(internal::ParsePolicyRegistryValueForTesting(
      PolicyValueType::kMultiString, REG_MULTI_SZ,
      RawWideBytes(missing_double_null), &output, &error));

  std::wstring trailing_after_terminator = L"https://one.example/";
  trailing_after_terminator.push_back(L'\0');
  trailing_after_terminator.push_back(L'\0');
  trailing_after_terminator.append(L"https://two.example/");
  trailing_after_terminator.push_back(L'\0');
  trailing_after_terminator.push_back(L'\0');
  EXPECT_FALSE(internal::ParsePolicyRegistryValueForTesting(
      PolicyValueType::kMultiString, REG_MULTI_SZ,
      RawWideBytes(trailing_after_terminator), &output, &error));
}

TEST(InstallerPolicyTest, RegistryAcceptsExactlyTerminatedStrings) {
  std::wstring string_value = L"C:\\mirror";
  string_value.push_back(L'\0');
  PolicyValue output;
  std::string error;
  ASSERT_TRUE(internal::ParsePolicyRegistryValueForTesting(
      PolicyValueType::kString, REG_SZ, RawWideBytes(string_value), &output,
      &error));
  EXPECT_EQ(L"C:\\mirror", output.string_value);

  std::wstring multi_value = L"https://one.example/";
  multi_value.push_back(L'\0');
  multi_value.append(L"https://two.example/");
  multi_value.push_back(L'\0');
  multi_value.push_back(L'\0');
  ASSERT_TRUE(internal::ParsePolicyRegistryValueForTesting(
      PolicyValueType::kMultiString, REG_MULTI_SZ, RawWideBytes(multi_value),
      &output, &error));
  EXPECT_EQ((std::vector<std::wstring>{L"https://one.example/",
                                       L"https://two.example/"}),
            output.multi_string_value);
}
#endif

}  // namespace
}  // namespace cef_installer
