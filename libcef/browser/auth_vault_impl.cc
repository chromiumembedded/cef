// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/auth_vault_impl.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/logging.h"
#include "base/containers/span.h"
#include "base/environment.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/values_impl.h"
#include "crypto/aead.h"
#include "crypto/random.h"

namespace {

constexpr char kAgentBrowserDirectory[] = ".agent-browser";
constexpr char kAuthDirectory[] = "auth";
constexpr char kEncryptionKeyFile[] = ".encryption-key";
constexpr char kJsonExtension[] = ".json";
constexpr size_t kEncryptionKeyBytes = 32;
constexpr size_t kNonceBytes = 12;

base::FilePath GetHomeDirectory() {
  base::FilePath home_path;
  if (base::PathService::Get(base::DIR_HOME, &home_path)) {
    return home_path;
  }
  return base::FilePath();
}

base::FilePath GetAgentBrowserDirectory() {
  const auto home_path = GetHomeDirectory();
  if (home_path.empty()) {
    return base::FilePath();
  }
  return home_path.AppendASCII(kAgentBrowserDirectory);
}

base::FilePath GetDefaultAuthDirectory() {
  const auto root_path = GetAgentBrowserDirectory();
  if (root_path.empty()) {
    return base::FilePath();
  }
  return root_path.AppendASCII(kAuthDirectory);
}

base::FilePath GetDefaultEncryptionKeyPath() {
  const auto root_path = GetAgentBrowserDirectory();
  if (root_path.empty()) {
    return base::FilePath();
  }
  return root_path.AppendASCII(kEncryptionKeyFile);
}

bool IsProfileFile(const base::FilePath& path) {
  return base::EndsWith(path.BaseName().AsUTF8Unsafe(), kJsonExtension);
}

std::optional<std::vector<uint8_t>> ParseKeyHex(const std::string& hex) {
  std::string key_hex = base::TrimWhitespaceASCII(hex, base::TRIM_ALL);
  if (key_hex.size() != (kEncryptionKeyBytes * 2)) {
    return std::nullopt;
  }

  std::vector<uint8_t> key;
  key.reserve(kEncryptionKeyBytes);
  for (size_t i = 0; i < key_hex.size(); i += 2) {
    uint32_t value = 0;
    if (!base::HexStringToUInt(key_hex.substr(i, 2), &value)) {
      return std::nullopt;
    }
    key.push_back(static_cast<uint8_t>(value));
  }

  return key;
}

bool IsValidProfileName(const std::string& name) {
  if (name.empty()) {
    return false;
  }

  for (const char c : name) {
    if (!(base::IsAsciiAlphaNumeric(c) || c == '_' || c == '-')) {
      return false;
    }
  }

  return true;
}

std::optional<std::vector<uint8_t>> GetEncryptionKeyFromEnv() {
  auto env = base::Environment::Create();
  std::string env_value;
  if (!env->GetVar("AGENT_BROWSER_ENCRYPTION_KEY", &env_value)) {
    return std::nullopt;
  }
  return ParseKeyHex(env_value);
}

std::optional<std::vector<uint8_t>> ReadEncryptionKeyFile(
    const base::FilePath& path) {
  std::string key_hex;
  if (!base::ReadFileToString(path, &key_hex)) {
    return std::nullopt;
  }
  return ParseKeyHex(key_hex);
}

bool WriteEncryptionKeyFile(const base::FilePath& path,
                            const std::vector<uint8_t>& key) {
  const std::string key_hex = base::HexEncode(key);
  if (!base::WriteFile(path, key_hex + "\n")) {
    return false;
  }

#if BUILDFLAG(IS_POSIX)
  base::SetPosixFilePermissions(path, 0600);
#endif
  return true;
}

std::optional<std::vector<uint8_t>> EnsureEncryptionKey(
    const base::FilePath& key_path) {
  if (auto key = GetEncryptionKeyFromEnv()) {
    return key;
  }

  if (auto key = ReadEncryptionKeyFile(key_path)) {
    return key;
  }

  auto parent_dir = key_path.DirName();
  if (parent_dir.empty() || !base::CreateDirectory(parent_dir)) {
    return std::nullopt;
  }

#if BUILDFLAG(IS_POSIX)
  base::SetPosixFilePermissions(parent_dir, 0700);
#endif

  std::vector<uint8_t> key(kEncryptionKeyBytes);
  crypto::RandBytes(key);
  if (!WriteEncryptionKeyFile(key_path, key)) {
    return std::nullopt;
  }

  return key;
}

std::optional<std::string> EncryptProfileData(const std::string& plaintext,
                                              const std::vector<uint8_t>& key) {
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  if (!aead.Init(key)) {
    return std::nullopt;
  }

  std::vector<uint8_t> nonce(kNonceBytes);
  crypto::RandBytes(nonce);

  const std::vector<uint8_t> plaintext_bytes(plaintext.begin(), plaintext.end());
  const std::vector<uint8_t> ciphertext =
      aead.Seal(plaintext_bytes, nonce, std::vector<uint8_t>());

  if (ciphertext.size() < aead.AuthTagLength()) {
    return std::nullopt;
  }

  const size_t tag_offset = ciphertext.size() - aead.AuthTagLength();
  const std::string data_bytes(
      reinterpret_cast<const char*>(ciphertext.data()), tag_offset);
  const std::string auth_tag_bytes(
      reinterpret_cast<const char*>(ciphertext.data() + tag_offset),
      ciphertext.size() - tag_offset);
  const std::string nonce_bytes(reinterpret_cast<const char*>(nonce.data()),
                                nonce.size());
  const std::string data_b64 = base::Base64Encode(data_bytes);
  const std::string auth_tag_b64 = base::Base64Encode(auth_tag_bytes);
  const std::string nonce_b64 = base::Base64Encode(nonce_bytes);

  base::Value::Dict payload;
  payload.Set("version", 1);
  payload.Set("encrypted", true);
  payload.Set("iv", nonce_b64);
  payload.Set("authTag", auth_tag_b64);
  payload.Set("data", data_b64);

  std::string output;
  if (!base::JSONWriter::WriteWithOptions(
          payload, base::JSONWriter::OPTIONS_PRETTY_PRINT, &output)) {
    return std::nullopt;
  }

  return output;
}

std::optional<base::Value::Dict> DecryptProfileData(
    const std::string& input,
    const std::vector<uint8_t>& key) {
  std::optional<base::Value::Dict> payload = base::JSONReader::ReadDict(input);
  if (!payload.has_value()) {
    return std::nullopt;
  }

  const std::string* iv_b64 = payload->FindString("iv");
  const std::string* auth_tag_b64 = payload->FindString("authTag");
  const std::string* data_b64 = payload->FindString("data");
  if (!payload->FindBool("encrypted").value_or(false) || !iv_b64 ||
      !auth_tag_b64 || !data_b64) {
    return std::nullopt;
  }

  std::string nonce;
  std::string auth_tag;
  std::string ciphertext;
  if (!base::Base64Decode(*iv_b64, &nonce) ||
      !base::Base64Decode(*auth_tag_b64, &auth_tag) ||
      !base::Base64Decode(*data_b64, &ciphertext)) {
    return std::nullopt;
  }

  if (nonce.size() != kNonceBytes) {
    return std::nullopt;
  }

  std::string combined = ciphertext + auth_tag;
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  if (!aead.Init(key)) {
    return std::nullopt;
  }

  const std::vector<uint8_t> combined_bytes(combined.begin(), combined.end());
  const std::vector<uint8_t> nonce_bytes(nonce.begin(), nonce.end());
  const std::optional<std::vector<uint8_t>> plaintext =
      aead.Open(combined_bytes, nonce_bytes, std::vector<uint8_t>());
  if (!plaintext.has_value()) {
    return std::nullopt;
  }

  return base::JSONReader::ReadDict(
      std::string(plaintext->begin(), plaintext->end()));
}

std::optional<base::Value::Dict> ReadProfileDict(
    const base::FilePath& path,
    const base::FilePath& encryption_key_path) {
  std::string input;
  if (!base::ReadFileToString(path, &input)) {
    return std::nullopt;
  }

  if (auto key = EnsureEncryptionKey(encryption_key_path)) {
    if (auto decrypted = DecryptProfileData(input, *key)) {
      return decrypted;
    }
  }

  return base::JSONReader::ReadDict(input);
}

base::FilePath BuildProfilePath(const base::FilePath& directory,
                                const std::string& name) {
  return directory.AppendASCII(name + kJsonExtension);
}

base::Value::Dict MakeProfileMetadata(const base::FilePath& path,
                                      const base::Value::Dict& profile) {
  base::Value::Dict metadata = profile.Clone();
  metadata.Set("path", path.AsUTF8Unsafe());
  metadata.Set("name", path.BaseName().RemoveExtension().AsUTF8Unsafe());
  metadata.Set("encrypted", true);
  metadata.Set("has_password", profile.FindString("password").has_value());
  metadata.Remove("password");
  return metadata;
}

struct AuthVaultActionResult {
  bool success = false;
  std::string error;
  base::FilePath path;
};

struct AuthVaultReadResult {
  bool success = false;
  std::string error;
  base::Value::Dict profile;
};

struct AuthVaultListResult {
  bool success = false;
  std::string error;
  std::vector<base::Value::Dict> profiles;
};

AuthVaultActionResult SaveProfileOnBlockingThread(
    base::FilePath directory,
    base::FilePath encryption_key_path,
    base::Value::Dict profile) {
  AuthVaultActionResult result;

  const std::string* name = profile.FindString("name");
  if (!name || !IsValidProfileName(*name)) {
    result.error = "Profile name must match /^[A-Za-z0-9_-]+$/.";
    return result;
  }

  if (directory.empty()) {
    result.error = "Auth vault directory is unavailable.";
    return result;
  }

  if (!base::CreateDirectory(directory)) {
    result.error = "Failed to create auth vault directory.";
    return result;
  }

#if BUILDFLAG(IS_POSIX)
  base::SetPosixFilePermissions(directory, 0700);
#endif

  const auto key = EnsureEncryptionKey(encryption_key_path);
  if (!key.has_value()) {
    result.error = "Failed to initialize encryption key.";
    return result;
  }

  const auto path = BuildProfilePath(directory, *name);
  std::string plaintext;
  if (!base::JSONWriter::WriteWithOptions(
          profile, base::JSONWriter::OPTIONS_PRETTY_PRINT, &plaintext)) {
    result.error = "Failed to serialize auth profile.";
    return result;
  }

  const auto encrypted = EncryptProfileData(plaintext, *key);
  if (!encrypted.has_value()) {
    result.error = "Failed to encrypt auth profile.";
    return result;
  }

  if (!base::WriteFile(path, *encrypted)) {
    result.error = "Failed to write auth profile.";
    return result;
  }

#if BUILDFLAG(IS_POSIX)
  base::SetPosixFilePermissions(path, 0600);
#endif

  result.success = true;
  result.path = path;
  return result;
}

AuthVaultReadResult ReadProfileOnBlockingThread(base::FilePath directory,
                                                base::FilePath key_path,
                                                std::string name) {
  AuthVaultReadResult result;

  if (!IsValidProfileName(name)) {
    result.error = "Profile name must match /^[A-Za-z0-9_-]+$/.";
    return result;
  }

  const auto path = BuildProfilePath(directory, name);
  auto profile = ReadProfileDict(path, key_path);
  if (!profile.has_value()) {
    result.error = "Auth profile does not exist.";
    return result;
  }

  result.profile = MakeProfileMetadata(path, *profile);
  result.success = true;
  return result;
}

AuthVaultActionResult DeleteProfileOnBlockingThread(base::FilePath directory,
                                                    std::string name) {
  AuthVaultActionResult result;

  if (!IsValidProfileName(name)) {
    result.error = "Profile name must match /^[A-Za-z0-9_-]+$/.";
    return result;
  }

  const auto path = BuildProfilePath(directory, name);
  if (!base::DeleteFile(path)) {
    result.error = "Failed to delete auth profile.";
    return result;
  }

  result.success = true;
  result.path = path;
  return result;
}

AuthVaultListResult ListProfilesOnBlockingThread(base::FilePath directory,
                                                 base::FilePath key_path) {
  AuthVaultListResult result;

  if (directory.empty()) {
    result.error = "Auth vault directory is unavailable.";
    return result;
  }

  if (!base::PathExists(directory)) {
    result.success = true;
    return result;
  }

  base::FileEnumerator enumerator(directory, false, base::FileEnumerator::FILES);
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    if (!IsProfileFile(entry)) {
      continue;
    }

    auto parsed = ReadProfileDict(entry, key_path);
    if (!parsed.has_value()) {
      continue;
    }

    result.profiles.push_back(MakeProfileMetadata(entry, *parsed));
  }

