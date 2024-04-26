// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_ALLOY_DOWNLOAD_MANAGER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_ALLOY_ALLOY_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include "libcef/browser/download_manager_delegate_impl.h"

// Specialization for the Alloy bootstrap.
class AlloyDownloadManagerDelegate : public CefDownloadManagerDelegateImpl {
 public:
  explicit AlloyDownloadManagerDelegate(content::DownloadManager* manager);

  AlloyDownloadManagerDelegate(const AlloyDownloadManagerDelegate&) = delete;
  AlloyDownloadManagerDelegate& operator=(const AlloyDownloadManagerDelegate&) =
      delete;

 private:
  // DownloadManagerDelegate methods.
  void GetNextId(content::DownloadIdCallback callback) override;
  std::string ApplicationClientIdForFileScanning() override;
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_ALLOY_DOWNLOAD_MANAGER_DELEGATE_H_
