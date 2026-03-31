// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_STORAGE_STATE_MANAGER_IMPL_H_
#define CEF_LIBCEF_BROWSER_STORAGE_STATE_MANAGER_IMPL_H_
#pragma once

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "cef/include/cef_storage_state.h"
#include <string>
#include <vector>

#include "cef/libcef/browser/browser_context.h"
#include "cef/libcef/browser/thread_util.h"

struct StorageStateListResult;
struct StorageStateReadResult;
struct StorageStateActionResult;

// Implementation of the CefStorageStateManager interface. May be created on any
// thread. Methods execute on the browser process UI thread unless otherwise
// indicated by the underlying filesystem task implementation.
class CefStorageStateManagerImpl : public CefStorageStateManager {
 public:
  CefStorageStateManagerImpl();

  CefStorageStateManagerImpl(const CefStorageStateManagerImpl&) = delete;
  CefStorageStateManagerImpl& operator=(const CefStorageStateManagerImpl&) =
      delete;

  // CefStorageStateManager methods.
  void Save(const CefString& path,
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefStorageStateActionCallback> callback) override;
  void Load(const CefString& path,
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefStorageStateActionCallback> callback) override;
  void List(CefRefPtr<CefStorageStateListCallback> callback) override;
  void Show(const CefString& path,
            CefRefPtr<CefStorageStateReadCallback> callback) override;
  void Rename(const CefString& path,
              const CefString& new_name,
              CefRefPtr<CefStorageStateActionCallback> callback) override;
  void Clear(const CefString& path,
             CefRefPtr<CefStorageStateActionCallback> callback) override;
  void CleanOlderThan(
      int days,
      CefRefPtr<CefStorageStateReadCallback> callback) override;
  void SetSessionName(const CefString& session_name) override;
  CefString GetSessionName() override;
  CefString GetDefaultStateDirectory() override;
  CefString GetDefaultStatePath() override;

  // Called on the UI thread after object creation and before any other object
  // methods are executed on the UI thread.
  void Initialize(const CefBrowserContext::Getter& browser_context_getter,
                  CefRefPtr<CefCompletionCallback> callback);
  bool is_initialized() const { return initialized_; }

 private:
  ~CefStorageStateManagerImpl() override = default;

  void SaveInternal(const CefString& path,
                    CefRefPtr<CefBrowser> browser,
                    CefRefPtr<CefStorageStateActionCallback> callback);
  void LoadInternal(const CefString& path,
                    CefRefPtr<CefBrowser> browser,
                    CefRefPtr<CefStorageStateActionCallback> callback);
  void ListInternal(CefRefPtr<CefStorageStateListCallback> callback);
  void ShowInternal(const CefString& path,
                    CefRefPtr<CefStorageStateReadCallback> callback);
  void RenameInternal(const CefString& path,
                      const CefString& new_name,
                      CefRefPtr<CefStorageStateActionCallback> callback);
  void ClearInternal(const CefString& path,
                     CefRefPtr<CefStorageStateActionCallback> callback);
  void CleanOlderThanInternal(
      int days,
      CefRefPtr<CefStorageStateReadCallback> callback);

  void OnListComplete(CefRefPtr<CefStorageStateListCallback> callback,
                      StorageStateListResult result);
  void OnReadComplete(CefRefPtr<CefStorageStateReadCallback> callback,
                      StorageStateReadResult result);
  void OnActionComplete(CefRefPtr<CefStorageStateActionCallback> callback,
                        StorageStateActionResult result);

  void StoreOrTriggerInitCallback(base::OnceClosure callback);

  base::FilePath GetDefaultStateDirectoryPath() const;
  base::FilePath GetDefaultStateFilePath() const;

  void RunActionCallback(CefRefPtr<CefStorageStateActionCallback> callback,
                         bool success,
                         const std::string& error,
                         const base::FilePath& path);
  base::FilePath ResolveManagedStatePath(const CefString& path) const;

  CefBrowserContext::Getter browser_context_getter_;
  bool initialized_ = false;
  std::vector<base::OnceClosure> init_callbacks_;
  std::string session_name_;

  IMPLEMENT_REFCOUNTING(CefStorageStateManagerImpl);
};

#endif  // CEF_LIBCEF_BROWSER_STORAGE_STATE_MANAGER_IMPL_H_
