// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/file_dialog_manager.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/include/cef_dialog_handler.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/thread_util.h"
#include "chrome/browser/file_select_helper.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

using blink::mojom::FileChooserParams;

namespace {

class CefFileDialogCallbackImpl : public CefFileDialogCallback {
 public:
  using CallbackType = CefFileDialogManager::RunFileChooserCallback;

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

  void Continue(const std::vector<CefString>& file_paths) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        std::vector<base::FilePath> vec;
        if (!file_paths.empty()) {
          std::vector<CefString>::const_iterator it = file_paths.begin();
          for (; it != file_paths.end(); ++it) {
            vec.push_back(base::FilePath(*it));
          }
        }
        std::move(callback_).Run(vec);
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefFileDialogCallbackImpl::Continue, this,
                                   file_paths));
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

  [[nodiscard]] CallbackType Disconnect() { return std::move(callback_); }

 private:
  static void CancelNow(CallbackType callback) {
    CEF_REQUIRE_UIT();
    std::vector<base::FilePath> file_paths;
    std::move(callback).Run(file_paths);
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefFileDialogCallbackImpl);
};

void RunFileDialogDismissed(CefRefPtr<CefRunFileDialogCallback> callback,
                            const std::vector<base::FilePath>& file_paths) {
  std::vector<CefString> paths;
  if (file_paths.size() > 0) {
    for (const auto& file_path : file_paths) {
      paths.push_back(file_path.value());
    }
  }
  callback->OnFileDialogDismissed(paths);
}

// Based on net/base/filename_util_internal.cc FilePathToString16().
std::u16string FilePathTypeToString16(const base::FilePath::StringType& str) {
  std::u16string result;
#if BUILDFLAG(IS_WIN)
  result.assign(str.begin(), str.end());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  if (!str.empty()) {
    base::UTF8ToUTF16(str.c_str(), str.size(), &result);
  }
#endif
  return result;
}

FileChooserParams SelectFileToFileChooserParams(
    ui::SelectFileDialog::Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const ui::SelectFileDialog::FileTypeInfo* file_types) {
  FileChooserParams params;

  std::optional<FileChooserParams::Mode> mode;
  switch (type) {
    case ui::SelectFileDialog::Type::SELECT_UPLOAD_FOLDER:
      mode = FileChooserParams::Mode::kUploadFolder;
      break;
    case ui::SelectFileDialog::Type::SELECT_SAVEAS_FILE:
      mode = FileChooserParams::Mode::kSave;
      break;
    case ui::SelectFileDialog::Type::SELECT_OPEN_FILE:
      mode = FileChooserParams::Mode::kOpen;
      break;
    case ui::SelectFileDialog::Type::SELECT_OPEN_MULTI_FILE:
      mode = FileChooserParams::Mode::kOpenMultiple;
      break;
    default:
      NOTIMPLEMENTED();
      return params;
  }

  params.mode = *mode;
  params.title = title;
  params.default_file_name = default_path;

  if (file_types) {
    // |file_types| comes from FileSelectHelper::GetFileTypesFromAcceptType.
    // |extensions| is a list of allowed extensions. For example, it might be
    //   { { "htm", "html" }, { "txt" } }
    for (size_t i = 0; i < file_types->extensions.size(); ++i) {
      if (file_types->extension_mimetypes.size() > i &&
          !file_types->extension_mimetypes[i].empty()) {
        // Use the original mime type.
        params.accept_types.push_back(file_types->extension_mimetypes[i]);
      } else if (file_types->extensions[i].size() == 1) {
        // Use the single file extension. We ignore the "Custom Files" filter
        // which is the only instance of multiple file extensions.
        params.accept_types.push_back(FilePathTypeToString16(
            FILE_PATH_LITERAL(".") + file_types->extensions[i][0]));
      }
    }
  }

  return params;
}

class CefFileSelectListener : public content::FileSelectListener {
 public:
  using CallbackType = CefFileDialogManager::RunFileChooserCallback;

  explicit CefFileSelectListener(CallbackType callback)
      : callback_(std::move(callback)) {}

