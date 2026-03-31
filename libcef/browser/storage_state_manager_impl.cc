// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/storage_state_manager_impl.h"

#include <optional>
#include <utility>

#include <algorithm>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/values_impl.h"

namespace {

constexpr char kAgentBrowserDirectory[] = ".agent-browser";
constexpr char kSessionsDirectory[] = "sessions";
constexpr char kDefaultSessionName[] = "default";
constexpr char kJsonExtension[] = ".json";
constexpr char kJsonEncryptedExtension[] = ".json.enc";

base::FilePath GetHomeDirectory() {
  base::FilePath home_path;
  if (base::PathService::Get(base::DIR_HOME, &home_path)) {
    return home_path;
  }
  return base::FilePath();
}

base::FilePath GetDefaultSessionsDirectory() {
  base::FilePath home_path = GetHomeDirectory();
  if (home_path.empty()) {
    return base::FilePath();
  }
  return home_path.AppendASCII(kAgentBrowserDirectory)
      .AppendASCII(kSessionsDirectory);
}

bool IsStateFile(const base::FilePath& path) {
  const auto filename = path.BaseName().AsUTF8Unsafe();
  return base::EndsWith(filename, kJsonExtension) ||
         base::EndsWith(filename, kJsonEncryptedExtension);
}

bool IsValidSessionName(const std::string& session_name) {
  if (session_name.empty()) {
    return true;
  }

  for (const char c : session_name) {
    if (!(base::IsAsciiAlphaNumeric(c) || c == '_' || c == '-')) {
      return false;
    }
  }

  return true;
}

base::Value::Dict MakeEntryDict(const base::FilePath& path,
                                const base::File::Info& info) {
  base::Value::Dict dict;
  dict.Set("filename", path.BaseName().AsUTF8Unsafe());
  dict.Set("path", path.AsUTF8Unsafe());
  dict.Set("size", static_cast<double>(info.size));
  dict.Set("modified",
           info.last_modified.InSecondsFSinceUnixEpoch());
  dict.Set("encrypted",
           base::EndsWith(path.BaseName().AsUTF8Unsafe(),
                          kJsonEncryptedExtension));
  return dict;
}

base::FilePath BuildStatePath(const base::FilePath& directory,
                              const std::string& session_name) {
  const std::string& safe_name =
      session_name.empty() ? std::string(kDefaultSessionName) : session_name;
  return directory.AppendASCII(safe_name + kJsonExtension);
}

struct StorageStateListResult {
  bool success = false;
  std::string error;
  base::FilePath directory;
  std::vector<base::Value::Dict> entries;
};

struct StorageStateReadResult {
  bool success = false;
  std::string error;
  base::Value::Dict result;
};

struct StorageStateActionResult {
  bool success = false;
  std::string error;
  base::FilePath path;
};

StorageStateListResult ListStatesOnBlockingThread(base::FilePath directory) {
  StorageStateListResult result;
  result.directory = directory;

  if (directory.empty()) {
    result.error = "Default state directory is unavailable.";
    return result;
  }

  if (!base::PathExists(directory)) {
    result.success = true;
    return result;
  }

  base::FileEnumerator enumerator(directory, false, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (!IsStateFile(path)) {
      continue;
    }

    const auto& info = enumerator.GetInfo();
    result.entries.push_back(MakeEntryDict(path, info));
  }

  result.success = true;
  return result;
}

StorageStateReadResult ShowStateOnBlockingThread(base::FilePath path) {
  StorageStateReadResult result;

  if (path.empty()) {
    result.error = "State path is empty.";
    return result;
  }

  base::File::Info info;
  if (!base::GetFileInfo(path, &info)) {
    result.error = "State file does not exist.";
    return result;
  }

  result.result = MakeEntryDict(path, info);
  result.result.Set("summary", "Storage state scaffolding is not yet implemented.");
  result.success = true;
  return result;
}

StorageStateActionResult RenameStateOnBlockingThread(base::FilePath old_path,
                                                     std::string new_name) {
  StorageStateActionResult result;

  if (old_path.empty()) {
    result.error = "State path is empty.";
    return result;
  }

  if (new_name.empty()) {
    result.error = "New state name is empty.";
    return result;
  }

  if (!IsValidSessionName(new_name)) {
    result.error = "State name must match /^[A-Za-z0-9_-]+$/.";
    return result;
  }

  if (!base::PathExists(old_path)) {
    result.error = "State file does not exist.";
    return result;
  }

  const bool encrypted =
      base::EndsWith(old_path.BaseName().AsUTF8Unsafe(), kJsonEncryptedExtension);
  const std::string extension =
      encrypted ? std::string(kJsonEncryptedExtension) : std::string(kJsonExtension);
  base::FilePath new_path =
      old_path.DirName().AppendASCII(new_name + extension);

  if (!base::Move(old_path, new_path)) {
    result.error = "Failed to rename state file.";
    return result;
  }

  result.success = true;
  result.path = new_path;
  return result;
}

StorageStateActionResult ClearStatesOnBlockingThread(base::FilePath directory,
                                                     std::optional<base::FilePath> path) {
  StorageStateActionResult result;

  if (path.has_value()) {
    if (!base::DeleteFile(*path)) {
      result.error = "Failed to delete state file.";
      return result;
    }
    result.success = true;
    result.path = *path;
    return result;
  }

  if (directory.empty()) {
    result.error = "Default state directory is unavailable.";
    return result;
  }

  if (!base::PathExists(directory)) {
    result.success = true;
    result.path = directory;
    return result;
  }

  base::FileEnumerator enumerator(directory, false, base::FileEnumerator::FILES);
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    if (!IsStateFile(entry)) {
      continue;
    }
    base::DeleteFile(entry);
  }

