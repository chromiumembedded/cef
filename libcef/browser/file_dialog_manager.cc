// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/file_dialog_manager.h"

#include <utility>

#include "include/cef_dialog_handler.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/thread_util.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/common/file_chooser_file_info.h"
#include "net/base/directory_lister.h"

namespace {

class CefFileDialogCallbackImpl : public CefFileDialogCallback {
 public:
  explicit CefFileDialogCallbackImpl(
      const CefFileDialogRunner::RunFileChooserCallback& callback)
      : callback_(callback) {
  }
  ~CefFileDialogCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        CancelNow(callback_);
      } else {
        CEF_POST_TASK(CEF_UIT,
            base::Bind(&CefFileDialogCallbackImpl::CancelNow, callback_));
      }
    }
  }

  void Continue(int selected_accept_filter,
                const std::vector<CefString>& file_paths) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        std::vector<base::FilePath> vec;
        if (!file_paths.empty()) {
          std::vector<CefString>::const_iterator it = file_paths.begin();
          for (; it != file_paths.end(); ++it)
            vec.push_back(base::FilePath(*it));
        }
        callback_.Run(selected_accept_filter, vec);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefFileDialogCallbackImpl::Continue, this,
                     selected_accept_filter, file_paths));
    }
  }

  void Cancel() override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        CancelNow(callback_);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefFileDialogCallbackImpl::Cancel, this));
    }
  }

  bool IsConnected() {
    return !callback_.is_null();
  }

  void Disconnect() {
    callback_.Reset();
  }

 private:
  static void CancelNow(
      const CefFileDialogRunner::RunFileChooserCallback& callback) {
    CEF_REQUIRE_UIT();
    std::vector<base::FilePath> file_paths;
    callback.Run(0, file_paths);
  }

  CefFileDialogRunner::RunFileChooserCallback callback_;

  IMPLEMENT_REFCOUNTING(CefFileDialogCallbackImpl);
};

void RunFileDialogDismissed(
    CefRefPtr<CefRunFileDialogCallback> callback,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  std::vector<CefString> paths;
  if (file_paths.size() > 0) {
    for (size_t i = 0; i < file_paths.size(); ++i)
      paths.push_back(file_paths[i].value());
  }
  callback->OnFileDialogDismissed(selected_accept_filter, paths);
}

class UploadFolderHelper :
    public net::DirectoryLister::DirectoryListerDelegate {
 public:
  explicit UploadFolderHelper(
      const CefFileDialogRunner::RunFileChooserCallback& callback)
      : callback_(callback) {
  }

  ~UploadFolderHelper() override {
    if (!callback_.is_null()) {
      if (CEF_CURRENTLY_ON_UIT()) {
        CancelNow(callback_);
      } else {
        CEF_POST_TASK(CEF_UIT,
            base::Bind(&UploadFolderHelper::CancelNow, callback_));
      }
    }
  }

  void OnListFile(
      const net::DirectoryLister::DirectoryListerData& data) override {
    CEF_REQUIRE_UIT();
    if (!data.info.IsDirectory())
      select_files_.push_back(data.path);
  }

  void OnListDone(int error) override {
    CEF_REQUIRE_UIT();
    if (!callback_.is_null()) {
      callback_.Run(0, select_files_);
      callback_.Reset();
    }
  }

 private:
  static void CancelNow(
      const CefFileDialogRunner::RunFileChooserCallback& callback) {
    CEF_REQUIRE_UIT();
    std::vector<base::FilePath> file_paths;
    callback.Run(0, file_paths);
  }

  CefFileDialogRunner::RunFileChooserCallback callback_;
  std::vector<base::FilePath> select_files_;

  DISALLOW_COPY_AND_ASSIGN(UploadFolderHelper);
};

}  // namespace

CefFileDialogManager::CefFileDialogManager(
    CefBrowserHostImpl* browser,
    std::unique_ptr<CefFileDialogRunner> runner)
    : content::WebContentsObserver(browser->web_contents()),
      browser_(browser),
      runner_(std::move(runner)),
      file_chooser_pending_(false),
      render_frame_host_(nullptr),
      weak_ptr_factory_(this) {
  DCHECK(web_contents());
}