 private:
  ~CefFileSelectListener() override = default;

  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    FileChooserParams::Mode mode) override {
    std::vector<base::FilePath> paths;
    if (mode == FileChooserParams::Mode::kUploadFolder) {
      if (!base_dir.empty()) {
        paths.push_back(base_dir);
      }
    } else if (!files.empty()) {
      for (auto& file : files) {
        if (file->is_native_file()) {
          paths.push_back(file->get_native_file()->file_path);
        } else {
          NOTIMPLEMENTED();
        }
      }
    }

    std::move(callback_).Run(paths);
  }

  void FileSelectionCanceled() override { std::move(callback_).Run({}); }

  CallbackType callback_;
};

}  // namespace

class CefSelectFileDialogListener : public ui::SelectFileDialog::Listener {
 public:
  CefSelectFileDialogListener(ui::SelectFileDialog::Listener* listener,
                              base::OnceClosure callback)
      : listener_(listener), callback_(std::move(callback)) {}

  CefSelectFileDialogListener(const CefSelectFileDialogListener&) = delete;
  CefSelectFileDialogListener& operator=(const CefSelectFileDialogListener&) =
      delete;

  void Cancel(bool listener_destroyed) {
    if (executing_) {
      // We're likely still on the stack. Do nothing and wait for Destroy().
      return;
    }
    if (listener_destroyed) {
      // Don't execute the listener.
      Destroy();
    } else {
      FileSelectionCanceled();
    }
  }

  ui::SelectFileDialog::Listener* listener() const { return listener_; }

 private:
  ~CefSelectFileDialogListener() override = default;

  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    executing_ = true;
    listener_.ExtractAsDangling()->FileSelected(file, index);
    Destroy();
  }

  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override {
    executing_ = true;
    listener_.ExtractAsDangling()->MultiFilesSelected(files);
    Destroy();
  }

  void FileSelectionCanceled() override {
    executing_ = true;
    listener_.ExtractAsDangling()->FileSelectionCanceled();
    Destroy();
  }

  void Destroy() {
    std::move(callback_).Run();
    delete this;
  }

  raw_ptr<ui::SelectFileDialog::Listener> listener_;
  base::OnceClosure callback_;

  // Used to avoid re-entrancy from Cancel().
  bool executing_ = false;
};

CefFileDialogManager::CefFileDialogManager(CefBrowserHostBase* browser)
    : browser_(browser) {}

CefFileDialogManager::~CefFileDialogManager() = default;

void CefFileDialogManager::Destroy() {
  if (dialog_listener_) {
    // Cancel the listener and delete related objects.
    SelectFileDoneByListenerCallback(/*listener=*/nullptr,
                                     /*listener_destroyed=*/false);
  }
  DCHECK(!dialog_);
  DCHECK(!dialog_listener_);
  DCHECK(active_listeners_.empty());
}

