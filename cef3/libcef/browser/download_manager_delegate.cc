// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/download_manager_delegate.h"

#include "include/cef_download_handler.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/download_item_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/file_chooser_params.h"
#include "net/base/net_util.h"

using content::DownloadItem;
using content::DownloadManager;
using content::WebContents;


namespace {

// Helper function to retrieve the CefBrowserHostImpl.
CefRefPtr<CefBrowserHostImpl> GetBrowser(DownloadItem* item) {
  content::WebContents* contents = item->GetWebContents();
  if (!contents)
    return NULL;

  return CefBrowserHostImpl::GetBrowserForContents(contents).get();
}

// Helper function to retrieve the CefDownloadHandler.
CefRefPtr<CefDownloadHandler> GetDownloadHandler(
    CefRefPtr<CefBrowserHostImpl> browser) {
  CefRefPtr<CefClient> client = browser->GetClient();
  if (client.get())
    return client->GetDownloadHandler();
  return NULL;
}


// CefBeforeDownloadCallback implementation.
class CefBeforeDownloadCallbackImpl : public CefBeforeDownloadCallback {
 public:
  CefBeforeDownloadCallbackImpl(
      const base::WeakPtr<DownloadManager>& manager,
      int32 download_id,
      const FilePath& suggested_name,
      const content::DownloadTargetCallback& callback)
      : manager_(manager),
        download_id_(download_id),
        suggested_name_(suggested_name),
        callback_(callback) {
  }

  virtual void Continue(const CefString& download_path,
                        bool show_dialog) OVERRIDE {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (download_id_ <= 0)
        return;

      if (manager_) {
        FilePath path = FilePath(download_path);
        CEF_POST_TASK(CEF_FILET,
            base::Bind(&CefBeforeDownloadCallbackImpl::GenerateFilename,
                       manager_, download_id_, suggested_name_, path,
                       show_dialog, callback_));
      }

      download_id_ = 0;
      callback_.Reset();
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefBeforeDownloadCallbackImpl::Continue, this,
                     download_path, show_dialog));
    }
  }

 private:
  static void GenerateFilename(
      base::WeakPtr<DownloadManager> manager,
      int32 download_id,
      const FilePath& suggested_name,
      const FilePath& download_path,
      bool show_dialog,
      const content::DownloadTargetCallback& callback) {
    FilePath suggested_path = download_path;
    if (!suggested_path.empty()) {
      // Create the directory if necessary.
      FilePath dir_path = suggested_path.DirName();
      if (!file_util::DirectoryExists(dir_path) &&
          !file_util::CreateDirectory(dir_path)) {
        NOTREACHED() << "failed to create the download directory";
        suggested_path.clear();
      }
    }

    if (suggested_path.empty()) {
      if (PathService::Get(base::DIR_TEMP, &suggested_path)) {
        // Use the temp directory.
        suggested_path = suggested_path.Append(suggested_name);
      } else {
        // Use the current working directory.
        suggested_path = suggested_name;
      }
    }

    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBeforeDownloadCallbackImpl::ChooseDownloadPath,
                   manager, download_id, suggested_path, show_dialog,
                   callback));
  }

  static void ChooseDownloadPath(
      base::WeakPtr<DownloadManager> manager,
      int32 download_id,
      const FilePath& suggested_path,
      bool show_dialog,
      const content::DownloadTargetCallback& callback) {
    if (!manager)
      return;

    DownloadItem* item = manager->GetDownload(download_id);
    if (!item || !item->IsInProgress())
      return;

    bool handled = false;

    if (show_dialog) {
      WebContents* web_contents = item->GetWebContents();
      CefRefPtr<CefBrowserHostImpl> browser =
          CefBrowserHostImpl::GetBrowserForContents(web_contents);
      if (browser.get()) {
        handled = true;

        content::FileChooserParams params;
        params.mode = content::FileChooserParams::Save;
        if (!suggested_path.empty()) {
          params.default_file_name = suggested_path;
          if (!suggested_path.Extension().empty()) {
            params.accept_types.push_back(
                CefString(suggested_path.Extension()));
          }
        }

        browser->RunFileChooser(params,
            base::Bind(
                &CefBeforeDownloadCallbackImpl::ChooseDownloadPathCallback,
                callback));
      }
    }

    if (!handled) {
      callback.Run(suggested_path,
                   DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                   content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                   suggested_path);
    }
  }

  static void ChooseDownloadPathCallback(
      const content::DownloadTargetCallback& callback,
      const std::vector<FilePath>& file_paths) {
    DCHECK_LE(file_paths.size(), (size_t) 1);

    FilePath path;
    if (file_paths.size() > 0)
      path = file_paths.front();

    // The download will be cancelled if |path| is empty.
    callback.Run(path,
                 DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                 path);
  }

  base::WeakPtr<DownloadManager> manager_;
  int32 download_id_;
  FilePath suggested_name_;
  content::DownloadTargetCallback callback_;

  IMPLEMENT_REFCOUNTING(CefBeforeDownloadCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefBeforeDownloadCallbackImpl);
};


