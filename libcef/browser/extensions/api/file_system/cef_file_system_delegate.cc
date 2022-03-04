// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/api/file_system/cef_file_system_delegate.h"

#include "libcef/browser/alloy/alloy_dialog_util.h"

#include "apps/saved_files_service.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/api/file_system.h"
#include "extensions/common/extension.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using blink::mojom::FileChooserParams;

namespace extensions {
namespace cef {

CefFileSystemDelegate::CefFileSystemDelegate() = default;

CefFileSystemDelegate::~CefFileSystemDelegate() = default;

base::FilePath CefFileSystemDelegate::GetDefaultDirectory() {
  return base::FilePath();
}

base::FilePath CefFileSystemDelegate::GetManagedSaveAsDirectory(
    content::BrowserContext* browser_context,
    const Extension& extension) {
  return base::FilePath();
}

bool CefFileSystemDelegate::ShowSelectFileDialog(
    scoped_refptr<ExtensionFunction> extension_function,
    ui::SelectFileDialog::Type type,
    const base::FilePath& default_path,
    const ui::SelectFileDialog::FileTypeInfo* file_types,
    FileSystemDelegate::FilesSelectedCallback files_selected_callback,
    base::OnceClosure file_selection_canceled_callback) {
  auto web_contents = extension_function->GetSenderWebContents();
  if (!web_contents) {
    return false;
  }

  absl::optional<FileChooserParams::Mode> mode;
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
      return false;
  }

  FileChooserParams params;
  params.mode = *mode;
  params.default_file_name = default_path;
  if (file_types) {
    // A list of allowed extensions. For example, it might be
    //   { { "htm", "html" }, { "txt" } }
    for (auto& vec : file_types->extensions) {
      for (auto& ext : vec) {
        params.accept_types.push_back(
            alloy::FilePathTypeToString16(FILE_PATH_LITERAL(".") + ext));
      }
    }
  }

  alloy::RunFileChooser(
      web_contents, params,
      base::BindOnce(&CefFileSystemDelegate::FileDialogDismissed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(files_selected_callback),
                     std::move(file_selection_canceled_callback)));

  return true;
}

void CefFileSystemDelegate::FileDialogDismissed(
    FileSystemDelegate::FilesSelectedCallback files_selected_callback,
    base::OnceClosure file_selection_canceled_callback,
    int selected_accept_filter,
    const std::vector<base::FilePath>& file_paths) {
  if (!file_paths.empty()) {
    std::move(files_selected_callback).Run(file_paths);
  } else {
    std::move(file_selection_canceled_callback).Run();
  }
}

void CefFileSystemDelegate::ConfirmSensitiveDirectoryAccess(
    bool has_write_permission,
    const std::u16string& app_name,
    content::WebContents* web_contents,
    base::OnceClosure on_accept,
    base::OnceClosure on_cancel) {
  NOTIMPLEMENTED();

  // Run the cancel callback by default.
  std::move(on_cancel).Run();
}

// Based on ChromeFileSystemDelegate::GetDescriptionIdForAcceptType.
int CefFileSystemDelegate::GetDescriptionIdForAcceptType(
    const std::string& accept_type) {
  if (accept_type == "image/*")
    return IDS_IMAGE_FILES;
  if (accept_type == "audio/*")
    return IDS_AUDIO_FILES;
  if (accept_type == "video/*")
    return IDS_VIDEO_FILES;
  return 0;
}

SavedFilesServiceInterface* CefFileSystemDelegate::GetSavedFilesService(
    content::BrowserContext* browser_context) {
  return apps::SavedFilesService::Get(browser_context);
}

}  // namespace cef
}  // namespace extensions
