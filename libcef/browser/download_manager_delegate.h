// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include <set>

#include "libcef/browser/browser_host_impl.h"

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"

class CefDownloadManagerDelegate
    : public content::DownloadItem::Observer,
      public content::DownloadManager::Observer,
      public content::DownloadManagerDelegate,
      public CefBrowserHostImpl::Observer {
 public:
  explicit CefDownloadManagerDelegate(content::DownloadManager* manager);
  ~CefDownloadManagerDelegate() override;

 private:
  // DownloadItem::Observer methods.
  void OnDownloadUpdated(content::DownloadItem* item) override;
  void OnDownloadDestroyed(content::DownloadItem* item) override;

  // DownloadManager::Observer methods.
  void OnDownloadCreated(content::DownloadManager* manager,
                         content::DownloadItem* item) override;
  void ManagerGoingDown(content::DownloadManager* manager) override;

  // DownloadManagerDelegate methods.
  bool DetermineDownloadTarget(
      content::DownloadItem* item,
      const content::DownloadTargetCallback& callback) override;
  void GetNextId(const content::DownloadIdCallback& callback) override;

  // CefBrowserHostImpl::Observer methods.
  void OnBrowserDestroyed(CefBrowserHostImpl* browser) override;

  CefBrowserHostImpl* GetBrowser(content::DownloadItem* item);

  content::DownloadManager* manager_;
  base::WeakPtrFactory<content::DownloadManager> manager_ptr_factory_;

  // Map of DownloadItem to originating CefBrowserHostImpl. Maintaining this
  // map is necessary because DownloadItem::GetWebContents() may return NULL if
  // the browser navigates while the download is in progress.
  typedef std::map<content::DownloadItem*, CefBrowserHostImpl* > ItemBrowserMap;
  ItemBrowserMap item_browser_map_;

  DISALLOW_COPY_AND_ASSIGN(CefDownloadManagerDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