  result.success = true;
  return result;
}

}  // namespace

CefAuthVaultImpl::CefAuthVaultImpl() = default;

void CefAuthVaultImpl::MarkProfileDirty(const std::string& profile_name) {
  dirty_profiles_.insert(profile_name);
}

bool CefAuthVaultImpl::IsProfileDirty(
    const std::string& profile_name) const {
  return dirty_profiles_.find(profile_name) != dirty_profiles_.end();
}

void CefAuthVaultImpl::ClearProfileDirty(const std::string& profile_name) {
  dirty_profiles_.erase(profile_name);
}

// static
CefRefPtr<CefAuthVault> CefAuthVault::GetGlobalVault() {
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return nullptr;
  }

  return CefContext::Get()->GetAuthVault();
}

void CefAuthVaultImpl::SaveProfile(
    CefRefPtr<CefDictionaryValue> profile,
    CefRefPtr<CefAuthVaultActionCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefAuthVaultImpl::SaveProfile, this,
                                          profile, callback));
    return;
  }

  if (!profile) {
    RunActionCallback(callback, false, "Profile metadata is empty.",
                      base::FilePath());
    return;
  }

  std::unique_ptr<base::Value> profile_value =
      static_cast<CefDictionaryValueImpl*>(profile.get())->CopyOrDetachValue(
          nullptr);
  if (!profile_value || !profile_value->is_dict()) {
    RunActionCallback(callback, false, "Profile metadata is invalid.",
                      base::FilePath());
    return;
  }

  // Extract profile name to use for dirty tracking.
  const std::string* name = profile_value->GetDict().FindString("name");
  std::string profile_name = name ? *name : std::string();

  // The caller is providing new data, so mark the profile as dirty.
  if (!profile_name.empty()) {
    MarkProfileDirty(profile_name);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SaveProfileOnBlockingThread, GetVaultPathInternal(),
                     GetEncryptionKeyPathInternal(),
                     std::move(profile_value->GetDict())),
      base::BindOnce(&CefAuthVaultImpl::OnActionComplete, this, callback,
                     profile_name));
}

