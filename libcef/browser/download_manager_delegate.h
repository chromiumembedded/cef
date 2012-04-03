// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

  void SetDownloadManager(content::DownloadManager* manager);

  // DownloadManagerDelegate methods.
  virtual content::DownloadId GetNextId() OVERRIDE;
  virtual bool ShouldStartDownload(int32 download_id) OVERRIDE;
  virtual void ChooseDownloadPath(content::WebContents* web_contents,
                                  const FilePath& suggested_path,
                                  int32 download_id) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<CefDownloadManagerDelegate>;

  virtual ~CefDownloadManagerDelegate();

  void GenerateFilename(int32 download_id,
                        DownloadStateInfo state,
                        const FilePath& generated_name);
  void RestartDownload(int32 download_id,
                       DownloadStateInfo state);

  content::DownloadManager* download_manager_;

  DISALLOW_COPY_AND_ASSIGN(CefDownloadManagerDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