void CefFileDialogManager::RunFileDialog(
    cef_file_dialog_mode_t mode,
    const CefString& title,
    const CefString& default_file_path,
    const std::vector<CefString>& accept_filters,
    CefRefPtr<CefRunFileDialogCallback> callback) {
  DCHECK(callback.get());
  if (!callback.get()) {
    return;
  }

  blink::mojom::FileChooserParams params;
  switch (mode) {
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

  params.title = title;
  if (!default_file_path.empty()) {
    params.default_file_name = base::FilePath(default_file_path);
  }

  if (!accept_filters.empty()) {
    std::vector<CefString>::const_iterator it = accept_filters.begin();
    for (; it != accept_filters.end(); ++it) {
      params.accept_types.push_back(*it);
    }
  }

  RunFileChooser(params, base::BindOnce(RunFileDialogDismissed, callback));
}

void CefFileDialogManager::RunFileChooser(
    const blink::mojom::FileChooserParams& params,
    RunFileChooserCallback callback) {
  CEF_REQUIRE_UIT();

  // Execute the delegate with the most exact version of |params|. If not
  // handled here there will be another call to the delegate from RunSelectFile.
  // It might be better to execute the delegate only the single time here, but
  // we don't currently have sufficient state in RunSelectFile to know that the
  // delegate has already been executed. Also, we haven't retrieved file
  // extension data at this point.
  callback = MaybeRunDelegate(params, Extensions(), Descriptions(),
                              std::move(callback));
  if (callback.is_null()) {
    // The delegate kept the callback.
    return;
  }

  FileChooserParams new_params = params;

  // Make sure we get native files in CefFileSelectListener.
  new_params.need_local_path = true;

  // Requirements of FileSelectHelper.
  if (params.mode != FileChooserParams::Mode::kSave) {
    new_params.default_file_name = base::FilePath();
  } else {
    new_params.default_file_name = new_params.default_file_name.BaseName();
  }

  // FileSelectHelper is usually only used for renderer-initiated dialogs via
  // WebContentsDelegate::RunFileChooser. We choose to use it here instead of
  // calling ui::SelectFileDialog::Create directly because it provides some nice
  // functionality related to default dialog settings and filter list
  // generation. We customize the behavior slightly for non-renderer-initiated
  // dialogs by passing the |run_from_cef=true| flag. FileSelectHelper uses
  // ui::SelectFileDialog::Create internally and that call will be intercepted
  // by CefSelectFileDialogFactory, resulting in call to RunSelectFile below.
  // See related comments on CefSelectFileDialogFactory.
  FileSelectHelper::RunFileChooser(
      browser_->GetWebContents()->GetPrimaryMainFrame(),
      base::MakeRefCounted<CefFileSelectListener>(std::move(callback)),
      new_params, /*run_from_cef=*/true);
}

void CefFileDialogManager::RunSelectFile(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy,
    ui::SelectFileDialog::Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const ui::SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window) {
  CEF_REQUIRE_UIT();

  active_listeners_.insert(listener);

  auto chooser_params =
      SelectFileToFileChooserParams(type, title, default_path, file_types);
  auto callback = base::BindOnce(
      &CefFileDialogManager::SelectFileDoneByDelegateCallback,
      weak_ptr_factory_.GetWeakPtr(), base::UnsafeDangling(listener));
  callback = MaybeRunDelegate(chooser_params, file_types->extensions,
                              file_types->extension_description_overrides,
                              std::move(callback));
  if (callback.is_null()) {
    // The delegate kept the callback.
    return;
  }

  if (dialog_) {
    LOG(ERROR) << "Multiple simultaneous dialogs are not supported; "
                  "canceling the file dialog";
    std::move(callback).Run({});
    return;
  }

#if BUILDFLAG(IS_LINUX)
  // We can't use GtkUi in combination with multi-threaded-message-loop because
  // Chromium's GTK implementation doesn't use GDK threads.
  if (!!CefContext::Get()->settings().multi_threaded_message_loop) {
    LOG(ERROR) << "Default dialog implementation is not available; "
                  "canceling the file dialog";
    std::move(callback).Run({});
    return;
  }
#endif

  // |callback| is no longer used at this point.
  callback.Reset();

  DCHECK(!dialog_listener_);

  // This object will delete itself.
  dialog_listener_ = new CefSelectFileDialogListener(
      listener,
      base::BindOnce(&CefFileDialogManager::SelectFileDoneByListenerCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::UnsafeDangling(listener),
                     /*listener_destroyed=*/true));

  // This call will not be intercepted by CefSelectFileDialogFactory due to the
  // |run_from_cef=true| flag.
  // See related comments on CefSelectFileDialogFactory.
  dialog_ = ui::SelectFileDialog::Create(dialog_listener_, std::move(policy),
                                         /*run_from_cef=*/true);

  // With windowless rendering use the parent handle specified by the client.
  if (browser_->IsWindowless()) {
    DCHECK(!owning_window);
    dialog_->set_owning_widget(browser_->GetWindowHandle());
  }

  dialog_->SelectFile(type, title, default_path, file_types, file_type_index,
                      default_extension, owning_window);
}

void CefFileDialogManager::SelectFileListenerDestroyed(
    ui::SelectFileDialog::Listener* listener) {
  CEF_REQUIRE_UIT();
  DCHECK(listener);

  // This notification will arrive from whomever owns |listener|, so we don't
  // want to execute any |listener| methods after this point.
  if (dialog_listener_) {
    // Cancel the currently active dialog.
    SelectFileDoneByListenerCallback(listener, /*listener_destroyed=*/true);
  } else {
    // Any future SelectFileDoneByDelegateCallback call for |listener| becomes a
    // no-op.
    active_listeners_.erase(listener);
  }
}

CefFileDialogManager::RunFileChooserCallback
CefFileDialogManager::MaybeRunDelegate(
    const blink::mojom::FileChooserParams& params,
    const Extensions& extensions,
    const Descriptions& descriptions,
    RunFileChooserCallback callback) {
  // |extensions| and |descriptions| may be empty, or may contain 1 additional
  // entry for the "Custom Files" filter.
  DCHECK(extensions.empty() || extensions.size() >= params.accept_types.size());
  DCHECK(descriptions.empty() ||
         descriptions.size() >= params.accept_types.size());

  if (auto client = browser_->client()) {
    if (auto handler = browser_->client()->GetDialogHandler()) {
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
          DCHECK(false);
          break;
      }

      std::vector<CefString> accept_filters;
      for (auto& accept_type : params.accept_types) {
        accept_filters.push_back(accept_type);
      }

      std::vector<CefString> accept_extensions;
      std::vector<CefString> accept_descriptions;
      if (extensions.empty()) {
        // We don't know the expansion of mime type values at this time,
        // so only include the single file extensions.
        for (auto& accept_type : params.accept_types) {
          accept_extensions.push_back(
              accept_type.ends_with(u"/*") ? std::u16string() : accept_type);
        }
        // Empty descriptions.
        accept_descriptions.resize(params.accept_types.size());
      } else {
        // There may be 1 additional entry in |extensions| and |descriptions|
        // that we want to ignore (for the "Custom Files" filter).
        for (size_t i = 0; i < params.accept_types.size(); ++i) {
          const auto& extension_list = extensions[i];
          std::u16string ext_str;
          for (auto& ext : extension_list) {
            if (!ext_str.empty()) {
              ext_str += u";";
            }
            ext_str += FilePathTypeToString16(FILE_PATH_LITERAL(".") + ext);
          }
          accept_extensions.push_back(ext_str);
          if (descriptions.size() == extensions.size()) {
            accept_descriptions.push_back(descriptions[i]);
          }
        }
      }

      CefRefPtr<CefFileDialogCallbackImpl> callbackImpl(
          new CefFileDialogCallbackImpl(std::move(callback)));
      const bool handled = handler->OnFileDialog(
          browser_.get(), static_cast<cef_file_dialog_mode_t>(mode),
          params.title, params.default_file_name.value(), accept_filters,
          accept_extensions, accept_descriptions, callbackImpl.get());
      if (!handled) {
        // May return nullptr if the client has already executed the callback.
        callback = callbackImpl->Disconnect();
        LOG_IF(ERROR, callback.is_null())
            << "Should return true from OnFileDialog when executing the "
               "callback";
      }
    }
  }

  return callback;
}

