// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_FILE_DIALOG_RUNNER_H_
#define CEF_LIBCEF_BROWSER_FILE_DIALOG_RUNNER_H_
#pragma once

#include <vector>

#include "base/files/file_path.h"
#include "base/callback.h"
#include "content/public/common/file_chooser_params.h"

class CefBrowserHostImpl;

class CefFileDialogRunner {
 public:
  // Extend content::FileChooserParams with some options unique to CEF.
  struct FileChooserParams : public content::FileChooserParams {
    // 0-based index of the selected value in |accept_types|.
    int selected_accept_filter = 0;

    // True if the Save dialog should prompt before overwriting files.
    bool overwriteprompt = true;

    // True if read-only files should be hidden.
    bool hidereadonly = true;
  };

  // The argument vector will be empty if the dialog was canceled.
  typedef base::Callback<void(int, const std::vector<base::FilePath>&)>
      RunFileChooserCallback;

  // Display the file chooser dialog. Execute |callback| on completion.
  virtual void Run(CefBrowserHostImpl* browser,
                   const FileChooserParams& params,
                   RunFileChooserCallback callback) = 0;

 protected:
  // Allow deletion via scoped_ptr only.
  friend std::default_delete<CefFileDialogRunner>;

  CefFileDialogRunner() {}
  virtual ~CefFileDialogRunner() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CefFileDialogRunner);
};

#endif  // CEF_LIBCEF_BROWSER_FILE_DIALOG_RUNNER_H_