CefFileDialogManager::~CefFileDialogManager() {
}

void CefFileDialogManager::Destroy() {
  DCHECK(!file_chooser_pending_);
  runner_.reset(NULL);
}

void CefFileDialogManager::RunFileDialog(
      cef_file_dialog_mode_t mode,
      const CefString& title,
      const CefString& default_file_path,
      const std::vector<CefString>& accept_filters,
      int selected_accept_filter,
      CefRefPtr<CefRunFileDialogCallback> callback) {
  DCHECK(callback.get());
  if (!callback.get())
    return;

  CefFileDialogRunner::FileChooserParams params;
  switch (mode & FILE_DIALOG_TYPE_MASK) {
    case FILE_DIALOG_OPEN:
      params.mode = content::FileChooserParams::Open;
      break;
    case FILE_DIALOG_OPEN_MULTIPLE:
      params.mode = content::FileChooserParams::OpenMultiple;
      break;
    case FILE_DIALOG_OPEN_FOLDER:
      params.mode = content::FileChooserParams::UploadFolder;
      break;
    case FILE_DIALOG_SAVE:
      params.mode = content::FileChooserParams::Save;
      break;
  }

  DCHECK_GE(selected_accept_filter, 0);
  params.selected_accept_filter = selected_accept_filter;

  params.overwriteprompt = !!(mode & FILE_DIALOG_OVERWRITEPROMPT_FLAG);
  params.hidereadonly = !!(mode & FILE_DIALOG_HIDEREADONLY_FLAG);

  params.title = title;
  if (!default_file_path.empty())
    params.default_file_name = base::FilePath(default_file_path);

  if (!accept_filters.empty()) {
    std::vector<CefString>::const_iterator it = accept_filters.begin();
    for (; it != accept_filters.end(); ++it)
      params.accept_types.push_back(*it);
  }

  RunFileChooser(params, base::Bind(RunFileDialogDismissed, callback));
}

void CefFileDialogManager::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    const content::FileChooserParams& params) {
  CEF_REQUIRE_UIT();
  DCHECK(render_frame_host);

  CefFileDialogRunner::FileChooserParams cef_params;
  static_cast<content::FileChooserParams&>(cef_params) = params;

  CefFileDialogRunner::RunFileChooserCallback callback;
  if (params.mode == content::FileChooserParams::UploadFolder) {
    callback = base::Bind(
        &CefFileDialogManager::OnRunFileChooserUploadFolderDelegateCallback,
        weak_ptr_factory_.GetWeakPtr(), params.mode);
  } else {
    callback = base::Bind(
        &CefFileDialogManager::OnRunFileChooserDelegateCallback,
        weak_ptr_factory_.GetWeakPtr(), params.mode);
  }

  RunFileChooserInternal(render_frame_host, cef_params, callback);
}

void CefFileDialogManager::RunFileChooser(
    const CefFileDialogRunner::FileChooserParams& params,
    const CefFileDialogRunner::RunFileChooserCallback& callback) {
  const CefFileDialogRunner::RunFileChooserCallback& host_callback =
      base::Bind(&CefFileDialogManager::OnRunFileChooserCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);
  RunFileChooserInternal(nullptr, params, host_callback);
}

