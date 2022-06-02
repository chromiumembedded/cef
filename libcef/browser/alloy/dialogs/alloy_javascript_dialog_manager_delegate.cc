// Copyright 2022 The Chromium Embedded Framework Authors.
// Portions copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/dialogs/alloy_javascript_dialog_manager_delegate.h"

#include "libcef/browser/browser_host_base.h"

#include "base/logging.h"

namespace {

class AlloyJavaScriptTabModalDialogManagerDelegateDesktop
    : public JavaScriptTabModalDialogManagerDelegateDesktop {
 public:
  explicit AlloyJavaScriptTabModalDialogManagerDelegateDesktop(
      content::WebContents* web_contents)
      : JavaScriptTabModalDialogManagerDelegateDesktop(web_contents),
        web_contents_(web_contents) {}

  AlloyJavaScriptTabModalDialogManagerDelegateDesktop(
      const AlloyJavaScriptTabModalDialogManagerDelegateDesktop&) = delete;
  AlloyJavaScriptTabModalDialogManagerDelegateDesktop& operator=(
      const AlloyJavaScriptTabModalDialogManagerDelegateDesktop&) = delete;

  // javascript_dialogs::TabModalDialogManagerDelegate methods:
  void WillRunDialog() override {}

  void DidCloseDialog() override {}

  void SetTabNeedsAttention(bool attention) override {}

  bool IsWebContentsForemost() override {
    if (auto browser =
            CefBrowserHostBase::GetBrowserForContents(web_contents_)) {
      return browser->IsVisible();
    }
    return false;
  }

  bool IsApp() override { return false; }

 private:
  // The WebContents for the tab over which the dialog will be modal. This may
  // be different from the WebContents that requested the dialog, such as with
  // Chrome app <webview>s.
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace

std::unique_ptr<JavaScriptTabModalDialogManagerDelegateDesktop>
CreateAlloyJavaScriptTabModalDialogManagerDelegateDesktop(
    content::WebContents* web_contents) {
  return std::make_unique<AlloyJavaScriptTabModalDialogManagerDelegateDesktop>(
      web_contents);
}
