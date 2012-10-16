// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/download_manager_delegate.h"

struct DownloadStateInfo;

namespace content {
class DownloadManager;
}

class CefDownloadManagerDelegate
    : public content::DownloadManagerDelegate,
      public base::RefCountedThreadSafe<CefDownloadManagerDelegate> {
 public:
  CefDownloadManagerDelegate();

  // DownloadManagerDelegate methods.
  virtual void Shutdown() OVERRIDE;
  virtual bool DetermineDownloadTarget(
      content::DownloadItem* item,
      const content::DownloadTargetCallback& callback) OVERRIDE;
  virtual void AddItemToPersistentStore(content::DownloadItem* item) OVERRIDE;
  virtual void UpdateItemInPersistentStore(
      content::DownloadItem* item) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<CefDownloadManagerDelegate>;

  virtual ~CefDownloadManagerDelegate();

  DISALLOW_COPY_AND_ASSIGN(CefDownloadManagerDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