  result.success = true;
  result.path = directory;
  return result;
}

StorageStateReadResult CleanStatesOnBlockingThread(base::FilePath directory,
                                                   int days) {
  StorageStateReadResult result;
  result.result.Set("days", days);

  if (directory.empty()) {
    result.error = "Default state directory is unavailable.";
    return result;
  }

  if (!base::PathExists(directory)) {
    result.result.Set("cleaned", 0);
    result.result.Set("kept", 0);
    result.success = true;
    return result;
  }

  const base::Time now = base::Time::Now();
  const base::TimeDelta max_age = base::Days(std::max(days, 0));
  int cleaned = 0;
  int kept = 0;

  base::FileEnumerator enumerator(directory, false, base::FileEnumerator::FILES);
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    if (!IsStateFile(entry)) {
      continue;
    }

    const auto& info = enumerator.GetInfo();
    if (now - info.last_modified > max_age) {
      if (base::DeleteFile(entry)) {
        ++cleaned;
        continue;
      }
    }
    ++kept;
  }

  result.result.Set("cleaned", cleaned);
  result.result.Set("kept", kept);
  result.success = true;
  return result;
}

}  // namespace

CefStorageStateManagerImpl::CefStorageStateManagerImpl() = default;

void CefStorageStateManagerImpl::Initialize(
    const CefBrowserContext::Getter& browser_context_getter,
    CefRefPtr<CefCompletionCallback> callback) {
  CEF_REQUIRE_UIT();
  DCHECK(!browser_context_getter.is_null());

  if (initialized_) {
    if (callback) {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefCompletionCallback::OnComplete, callback));
    }
    return;
  }

  browser_context_getter_ = browser_context_getter;
  initialized_ = true;

  if (!init_callbacks_.empty()) {
    for (auto& init_callback : init_callbacks_) {
      std::move(init_callback).Run();
    }
    init_callbacks_.clear();
  }

  if (callback) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefCompletionCallback::OnComplete, callback));
  }
}

void CefStorageStateManagerImpl::Save(
    const CefString& path,
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefStorageStateActionCallback> callback) {
  StoreOrTriggerInitCallback(base::BindOnce(
      &CefStorageStateManagerImpl::SaveInternal, this, path, browser, callback));
}

void CefStorageStateManagerImpl::Load(
    const CefString& path,
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefStorageStateActionCallback> callback) {
  StoreOrTriggerInitCallback(base::BindOnce(
      &CefStorageStateManagerImpl::LoadInternal, this, path, browser, callback));
}

void CefStorageStateManagerImpl::List(
    CefRefPtr<CefStorageStateListCallback> callback) {
  StoreOrTriggerInitCallback(
      base::BindOnce(&CefStorageStateManagerImpl::ListInternal, this, callback));
}

void CefStorageStateManagerImpl::Show(
    const CefString& path,
    CefRefPtr<CefStorageStateReadCallback> callback) {
  StoreOrTriggerInitCallback(
      base::BindOnce(&CefStorageStateManagerImpl::ShowInternal, this, path,
                     callback));
}

void CefStorageStateManagerImpl::Rename(
    const CefString& path,
    const CefString& new_name,
    CefRefPtr<CefStorageStateActionCallback> callback) {
  StoreOrTriggerInitCallback(base::BindOnce(
      &CefStorageStateManagerImpl::RenameInternal, this, path, new_name,
      callback));
}

