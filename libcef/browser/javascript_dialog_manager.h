// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_
#define CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_
#pragma once

#include <memory>
#include <string>

#include "include/cef_jsdialog_handler.h"
#include "libcef/browser/javascript_dialog_runner.h"

#include "base/memory/weak_ptr.h"
#include "content/public/browser/javascript_dialog_manager.h"

class CefBrowserHostBase;

class CefJavaScriptDialogManager : public content::JavaScriptDialogManager {
 public:
  // |runner| may be NULL if the platform doesn't implement dialogs.
  explicit CefJavaScriptDialogManager(CefBrowserHostBase* browser);

  CefJavaScriptDialogManager(const CefJavaScriptDialogManager&) = delete;
  CefJavaScriptDialogManager& operator=(const CefJavaScriptDialogManager&) =
      delete;

  ~CefJavaScriptDialogManager() override;

  // Delete the runner to free any platform constructs.
  void Destroy();

  // JavaScriptDialogManager methods.
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           content::RenderFrameHost* render_frame_host,
                           content::JavaScriptDialogType message_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             content::RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override;
  bool HandleJavaScriptDialog(content::WebContents* web_contents,
                              bool accept,
                              const std::u16string* prompt_override) override;
  void CancelDialogs(content::WebContents* web_contents,
                     bool reset_state) override;

 private:
  // Method executed by the callback passed to CefJavaScriptDialogRunner::Run.
  void DialogClosed(DialogClosedCallback callback,
                    bool success,
                    const std::u16string& user_input);

  bool InitializeRunner();

  bool CanUseChromeDialogs() const;

  // CefBrowserHostBase pointer is guaranteed to outlive this object.
  CefBrowserHostBase* const browser_;

  CefRefPtr<CefJSDialogHandler> handler_;

  std::unique_ptr<CefJavaScriptDialogRunner> runner_;
  bool runner_initialized_ = false;

  // Must be the last member.
  base::WeakPtrFactory<CefJavaScriptDialogManager> weak_ptr_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_MANAGER_H_