void CefAuthVaultImpl::ReadProfile(
    const CefString& name,
    CefRefPtr<CefAuthVaultReadCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefAuthVaultImpl::ReadProfile, this,
                                          name, callback));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadProfileOnBlockingThread, GetVaultPathInternal(),
                     GetEncryptionKeyPathInternal(), name.ToString()),
      base::BindOnce(&CefAuthVaultImpl::OnReadComplete, this, callback));
}

void CefAuthVaultImpl::DeleteProfile(
    const CefString& name,
    CefRefPtr<CefAuthVaultActionCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefAuthVaultImpl::DeleteProfile, this, name,
                                 callback));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&DeleteProfileOnBlockingThread, GetVaultPathInternal(),
                     name.ToString()),
      base::BindOnce(&CefAuthVaultImpl::OnActionComplete, this, callback,
                     std::string()));
}

void CefAuthVaultImpl::VisitProfiles(CefRefPtr<CefAuthProfileVisitor> visitor) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefAuthVaultImpl::VisitProfiles,
                                          this, visitor));
    return;
  }

  if (!visitor) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ListProfilesOnBlockingThread, GetVaultPathInternal(),
                     GetEncryptionKeyPathInternal()),
      base::BindOnce(&CefAuthVaultImpl::OnVisitComplete, this, visitor));
}