void CefStorageStateManagerImpl::Clear(
    const CefString& path,
    CefRefPtr<CefStorageStateActionCallback> callback) {
  StoreOrTriggerInitCallback(
      base::BindOnce(&CefStorageStateManagerImpl::ClearInternal, this, path,
                     callback));
}

void CefStorageStateManagerImpl::CleanOlderThan(
    int days,
    CefRefPtr<CefStorageStateReadCallback> callback) {
  StoreOrTriggerInitCallback(base::BindOnce(
      &CefStorageStateManagerImpl::CleanOlderThanInternal, this, days,
      callback));
}

void CefStorageStateManagerImpl::SetSessionName(const CefString& session_name) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefStorageStateManagerImpl::SetSessionName,
                       CefRefPtr<CefStorageStateManagerImpl>(this),
                       session_name));
    return;
  }

  const std::string value = session_name.ToString();
  if (!IsValidSessionName(value)) {
    DCHECK(false) << "invalid session name";
    return;
  }

  session_name_ = value;
}

CefString CefStorageStateManagerImpl::GetSessionName() {
  CEF_REQUIRE_UIT_RETURN(CefString());
  return session_name_;
}

CefString CefStorageStateManagerImpl::GetDefaultStateDirectory() {
  CEF_REQUIRE_UIT_RETURN(CefString());
  return GetDefaultStateDirectoryPath().AsUTF8Unsafe();
}

CefString CefStorageStateManagerImpl::GetDefaultStatePath() {
  CEF_REQUIRE_UIT_RETURN(CefString());
  const auto path = GetDefaultStateFilePath();
  return path.empty() ? CefString() : CefString(path.AsUTF8Unsafe());
}

void CefStorageStateManagerImpl::SaveInternal(
    const CefString& path,
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefStorageStateActionCallback> callback) {
  CEF_REQUIRE_UIT();
  base::FilePath output_path =
      path.empty() ? GetDefaultStateFilePath() : ResolveManagedStatePath(path);
  if (output_path.empty()) {
    const std::string error = path.empty() ? "Default state path is unavailable."
                                           : "State path must reference a managed session file.";
    RunActionCallback(callback, false, error, base::FilePath());
    return;
  }

  RunActionCallback(
      callback, false,
      "Storage state save is not implemented in this scaffold.",
      output_path);
}

void CefStorageStateManagerImpl::LoadInternal(
    const CefString& path,
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefStorageStateActionCallback> callback) {
  CEF_REQUIRE_UIT();
  if (path.empty()) {
    RunActionCallback(callback, false, "State path is empty.", base::FilePath());
    return;
  }

  const auto input_path = ResolveManagedStatePath(path);
  if (input_path.empty()) {
    RunActionCallback(callback, false,
                      "State path must reference a managed session file.",
                      base::FilePath());
    return;
  }

  RunActionCallback(
      callback, false,
      "Storage state load is not implemented in this scaffold.",
      input_path);
}

void CefStorageStateManagerImpl::ListInternal(
    CefRefPtr<CefStorageStateListCallback> callback) {
  CEF_REQUIRE_UIT();
  const auto directory = GetDefaultStateDirectoryPath();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ListStatesOnBlockingThread, directory),
      base::BindOnce(&CefStorageStateManagerImpl::OnListComplete,
                     CefRefPtr<CefStorageStateManagerImpl>(this), callback));
}

void CefStorageStateManagerImpl::ShowInternal(
    const CefString& path,
    CefRefPtr<CefStorageStateReadCallback> callback) {
  CEF_REQUIRE_UIT();
  const auto state_path = ResolveManagedStatePath(path);
  if (state_path.empty()) {
    StorageStateReadResult result;
    result.error = "State path must reference a managed session file.";
    OnReadComplete(callback, std::move(result));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ShowStateOnBlockingThread, state_path),
      base::BindOnce(&CefStorageStateManagerImpl::OnReadComplete,
                     CefRefPtr<CefStorageStateManagerImpl>(this), callback));
}

void CefStorageStateManagerImpl::RenameInternal(
    const CefString& path,
    const CefString& new_name,
    CefRefPtr<CefStorageStateActionCallback> callback) {
  CEF_REQUIRE_UIT();
  const auto old_path = ResolveManagedStatePath(path);
  if (old_path.empty()) {
    RunActionCallback(callback, false,
                      "State path must reference a managed session file.",
                      base::FilePath());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&RenameStateOnBlockingThread, old_path, new_name.ToString()),
      base::BindOnce(&CefStorageStateManagerImpl::OnActionComplete,
                     CefRefPtr<CefStorageStateManagerImpl>(this), callback));
}

