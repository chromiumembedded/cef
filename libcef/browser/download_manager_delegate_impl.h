// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_IMPL_H_
#define CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_IMPL_H_
#pragma once

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/download_manager_delegate.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_target_info.h"
#include "content/public/browser/download_manager.h"

class CefBrowserHostBase;

class CefDownloadManagerDelegateImpl
    : public download::DownloadItem::Observer,
      public content::DownloadManager::Observer,
      public cef::DownloadManagerDelegate,
      public CefBrowserHostBase::Observer {
 public:
  CefDownloadManagerDelegateImpl(content::DownloadManager* manager,
                                 bool alloy_bootstrap);

  CefDownloadManagerDelegateImpl(const CefDownloadManagerDelegateImpl&) =
      delete;
  CefDownloadManagerDelegateImpl& operator=(
      const CefDownloadManagerDelegateImpl&) = delete;

  ~CefDownloadManagerDelegateImpl() override;

 private:
  // DownloadItem::Observer methods.
  void OnDownloadUpdated(download::DownloadItem* item) override;
  void OnDownloadDestroyed(download::DownloadItem* item) override;

  // DownloadManager::Observer methods.
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void ManagerGoingDown(content::DownloadManager* manager) override;

  // DownloadManagerDelegate methods.
  bool DetermineDownloadTarget(
      download::DownloadItem* item,
      download::DownloadTargetCallback* callback) override;

  // CefBrowserHostBase::Observer methods.
  void OnBrowserDestroyed(CefBrowserHostBase* browser) override;

  CefRefPtr<CefBrowserHostBase> GetOrAssociateBrowser(
      download::DownloadItem* item);
  CefRefPtr<CefBrowserHostBase> GetBrowser(download::DownloadItem* item);

  void ResetManager();

  raw_ptr<content::DownloadManager> manager_;
  base::WeakPtrFactory<content::DownloadManager> manager_ptr_factory_;
  const bool alloy_bootstrap_;

  // Map of DownloadItem to originating CefBrowserHostBase. Maintaining this
  // map is necessary because DownloadItem::GetWebContents() may return NULL if
  // the browser navigates while the download is in progress.
  using ItemBrowserMap =
      std::map<raw_ptr<download::DownloadItem>, raw_ptr<CefBrowserHostBase>>;
  ItemBrowserMap item_browser_map_;
};

#endif  // CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_IMPL_H_