CefString CefAuthVaultImpl::GetVaultPath() {
  return GetVaultPathInternal().AsUTF8Unsafe();
}

CefString CefAuthVaultImpl::GetEncryptionKeyPath() {
  return GetEncryptionKeyPathInternal().AsUTF8Unsafe();
}

void CefAuthVaultImpl::OnActionComplete(
    CefRefPtr<CefAuthVaultActionCallback> callback,
    const std::string& profile_name,
    AuthVaultActionResult result) {
  CEF_REQUIRE_UIT();
  if (result.success && !profile_name.empty()) {
    ClearProfileDirty(profile_name);
  }
  RunActionCallback(callback, result.success, result.error, result.path);
}

void CefAuthVaultImpl::OnReadComplete(
    CefRefPtr<CefAuthVaultReadCallback> callback,
    AuthVaultReadResult result) {
  CEF_REQUIRE_UIT();
  if (!callback) {
    return;
  }

  CefRefPtr<CefDictionaryValue> profile;
  if (!result.profile.empty()) {
    profile = new CefDictionaryValueImpl(std::move(result.profile), false);
  }
  callback->OnComplete(result.success, profile, result.error);
}

void CefAuthVaultImpl::OnVisitComplete(
    CefRefPtr<CefAuthProfileVisitor> visitor,
    AuthVaultListResult result) {
  CEF_REQUIRE_UIT();
  if (!visitor || !result.success) {
    return;
  }

  const int total = static_cast<int>(result.profiles.size());
  int count = 0;
  for (const auto& profile_value : result.profiles) {
    CefAuthProfile profile;
    if (const auto* name = profile_value.FindString("name")) {
      CefString(&profile.name) = *name;
    }
    if (const auto* url = profile_value.FindString("url")) {
      CefString(&profile.url) = *url;
    }
    if (const auto* username = profile_value.FindString("username")) {
      CefString(&profile.username) = *username;
    }
    if (const auto* username_selector =
            profile_value.FindString("username_selector")) {
      CefString(&profile.username_selector) = *username_selector;
    }
    if (const auto* password_selector =
            profile_value.FindString("password_selector")) {
      CefString(&profile.password_selector) = *password_selector;
    }
    if (const auto* submit_selector =
            profile_value.FindString("submit_selector")) {
      CefString(&profile.submit_selector) = *submit_selector;
    }
    if (const auto* created_at = profile_value.FindString("created_at")) {
      CefString(&profile.created_at) = *created_at;
    }
    if (const auto* last_login_at =
            profile_value.FindString("last_login_at")) {
      CefString(&profile.last_login_at) = *last_login_at;
    }
    profile.encrypted = profile_value.FindBool("encrypted").value_or(true);

    if (!visitor->Visit(profile, count++, total)) {
      break;
    }
  }
}

void CefAuthVaultImpl::RunActionCallback(
    CefRefPtr<CefAuthVaultActionCallback> callback,
    bool success,
    const std::string& error,
    const base::FilePath& path) {
  if (!callback) {
    return;
  }

  callback->OnComplete(success, path.AsUTF8Unsafe(), error);
}

base::FilePath CefAuthVaultImpl::GetVaultPathInternal() const {
  if (cached_vault_path_.empty()) {
    cached_vault_path_ = GetDefaultAuthDirectory();
  }
  return cached_vault_path_;
}

base::FilePath CefAuthVaultImpl::GetEncryptionKeyPathInternal() const {
  if (cached_encryption_key_path_.empty()) {
    cached_encryption_key_path_ = GetDefaultEncryptionKeyPath();
  }
  return cached_encryption_key_path_;
}
