// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_POLICY_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_POLICY_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"

namespace cef_installer {

inline constexpr wchar_t kEnterprisePolicyRegistryKey[] =
    L"SOFTWARE\\Policies\\CEF";
inline constexpr wchar_t kAllowSharedUserStorePolicyValue[] =
    L"AllowSharedUserStore";
inline constexpr wchar_t kCdnUrlsPolicyValue[] = L"CdnUrls";
inline constexpr wchar_t kDownloadPathPolicyValue[] = L"DownloadPath";
inline constexpr wchar_t kDisableDownloadsPolicyValue[] = L"DisableDownloads";

inline constexpr size_t kMaxCdnUrlBytes = 2048;
inline constexpr size_t kMaxCdnUrls = 3;

enum class DownloadSourceKind {
  kDefaultOrApplication,
  kPolicyUrls,
  kPolicyMirror,
  kDisabled,
};

enum class DownloadSourceAuthority {
  kApplication,
  kEnterprisePolicy,
};

struct EffectiveDownloadSource {
  DownloadSourceKind kind = DownloadSourceKind::kDefaultOrApplication;
  DownloadSourceAuthority authority = DownloadSourceAuthority::kApplication;
  std::vector<std::string> urls;
  base::FilePath mirror_path;

  bool downloads_disabled() const {
    return kind == DownloadSourceKind::kDisabled;
  }
  bool persist_revocations() const {
    return authority == DownloadSourceAuthority::kEnterprisePolicy &&
           kind == DownloadSourceKind::kPolicyMirror;
  }
};

struct EnterprisePolicy {
  bool allow_shared_user_store = true;
  EffectiveDownloadSource download_source;
};

enum class PolicyValueType {
  kDword,
  kString,
  kMultiString,
};

// Immutable copy of the known registry values from one policy load pass.
struct PolicyValue {
  PolicyValueType type = PolicyValueType::kDword;
  uint32_t dword_value = 0;
  std::wstring string_value;
  std::vector<std::wstring> multi_string_value;
};

struct EnterprisePolicySnapshot {
  std::optional<PolicyValue> allow_shared_user_store;
  std::optional<PolicyValue> cdn_urls;
  std::optional<PolicyValue> download_path;
  std::optional<PolicyValue> disable_downloads;
};

enum class PolicyLoadStatus {
  kAbsent,
  kValid,
  kInvalid,
};

struct PolicyLoadResult {
  PolicyLoadStatus status = PolicyLoadStatus::kAbsent;
  EnterprisePolicy policy;
  std::string diagnostic;

  bool valid() const { return status != PolicyLoadStatus::kInvalid; }
};

// Pure snapshot validation. This is the primary injection seam for tests.
PolicyLoadResult ValidateEnterprisePolicySnapshot(
    const EnterprisePolicySnapshot& snapshot);

// Reads and validates HKLM enterprise policy using the shared machine view.
PolicyLoadResult LoadEnterprisePolicy();

// Validates and normalizes one through three HTTPS CDN base URLs. Supplied
// order and duplicates are preserved.
bool ValidateAndNormalizeCdnUrls(const std::vector<std::string>& urls,
                                 std::vector<std::string>* normalized,
                                 std::string* diagnostic = nullptr);

// Resolves the immutable source for one operation. Policy wins; otherwise an
// operation mirror wins, followed by operation URLs, selected application
// URLs, and the hardcoded default.
EffectiveDownloadSource ResolveEffectiveDownloadSource(
    const EnterprisePolicy& policy,
    const std::vector<std::string>& operation_urls,
    const std::vector<std::string>& application_urls,
    const base::FilePath& operation_mirror);

// Returns a bounded digest identity for freshness/backoff state. The identity
// includes source kind and the complete ordered source list.
std::string GetDownloadSourceIdentity(const EffectiveDownloadSource& source);

namespace internal {

// Strict raw registry parser used by the machine reader and malformed-data
// tests. |registry_type| must exactly match |expected_type|.
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
bool ParsePolicyRegistryValueForTesting(PolicyValueType expected_type,
                                        uint32_t registry_type,
                                        const std::vector<uint8_t>& bytes,
                                        PolicyValue* output,
                                        std::string* error);
#endif

// Overrides the operation policy snapshot in non-official test builds. Passing
// nullopt clears the override.
void OverrideEnterprisePolicyForTesting(std::optional<PolicyLoadResult> policy);

}  // namespace internal

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_POLICY_H_
