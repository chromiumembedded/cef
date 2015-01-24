// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/javascript_dialog.h"
#include "libcef/browser/javascript_dialog_manager.h"

CefJavaScriptDialog::CefJavaScriptDialog(
    CefJavaScriptDialogManager* creator,
    content::JavaScriptMessageType message_type,
    const base::string16& display_url,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    const content::JavaScriptDialogManager::DialogClosedCallback& callback)
    : creator_(creator),
      callback_(callback) {
  NOTIMPLEMENTED();
  callback_.Run(false, base::string16());
  creator_->DialogClosed(this);
}

CefJavaScriptDialog::~CefJavaScriptDialog() {
}

void CefJavaScriptDialog::Cancel() {
}