void CefFileDialogManager::SelectFileDoneByDelegateCallback(
    MayBeDangling<ui::SelectFileDialog::Listener> listener,
    const std::vector<base::FilePath>& paths) {
  CEF_REQUIRE_UIT();

  // The listener may already be gone. This can occur if the client holds a
  // RunFileChooserCallback past the call to SelectFileListenerDestroyed().
  if (active_listeners_.find(listener) == active_listeners_.end()) {
    return;
  }

  active_listeners_.erase(listener.get());

  if (paths.empty()) {
    listener->FileSelectionCanceled();
  } else if (paths.size() == 1) {
    listener->FileSelected(ui::SelectedFileInfo(paths[0]), /*index=*/0);
  } else {
    listener->MultiFilesSelected(ui::FilePathListToSelectedFileInfoList(paths));
  }
  // |listener| is likely deleted at this point.
}

void CefFileDialogManager::SelectFileDoneByListenerCallback(
    MayBeDangling<ui::SelectFileDialog::Listener> listener,
    bool listener_destroyed) {
  CEF_REQUIRE_UIT();

  // |listener| will be provided iff |listener_destroyed=true|, as
  // |dialog_listener_->listener()| will return nullptr at this point.
  DCHECK(!listener || listener_destroyed);

  // Avoid re-entrancy of this method. CefSelectFileDialogListener callbacks to
  // the delegated listener may result in an immediate call to
  // SelectFileListenerDestroyed() while |dialog_listener_| is still on the
  // stack, followed by another execution from
  // CefSelectFileDialogListener::Destroy(). Similarly, the below call to
  // Cancel() may trigger another execution from
  // CefSelectFileDialogListener::Destroy().
  if (!dialog_listener_) {
    return;
  }

  DCHECK(dialog_);
  DCHECK(dialog_listener_);

  active_listeners_.erase(listener ? listener.get()
                                   : dialog_listener_->listener());

  // Clear |dialog_listener_| before calling Cancel() to avoid re-entrancy.
  auto dialog_listener = dialog_listener_;
  dialog_listener_ = nullptr;
  dialog_listener->Cancel(listener_destroyed);

  // There should be no further listener callbacks after this call.
  dialog_->ListenerDestroyed();
  dialog_ = nullptr;
}
