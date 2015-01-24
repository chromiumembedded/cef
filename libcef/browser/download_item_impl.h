// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DOWNLOAD_ITEM_IMPL_H_
#define CEF_LIBCEF_BROWSER_DOWNLOAD_ITEM_IMPL_H_
#pragma once

#include "include/cef_download_item.h"
#include "libcef/common/value_base.h"

namespace content {
class DownloadItem;
}

// CefDownloadItem implementation
class CefDownloadItemImpl
    : public CefValueBase<CefDownloadItem, content::DownloadItem> {
 public:
  explicit CefDownloadItemImpl(content::DownloadItem* value);

  // CefDownloadItem methods.
  bool IsValid() override;
  bool IsInProgress() override;
  bool IsComplete() override;
  bool IsCanceled() override;
  int64 GetCurrentSpeed() override;
  int GetPercentComplete() override;
  int64 GetTotalBytes() override;
  int64 GetReceivedBytes() override;
  CefTime GetStartTime() override;
  CefTime GetEndTime() override;
  CefString GetFullPath() override;
  uint32 GetId() override;
  CefString GetURL() override;
  CefString GetOriginalUrl() override;
  CefString GetSuggestedFileName() override;
  CefString GetContentDisposition() override;
  CefString GetMimeType() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefDownloadItemImpl);
};

#endif  // CEF_LIBCEF_BROWSER_DOWNLOAD_ITEM_IMPL_H_
