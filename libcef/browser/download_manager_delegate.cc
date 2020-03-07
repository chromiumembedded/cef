// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/download_manager_delegate.h"

#include "include/cef_download_handler.h"
#include "libcef/browser/context.h"
#include "libcef/browser/download_item_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"

using content::DownloadManager;
using content::WebContents;
using download::DownloadItem;

namespace {

// Helper function to retrieve the CefDownloadHandler.
CefRefPtr<CefDownloadHandler> GetDownloadHandler(
    CefRefPtr<CefBrowserHostImpl> browser) {
  CefRefPtr<CefClient> client = browser->GetClient();
  if (client.get())
    return client->GetDownloadHandler();
  return nullptr;
}

// CefBeforeDownloadCallback implementation.
class CefBeforeDownloadCallbackImpl : public CefBeforeDownloadCallback {
 public:
  CefBeforeDownloadCallbackImpl(const base::WeakPtr<DownloadManager>& manager,
                                uint32 download_id,
                                const base::FilePath& suggested_name,
                                content::DownloadTargetCallback callback)
      : manager_(manager),
        download_id_(download_id),
        suggested_name_(suggested_name),
        callback_(std::move(callback)) {}

  void Continue(const CefString& download_path, bool show_dialog) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (download_id_ <= 0)
        return;

      if (manager_) {
        base::FilePath path = base::FilePath(download_path);
        CEF_POST_USER_VISIBLE_TASK(
            base::BindOnce(&CefBeforeDownloadCallbackImpl::GenerateFilename,
                           manager_, download_id_, suggested_name_, path,
                           show_dialog, std::move(callback_)));
      }

      download_id_ = 0;
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefBeforeDownloadCallbackImpl::Continue,
                                   this, download_path, show_dialog));
    }
  }

 private:
  static void GenerateFilename(base::WeakPtr<DownloadManager> manager,
                               uint32 download_id,
                               const base::FilePath& suggested_name,
                               const base::FilePath& download_path,
                               bool show_dialog,
                               content::DownloadTargetCallback callback) {
    CEF_REQUIRE_BLOCKING();

    base::FilePath suggested_path = download_path;
    if (!suggested_path.empty()) {
      // Create the directory if necessary.
      base::FilePath dir_path = suggested_path.DirName();
      if (!base::DirectoryExists(dir_path) &&
          !base::CreateDirectory(dir_path)) {
        NOTREACHED() << "failed to create the download directory";
        suggested_path.clear();
      }
    }

    if (suggested_path.empty()) {
      if (base::PathService::Get(base::DIR_TEMP, &suggested_path)) {
        // Use the temp directory.
        suggested_path = suggested_path.Append(suggested_name);
      } else {
        // Use the current working directory.
        suggested_path = suggested_name;
      }
    }

    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBeforeDownloadCallbackImpl::ChooseDownloadPath,
                       manager, download_id, suggested_path, show_dialog,
                       std::move(callback)));
  }

  static void ChooseDownloadPath(base::WeakPtr<DownloadManager> manager,
                                 uint32 download_id,
                                 const base::FilePath& suggested_path,
                                 bool show_dialog,
                                 content::DownloadTargetCallback callback) {
    if (!manager)
      return;

    DownloadItem* item = manager->GetDownload(download_id);
    if (!item || item->GetState() != DownloadItem::IN_PROGRESS)
      return;

    bool handled = false;

    if (show_dialog) {
      WebContents* web_contents =
          content::DownloadItemUtils::GetWebContents(item);
      CefRefPtr<CefBrowserHostImpl> browser =
          CefBrowserHostImpl::GetBrowserForContents(web_contents);
      if (browser.get()) {
        handled = true;

        CefFileDialogRunner::FileChooserParams params;
        params.mode = blink::mojom::FileChooserParams::Mode::kSave;
        if (!suggested_path.empty()) {
          params.default_file_name = suggested_path;
          if (!suggested_path.Extension().empty()) {
            params.accept_types.push_back(
                CefString(suggested_path.Extension()));
          }
        }

        browser->RunFileChooser(
            params,
            base::BindOnce(
                &CefBeforeDownloadCallbackImpl::ChooseDownloadPathCallback,
                std::move(callback)));
      }
    }

    if (!handled) {
      std::move(callback).Run(
          suggested_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DownloadItem::MixedContentStatus::UNKNOWN, suggested_path,
          download::DOWNLOAD_INTERRUPT_REASON_NONE);
    }
  }

  static void ChooseDownloadPathCallback(
      content::DownloadTargetCallback callback,
      int selected_accept_filter,
      const std::vector<base::FilePath>& file_paths) {
    DCHECK_LE(file_paths.size(), (size_t)1);

    base::FilePath path;
    if (file_paths.size() > 0)
      path = file_paths.front();

    // The download will be cancelled if |path| is empty.
    std::move(callback).Run(path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                            download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                            download::DownloadItem::MixedContentStatus::UNKNOWN,
                            path, download::DOWNLOAD_INTERRUPT_REASON_NONE);
  }

  base::WeakPtr<DownloadManager> manager_;
  uint32 download_id_;
  base::FilePath suggested_name_;
  content::DownloadTargetCallback callback_;

  IMPLEMENT_REFCOUNTING(CefBeforeDownloadCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefBeforeDownloadCallbackImpl);
};