// CefDownloadItemCallback implementation.
class CefDownloadItemCallbackImpl : public CefDownloadItemCallback {
 public:
  explicit CefDownloadItemCallbackImpl(
      const base::WeakPtr<DownloadManager>& manager,
      int32 download_id)
      : manager_(manager),
        download_id_(download_id) {
  }

  virtual void Cancel() OVERRIDE {
     CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefDownloadItemCallbackImpl::DoCancel, this));
  }

 private:
  void DoCancel() {
    if (download_id_ <= 0)
      return;

    if (manager_) {
      DownloadItem* item = manager_->GetDownload(download_id_);
      if (item && item->IsInProgress())
        item->Cancel(true);
    }

    download_id_ = 0;
  }

  base::WeakPtr<DownloadManager> manager_;
  int32 download_id_;

  IMPLEMENT_REFCOUNTING(CefDownloadItemCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefDownloadItemCallbackImpl);
};

}  // namespace


CefDownloadManagerDelegate::CefDownloadManagerDelegate(
    DownloadManager* manager)
    : manager_(manager),
      manager_ptr_factory_(manager) {
  DCHECK(manager);
  manager->AddObserver(this);

  DownloadManager::DownloadVector items;
  manager->GetAllDownloads(&items);
  DownloadManager::DownloadVector::const_iterator it = items.begin();
  for (; it != items.end(); ++it) {
    (*it)->AddObserver(this);
    observing_.insert(*it);
  }
}

CefDownloadManagerDelegate::~CefDownloadManagerDelegate() {
  if (manager_) {
    manager_->SetDelegate(NULL);
    manager_->RemoveObserver(this);
  }

  std::set<DownloadItem*>::const_iterator it = observing_.begin();
  for (; it != observing_.end(); ++it)
    (*it)->RemoveObserver(this);
  observing_.clear();
}

void CefDownloadManagerDelegate::OnDownloadUpdated(
    DownloadItem* download) {
  CefRefPtr<CefBrowserHostImpl> browser = GetBrowser(download);
  CefRefPtr<CefDownloadHandler> handler;
  if (browser.get())
    handler = GetDownloadHandler(browser);

  if (handler.get()) {
    CefRefPtr<CefDownloadItemImpl> download_item(
        new CefDownloadItemImpl(download));
    CefRefPtr<CefDownloadItemCallback> callback(
        new CefDownloadItemCallbackImpl(manager_ptr_factory_.GetWeakPtr(),
                                        download->GetId()));

    handler->OnDownloadUpdated(browser.get(), download_item.get(), callback);

    download_item->Detach(NULL);
  }
}

void CefDownloadManagerDelegate::OnDownloadDestroyed(
    DownloadItem* download) {
  download->RemoveObserver(this);
  observing_.erase(download);
}

void CefDownloadManagerDelegate::OnDownloadCreated(
    DownloadManager* manager,
    DownloadItem* item) {
  item->AddObserver(this);
  observing_.insert(item);
}

void CefDownloadManagerDelegate::ManagerGoingDown(
    DownloadManager* manager) {
  DCHECK_EQ(manager, manager_);
  manager->SetDelegate(NULL);
  manager->RemoveObserver(this);
  manager_ptr_factory_.InvalidateWeakPtrs();
  manager_ = NULL;
}

bool CefDownloadManagerDelegate::DetermineDownloadTarget(
    DownloadItem* item,
    const content::DownloadTargetCallback& callback) {
  if (!item->GetForcedFilePath().empty()) {
    callback.Run(item->GetForcedFilePath(),
                 DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                 item->GetForcedFilePath());
    return true;
  }

  CefRefPtr<CefBrowserHostImpl> browser = GetBrowser(item);
  CefRefPtr<CefDownloadHandler> handler;
  if (browser.get())
    handler = GetDownloadHandler(browser);

  if (handler.get()) {
    FilePath suggested_name = net::GenerateFileName(
        item->GetURL(),
        item->GetContentDisposition(),
        EmptyString(),
        item->GetSuggestedFilename(),
        item->GetMimeType(),
        "download");

    CefRefPtr<CefDownloadItemImpl> download_item(new CefDownloadItemImpl(item));
    CefRefPtr<CefBeforeDownloadCallback> callbackObj(
        new CefBeforeDownloadCallbackImpl(manager_ptr_factory_.GetWeakPtr(),
                                          item->GetId(), suggested_name,
                                          callback));

    handler->OnBeforeDownload(browser.get(), download_item.get(),
                              suggested_name.value(), callbackObj);

    download_item->Detach(NULL);
  }

  return true;
}
