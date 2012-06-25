// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/download_manager_delegate.h"

#if defined(OS_WIN)
#include <windows.h>
#include <commdlg.h>
#endif

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_util.h"

using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;
using content::WebContents;

CefDownloadManagerDelegate::CefDownloadManagerDelegate()
    : download_manager_(NULL) {
}

CefDownloadManagerDelegate::~CefDownloadManagerDelegate() {
}

void CefDownloadManagerDelegate::SetDownloadManager(
    DownloadManager* download_manager) {
  download_manager_ = download_manager;
}

bool CefDownloadManagerDelegate::ShouldStartDownload(int32 download_id) {
  DownloadItem* download =
      download_manager_->GetActiveDownloadItem(download_id);

  if (!download->GetForcedFilePath().empty()) {
    download->OnTargetPathDetermined(
        download->GetForcedFilePath(),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
    return true;
  }

  FilePath generated_name = net::GenerateFileName(
      download->GetURL(),
      download->GetContentDisposition(),
      download->GetReferrerCharset(),
      download->GetSuggestedFilename(),
      download->GetMimeType(),
      "download");

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &CefDownloadManagerDelegate::GenerateFilename,
          this, download_id, generated_name));
  return false;
}

void CefDownloadManagerDelegate::ChooseDownloadPath(
    content::DownloadItem* item) {
  FilePath result;
#if defined(OS_WIN) || defined(OS_MACOSX)
  WebContents* web_contents = item->GetWebContents();
  const FilePath suggested_path(item->GetTargetFilePath());
  result = PlatformChooseDownloadPath(web_contents, suggested_path);
#else
  NOTIMPLEMENTED();
#endif

  if (result.empty()) {
    download_manager_->FileSelectionCanceled(item->GetId());
  } else {
    download_manager_->FileSelected(result, item->GetId());
  }
}

void CefDownloadManagerDelegate::AddItemToPersistentStore(
    content::DownloadItem* item) {
  static int next_id;
  download_manager_->OnItemAddedToPersistentStore(item->GetId(), ++next_id);
}

void CefDownloadManagerDelegate::GenerateFilename(
    int32 download_id,
    const FilePath& generated_name) {
  FilePath suggested_path = download_manager_->GetBrowserContext()->GetPath().
      Append(FILE_PATH_LITERAL("Downloads"));
  if (!file_util::DirectoryExists(suggested_path))
    file_util::CreateDirectory(suggested_path);

  suggested_path = suggested_path.Append(generated_name);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &CefDownloadManagerDelegate::RestartDownload,
          this, download_id, suggested_path));
}

void CefDownloadManagerDelegate::RestartDownload(
    int32 download_id,
    const FilePath& suggested_path) {
  DownloadItem* download =
      download_manager_->GetActiveDownloadItem(download_id);
  if (!download)
    return;

  // Since we have no download UI, show the user a dialog always.
  download->OnTargetPathDetermined(suggested_path,
                                   DownloadItem::TARGET_DISPOSITION_PROMPT,
                                   content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  download_manager_->RestartDownload(download_id);
}
