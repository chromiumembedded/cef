// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_policy.h"

#include <windows.h>

#include <array>
#include <cstring>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "cef/libcef_dll/bootstrap/installer/installer_download.h"
#include "cef/libcef_dll/bootstrap/installer/installer_registry.h"
#include "crypto/sha2.h"

namespace cef_installer {
namespace {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
std::optional<PolicyLoadResult>& GetPolicyOverride() {
  static base::NoDestructor<std::optional<PolicyLoadResult>> value;
  return *value;
}
#endif

PolicyLoadResult Invalid(const char* value_name, const char* reason) {
  PolicyLoadResult result;
  result.status = PolicyLoadStatus::kInvalid;
  result.diagnostic = std::string("Invalid enterprise policy value ") +
                      value_name + ": " + reason;
  return result;
}

bool ParseRawPolicyValue(PolicyValueType expected_type,
                         DWORD registry_type,
                         base::span<const uint8_t> bytes,
                         PolicyValue* output,
                         std::string* error) {
  const DWORD expected_registry_type =
      expected_type == PolicyValueType::kDword
          ? REG_DWORD
          : (expected_type == PolicyValueType::kString ? REG_SZ : REG_MULTI_SZ);
  if (registry_type != expected_registry_type) {
    *error = "has the wrong registry type";
    return false;
  }

  PolicyValue value;
  value.type = expected_type;
  if (expected_type == PolicyValueType::kDword) {
    if (bytes.size() != sizeof(DWORD)) {
      *error = "has an invalid DWORD size";
      return false;
    }
    DWORD dword = 0;
    std::memcpy(&dword, bytes.data(), sizeof(dword));
    value.dword_value = dword;
    *output = std::move(value);
    return true;
  }

  if (bytes.empty() || bytes.size() % sizeof(wchar_t) != 0) {
    *error = "has invalid string data";
    return false;
  }
  std::wstring data(bytes.size() / sizeof(wchar_t), L'\0');
  std::memcpy(data.data(), bytes.data(), bytes.size());
  if (expected_type == PolicyValueType::kString) {
    if (data.back() != L'\0' ||
        std::find(data.begin(), data.end() - 1, L'\0') != data.end() - 1) {
      *error = "must contain one trailing null and no embedded nulls";
      return false;
    }
    value.string_value.assign(data.data(), data.size() - 1);
    *output = std::move(value);
    return true;
  }

  if (data.size() < 2 || data[data.size() - 1] != L'\0' ||
      data[data.size() - 2] != L'\0') {
    *error = "must end with exactly two nulls";
    return false;
  }
  const size_t terminator = data.size() - 2;
  size_t offset = 0;
  while (offset < terminator) {
    const auto end =
        std::find(data.begin() + offset, data.begin() + terminator + 1, L'\0');
    if (end == data.begin() + terminator + 1 || end == data.begin() + offset) {
      *error = "contains malformed or trailing multi-string data";
      return false;
    }
    value.multi_string_value.emplace_back(data.begin() + offset, end);
    offset = static_cast<size_t>(end - data.begin()) + 1;
  }
  if (offset != terminator + 1 && terminator != 0) {
    *error = "contains malformed multi-string termination";
    return false;
  }
  *output = std::move(value);
  return true;
}

bool ReadRawValue(const base::win::RegKey& key,
                  const wchar_t* name,
                  PolicyValueType expected_type,
                  std::optional<PolicyValue>* output,
                  std::string* error) {
  DWORD size = 0;
  DWORD type = REG_NONE;
  LONG status = key.ReadValue(name, nullptr, &size, &type);
  if (status == ERROR_FILE_NOT_FOUND) {
    output->reset();
    return true;
  }
  if (status != ERROR_SUCCESS) {
    *error = "could not be read";
    return false;
  }

  std::vector<uint8_t> bytes(size);
  if (size != 0) {
    status = key.ReadValue(name, bytes.data(), &size, &type);
    if (status != ERROR_SUCCESS) {
      *error = "could not be read consistently";
      return false;
    }
    bytes.resize(size);
  }
  PolicyValue value;
  if (!ParseRawPolicyValue(expected_type, type, bytes, &value, error)) {
    return false;
  }
  *output = std::move(value);
  return true;
}

bool ValidateBoolean(const std::optional<PolicyValue>& value,
                     const char* name,
                     bool default_value,
                     bool* result,
                     PolicyLoadResult* error) {
  *result = default_value;
  if (!value) {
    return true;
  }
  if (value->type != PolicyValueType::kDword) {
    *error = Invalid(name, "expected REG_DWORD");
    return false;
  }
  if (value->dword_value > 1) {
    *error = Invalid(name, "expected 0 or 1");
    return false;
  }
  *result = value->dword_value == 1;
  return true;
}

bool ValidateCdnUrl(const std::string& input, std::string* normalized) {
  if (input.empty() || input.size() > kMaxCdnUrlBytes ||
      input.find_first_of("?#\r\n") != std::string::npos ||
      input.find('\\') != std::string::npos || !IsValidDownloadUrl(input)) {
    return false;
  }

  const size_t authority_start = input.find("://") + 3;
  const size_t authority_end = input.find('/', authority_start);
  if (input.substr(authority_start, authority_end - authority_start)
          .find('@') != std::string::npos) {
    return false;
  }

  internal::UrlComponents components;
  if (!internal::ParseUrl(input, &components) || !components.is_https ||
      components.host.empty()) {
    return false;
  }

  const std::string path = base::WideToUTF8(components.path);
  if (path.find("/../") != std::string::npos ||
      base::EndsWith(path, "/..", base::CompareCase::SENSITIVE) ||
      path.find("/./") != std::string::npos ||
      base::EndsWith(path, "/.", base::CompareCase::SENSITIVE)) {
    return false;
  }

  *normalized = input;
  if (!base::EndsWith(*normalized, "/", base::CompareCase::SENSITIVE)) {
    normalized->push_back('/');
  }
  return true;
}

}  // namespace

bool ValidateAndNormalizeCdnUrls(const std::vector<std::string>& urls,
                                 std::vector<std::string>* normalized,
                                 std::string* diagnostic) {
  if (diagnostic) {
    diagnostic->clear();
  }
  if (!normalized) {
    if (diagnostic) {
      *diagnostic = "CDN URL output is null";
    }
    return false;
  }
  if (urls.empty() || urls.size() > kMaxCdnUrls) {
    if (diagnostic) {
      *diagnostic = "expected one through three URLs";
    }
    return false;
  }

  std::vector<std::string> result;
  result.reserve(urls.size());
  for (const auto& url : urls) {
    std::string value;
    if (!ValidateCdnUrl(url, &value)) {
      if (diagnostic) {
        *diagnostic = "contains an invalid HTTPS base URL";
      }
      return false;
    }
    result.push_back(std::move(value));
  }
  *normalized = std::move(result);
  return true;
}

PolicyLoadResult ValidateEnterprisePolicySnapshot(
    const EnterprisePolicySnapshot& snapshot) {
  PolicyLoadResult result;
  result.status = PolicyLoadStatus::kValid;

  bool disable_downloads = false;
  if (!ValidateBoolean(snapshot.allow_shared_user_store, "AllowSharedUserStore",
                       true, &result.policy.allow_shared_user_store, &result) ||
      !ValidateBoolean(snapshot.disable_downloads, "DisableDownloads", false,
                       &disable_downloads, &result)) {
    return result;
  }

  const bool has_urls = snapshot.cdn_urls.has_value();
  const bool has_mirror = snapshot.download_path.has_value();
  const int source_count = static_cast<int>(has_urls) +
                           static_cast<int>(has_mirror) +
                           static_cast<int>(disable_downloads);
  if (source_count > 1) {
    return Invalid("download source",
                   "CdnUrls, DownloadPath, and "
                   "DisableDownloads are mutually exclusive");
  }

  if (has_urls) {
    if (snapshot.cdn_urls->type != PolicyValueType::kMultiString) {
      return Invalid("CdnUrls", "expected REG_MULTI_SZ");
    }
    const auto& urls = snapshot.cdn_urls->multi_string_value;
    std::vector<std::string> utf8_urls;
    utf8_urls.reserve(urls.size());
    for (const auto& wide_url : urls) {
      utf8_urls.push_back(base::WideToUTF8(wide_url));
    }
    std::string diagnostic;
    if (!ValidateAndNormalizeCdnUrls(
            utf8_urls, &result.policy.download_source.urls, &diagnostic)) {
      return Invalid("CdnUrls", diagnostic.c_str());
    }
    result.policy.download_source.kind = DownloadSourceKind::kPolicyUrls;
    result.policy.download_source.authority =
        DownloadSourceAuthority::kEnterprisePolicy;
  } else if (has_mirror) {
    if (snapshot.download_path->type != PolicyValueType::kString) {
      return Invalid("DownloadPath", "expected REG_SZ");
    }
    base::FilePath path(snapshot.download_path->string_value);
    if (path.empty() || !path.IsAbsolute() || path.ReferencesParent()) {
      return Invalid("DownloadPath", "expected an absolute local or UNC path");
    }
    result.policy.download_source.kind = DownloadSourceKind::kPolicyMirror;
    result.policy.download_source.authority =
        DownloadSourceAuthority::kEnterprisePolicy;
    result.policy.download_source.mirror_path = std::move(path);
  } else if (disable_downloads) {
    result.policy.download_source.kind = DownloadSourceKind::kDisabled;
    result.policy.download_source.authority =
        DownloadSourceAuthority::kEnterprisePolicy;
  }
  return result;
}

PolicyLoadResult LoadEnterprisePolicy() {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (GetPolicyOverride()) {
    return *GetPolicyOverride();
  }
#endif
  const wchar_t* policy_key = kEnterprisePolicyRegistryKey;
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  std::wstring test_policy_key;
  bool use_test_hkcu = false;
  std::array<wchar_t, 512> test_key_buffer = {};
  const DWORD test_key_length = ::GetEnvironmentVariableW(
      L"CEF_INSTALLER_POLICY_TEST_KEY", test_key_buffer.data(),
      static_cast<DWORD>(test_key_buffer.size()));
  if (test_key_length > 0 && test_key_length < test_key_buffer.size()) {
    test_policy_key.assign(test_key_buffer.data(), test_key_length);
    constexpr std::wstring_view kAllowedTestPrefix =
        L"SOFTWARE\\CEF\\InstallerPolicyTests";
    if (!base::StartsWith(test_policy_key, kAllowedTestPrefix,
                          base::CompareCase::INSENSITIVE_ASCII) ||
        test_policy_key.size() <= kAllowedTestPrefix.size() ||
        test_policy_key[kAllowedTestPrefix.size()] !=
            static_cast<wchar_t>(92) ||
        test_policy_key.find(L"..") != std::wstring::npos) {
      return Invalid("test policy key", "is outside the isolated test root");
    }
    policy_key = test_policy_key.c_str();

    std::array<wchar_t, 16> test_hive_buffer = {};
    const DWORD test_hive_length = ::GetEnvironmentVariableW(
        L"CEF_INSTALLER_POLICY_TEST_HIVE", test_hive_buffer.data(),
        static_cast<DWORD>(test_hive_buffer.size()));
    if (test_hive_length > 0) {
      if (test_hive_length >= test_hive_buffer.size()) {
        return Invalid("test policy hive", "is invalid");
      }
      const std::wstring_view test_hive(test_hive_buffer.data(),
                                        test_hive_length);
      if (base::EqualsCaseInsensitiveASCII(test_hive, L"HKCU")) {
        use_test_hkcu = true;
      } else if (!base::EqualsCaseInsensitiveASCII(test_hive, L"HKLM")) {
        return Invalid("test policy hive", "must be HKCU or HKLM");
      }
    }
  }
#endif
  base::win::RegKey key;
  LONG open_result;
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (use_test_hkcu) {
    open_result = key.Open(HKEY_CURRENT_USER, policy_key,
                           GetSharedMachineRegistryAccess(KEY_READ));
  } else
#endif
  {
    open_result = OpenSharedMachineRegistryKey(policy_key, KEY_READ, &key);
  }
  if (open_result == ERROR_FILE_NOT_FOUND) {
    return PolicyLoadResult{};
  }
  if (open_result != ERROR_SUCCESS) {
    return Invalid("policy key", "could not be opened");
  }

  EnterprisePolicySnapshot snapshot;
  const std::array<std::tuple<const wchar_t*, const char*, PolicyValueType,
                              std::optional<PolicyValue>*>,
                   4>
      values = {{{kAllowSharedUserStorePolicyValue, "AllowSharedUserStore",
                  PolicyValueType::kDword, &snapshot.allow_shared_user_store},
                 {kCdnUrlsPolicyValue, "CdnUrls", PolicyValueType::kMultiString,
                  &snapshot.cdn_urls},
                 {kDownloadPathPolicyValue, "DownloadPath",
                  PolicyValueType::kString, &snapshot.download_path},
                 {kDisableDownloadsPolicyValue, "DisableDownloads",
                  PolicyValueType::kDword, &snapshot.disable_downloads}}};
  for (const auto& [wide_name, name, type, output] : values) {
    std::string error;
    if (!ReadRawValue(key, wide_name, type, output, &error)) {
      return Invalid(name, error.c_str());
    }
  }
  return ValidateEnterprisePolicySnapshot(snapshot);
}

EffectiveDownloadSource ResolveEffectiveDownloadSource(
    const EnterprisePolicy& policy,
    const std::vector<std::string>& operation_urls,
    const std::vector<std::string>& application_urls,
    const base::FilePath& operation_mirror) {
  if (policy.download_source.kind !=
      DownloadSourceKind::kDefaultOrApplication) {
    EffectiveDownloadSource source = policy.download_source;
    if (source.kind == DownloadSourceKind::kPolicyMirror &&
        source.urls.empty()) {
      source.urls.emplace_back(kDefaultCdnBaseUrl);
    }
    return source;
  }

  EffectiveDownloadSource source;
  source.authority = DownloadSourceAuthority::kApplication;
  if (!operation_mirror.empty()) {
    source.urls.emplace_back(kDefaultCdnBaseUrl);
    source.mirror_path = operation_mirror;
  } else if (!operation_urls.empty()) {
    source.urls = operation_urls;
  } else if (!application_urls.empty()) {
    source.urls = application_urls;
  } else {
    source.urls.emplace_back(kDefaultCdnBaseUrl);
  }
  return source;
}

std::string GetDownloadSourceIdentity(const EffectiveDownloadSource& source) {
  std::string serialized =
      "v1|" + base::NumberToString(static_cast<int>(source.kind));
  for (const auto& url : source.urls) {
    serialized += "|u:" + url;
  }
  if (!source.mirror_path.empty()) {
    serialized += "|m:" + source.mirror_path.AsUTF8Unsafe();
  }
  return base::ToLowerASCII(
      base::HexEncode(crypto::SHA256Hash(base::as_byte_span(serialized))));
}

namespace internal {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
bool ParsePolicyRegistryValueForTesting(PolicyValueType expected_type,
                                        uint32_t registry_type,
                                        const std::vector<uint8_t>& bytes,
                                        PolicyValue* output,
                                        std::string* error) {
  return ParseRawPolicyValue(expected_type, registry_type, bytes, output,
                             error);
}
#endif

void OverrideEnterprisePolicyForTesting(
    std::optional<PolicyLoadResult> policy) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  GetPolicyOverride() = std::move(policy);
#endif
}

}  // namespace internal

}  // namespace cef_installer
