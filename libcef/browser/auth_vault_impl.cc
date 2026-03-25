// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/auth_vault_impl.h"

#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/values_impl.h"

namespace {

constexpr char kAgentBrowserDirectory[] = ".agent-browser";
constexpr char kAuthDirectory[] = "auth";
constexpr char kEncryptionKeyFile[] = ".encryption-key";
constexpr char kJsonExtension[] = ".json";

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

AuthVaultActionResult SaveProfileOnBlockingThread(base::FilePath directory,
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

  const auto path = BuildProfilePath(directory, *name);
  std::string data;
  if (!base::JSONWriter::WriteWithOptions(profile, base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                          &data)) {
    result.error = "Failed to serialize auth profile.";
    return result;
  }

  if (!base::WriteFile(path, data)) {
    result.error = "Failed to write auth profile.";
    return result;
  }

  result.success = true;
  result.path = path;
  return result;
}

AuthVaultReadResult ReadProfileOnBlockingThread(base::FilePath directory,
                                                std::string name) {
  AuthVaultReadResult result;

  if (!IsValidProfileName(name)) {
    result.error = "Profile name must match /^[A-Za-z0-9_-]+$/.";
    return result;
  }

  const auto path = BuildProfilePath(directory, name);
  std::string data;
  if (!base::ReadFileToString(path, &data)) {
    result.error = "Auth profile does not exist.";
    return result;
  }

  auto parsed = base::JSONReader::ReadDict(data);
  if (!parsed.has_value()) {
    result.error = "Auth profile could not be parsed.";
    return result;
  }

  result.profile = MakeProfileMetadata(path, *parsed);
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

AuthVaultListResult ListProfilesOnBlockingThread(base::FilePath directory) {
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

    std::string data;
    if (!base::ReadFileToString(entry, &data)) {
      continue;
    }

    auto parsed = base::JSONReader::ReadDict(data);
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

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SaveProfileOnBlockingThread, GetVaultPathInternal(),
                     std::move(profile_value->GetDict())),
      base::BindOnce(&CefAuthVaultImpl::OnActionComplete, this, callback));
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
                     name.ToString()),
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
      base::BindOnce(&CefAuthVaultImpl::OnActionComplete, this, callback));
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
      base::BindOnce(&ListProfilesOnBlockingThread, GetVaultPathInternal()),
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
    AuthVaultActionResult result) {
  CEF_REQUIRE_UIT();
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
  return GetDefaultAuthDirectory();
}

base::FilePath CefAuthVaultImpl::GetEncryptionKeyPathInternal() const {
  return GetDefaultEncryptionKeyPath();
}
