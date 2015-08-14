// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/javascript_dialog_manager.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/javascript_dialog.h"
#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/net_util.h"

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
    CefBrowserHostImpl* browser)
    : browser_(browser) {
}

CefJavaScriptDialogManager::~CefJavaScriptDialogManager() {
}

void CefJavaScriptDialogManager::RunJavaScriptDialog(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& accept_lang,
    content::JavaScriptMessageType message_type,
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
          accept_lang,
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

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(TOOLKIT_GTK)
  *did_suppress_message = false;

  if (dialog_.get()) {
    // One dialog at a time, please.
    *did_suppress_message = true;
    return;
  }

  base::string16 display_url =
      url_formatter::FormatUrl(origin_url, accept_lang);

  dialog_.reset(new CefJavaScriptDialog(this,
                                        message_type,
                                        display_url,
                                        message_text,
                                        default_prompt_text,
                                        callback));
#else
  // TODO(port): implement CefJavaScriptDialog for other platforms.
  *did_suppress_message = true;
  return;
#endif
}

void CefJavaScriptDialogManager::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    const base::string16& message_text,
    bool is_reload,
    const DialogClosedCallback& callback) {
  if (browser_->destruction_state() >=
      CefBrowserHostImpl::DESTRUCTION_STATE_ACCEPTED) {
    // Currently destroying the browser. Accept the unload without showing
    // the prompt.
    callback.Run(true, base::string16());
    return;
  }

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

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(TOOLKIT_GTK)
  if (dialog_.get()) {
    // Seriously!?
    callback.Run(true, base::string16());
    return;
  }

  base::string16 new_message_text =
      message_text +
      base::ASCIIToUTF16("\n\nIs it OK to leave/reload this page?");

  dialog_.reset(
      new CefJavaScriptDialog(this,
                              content::JAVASCRIPT_MESSAGE_TYPE_CONFIRM,
                              base::string16(),  // display_url
                              new_message_text,
                              base::string16(),  // default_prompt_text
                              callback));
#else
  // TODO(port): implement CefJavaScriptDialog for other platforms.
  callback.Run(true, base::string16());
  return;
#endif
}

void CefJavaScriptDialogManager::CancelActiveAndPendingDialogs(
    content::WebContents* web_contents) {
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefJSDialogHandler> handler = client->GetJSDialogHandler();
    if (handler.get()) {
      // Execute the user callback.
      handler->OnResetDialogState(browser_);
    }
  }

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(TOOLKIT_GTK)
  if (dialog_.get()) {
    dialog_->Cancel();
    dialog_.reset();
  }
#endif
}

void CefJavaScriptDialogManager::ResetDialogState(
    content::WebContents* web_contents) {
}

void CefJavaScriptDialogManager::DialogClosed(CefJavaScriptDialog* dialog) {
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(TOOLKIT_GTK)
  DCHECK_EQ(dialog, dialog_.get());
  dialog_.reset();
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefJSDialogHandler> handler = client->GetJSDialogHandler();
    if (handler.get())
      handler->OnDialogClosed(browser_);
  }
#endif
}
