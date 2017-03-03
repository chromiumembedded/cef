// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_JAVASCRIPT_DIALOG_RUNNER_MAC_H_
#define CEF_LIBCEF_BROWSER_NATIVE_JAVASCRIPT_DIALOG_RUNNER_MAC_H_
#pragma once

#include "libcef/browser/javascript_dialog_runner.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"

#if __OBJC__
@class CefJavaScriptDialogHelper;
#else
class CefJavaScriptDialogHelper;
#endif  // __OBJC__

class CefJavaScriptDialogRunnerMac : public CefJavaScriptDialogRunner {
 public:
  CefJavaScriptDialogRunnerMac();
  ~CefJavaScriptDialogRunnerMac() override;

  // CefJavaScriptDialogRunner methods:
  void Run(
      CefBrowserHostImpl* browser,
      content::JavaScriptDialogType message_type,
      const base::string16& display_url,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const DialogClosedCallback& callback) override;
  void Cancel() override;

  // Callback from CefJavaScriptDialogHelper when the dialog is closed.
  void DialogClosed(bool success,
                    const base::string16& user_input);

 private:
  DialogClosedCallback callback_;

  base::scoped_nsobject<CefJavaScriptDialogHelper> helper_;

  // Must be the last member.
  base::WeakPtrFactory<CefJavaScriptDialogRunnerMac> weak_ptr_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_JAVASCRIPT_DIALOG_RUNNER_MAC_H_
