// Copyright 2019 The Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_FILE_MANAGER_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_FILE_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

#include <map>
#include <string>

namespace base {
class FilePath;
class SequencedTaskRunner;
class Value;
}  // namespace base

class AlloyBrowserHostImpl;
class PrefService;

// File management helper for DevTools.
// Based on chrome/browser/devtools/devtools_ui_bindings.cc and
// chrome/browser/devtools/devtools_file_helper.cc.
class CefDevToolsFileManager {
 public:
  CefDevToolsFileManager(AlloyBrowserHostImpl* browser_impl,
                         PrefService* prefs);

  CefDevToolsFileManager(const CefDevToolsFileManager&) = delete;
  CefDevToolsFileManager& operator=(const CefDevToolsFileManager&) = delete;

  void SaveToFile(const std::string& url,
                  const std::string& content,
                  bool save_as);
  void AppendToFile(const std::string& url, const std::string& content);

 private:
  // SaveToFile implementation:
  using SaveCallback = base::OnceCallback<void(const std::string&)>;
  using CancelCallback = base::OnceCallback<void()>;
  void Save(const std::string& url,
            const std::string& content,
            bool save_as,
            SaveCallback saveCallback,
            CancelCallback cancelCallback);
  void SaveAsDialogDismissed(const std::string& url,
                             const std::string& content,
                             SaveCallback saveCallback,
                             CancelCallback cancelCallback,
                             const std::vector<base::FilePath>& file_paths);
  void SaveAsFileSelected(const std::string& url,
                          const std::string& content,
                          SaveCallback callback,
                          const base::FilePath& path);
  void FileSavedAs(const std::string& url, const std::string& file_system_path);
  void CanceledFileSaveAs(const std::string& url);

  // AppendToFile implementation:
  using AppendCallback = base::OnceCallback<void(void)>;
  void Append(const std::string& url,
              const std::string& content,
              AppendCallback callback);
  void AppendedTo(const std::string& url);

  void CallClientFunction(const std::string& function_name,
                          const base::Value* arg1,
                          const base::Value* arg2,
                          const base::Value* arg3);

  // Guaranteed to outlive this object.
  AlloyBrowserHostImpl* browser_impl_;
  PrefService* prefs_;

  using PathsMap = std::map<std::string, base::FilePath>;
  PathsMap saved_files_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  base::WeakPtrFactory<CefDevToolsFileManager> weak_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_FILE_MANAGER_H_