void CefFileDialogManager::RunFileChooserInternal(
    content::RenderFrameHost* render_frame_host,
    const CefFileDialogRunner::FileChooserParams& params,
    const CefFileDialogRunner::RunFileChooserCallback& callback) {
  CEF_REQUIRE_UIT();

  if (file_chooser_pending_) {
    // Dismiss the new dialog immediately.
    callback.Run(0, std::vector<base::FilePath>());
    return;
  }

  file_chooser_pending_ = true;
  render_frame_host_ = render_frame_host;

  bool handled = false;

  if (browser_->client().get()) {
    CefRefPtr<CefDialogHandler> handler =
        browser_->client()->GetDialogHandler();
    if (handler.get()) {
      int mode = FILE_DIALOG_OPEN;
      switch (params.mode) {
        case content::FileChooserParams::Open:
          mode = FILE_DIALOG_OPEN;
          break;
        case content::FileChooserParams::OpenMultiple:
          mode = FILE_DIALOG_OPEN_MULTIPLE;
          break;
        case content::FileChooserParams::UploadFolder:
          mode = FILE_DIALOG_OPEN_FOLDER;
          break;
        case content::FileChooserParams::Save:
          mode = FILE_DIALOG_SAVE;
          break;
        default:
          NOTREACHED();
          break;
      }

      if (params.overwriteprompt)
        mode |= FILE_DIALOG_OVERWRITEPROMPT_FLAG;
      if (params.hidereadonly)
        mode |= FILE_DIALOG_HIDEREADONLY_FLAG;

      std::vector<base::string16>::const_iterator it;

      std::vector<CefString> accept_filters;
      it = params.accept_types.begin();
      for (; it != params.accept_types.end(); ++it)
        accept_filters.push_back(*it);

      CefRefPtr<CefFileDialogCallbackImpl> callbackImpl(
          new CefFileDialogCallbackImpl(callback));
      handled = handler->OnFileDialog(
          browser_,
          static_cast<cef_file_dialog_mode_t>(mode),
          params.title,
          params.default_file_name.value(),
          accept_filters,
          params.selected_accept_filter,
          callbackImpl.get());
      if (!handled) {
        if (callbackImpl->IsConnected()) {
          callbackImpl->Disconnect();
        } else {
          // User executed the callback even though they returned false.
          NOTREACHED();
          handled = true;
        }
      }
    }
  }

  if (!handled) {
    if (runner_.get()) {
      runner_->Run(browser_, params, callback);
    } else {
      LOG(WARNING) << "No file dialog runner available for this platform";
      callback.Run(0, std::vector<base::FilePath>());
    }
  }
}

void CefFileDialogManager::OnRunFileChooserCallback(
    const CefFileDialogRunner::RunFileChooserCallback& callback,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  CEF_REQUIRE_UIT();

  Cleanup();

  // Execute the callback asynchronously.
  CEF_POST_TASK(CEF_UIT,
      base::Bind(callback, selected_accept_filter, file_paths));
}

void CefFileDialogManager::OnRunFileChooserUploadFolderDelegateCallback(
    const content::FileChooserParams::Mode mode,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  CEF_REQUIRE_UIT();
  DCHECK_EQ(mode, content::FileChooserParams::UploadFolder);

  if (file_paths.size() == 0) {
    // Client canceled the file chooser.
    OnRunFileChooserDelegateCallback(mode, selected_accept_filter, file_paths);
  } else {
    lister_.reset(new net::DirectoryLister(
        file_paths[0],
        net::DirectoryLister::NO_SORT_RECURSIVE,
        new UploadFolderHelper(
            base::Bind(&CefFileDialogManager::OnRunFileChooserDelegateCallback,
                       weak_ptr_factory_.GetWeakPtr(), mode))));
    lister_->Start();
  }
}

void CefFileDialogManager::OnRunFileChooserDelegateCallback(
    content::FileChooserParams::Mode mode,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  CEF_REQUIRE_UIT();

  // Convert FilePath list to SelectedFileInfo list.
  std::vector<content::FileChooserFileInfo> selected_files;
  for (size_t i = 0; i < file_paths.size(); ++i) {
    content::FileChooserFileInfo info;
    info.file_path = file_paths[i];
    selected_files.push_back(info);
  }

  // Notify our RenderViewHost in all cases.
  if (render_frame_host_)
    render_frame_host_->FilesSelectedInChooser(selected_files, mode);

  Cleanup();
}

void CefFileDialogManager::Cleanup() {
  if (lister_)
    lister_.reset();

  render_frame_host_ = nullptr;
  file_chooser_pending_ = false;
}

void CefFileDialogManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == render_frame_host_)
    render_frame_host_ = nullptr;
}
