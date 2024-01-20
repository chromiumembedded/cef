// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/api/file_system/cef_file_system_delegate.h"

#include "apps/saved_files_service.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/browser/extensions/api/file_system/file_entry_picker.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/api/file_system.h"
#include "extensions/common/extension.h"

namespace extensions::cef {

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

  // The file picker will hold a reference to the ExtensionFunction
  // instance, preventing its destruction (and subsequent sending of the
  // function response) until the user has selected a file or cancelled the
  // picker. At that point, the picker will delete itself, which will also free
  // the function instance.
  new FileEntryPicker(web_contents, default_path, *file_types, type,
                      std::move(files_selected_callback),
                      std::move(file_selection_canceled_callback));
  return true;
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
  if (accept_type == "image/*") {
    return IDS_IMAGE_FILES;
  }
  if (accept_type == "audio/*") {
    return IDS_AUDIO_FILES;
  }
  if (accept_type == "video/*") {
    return IDS_VIDEO_FILES;
  }
  return 0;
}

SavedFilesServiceInterface* CefFileSystemDelegate::GetSavedFilesService(
    content::BrowserContext* browser_context) {
  return apps::SavedFilesService::Get(browser_context);
}

}  // namespace extensions::cef
