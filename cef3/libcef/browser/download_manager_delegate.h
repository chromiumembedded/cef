// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include <set>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"

class CefDownloadManagerDelegate
    : public content::DownloadItem::Observer,
      public content::DownloadManager::Observer,
      public content::DownloadManagerDelegate {
 public:
  explicit CefDownloadManagerDelegate(content::DownloadManager* manager);
  ~CefDownloadManagerDelegate();

 private:
  // DownloadItem::Observer methods.
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadDestroyed(content::DownloadItem* download) OVERRIDE;

  // DownloadManager::Observer methods.
  virtual void OnDownloadCreated(content::DownloadManager* manager,
                                 content::DownloadItem* item) OVERRIDE;
  virtual void ManagerGoingDown(content::DownloadManager* manager) OVERRIDE;

  // DownloadManagerDelegate methods.
  virtual bool DetermineDownloadTarget(
      content::DownloadItem* item,
      const content::DownloadTargetCallback& callback) OVERRIDE;

  content::DownloadManager* manager_;
  base::WeakPtrFactory<content::DownloadManager> manager_ptr_factory_;
  std::set<content::DownloadItem*> observing_;

  DISALLOW_COPY_AND_ASSIGN(CefDownloadManagerDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
