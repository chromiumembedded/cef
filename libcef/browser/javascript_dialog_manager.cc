// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/javascript_dialog_manager.h"

#include <utility>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/elide_url.h"

namespace {

class CefJSDialogCallbackImpl : public CefJSDialogCallback {
 public:
  CefJSDialogCallbackImpl(
      const content::JavaScriptDialogManager::DialogClosedCallback& callback)
      : callback_(callback) {
  }
  ~CefJSDialogCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        CancelNow(callback_);
      } else {
        CEF_POST_TASK(CEF_UIT,
            base::Bind(&CefJSDialogCallbackImpl::CancelNow, callback_));
      }
    }
  }

  void Continue(bool success,
                const CefString& user_input) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        callback_.Run(success, user_input);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefJSDialogCallbackImpl::Continue, this, success,
              user_input));
    }
  }

  void Disconnect() {
    callback_.Reset();
  }

 private:
  static void CancelNow(
      const content::JavaScriptDialogManager::DialogClosedCallback& callback) {
    CEF_REQUIRE_UIT();
    callback.Run(false, base::string16());
  }

  content::JavaScriptDialogManager::DialogClosedCallback callback_;

  IMPLEMENT_REFCOUNTING(CefJSDialogCallbackImpl);
};

}  // namespace


CefJavaScriptDialogManager::CefJavaScriptDialogManager(
    CefBrowserHostImpl* browser,
    std::unique_ptr<CefJavaScriptDialogRunner> runner)
    : browser_(browser),
      runner_(std::move(runner)),
      dialog_running_(false),
      weak_ptr_factory_(this) {
}

CefJavaScriptDialogManager::~CefJavaScriptDialogManager() {
}

void CefJavaScriptDialogManager::Destroy() {
  if (runner_.get()) {
    DCHECK(!dialog_running_);
    runner_.reset(NULL);
  }
}

void CefJavaScriptDialogManager::RunJavaScriptDialog(
    content::WebContents* web_contents,
    const GURL& origin_url,
    content::JavaScriptDialogType message_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    const DialogClosedCallback& callback,
    bool* did_suppress_message) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefJSDialogHandler> handler = client->GetJSDialogHandler();
    if (handler.get()) {
      *did_suppress_message = false;

      CefRefPtr<CefJSDialogCallbackImpl> callbackPtr(
          new CefJSDialogCallbackImpl(callback));

      // Execute the user callback.
      bool handled = handler->OnJSDialog(browser_, origin_url.spec(),
          static_cast<cef_jsdialog_type_t>(message_type),
          message_text, default_prompt_text, callbackPtr.get(),
          *did_suppress_message);
      if (handled) {
        // Invalid combination of values. Crash sooner rather than later.
        CHECK(!*did_suppress_message);
        return;
      }

      callbackPtr->Disconnect();
      if (*did_suppress_message)
        return;
    }
  }

  *did_suppress_message = false;

  if (!runner_.get() || dialog_running_) {
    // Suppress the dialog if there is no platform runner or if the dialog is
    // currently running.
    if (!runner_.get())
      LOG(WARNING) << "No javascript dialog runner available for this platform";
    *did_suppress_message = true;
    return;
  }

  dialog_running_ = true;

  const base::string16& display_url =
      url_formatter::FormatUrlForSecurityDisplay(origin_url);

  runner_->Run(browser_, message_type, display_url, message_text,
               default_prompt_text,
               base::Bind(&CefJavaScriptDialogManager::DialogClosed,
                          weak_ptr_factory_.GetWeakPtr(), callback));
}

void CefJavaScriptDialogManager::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    bool is_reload,
    const DialogClosedCallback& callback) {
  if (browser_->destruction_state() >=
      CefBrowserHostImpl::DESTRUCTION_STATE_ACCEPTED) {
    // Currently destroying the browser. Accept the unload without showing
    // the prompt.
    callback.Run(true, base::string16());
    return;
  }

  const base::string16& message_text =
      base::ASCIIToUTF16("Is it OK to leave/reload this page?");

  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefJSDialogHandler> handler = client->GetJSDialogHandler();
    if (handler.get()) {
      CefRefPtr<CefJSDialogCallbackImpl> callbackPtr(
          new CefJSDialogCallbackImpl(callback));

      // Execute the user callback.
      bool handled = handler->OnBeforeUnloadDialog(browser_, message_text,
          is_reload, callbackPtr.get());
      if (handled)
        return;

      callbackPtr->Disconnect();
    }
  }

  if (!runner_.get() || dialog_running_) {
    if (!runner_.get())
      LOG(WARNING) << "No javascript dialog runner available for this platform";
    // Suppress the dialog if there is no platform runner or if the dialog is
    // currently running.
    callback.Run(true, base::string16());
    return;
  }

  dialog_running_ = true;

  runner_->Run(browser_,
               content::JAVASCRIPT_DIALOG_TYPE_CONFIRM,
               base::string16(),  // display_url
               message_text,
               base::string16(),  // default_prompt_text
               base::Bind(&CefJavaScriptDialogManager::DialogClosed,
                          weak_ptr_factory_.GetWeakPtr(), callback));
}

void CefJavaScriptDialogManager::CancelDialogs(
    content::WebContents* web_contents,
    bool reset_state) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefJSDialogHandler> handler = client->GetJSDialogHandler();
    if (handler.get()) {
      // Execute the user callback.
      handler->OnResetDialogState(browser_);
    }
  }

  if (runner_.get() && dialog_running_) {
    runner_->Cancel();
    dialog_running_ = false;
  }
}

void CefJavaScriptDialogManager::DialogClosed(
    const DialogClosedCallback& callback,
    bool success,
    const base::string16& user_input) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefJSDialogHandler> handler = client->GetJSDialogHandler();
    if (handler.get())
      handler->OnDialogClosed(browser_);
  }

  DCHECK(runner_.get());
  DCHECK(dialog_running_);

  dialog_running_ = false;

  callback.Run(success, user_input);
}
