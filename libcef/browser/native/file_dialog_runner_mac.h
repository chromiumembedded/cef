// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_FILE_DIALOG_RUNNER_MAC_H_
#define CEF_LIBCEF_BROWSER_NATIVE_FILE_DIALOG_RUNNER_MAC_H_
#pragma once

#include "libcef/browser/file_dialog_runner.h"

class CefFileDialogRunnerMac : public CefFileDialogRunner {
 public:
  CefFileDialogRunnerMac();

  // CefFileDialogRunner methods:
  void Run(CefBrowserHostImpl* browser,
           const FileChooserParams& params,
           RunFileChooserCallback callback) override;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_FILE_DIALOG_RUNNER_MAC_H_
