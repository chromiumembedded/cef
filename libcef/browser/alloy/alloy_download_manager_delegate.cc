// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/alloy/alloy_download_manager_delegate.h"

#include "chrome/common/chrome_constants.h"
#include "components/download/public/common/download_item.h"

AlloyDownloadManagerDelegate::AlloyDownloadManagerDelegate(
    content::DownloadManager* manager)
    : CefDownloadManagerDelegateImpl(manager, /*alloy_bootstrap=*/true) {}

void AlloyDownloadManagerDelegate::GetNextId(
    content::DownloadIdCallback callback) {
  static uint32_t next_id = download::DownloadItem::kInvalidId + 1;
  std::move(callback).Run(next_id++);
}

std::string AlloyDownloadManagerDelegate::ApplicationClientIdForFileScanning() {
  return std::string(chrome::kApplicationClientIDStringForAVScanning);
}
