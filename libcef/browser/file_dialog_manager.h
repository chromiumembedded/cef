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
class WebContents;
}

namespace net {
class DirectoryLister;
}

class CefBrowserHostImpl;

class CefFileDialogManager : public content::WebContentsObserver {
 public:
  // |runner| may be NULL if the platform doesn't implement dialogs.
  CefFileDialogManager(
      CefBrowserHostImpl* browser,
      std::unique_ptr<CefFileDialogRunner> runner);
  ~CefFileDialogManager() override;

  // Delete the runner to free any platform constructs.
  void Destroy();

  // Called from CefBrowserHostImpl::RunFileChooser.
  // See CefBrowserHost::RunFileDialog documentation.
  void RunFileDialog(
      cef_file_dialog_mode_t mode,
      const CefString& title,
      const CefString& default_file_path,
      const std::vector<CefString>& accept_filters,
      int selected_accept_filter,
      CefRefPtr<CefRunFileDialogCallback> callback);

  // Called from CefBrowserHostImpl::RunFileChooser.
  // See WebContentsDelegate::RunFileChooser documentation.
  void RunFileChooser(
      content::RenderFrameHost* render_frame_host,
      const content::FileChooserParams& params);

  // Run the file chooser dialog specified by |params|. Only a single dialog may
  // be pending at any given time. |callback| will be executed asynchronously
  // after the dialog is dismissed or if another dialog is already pending.
  void RunFileChooser(
      const CefFileDialogRunner::FileChooserParams& params,
      const CefFileDialogRunner::RunFileChooserCallback& callback);

 private:
  void RunFileChooserInternal(
      content::RenderFrameHost* render_frame_host,
      const CefFileDialogRunner::FileChooserParams& params,
      const CefFileDialogRunner::RunFileChooserCallback& callback);

  // Used with the RunFileChooser variant where the caller specifies a callback
  // (no associated RenderFrameHost).
  void OnRunFileChooserCallback(
      const CefFileDialogRunner::RunFileChooserCallback& callback,
      int selected_accept_filter,
      const std::vector<base::FilePath>& file_paths);

  // Used with WebContentsDelegate::RunFileChooser when mode is
  // content::FileChooserParams::UploadFolder.
  void OnRunFileChooserUploadFolderDelegateCallback(
      const content::FileChooserParams::Mode mode,
      int selected_accept_filter,
      const std::vector<base::FilePath>& file_paths);

  // Used with WebContentsDelegate::RunFileChooser to notify the
  // RenderFrameHost.
  void OnRunFileChooserDelegateCallback(
      content::FileChooserParams::Mode mode,
      int selected_accept_filter,
      const std::vector<base::FilePath>& file_paths);

  // Clean up state associated with the last run.
  void Cleanup();

  // WebContentsObserver methods:
  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override;

  // CefBrowserHostImpl pointer is guaranteed to outlive this object.
  CefBrowserHostImpl* browser_;

  std::unique_ptr<CefFileDialogRunner> runner_;

  // True if a file chooser is currently pending.
  bool file_chooser_pending_;

  // RenderFrameHost associated with the pending file chooser. May be nullptr.
  content::RenderFrameHost* render_frame_host_;

  // Used for asynchronously listing directory contents.
  std::unique_ptr<net::DirectoryLister> lister_;

  // Must be the last member.
  base::WeakPtrFactory<CefFileDialogManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefFileDialogManager);
};

#endif  // CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_
