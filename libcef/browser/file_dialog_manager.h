// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_FILE_DIALOG_MANAGER_H_
#define CEF_LIBCEF_BROWSER_FILE_DIALOG_MANAGER_H_
#pragma once

#include "include/cef_browser.h"
#include "libcef/browser/file_dialog_runner.h"

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class FileSelectListener;
class WebContents;
}  // namespace content

namespace net {
class DirectoryLister;
}

class AlloyBrowserHostImpl;

class CefFileDialogManager {
 public:
  // |runner| may be NULL if the platform doesn't implement dialogs.
  CefFileDialogManager(AlloyBrowserHostImpl* browser,
                       std::unique_ptr<CefFileDialogRunner> runner);

  CefFileDialogManager(const CefFileDialogManager&) = delete;
  CefFileDialogManager& operator=(const CefFileDialogManager&) = delete;

  ~CefFileDialogManager();

  // Delete the runner to free any platform constructs.
  void Destroy();

  // Called from AlloyBrowserHostImpl::RunFileChooser.
  // See CefBrowserHost::RunFileDialog documentation.
  void RunFileDialog(cef_file_dialog_mode_t mode,
                     const CefString& title,
                     const CefString& default_file_path,
                     const std::vector<CefString>& accept_filters,
                     int selected_accept_filter,
                     CefRefPtr<CefRunFileDialogCallback> callback);

  // Called from AlloyBrowserHostImpl::RunFileChooser.
  // See WebContentsDelegate::RunFileChooser documentation.
  void RunFileChooser(scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params);

  // Run the file chooser dialog specified by |params|. Only a single dialog may
  // be pending at any given time. |callback| will be executed asynchronously
  // after the dialog is dismissed or if another dialog is already pending.
  void RunFileChooser(const CefFileDialogRunner::FileChooserParams& params,
                      CefFileDialogRunner::RunFileChooserCallback callback);

 private:
  void RunFileChooserInternal(
      const CefFileDialogRunner::FileChooserParams& params,
      CefFileDialogRunner::RunFileChooserCallback callback);

  // Used with the RunFileChooser variant where the caller specifies a callback
  // (no associated RenderFrameHost).
  void OnRunFileChooserCallback(
      CefFileDialogRunner::RunFileChooserCallback callback,
      int selected_accept_filter,
      const std::vector<base::FilePath>& file_paths);

  // Used with WebContentsDelegate::RunFileChooser when mode is
  // blink::mojom::FileChooserParams::Mode::kUploadFolder.
  void OnRunFileChooserUploadFolderDelegateCallback(
      const blink::mojom::FileChooserParams::Mode mode,
      scoped_refptr<content::FileSelectListener> listener,
      int selected_accept_filter,
      const std::vector<base::FilePath>& file_paths);

  // Used with WebContentsDelegate::RunFileChooser to notify the
  // RenderFrameHost.
  void OnRunFileChooserDelegateCallback(
      blink::mojom::FileChooserParams::Mode mode,
      scoped_refptr<content::FileSelectListener> listener,
      int selected_accept_filter,
      const std::vector<base::FilePath>& file_paths);

  // Clean up state associated with the last run.
  void Cleanup();

  // AlloyBrowserHostImpl pointer is guaranteed to outlive this object.
  AlloyBrowserHostImpl* browser_;

  std::unique_ptr<CefFileDialogRunner> runner_;

  // True if a file chooser is currently pending.
  bool file_chooser_pending_;

  // Used for asynchronously listing directory contents.
  std::unique_ptr<net::DirectoryLister> lister_;

  // Must be the last member.
  base::WeakPtrFactory<CefFileDialogManager> weak_ptr_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_
