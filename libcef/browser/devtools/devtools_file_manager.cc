// Copyright 2019 The Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/devtools/devtools_file_manager.h"

#include "libcef/browser/browser_host_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"

namespace {

base::LazyInstance<base::FilePath>::Leaky g_last_save_path =
    LAZY_INSTANCE_INITIALIZER;

void WriteToFile(const base::FilePath& path, const std::string& content) {
  DCHECK(!path.empty());
  base::WriteFile(path, content.c_str(), content.length());
}

void AppendToFile(const base::FilePath& path, const std::string& content) {
  DCHECK(!path.empty());
  base::AppendToFile(path, content.c_str(), content.size());
}

}  // namespace

CefDevToolsFileManager::CefDevToolsFileManager(CefBrowserHostImpl* browser_impl,
                                               PrefService* prefs)
    : browser_impl_(browser_impl),
      prefs_(prefs),
      file_task_runner_(
          base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()})),
      weak_factory_(this) {}

void CefDevToolsFileManager::SaveToFile(const std::string& url,
                                        const std::string& content,
                                        bool save_as) {
  Save(url, content, save_as,
       base::Bind(&CefDevToolsFileManager::FileSavedAs,
                  weak_factory_.GetWeakPtr(), url),
       base::Bind(&CefDevToolsFileManager::CanceledFileSaveAs,
                  weak_factory_.GetWeakPtr(), url));
}

void CefDevToolsFileManager::AppendToFile(const std::string& url,
                                          const std::string& content) {
  Append(url, content,
         base::Bind(&CefDevToolsFileManager::AppendedTo,
                    weak_factory_.GetWeakPtr(), url));
}

void CefDevToolsFileManager::Save(const std::string& url,
                                  const std::string& content,
                                  bool save_as,
                                  const SaveCallback& saveCallback,
                                  const CancelCallback& cancelCallback) {
  auto it = saved_files_.find(url);
  if (it != saved_files_.end() && !save_as) {
    SaveAsFileSelected(url, content, saveCallback, it->second);
    return;
  }

  const base::DictionaryValue* file_map =
      prefs_->GetDictionary(prefs::kDevToolsEditedFiles);
  base::FilePath initial_path;

  const base::Value* path_value;
  if (file_map->Get(base::MD5String(url), &path_value)) {
    // Ignore base::GetValueAsFilePath() failure since we handle empty
    // |initial_path| below.
    ignore_result(base::GetValueAsFilePath(*path_value, &initial_path));
  }

  if (initial_path.empty()) {
    GURL gurl(url);
    std::string suggested_file_name =
        gurl.is_valid() ? gurl.ExtractFileName() : url;

    if (suggested_file_name.length() > 64)
      suggested_file_name = suggested_file_name.substr(0, 64);

    if (!g_last_save_path.Pointer()->empty()) {
      initial_path = g_last_save_path.Pointer()->DirName().AppendASCII(
          suggested_file_name);
    } else {
      // Use the temp directory. It may be an empty value.
      base::PathService::Get(base::DIR_TEMP, &initial_path);
      initial_path = initial_path.AppendASCII(suggested_file_name);
    }
  }

  CefFileDialogRunner::FileChooserParams params;
  params.mode = blink::mojom::FileChooserParams::Mode::kSave;
  if (!initial_path.empty()) {
    params.default_file_name = initial_path;
    if (!initial_path.Extension().empty()) {
      params.accept_types.push_back(CefString(initial_path.Extension()));
    }
  }

  browser_impl_->RunFileChooser(
      params, base::Bind(&CefDevToolsFileManager::SaveAsDialogDismissed,
                         weak_factory_.GetWeakPtr(), url, content, saveCallback,
                         cancelCallback));
}

void CefDevToolsFileManager::SaveAsDialogDismissed(
    const std::string& url,
    const std::string& content,
    const SaveCallback& saveCallback,
    const CancelCallback& cancelCallback,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  if (file_paths.size() == 1) {
    SaveAsFileSelected(url, content, saveCallback, file_paths[0]);
  } else {
    cancelCallback.Run();
  }
}

void CefDevToolsFileManager::SaveAsFileSelected(const std::string& url,
                                                const std::string& content,
                                                const SaveCallback& callback,
                                                const base::FilePath& path) {
  *g_last_save_path.Pointer() = path;
  saved_files_[url] = path;

  DictionaryPrefUpdate update(prefs_, prefs::kDevToolsEditedFiles);
  base::DictionaryValue* files_map = update.Get();
  files_map->SetKey(base::MD5String(url), base::CreateFilePathValue(path));
  std::string file_system_path = path.AsUTF8Unsafe();
  callback.Run(file_system_path);
  file_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&::WriteToFile, path, content));
}

void CefDevToolsFileManager::FileSavedAs(const std::string& url,
                                         const std::string& file_system_path) {
  base::Value url_value(url);
  base::Value file_system_path_value(file_system_path);
  CallClientFunction("DevToolsAPI.savedURL", &url_value,
                     &file_system_path_value, NULL);
}

void CefDevToolsFileManager::CanceledFileSaveAs(const std::string& url) {
  base::Value url_value(url);
  CallClientFunction("DevToolsAPI.canceledSaveURL", &url_value, NULL, NULL);
}

void CefDevToolsFileManager::Append(const std::string& url,
                                    const std::string& content,
                                    const AppendCallback& callback) {
  auto it = saved_files_.find(url);
  if (it == saved_files_.end())
    return;
  callback.Run();
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&::AppendToFile, it->second, content));
}

void CefDevToolsFileManager::AppendedTo(const std::string& url) {
  base::Value url_value(url);
  CallClientFunction("DevToolsAPI.appendedToURL", &url_value, NULL, NULL);
}

void CefDevToolsFileManager::CallClientFunction(
    const std::string& function_name,
    const base::Value* arg1,
    const base::Value* arg2,
    const base::Value* arg3) {
  std::string javascript = function_name + "(";
  if (arg1) {
    std::string json;
    base::JSONWriter::Write(*arg1, &json);
    javascript.append(json);
    if (arg2) {
      base::JSONWriter::Write(*arg2, &json);
      javascript.append(", ").append(json);
      if (arg3) {
        base::JSONWriter::Write(*arg3, &json);
        javascript.append(", ").append(json);
      }
    }
  }
  javascript.append(");");
  browser_impl_->web_contents()->GetMainFrame()->ExecuteJavaScript(
      base::UTF8ToUTF16(javascript));
}