// CefDownloadItemCallback implementation.
class CefDownloadItemCallbackImpl : public CefDownloadItemCallback {
 public:
  explicit CefDownloadItemCallbackImpl(
      const base::WeakPtr<DownloadManager>& manager,
      uint32 download_id)
      : manager_(manager), download_id_(download_id) {}

  void Cancel() override {
    CEF_POST_TASK(CEF_UIT,
                  base::Bind(&CefDownloadItemCallbackImpl::DoCancel, this));
  }

  void Pause() override {
    CEF_POST_TASK(CEF_UIT,
                  base::Bind(&CefDownloadItemCallbackImpl::DoPause, this));
  }

  void Resume() override {
    CEF_POST_TASK(CEF_UIT,
                  base::Bind(&CefDownloadItemCallbackImpl::DoResume, this));
  }

 private:
  void DoCancel() {
    if (download_id_ <= 0)
      return;

    if (manager_) {
      DownloadItem* item = manager_->GetDownload(download_id_);
      if (item && item->GetState() == DownloadItem::IN_PROGRESS)
        item->Cancel(true);
    }

    download_id_ = 0;
  }

  void DoPause() {
    if (download_id_ <= 0)
      return;

    if (manager_) {
      DownloadItem* item = manager_->GetDownload(download_id_);
      if (item && item->GetState() == DownloadItem::IN_PROGRESS)
        item->Pause();
    }
  }

  void DoResume() {
    if (download_id_ <= 0)
      return;

    if (manager_) {
      DownloadItem* item = manager_->GetDownload(download_id_);
      if (item && item->CanResume())
        item->Resume(true);
    }
  }

  base::WeakPtr<DownloadManager> manager_;
  uint32 download_id_;

  IMPLEMENT_REFCOUNTING(CefDownloadItemCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefDownloadItemCallbackImpl);
};

}  // namespace

CefDownloadManagerDelegate::CefDownloadManagerDelegate(DownloadManager* manager)
    : manager_(manager), manager_ptr_factory_(manager) {
  DCHECK(manager);
  manager->AddObserver(this);

  DownloadManager::DownloadVector items;
  manager->GetAllDownloads(&items);
  DownloadManager::DownloadVector::const_iterator it = items.begin();
  for (; it != items.end(); ++it)
    OnDownloadCreated(manager, *it);
}

CefDownloadManagerDelegate::~CefDownloadManagerDelegate() {
  if (manager_) {
    manager_->SetDelegate(nullptr);
    manager_->RemoveObserver(this);
  }

  while (!item_browser_map_.empty())
    OnDownloadDestroyed(item_browser_map_.begin()->first);
}

void CefDownloadManagerDelegate::OnDownloadUpdated(DownloadItem* download) {
  CefRefPtr<CefBrowserHostImpl> browser = GetBrowser(download);
  CefRefPtr<CefDownloadHandler> handler;
  if (browser.get())
    handler = GetDownloadHandler(browser);

  if (handler.get()) {
    CefRefPtr<CefDownloadItemImpl> download_item(
        new CefDownloadItemImpl(download));
    CefRefPtr<CefDownloadItemCallback> callback(new CefDownloadItemCallbackImpl(
        manager_ptr_factory_.GetWeakPtr(), download->GetId()));

    handler->OnDownloadUpdated(browser.get(), download_item.get(), callback);

    download_item->Detach(nullptr);
  }
}

void CefDownloadManagerDelegate::OnDownloadDestroyed(DownloadItem* item) {
  item->RemoveObserver(this);

  CefBrowserHostImpl* browser = nullptr;

  ItemBrowserMap::iterator it = item_browser_map_.find(item);
  DCHECK(it != item_browser_map_.end());
  if (it != item_browser_map_.end()) {
    browser = it->second;
    item_browser_map_.erase(it);
  }

  if (browser) {
    // Determine if any remaining DownloadItems are associated with the same
    // browser. If not, then unregister as an observer.
    bool has_remaining = false;
    ItemBrowserMap::const_iterator it2 = item_browser_map_.begin();
    for (; it2 != item_browser_map_.end(); ++it2) {
      if (it2->second == browser) {
        has_remaining = true;
        break;
      }
    }

    if (!has_remaining)
      browser->RemoveObserver(this);
  }
}

