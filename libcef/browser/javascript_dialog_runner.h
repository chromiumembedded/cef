// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_RUNNER_H_
#define CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_RUNNER_H_
#pragma once

#include "base/callback.h"
#include "base/strings/string16.h"
#include "content/public/common/javascript_dialog_type.h"

class CefBrowserHostImpl;

class CefJavaScriptDialogRunner {
 public:
  typedef base::Callback<void(bool /* success */,
                              const base::string16& /* user_input */)>
                                  DialogClosedCallback;

  // Run the dialog. Execute |callback| on completion.
  virtual void Run(
      CefBrowserHostImpl* browser,
      content::JavaScriptDialogType message_type,
      const base::string16& display_url,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const DialogClosedCallback& callback) = 0;

  // Cancel a dialog mid-flight.
  virtual void Cancel() = 0;

 protected:
  // Allow deletion via scoped_ptr only.
  friend std::default_delete<CefJavaScriptDialogRunner>;

  CefJavaScriptDialogRunner() {}
  virtual ~CefJavaScriptDialogRunner() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CefJavaScriptDialogRunner);
};

#endif  // CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_RUNNER_H_
