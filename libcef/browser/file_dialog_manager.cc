// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/file_dialog_manager.h"

#include <utility>

#include "include/cef_dialog_handler.h"
#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/thread_util.h"

#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/directory_lister.h"

namespace {

class CefFileDialogCallbackImpl : public CefFileDialogCallback {
 public:
  using CallbackType = CefFileDialogRunner::RunFileChooserCallback;

  explicit CefFileDialogCallbackImpl(CallbackType callback)
      : callback_(std::move(callback)) {}

  ~CefFileDialogCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        CancelNow(std::move(callback_));
      } else {
        CEF_POST_TASK(CEF_UIT,
                      base::BindOnce(&CefFileDialogCallbackImpl::CancelNow,
                                     std::move(callback_)));
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
        std::move(callback_).Run(selected_accept_filter, vec);
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefFileDialogCallbackImpl::Continue, this,
                                   selected_accept_filter, file_paths));
    }
  }

  void Cancel() override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        CancelNow(std::move(callback_));
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefFileDialogCallbackImpl::Cancel, this));
    }
  }

  CallbackType Disconnect() WARN_UNUSED_RESULT { return std::move(callback_); }

 private:
  static void CancelNow(CallbackType callback) {
    CEF_REQUIRE_UIT();
    std::vector<base::FilePath> file_paths;
    std::move(callback).Run(0, file_paths);
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefFileDialogCallbackImpl);
};

void RunFileDialogDismissed(CefRefPtr<CefRunFileDialogCallback> callback,
                            int selected_accept_filter,
                            const std::vector<base::FilePath>& file_paths) {
  std::vector<CefString> paths;
  if (file_paths.size() > 0) {
    for (size_t i = 0; i < file_paths.size(); ++i)
      paths.push_back(file_paths[i].value());
  }
  callback->OnFileDialogDismissed(selected_accept_filter, paths);
}

class UploadFolderHelper
    : public net::DirectoryLister::DirectoryListerDelegate {
 public:
  explicit UploadFolderHelper(
      CefFileDialogRunner::RunFileChooserCallback callback)
      : callback_(std::move(callback)) {}

  UploadFolderHelper(const UploadFolderHelper&) = delete;
  UploadFolderHelper& operator=(const UploadFolderHelper&) = delete;

  ~UploadFolderHelper() override {
    if (!callback_.is_null()) {
      if (CEF_CURRENTLY_ON_UIT()) {
        CancelNow(std::move(callback_));
      } else {
        CEF_POST_TASK(CEF_UIT, base::BindOnce(&UploadFolderHelper::CancelNow,
                                              std::move(callback_)));
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
      std::move(callback_).Run(0, select_files_);
    }
  }

 private:
  static void CancelNow(CefFileDialogRunner::RunFileChooserCallback callback) {
    CEF_REQUIRE_UIT();
    std::vector<base::FilePath> file_paths;
    std::move(callback).Run(0, file_paths);
  }

  CefFileDialogRunner::RunFileChooserCallback callback_;
  std::vector<base::FilePath> select_files_;
};

}  // namespace

CefFileDialogManager::CefFileDialogManager(
    AlloyBrowserHostImpl* browser,
    std::unique_ptr<CefFileDialogRunner> runner)
    : browser_(browser),
      runner_(std::move(runner)),
      file_chooser_pending_(false),
      weak_ptr_factory_(this) {}

CefFileDialogManager::~CefFileDialogManager() {}

void CefFileDialogManager::Destroy() {
  DCHECK(!file_chooser_pending_);
  runner_.reset(nullptr);
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
      params.mode = blink::mojom::FileChooserParams::Mode::kOpen;
      break;
    case FILE_DIALOG_OPEN_MULTIPLE:
      params.mode = blink::mojom::FileChooserParams::Mode::kOpenMultiple;
      break;
    case FILE_DIALOG_OPEN_FOLDER:
      params.mode = blink::mojom::FileChooserParams::Mode::kUploadFolder;
      break;
    case FILE_DIALOG_SAVE:
      params.mode = blink::mojom::FileChooserParams::Mode::kSave;
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

  RunFileChooser(params, base::BindOnce(RunFileDialogDismissed, callback));
}

void CefFileDialogManager::RunFileChooser(
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  CEF_REQUIRE_UIT();

  CefFileDialogRunner::FileChooserParams cef_params;
  static_cast<blink::mojom::FileChooserParams&>(cef_params) = params;

  CefFileDialogRunner::RunFileChooserCallback callback;
  if (params.mode == blink::mojom::FileChooserParams::Mode::kUploadFolder) {
    callback = base::BindOnce(
        &CefFileDialogManager::OnRunFileChooserUploadFolderDelegateCallback,
        weak_ptr_factory_.GetWeakPtr(), params.mode, listener);
  } else {
    callback =
        base::BindOnce(&CefFileDialogManager::OnRunFileChooserDelegateCallback,
                       weak_ptr_factory_.GetWeakPtr(), params.mode, listener);
  }

  RunFileChooserInternal(cef_params, std::move(callback));
}