void CefStorageStateManagerImpl::ClearInternal(
    const CefString& path,
    CefRefPtr<CefStorageStateActionCallback> callback) {
  CEF_REQUIRE_UIT();
  const auto directory = GetDefaultStateDirectoryPath();
  std::optional<base::FilePath> file_path;
  if (!path.empty()) {
    const auto resolved_path = ResolveManagedStatePath(path);
    if (resolved_path.empty()) {
      RunActionCallback(callback, false,
                        "State path must reference a managed session file.",
                        base::FilePath());
      return;
    }
    file_path = resolved_path;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ClearStatesOnBlockingThread, directory, file_path),
      base::BindOnce(&CefStorageStateManagerImpl::OnActionComplete,
                     CefRefPtr<CefStorageStateManagerImpl>(this), callback));
}

void CefStorageStateManagerImpl::CleanOlderThanInternal(
    int days,
    CefRefPtr<CefStorageStateReadCallback> callback) {
  CEF_REQUIRE_UIT();
  const auto directory = GetDefaultStateDirectoryPath();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CleanStatesOnBlockingThread, directory, days),
      base::BindOnce(&CefStorageStateManagerImpl::OnReadComplete,
                     CefRefPtr<CefStorageStateManagerImpl>(this), callback));
}

void CefStorageStateManagerImpl::OnListComplete(
    CefRefPtr<CefStorageStateListCallback> callback,
    StorageStateListResult result) {
  CEF_REQUIRE_UIT();
  if (!callback) {
    return;
  }

  CefRefPtr<CefListValue> entries = CefListValue::Create();
  size_t index = 0;
  for (auto& entry : result.entries) {
    entries->SetDictionary(
        index++, new CefDictionaryValueImpl(std::move(entry), false));
  }

  callback->OnComplete(result.success, result.error, entries,
                       result.directory.AsUTF8Unsafe());
}

void CefStorageStateManagerImpl::OnReadComplete(
    CefRefPtr<CefStorageStateReadCallback> callback,
    StorageStateReadResult result) {
  CEF_REQUIRE_UIT();
  if (!callback) {
    return;
  }

  CefRefPtr<CefDictionaryValue> value;
  if (!result.result.empty()) {
    value = new CefDictionaryValueImpl(std::move(result.result), false);
  }
  callback->OnComplete(result.success, result.error, value);
}

void CefStorageStateManagerImpl::OnActionComplete(
    CefRefPtr<CefStorageStateActionCallback> callback,
    StorageStateActionResult result) {
  CEF_REQUIRE_UIT();
  RunActionCallback(callback, result.success, result.error, result.path);
}

void CefStorageStateManagerImpl::StoreOrTriggerInitCallback(
    base::OnceClosure callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefStorageStateManagerImpl::StoreOrTriggerInitCallback,
                       CefRefPtr<CefStorageStateManagerImpl>(this),
                       std::move(callback)));
    return;
  }

  if (initialized_) {
    std::move(callback).Run();
  } else {
    init_callbacks_.push_back(std::move(callback));
  }
}

base::FilePath CefStorageStateManagerImpl::GetDefaultStateDirectoryPath() const {
  return GetDefaultSessionsDirectory();
}

base::FilePath CefStorageStateManagerImpl::GetDefaultStateFilePath() const {
  const auto directory = GetDefaultStateDirectoryPath();
  if (directory.empty()) {
    return base::FilePath();
  }
  return BuildStatePath(directory, session_name_);
}

void CefStorageStateManagerImpl::RunActionCallback(
    CefRefPtr<CefStorageStateActionCallback> callback,
    bool success,
    const std::string& error,
    const base::FilePath& path) {
  if (!callback) {
    return;
  }

  callback->OnComplete(success, error, path.AsUTF8Unsafe());
}

base::FilePath CefStorageStateManagerImpl::ResolveManagedStatePath(
    const CefString& path) const {
  const auto directory = GetDefaultStateDirectoryPath();
  if (directory.empty()) {
    return base::FilePath();
  }

  const base::FilePath input_path(path.ToString());
  if (input_path.empty()) {
    return base::FilePath();
  }

  if (input_path.IsAbsolute()) {
    if (input_path.DirName() != directory || !IsStateFile(input_path)) {
      return base::FilePath();
    }
    return input_path;
  }

  if (input_path.BaseName() != input_path || !IsStateFile(input_path)) {
    return base::FilePath();
  }

  return directory.Append(input_path);
}
