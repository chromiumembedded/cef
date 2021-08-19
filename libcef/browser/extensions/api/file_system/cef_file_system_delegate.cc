// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/api/file_system/cef_file_system_delegate.h"

#include "apps/saved_files_service.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "extensions/common/api/file_system.h"
#include "extensions/common/extension.h"

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
  NOTIMPLEMENTED();

  // Run the cancel callback by default.
  std::move(file_selection_canceled_callback).Run();

  // Return true since this isn't a disallowed call, just not implemented.
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

int CefFileSystemDelegate::GetDescriptionIdForAcceptType(
    const std::string& accept_type) {
  NOTIMPLEMENTED();
  return 0;
}

SavedFilesServiceInterface* CefFileSystemDelegate::GetSavedFilesService(
    content::BrowserContext* browser_context) {
  return apps::SavedFilesService::Get(browser_context);
}

}  // namespace cef
}  // namespace extensions
