// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/download_item_impl.h"

#include "libcef/common/time_util.h"

#include "components/download/public/common/download_item.h"
#include "url/gurl.h"

CefDownloadItemImpl::CefDownloadItemImpl(download::DownloadItem* value)
    : CefValueBase<CefDownloadItem, download::DownloadItem>(
          value,
          nullptr,
          kOwnerNoDelete,
          true,
          new CefValueControllerNonThreadSafe()) {
  // Indicate that this object owns the controller.
  SetOwnsController();
}

bool CefDownloadItemImpl::IsValid() {
  return !detached();
}

bool CefDownloadItemImpl::IsInProgress() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().GetState() == download::DownloadItem::IN_PROGRESS;
}

bool CefDownloadItemImpl::IsComplete() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().GetState() == download::DownloadItem::COMPLETE;
}

bool CefDownloadItemImpl::IsCanceled() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().GetState() == download::DownloadItem::CANCELLED;
}

bool CefDownloadItemImpl::IsInterrupted() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().GetState() == download::DownloadItem::INTERRUPTED;
}

cef_download_interrupt_reason_t CefDownloadItemImpl::GetInterruptReason() {
  CEF_VALUE_VERIFY_RETURN(false, CEF_DOWNLOAD_INTERRUPT_REASON_NONE);
  return static_cast<cef_download_interrupt_reason_t>(
      const_value().GetLastReason());
}

int64_t CefDownloadItemImpl::GetCurrentSpeed() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().CurrentSpeed();
}

int CefDownloadItemImpl::GetPercentComplete() {
  CEF_VALUE_VERIFY_RETURN(false, -1);
  return const_value().PercentComplete();
}

int64_t CefDownloadItemImpl::GetTotalBytes() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().GetTotalBytes();
}

int64_t CefDownloadItemImpl::GetReceivedBytes() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().GetReceivedBytes();
}

CefBaseTime CefDownloadItemImpl::GetStartTime() {
  CEF_VALUE_VERIFY_RETURN(false, CefBaseTime());
  return const_value().GetStartTime();
}

CefBaseTime CefDownloadItemImpl::GetEndTime() {
  CEF_VALUE_VERIFY_RETURN(false, CefBaseTime());
  return const_value().GetEndTime();
}

CefString CefDownloadItemImpl::GetFullPath() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetFullPath().value();
}

uint32_t CefDownloadItemImpl::GetId() {
  CEF_VALUE_VERIFY_RETURN(false, 0);
  return const_value().GetId();
}

CefString CefDownloadItemImpl::GetURL() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetURL().spec();
}

CefString CefDownloadItemImpl::GetOriginalUrl() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetOriginalUrl().spec();
}

CefString CefDownloadItemImpl::GetSuggestedFileName() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetSuggestedFilename();
}

CefString CefDownloadItemImpl::GetContentDisposition() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetContentDisposition();
}

CefString CefDownloadItemImpl::GetMimeType() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetMimeType();
}
