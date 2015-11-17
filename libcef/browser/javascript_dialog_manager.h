// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_
#define CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_
#pragma once

#include <string>

#include "libcef/browser/javascript_dialog_runner.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/javascript_dialog_manager.h"

class CefBrowserHostImpl;

class CefJavaScriptDialogManager : public content::JavaScriptDialogManager {
 public:
  // |runner| may be NULL if the platform doesn't implement dialogs.
  CefJavaScriptDialogManager(
      CefBrowserHostImpl* browser,
      scoped_ptr<CefJavaScriptDialogRunner> runner);
  ~CefJavaScriptDialogManager() override;

  // Delete the runner to free any platform constructs.
  void Destroy();

  // JavaScriptDialogManager methods.
  void RunJavaScriptDialog(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& accept_lang,
      content::JavaScriptMessageType message_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const DialogClosedCallback& callback,
      bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(
      content::WebContents* web_contents,
      const base::string16& message_text,
      bool is_reload,
      const DialogClosedCallback& callback) override;
  void CancelActiveAndPendingDialogs(
      content::WebContents* web_contents) override;
  void ResetDialogState(
      content::WebContents* web_contents) override;

 private:
  // Method executed by the callback passed to CefJavaScriptDialogRunner::Run.
  void DialogClosed(const DialogClosedCallback& callback,
                    bool success,
                    const base::string16& user_input);

  // CefBrowserHostImpl pointer is guaranteed to outlive this object.
  CefBrowserHostImpl* browser_;

  scoped_ptr<CefJavaScriptDialogRunner> runner_;

  // True if a dialog is currently running.
  bool dialog_running_;

  // Must be the last member.
  base::WeakPtrFactory<CefJavaScriptDialogManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefJavaScriptDialogManager);
};

#endif  // CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_
