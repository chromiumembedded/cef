// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/javascript_dialog_manager.h"

#include <utility>

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"

namespace {

class CefJSDialogCallbackImpl : public CefJSDialogCallback {
 public:
  using CallbackType = content::JavaScriptDialogManager::DialogClosedCallback;

  explicit CefJSDialogCallbackImpl(CallbackType callback)
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

  [[nodiscard]] CallbackType Disconnect() { return std::move(callback_); }

 private:
  static void CancelNow(CallbackType callback) {
    CEF_REQUIRE_UIT();
    std::move(callback).Run(false, std::u16string());
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefJSDialogCallbackImpl);
};

javascript_dialogs::TabModalDialogManager* GetTabModalDialogManager(
    content::WebContents* web_contents) {
  if (auto* manager =
          javascript_dialogs::TabModalDialogManager::FromWebContents(
              web_contents)) {
    return manager;
  }

  // Try the owner WebContents if the dialog originates from a guest view such
  // as the PDF viewer or Print Preview.
  if (extensions::ExtensionsEnabled()) {
    if (auto* owner_contents =
            extensions::GetOwnerForGuestContents(web_contents)) {
      return javascript_dialogs::TabModalDialogManager::FromWebContents(
          owner_contents);
    }
  }

  return nullptr;
}

}  // namespace

CefJavaScriptDialogManager::CefJavaScriptDialogManager(
    CefBrowserHostBase* browser)
    : browser_(browser), weak_ptr_factory_(this) {}

CefJavaScriptDialogManager::~CefJavaScriptDialogManager() = default;

void CefJavaScriptDialogManager::Destroy() {
  if (handler_) {
    CancelDialogs(nullptr, false);
  }
  if (runner_) {
    runner_.reset();
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
  *did_suppress_message = false;

  const GURL& origin_url = render_frame_host->GetLastCommittedURL();

  // Always call DialogClosed().
  callback =
      base::BindOnce(&CefJavaScriptDialogManager::DialogClosed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  if (auto client = browser_->GetClient()) {
    if (auto handler = client->GetJSDialogHandler()) {
      // If the dialog is handled this will be cleared in DialogClosed().
      handler_ = handler;

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
      if (callback.is_null()) {
        LOG(WARNING)
            << "OnJSDialog should return true when executing the callback";
        return;
      }

      if (*did_suppress_message) {
        // Call OnResetDialogState but don't execute |callback|.
        CancelDialogs(web_contents, /*reset_state=*/true);
        return;
      }

      handler_ = nullptr;
    }
  }

  DCHECK(!handler_);

  if (InitializeRunner()) {
    runner_->Run(browser_, message_type, origin_url, message_text,
                 default_prompt_text, std::move(callback));
    return;
  }

  if (!CanUseChromeDialogs()) {
    // Dismiss the dialog.
    std::move(callback).Run(false, std::u16string());
    return;
  }

  auto manager = GetTabModalDialogManager(web_contents);
  if (!manager) {
    // Dismiss the dialog.
    std::move(callback).Run(false, std::u16string());
    return;
  }

  manager->RunJavaScriptDialog(web_contents, render_frame_host, message_type,
                               message_text, default_prompt_text,
                               std::move(callback), did_suppress_message);
}

void CefJavaScriptDialogManager::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  if (browser_->WillBeDestroyed()) {
    // Currently destroying the browser. Accept the unload without showing
    // the prompt.
    std::move(callback).Run(true, std::u16string());
    return;
  }

  const std::u16string& message_text = u"Is it OK to leave/reload this page?";

  // Always call DialogClosed().
  callback =
      base::BindOnce(&CefJavaScriptDialogManager::DialogClosed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  if (auto client = browser_->GetClient()) {
    if (auto handler = client->GetJSDialogHandler()) {
      // If the dialog is handled this will be cleared in DialogClosed().
      handler_ = handler;

      CefRefPtr<CefJSDialogCallbackImpl> callbackPtr(
          new CefJSDialogCallbackImpl(std::move(callback)));

      // Execute the user callback.
      bool handled = handler->OnBeforeUnloadDialog(
          browser_, message_text, is_reload, callbackPtr.get());
      if (handled) {
        return;
      }

      // |callback| may be null if the user executed it despite returning false.
      callback = callbackPtr->Disconnect();
      if (callback.is_null()) {
        LOG(WARNING) << "OnBeforeUnloadDialog should return true when "
                        "executing the callback";
        return;
      }

      handler_ = nullptr;
    }
  }

  DCHECK(!handler_);

  if (InitializeRunner()) {
    runner_->Run(browser_, content::JAVASCRIPT_DIALOG_TYPE_CONFIRM,
                 /*origin_url=*/GURL(), message_text,
                 /*default_prompt_text=*/std::u16string(), std::move(callback));
    return;
  }

  if (!CanUseChromeDialogs()) {
    // Accept the unload without showing the prompt.
    std::move(callback).Run(true, std::u16string());
    return;
  }

  auto manager = GetTabModalDialogManager(web_contents);
  if (!manager) {
    // Accept the unload without showing the prompt.
    std::move(callback).Run(true, std::u16string());
    return;
  }

  manager->RunBeforeUnloadDialog(web_contents, render_frame_host, is_reload,
                                 std::move(callback));
}

bool CefJavaScriptDialogManager::HandleJavaScriptDialog(
    content::WebContents* web_contents,
    bool accept,
    const std::u16string* prompt_override) {
  if (handler_) {
    DialogClosed(base::NullCallback(), accept,
                 prompt_override ? *prompt_override : std::u16string());
    return true;
  }

  if (runner_) {
    runner_->Handle(accept, prompt_override);
    return true;
  }

  if (!CanUseChromeDialogs()) {
    return true;
  }

  auto manager = GetTabModalDialogManager(web_contents);
  if (!manager) {
    return true;
  }

  return manager->HandleJavaScriptDialog(web_contents, accept, prompt_override);
}

void CefJavaScriptDialogManager::CancelDialogs(
    content::WebContents* web_contents,
    bool reset_state) {
  if (handler_) {
    if (reset_state) {
      handler_->OnResetDialogState(browser_);
    }
    handler_ = nullptr;
    return;
  }

  if (runner_) {
    runner_->Cancel();
    return;
  }

  // Null when called from DialogClosed() or Destroy().
  if (!web_contents) {
    return;
  }

  if (!CanUseChromeDialogs()) {
    return;
  }

  auto manager = GetTabModalDialogManager(web_contents);
  if (!manager) {
    return;
  }

  manager->CancelDialogs(web_contents, reset_state);
}

void CefJavaScriptDialogManager::DialogClosed(
    DialogClosedCallback callback,
    bool success,
    const std::u16string& user_input) {
  if (handler_) {
    handler_->OnDialogClosed(browser_);
    // Call OnResetDialogState.
    CancelDialogs(/*web_contents=*/nullptr, /*reset_state=*/true);
  }

  // Null when called from HandleJavaScriptDialog().
  if (!callback.is_null()) {
    std::move(callback).Run(success, user_input);
  }
}

bool CefJavaScriptDialogManager::InitializeRunner() {
  if (!runner_initialized_) {
    runner_ = browser_->platform_delegate()->CreateJavaScriptDialogRunner();
    runner_initialized_ = true;
  }
  return !!runner_.get();
}

bool CefJavaScriptDialogManager::CanUseChromeDialogs() const {
  if (browser_->IsWindowless() &&
      browser_->GetWindowHandle() == kNullWindowHandle) {
    LOG(ERROR) << "Default dialog implementation requires a parent window "
                  "handle; canceling the JS dialog";
    return false;
  }

  return true;
}
