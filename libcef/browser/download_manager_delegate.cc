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

content::DownloadId CefDownloadManagerDelegate::GetNextId() {
  static int next_id;
  return content::DownloadId(this, ++next_id);
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
    WebContents* web_contents,
    const FilePath& suggested_path,
    int32 download_id) {
  FilePath result;
#if defined(OS_WIN) && !defined(USE_AURA)
  std::wstring file_part = FilePath(suggested_path).BaseName().value();
  wchar_t file_name[MAX_PATH];
  base::wcslcpy(file_name, file_part.c_str(), arraysize(file_name));
  OPENFILENAME save_as;
  ZeroMemory(&save_as, sizeof(save_as));
  save_as.lStructSize = sizeof(OPENFILENAME);
  save_as.hwndOwner = web_contents->GetNativeView();
  save_as.lpstrFile = file_name;
  save_as.nMaxFile = arraysize(file_name);

  std::wstring directory;
  if (!suggested_path.empty())
    directory = suggested_path.DirName().value();

  save_as.lpstrInitialDir = directory.c_str();
  save_as.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_ENABLESIZING |
                  OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;

  if (GetSaveFileName(&save_as))
    result = FilePath(std::wstring(save_as.lpstrFile));
#else
  NOTIMPLEMENTED();
#endif

  if (result.empty()) {
    download_manager_->FileSelectionCanceled(download_id);
  } else {
    download_manager_->FileSelected(result, download_id);
  }
}

void CefDownloadManagerDelegate::GenerateFilename(
    int32 download_id,
    const FilePath& generated_name) {
  FilePath suggested_path = download_manager_->GetBrowserContext()->GetPath().
      Append(FILE_PATH_LITERAL("Downloads"));
  if (!file_util::PathExists(suggested_path))
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
