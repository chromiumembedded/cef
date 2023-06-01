// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DOWNLOAD_ITEM_IMPL_H_
#define CEF_LIBCEF_BROWSER_DOWNLOAD_ITEM_IMPL_H_
#pragma once

#include "include/cef_download_item.h"
#include "libcef/common/value_base.h"

namespace download {
class DownloadItem;
}

// CefDownloadItem implementation
class CefDownloadItemImpl
    : public CefValueBase<CefDownloadItem, download::DownloadItem> {
 public:
  explicit CefDownloadItemImpl(download::DownloadItem* value);

  CefDownloadItemImpl(const CefDownloadItemImpl&) = delete;
  CefDownloadItemImpl& operator=(const CefDownloadItemImpl&) = delete;

  // CefDownloadItem methods.
  bool IsValid() override;
  bool IsInProgress() override;
  bool IsComplete() override;
  bool IsCanceled() override;
  bool IsInterrupted() override;
  cef_download_interrupt_reason_t GetInterruptReason() override;
  int64_t GetCurrentSpeed() override;
  int GetPercentComplete() override;
  int64_t GetTotalBytes() override;
  int64_t GetReceivedBytes() override;
  CefBaseTime GetStartTime() override;
  CefBaseTime GetEndTime() override;
  CefString GetFullPath() override;
  uint32_t GetId() override;
  CefString GetURL() override;
  CefString GetOriginalUrl() override;
  CefString GetSuggestedFileName() override;
  CefString GetContentDisposition() override;
  CefString GetMimeType() override;
};

#endif  // CEF_LIBCEF_BROWSER_DOWNLOAD_ITEM_IMPL_H_