void CefDownloadManagerDelegate::OnDownloadCreated(DownloadManager* manager,
                                                   DownloadItem* item) {
  // This callback may arrive after DetermineDownloadTarget, so we allow
  // association from either method.
  CefRefPtr<CefBrowserHostImpl> browser = GetOrAssociateBrowser(item);
  if (!browser) {
    // If the download is rejected (e.g. ALT+click on an invalid protocol link)
    // then an "interrupted" download will be started via DownloadManagerImpl::
    // StartDownloadWithId (originating from CreateInterruptedDownload) with no
    // associated WebContents and consequently no associated CEF browser. In
    // that case DetermineDownloadTarget will be called before this method.
    // TODO(cef): Figure out how to expose this via a client callback.
    const std::vector<GURL>& url_chain = item->GetUrlChain();
    if (!url_chain.empty()) {
      LOG(INFO) << "Rejected download of " << url_chain.back().spec();
    }
    item->Cancel(true);
  }
}

void CefDownloadManagerDelegate::ManagerGoingDown(DownloadManager* manager) {
  DCHECK_EQ(manager, manager_);
  manager->SetDelegate(nullptr);
  manager->RemoveObserver(this);
  manager_ptr_factory_.InvalidateWeakPtrs();
  manager_ = nullptr;
}

bool CefDownloadManagerDelegate::DetermineDownloadTarget(
    DownloadItem* item,
    content::DownloadTargetCallback* callback) {
  if (!item->GetForcedFilePath().empty()) {
    std::move(*callback).Run(
        item->GetForcedFilePath(), DownloadItem::TARGET_DISPOSITION_OVERWRITE,
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        download::DownloadItem::MixedContentStatus::UNKNOWN,
        item->GetForcedFilePath(), download::DOWNLOAD_INTERRUPT_REASON_NONE);
    return true;
  }

  // This callback may arrive before OnDownloadCreated, so we allow association
  // from either method.
  CefRefPtr<CefBrowserHostImpl> browser = GetOrAssociateBrowser(item);
  CefRefPtr<CefDownloadHandler> handler;
  if (browser.get())
    handler = GetDownloadHandler(browser);

  if (handler.get()) {
    base::FilePath suggested_name = net::GenerateFileName(
        item->GetURL(), item->GetContentDisposition(), std::string(),
        item->GetSuggestedFilename(), item->GetMimeType(), "download");

    CefRefPtr<CefDownloadItemImpl> download_item(new CefDownloadItemImpl(item));
    CefRefPtr<CefBeforeDownloadCallback> callbackObj(
        new CefBeforeDownloadCallbackImpl(manager_ptr_factory_.GetWeakPtr(),
                                          item->GetId(), suggested_name,
                                          std::move(*callback)));

    handler->OnBeforeDownload(browser.get(), download_item.get(),
                              suggested_name.value(), callbackObj);

    download_item->Detach(nullptr);
  }

  return true;
}

void CefDownloadManagerDelegate::GetNextId(
    content::DownloadIdCallback callback) {
  static uint32 next_id = DownloadItem::kInvalidId + 1;
  std::move(callback).Run(next_id++);
}

std::string CefDownloadManagerDelegate::ApplicationClientIdForFileScanning() {
  const CefSettings& settings = CefContext::Get()->settings();
  if (settings.application_client_id_for_file_scanning.length > 0) {
    return CefString(&settings.application_client_id_for_file_scanning)
        .ToString();
  }
  return std::string();
}

void CefDownloadManagerDelegate::OnBrowserDestroyed(
    CefBrowserHostImpl* browser) {
  ItemBrowserMap::iterator it = item_browser_map_.begin();
  for (; it != item_browser_map_.end(); ++it) {
    if (it->second == browser) {
      // Don't call back into browsers that have been destroyed. We're not
      // canceling the download so it will continue silently until it completes
      // or until the associated browser context is destroyed.
      it->second = nullptr;
    }
  }
}

CefBrowserHostImpl* CefDownloadManagerDelegate::GetOrAssociateBrowser(
    download::DownloadItem* item) {
  ItemBrowserMap::const_iterator it = item_browser_map_.find(item);
  if (it != item_browser_map_.end())
    return it->second;

  CefBrowserHostImpl* browser = nullptr;
  content::WebContents* contents =
      content::DownloadItemUtils::GetWebContents(item);
  if (contents) {
    browser = CefBrowserHostImpl::GetBrowserForContents(contents).get();
    DCHECK(browser);
  }
  if (!browser)
    return nullptr;

  item->AddObserver(this);

  item_browser_map_.insert(std::make_pair(item, browser));

  // Register as an observer so that we can cancel associated DownloadItems when
  // the browser is destroyed.
  if (!browser->HasObserver(this))
    browser->AddObserver(this);

  return browser;
}

CefBrowserHostImpl* CefDownloadManagerDelegate::GetBrowser(DownloadItem* item) {
  ItemBrowserMap::const_iterator it = item_browser_map_.find(item);
  if (it != item_browser_map_.end())
    return it->second;

  // If the download is rejected (e.g. ALT+click on an invalid protocol link)
  // then an "interrupted" download will be started via DownloadManagerImpl::
  // StartDownloadWithId (originating from CreateInterruptedDownload) with no
  // associated WebContents and consequently no associated CEF browser. In that
  // case DetermineDownloadTarget will be called before OnDownloadCreated.
  DCHECK(!content::DownloadItemUtils::GetWebContents(item));
  return nullptr;
}
