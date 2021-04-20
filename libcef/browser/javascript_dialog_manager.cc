// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/javascript_dialog_manager.h"

#include <utility>

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/elide_url.h"

namespace {

class CefJSDialogCallbackImpl : public CefJSDialogCallback {
 public:
  using CallbackType = content::JavaScriptDialogManager::DialogClosedCallback;

  CefJSDialogCallbackImpl(CallbackType callback)
      : callback_(std::move(callback)) {}
  ~CefJSDialogCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        CancelNow(std::move(callback_));
      } else {
        CEF_POST_TASK(CEF_UIT,
                      base::BindOnce(&CefJSDialogCallbackImpl::CancelNow,
                                     std::move(callback_)));
      }
    }
  }

  void Continue(bool success, const CefString& user_input) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        std::move(callback_).Run(success, user_input);
      }
    } else {
      CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefJSDialogCallbackImpl::Continue,
                                            this, success, user_input));
    }
  }

  CallbackType Disconnect() WARN_UNUSED_RESULT { return std::move(callback_); }

 private:
  static void CancelNow(CallbackType callback) {
    CEF_REQUIRE_UIT();
    std::move(callback).Run(false, std::u16string());
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefJSDialogCallbackImpl);
};

}  // namespace

CefJavaScriptDialogManager::CefJavaScriptDialogManager(
    AlloyBrowserHostImpl* browser,
    std::unique_ptr<CefJavaScriptDialogRunner> runner)
    : browser_(browser),
      runner_(std::move(runner)),
      dialog_running_(false),
      weak_ptr_factory_(this) {}

CefJavaScriptDialogManager::~CefJavaScriptDialogManager() {}

void CefJavaScriptDialogManager::Destroy() {
  if (runner_.get()) {
    runner_.reset(nullptr);
  }
}

void CefJavaScriptDialogManager::RunJavaScriptDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    content::JavaScriptDialogType message_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  const GURL& origin_url = render_frame_host->GetLastCommittedURL();

  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefJSDialogHandler> handler = client->GetJSDialogHandler();
    if (handler.get()) {
      *did_suppress_message = false;

      CefRefPtr<CefJSDialogCallbackImpl> callbackPtr(
          new CefJSDialogCallbackImpl(std::move(callback)));

      // Execute the user callback.
      bool handled = handler->OnJSDialog(
          browser_, origin_url.spec(),
          static_cast<cef_jsdialog_type_t>(message_type), message_text,
          default_prompt_text, callbackPtr.get(), *did_suppress_message);
      if (handled) {
        // Invalid combination of values. Crash sooner rather than later.
        CHECK(!*did_suppress_message);
        return;
      }

      // |callback| may be null if the user executed it despite returning false.
      callback = callbackPtr->Disconnect();
      if (callback.is_null() || *did_suppress_message)
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

  const std::u16string& display_url =
      url_formatter::FormatUrlForSecurityDisplay(origin_url);

  DCHECK(!callback.is_null());
  runner_->Run(
      browser_, message_type, display_url, message_text, default_prompt_text,
      base::BindOnce(&CefJavaScriptDialogManager::DialogClosed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CefJavaScriptDialogManager::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  if (browser_->destruction_state() >=
      AlloyBrowserHostImpl::DESTRUCTION_STATE_ACCEPTED) {
    // Currently destroying the browser. Accept the unload without showing
    // the prompt.
    std::move(callback).Run(true, std::u16string());
    return;
  }

  const std::u16string& message_text = u"Is it OK to leave/reload this page?";

  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefJSDialogHandler> handler = client->GetJSDialogHandler();
    if (handler.get()) {
      CefRefPtr<CefJSDialogCallbackImpl> callbackPtr(
          new CefJSDialogCallbackImpl(std::move(callback)));

      // Execute the user callback.
      bool handled = handler->OnBeforeUnloadDialog(
          browser_, message_text, is_reload, callbackPtr.get());
      if (handled)
        return;

      // |callback| may be null if the user executed it despite returning false.
      callback = callbackPtr->Disconnect();
      if (callback.is_null())
        return;
    }
  }

  if (!runner_.get() || dialog_running_) {
    if (!runner_.get())
      LOG(WARNING) << "No javascript dialog runner available for this platform";
    // Suppress the dialog if there is no platform runner or if the dialog is
    // currently running.
    std::move(callback).Run(true, std::u16string());
    return;
  }

  dialog_running_ = true;

  DCHECK(!callback.is_null());
  runner_->Run(
      browser_, content::JAVASCRIPT_DIALOG_TYPE_CONFIRM,
      std::u16string(),  // display_url
      message_text,
      std::u16string(),  // default_prompt_text
      base::BindOnce(&CefJavaScriptDialogManager::DialogClosed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
    DialogClosedCallback callback,
    bool success,
    const std::u16string& user_input) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefJSDialogHandler> handler = client->GetJSDialogHandler();
    if (handler.get())
      handler->OnDialogClosed(browser_);
  }

  DCHECK(runner_.get());
  DCHECK(dialog_running_);

  dialog_running_ = false;

  std::move(callback).Run(success, user_input);
}
