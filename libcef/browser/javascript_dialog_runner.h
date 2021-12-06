// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_RUNNER_H_
#define CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_RUNNER_H_
#pragma once

#include "base/callback.h"
#include "content/public/common/javascript_dialog_type.h"

class AlloyBrowserHostImpl;

class CefJavaScriptDialogRunner {
 public:
  CefJavaScriptDialogRunner(const CefJavaScriptDialogRunner&) = delete;
  CefJavaScriptDialogRunner& operator=(const CefJavaScriptDialogRunner&) =
      delete;

  using DialogClosedCallback =
      base::OnceCallback<void(bool /* success */,
                              const std::u16string& /* user_input */)>;

  // Run the dialog. Execute |callback| on completion.
  virtual void Run(AlloyBrowserHostImpl* browser,
                   content::JavaScriptDialogType message_type,
                   const std::u16string& display_url,
                   const std::u16string& message_text,
                   const std::u16string& default_prompt_text,
                   DialogClosedCallback callback) = 0;

  // Cancel a dialog mid-flight.
  virtual void Cancel() = 0;

 protected:
  // Allow deletion via std::unique_ptr only.
  friend std::default_delete<CefJavaScriptDialogRunner>;

  CefJavaScriptDialogRunner() = default;
  virtual ~CefJavaScriptDialogRunner() = default;
};

#endif  // CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_RUNNER_H_
