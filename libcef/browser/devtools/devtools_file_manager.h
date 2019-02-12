// Copyright 2019 The Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_FILE_MANAGER_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_FILE_MANAGER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

#include <string>

namespace base {
class FilePath;
class SequencedTaskRunner;
class Value;
}  // namespace base

class CefBrowserHostImpl;
class PrefService;

// File management helper for DevTools.
// Based on chrome/browser/devtools/devtools_ui_bindings.cc and
// chrome/browser/devtools/devtools_file_helper.cc.
class CefDevToolsFileManager {
 public:
  CefDevToolsFileManager(CefBrowserHostImpl* browser_impl, PrefService* prefs);

  void SaveToFile(const std::string& url,
                  const std::string& content,
                  bool save_as);
  void AppendToFile(const std::string& url, const std::string& content);

 private:
  // SaveToFile implementation:
  typedef base::Callback<void(const std::string&)> SaveCallback;
  typedef base::Callback<void()> CancelCallback;
  void Save(const std::string& url,
            const std::string& content,
            bool save_as,
            const SaveCallback& saveCallback,
            const CancelCallback& cancelCallback);
  void SaveAsDialogDismissed(const std::string& url,
                             const std::string& content,
                             const SaveCallback& saveCallback,
                             const CancelCallback& cancelCallback,
                             int selected_accept_filter,
                             const std::vector<base::FilePath>& file_paths);
  void SaveAsFileSelected(const std::string& url,
                          const std::string& content,
                          const SaveCallback& callback,
                          const base::FilePath& path);
  void FileSavedAs(const std::string& url, const std::string& file_system_path);
  void CanceledFileSaveAs(const std::string& url);

  // AppendToFile implementation:
  typedef base::Callback<void(void)> AppendCallback;
  void Append(const std::string& url,
              const std::string& content,
              const AppendCallback& callback);
  void AppendedTo(const std::string& url);

  void CallClientFunction(const std::string& function_name,
                          const base::Value* arg1,
                          const base::Value* arg2,
                          const base::Value* arg3);

  // Guaranteed to outlive this object.
  CefBrowserHostImpl* browser_impl_;
  PrefService* prefs_;

  typedef std::map<std::string, base::FilePath> PathsMap;
  PathsMap saved_files_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  base::WeakPtrFactory<CefDevToolsFileManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefDevToolsFileManager);
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_FILE_MANAGER_H_