void CefFileDialogManager::RunFileChooser(
    const CefFileDialogRunner::FileChooserParams& params,
    CefFileDialogRunner::RunFileChooserCallback callback) {
  CefFileDialogRunner::RunFileChooserCallback host_callback =
      base::BindOnce(&CefFileDialogManager::OnRunFileChooserCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  RunFileChooserInternal(params, std::move(host_callback));
}

void CefFileDialogManager::RunFileChooserInternal(
    const CefFileDialogRunner::FileChooserParams& params,
    CefFileDialogRunner::RunFileChooserCallback callback) {
  CEF_REQUIRE_UIT();

  if (file_chooser_pending_) {
    // Dismiss the new dialog immediately.
    std::move(callback).Run(0, std::vector<base::FilePath>());
    return;
  }

  file_chooser_pending_ = true;

  bool handled = false;

  if (browser_->client().get()) {
    CefRefPtr<CefDialogHandler> handler =
        browser_->client()->GetDialogHandler();
    if (handler.get()) {
      int mode = FILE_DIALOG_OPEN;
      switch (params.mode) {
        case blink::mojom::FileChooserParams::Mode::kOpen:
          mode = FILE_DIALOG_OPEN;
          break;
        case blink::mojom::FileChooserParams::Mode::kOpenMultiple:
          mode = FILE_DIALOG_OPEN_MULTIPLE;
          break;
        case blink::mojom::FileChooserParams::Mode::kUploadFolder:
          mode = FILE_DIALOG_OPEN_FOLDER;
          break;
        case blink::mojom::FileChooserParams::Mode::kSave:
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

      std::vector<std::u16string>::const_iterator it;

      std::vector<CefString> accept_filters;
      it = params.accept_types.begin();
      for (; it != params.accept_types.end(); ++it)
        accept_filters.push_back(*it);

      CefRefPtr<CefFileDialogCallbackImpl> callbackImpl(
          new CefFileDialogCallbackImpl(std::move(callback)));
      handled = handler->OnFileDialog(
          browser_, static_cast<cef_file_dialog_mode_t>(mode), params.title,
          params.default_file_name.value(), accept_filters,
          params.selected_accept_filter, callbackImpl.get());
      if (!handled) {
        // May return nullptr if the client has already executed the callback.
        callback = callbackImpl->Disconnect();
      }
    }
  }

  if (!handled && !callback.is_null()) {
    if (runner_.get()) {
      runner_->Run(browser_, params, std::move(callback));
    } else {
      LOG(WARNING) << "No file dialog runner available for this platform";
      std::move(callback).Run(0, std::vector<base::FilePath>());
    }
  }
}

void CefFileDialogManager::OnRunFileChooserCallback(
    CefFileDialogRunner::RunFileChooserCallback callback,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  CEF_REQUIRE_UIT();

  Cleanup();

  // Execute the callback asynchronously.
  CEF_POST_TASK(CEF_UIT, base::BindOnce(std::move(callback),
                                        selected_accept_filter, file_paths));
}

void CefFileDialogManager::OnRunFileChooserUploadFolderDelegateCallback(
    const blink::mojom::FileChooserParams::Mode mode,
    scoped_refptr<content::FileSelectListener> listener,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  CEF_REQUIRE_UIT();
  DCHECK_EQ(mode, blink::mojom::FileChooserParams::Mode::kUploadFolder);

  if (file_paths.size() == 0) {
    // Client canceled the file chooser.
    OnRunFileChooserDelegateCallback(mode, listener, selected_accept_filter,
                                     file_paths);
  } else {
    lister_.reset(new net::DirectoryLister(
        file_paths[0], net::DirectoryLister::NO_SORT_RECURSIVE,
        new UploadFolderHelper(base::BindOnce(
            &CefFileDialogManager::OnRunFileChooserDelegateCallback,
            weak_ptr_factory_.GetWeakPtr(), mode, listener))));
    lister_->Start();
  }
}

void CefFileDialogManager::OnRunFileChooserDelegateCallback(
    blink::mojom::FileChooserParams::Mode mode,
    scoped_refptr<content::FileSelectListener> listener,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  CEF_REQUIRE_UIT();

  base::FilePath base_dir;
  std::vector<blink::mojom::FileChooserFileInfoPtr> selected_files;

  if (!file_paths.empty()) {
    if (mode == blink::mojom::FileChooserParams::Mode::kUploadFolder) {
      base_dir = file_paths[0].DirName();
    }

    // Convert FilePath list to SelectedFileInfo list.
    for (size_t i = 0; i < file_paths.size(); ++i) {
      auto info = blink::mojom::FileChooserFileInfo::NewNativeFile(
          blink::mojom::NativeFileInfo::New(file_paths[i], std::u16string()));
      selected_files.push_back(std::move(info));
    }
  }

  listener->FileSelected(std::move(selected_files), base_dir, mode);

  Cleanup();
}

void CefFileDialogManager::Cleanup() {
  if (lister_)
    lister_.reset();

  file_chooser_pending_ = false;
}
