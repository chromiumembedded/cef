// Copyright 2017 the Chromium Embedded Framework Authors. Portions copyright
// 2014 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/extensions/extension_function_details.h"

#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/thread_util.h"

#include "base/task_scheduler/post_task.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/error_utils.h"

using content::WebContents;
using content::RenderViewHost;

namespace extensions {

namespace keys = extensions::tabs_constants;

namespace {

class CefGetExtensionLoadFileCallbackImpl
    : public CefGetExtensionResourceCallback {
 public:
  CefGetExtensionLoadFileCallbackImpl(
      const std::string& file,
      CefExtensionFunctionDetails::LoadFileCallback callback)
      : file_(file), callback_(std::move(callback)) {}

  ~CefGetExtensionLoadFileCallbackImpl() {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        RunNow(file_, std::move(callback_), nullptr);
      } else {
        CEF_POST_TASK(
            CEF_UIT,
            base::BindOnce(&CefGetExtensionLoadFileCallbackImpl::RunNow, file_,
                           base::Passed(std::move(callback_)), nullptr));
      }
    }
  }

  void Continue(CefRefPtr<CefStreamReader> stream) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        // Always continue asynchronously.
        CEF_POST_TASK(
            CEF_UIT,
            base::BindOnce(&CefGetExtensionLoadFileCallbackImpl::RunNow, file_,
                           base::Passed(std::move(callback_)), stream));
      }
    } else {
      CEF_POST_TASK(CEF_UIT, base::BindOnce(
                                 &CefGetExtensionLoadFileCallbackImpl::Continue,
                                 this, stream));
    }
  }

  void Cancel() override { Continue(nullptr); }

  void Disconnect() { callback_.Reset(); }

 private:
  static void RunNow(const std::string& file,
                     CefExtensionFunctionDetails::LoadFileCallback callback,
                     CefRefPtr<CefStreamReader> stream) {
    CEF_REQUIRE_UIT();

    if (!stream) {
      std::move(callback).Run(nullptr);
      return;
    }

    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(LoadFileFromStream, file, stream), std::move(callback));
  }

  static std::unique_ptr<std::string> LoadFileFromStream(
      const std::string& file,
      CefRefPtr<CefStreamReader> stream) {
    base::ThreadRestrictions::AssertIOAllowed();

    // Move to the end of the stream.
    stream->Seek(0, SEEK_END);
    const int64 size = stream->Tell();
    if (size == 0) {
      LOG(WARNING) << "Extension resource " << file << " is empty.";
      return nullptr;
    }

    std::unique_ptr<std::string> result(new std::string());
    result->resize(size);

    // Move to the beginning of the stream.
    stream->Seek(0, SEEK_SET);

    // Read all stream contents into the string.
    int64 read, offset = 0;
    do {
      read =
          static_cast<int>(stream->Read(&(*result)[offset], 1, size - offset));
      offset += read;
    } while (read > 0 && offset < size);

    if (offset != size) {
      LOG(WARNING) << "Extension resource " << file << " read failed; expected "
                   << size << ", got " << offset << " bytes.";
      return nullptr;
    }

    return result;
  }

  const std::string file_;
  CefExtensionFunctionDetails::LoadFileCallback callback_;

  IMPLEMENT_REFCOUNTING(CefGetExtensionLoadFileCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefGetExtensionLoadFileCallbackImpl);
};

}  // namespace

CefExtensionFunctionDetails::CefExtensionFunctionDetails(
    UIThreadExtensionFunction* function)
    : function_(function) {}

CefExtensionFunctionDetails::~CefExtensionFunctionDetails() {}

Profile* CefExtensionFunctionDetails::GetProfile() const {
  return Profile::FromBrowserContext(function_->browser_context());
}

CefRefPtr<CefBrowserHostImpl> CefExtensionFunctionDetails::GetSenderBrowser()
    const {
  content::WebContents* web_contents = function_->GetSenderWebContents();
  if (web_contents)
    return CefBrowserHostImpl::GetBrowserForContents(web_contents);
  return nullptr;
}

CefRefPtr<CefBrowserHostImpl> CefExtensionFunctionDetails::GetCurrentBrowser()
    const {
  // Start with the browser hosting the extension.
  CefRefPtr<CefBrowserHostImpl> browser = GetSenderBrowser();
  if (browser && browser->client()) {
    CefRefPtr<CefExtensionHandler> handler = GetCefExtension()->GetHandler();
    if (handler) {
      // Give the handler an opportunity to specify a different browser.
      CefRefPtr<CefBrowser> active_browser = handler->GetActiveBrowser(
          GetCefExtension(), browser.get(), function_->include_incognito());
      if (active_browser && active_browser != browser) {
        CefRefPtr<CefBrowserHostImpl> active_browser_impl =
            static_cast<CefBrowserHostImpl*>(active_browser.get());

        // Make sure we're operating in the same BrowserContextImpl.
        if (CefBrowserContextImpl::GetForContext(
                browser->GetBrowserContext()) ==
            CefBrowserContextImpl::GetForContext(
                active_browser_impl->GetBrowserContext())) {
          browser = active_browser_impl;
        } else {
          LOG(WARNING) << "Browser with tabId "
                       << active_browser->GetIdentifier()
                       << " cannot be accessed because is uses a different "
                          "CefRequestContext";
        }
      }
    }
  }

  // May be null during startup/shutdown.
  return browser;
}

bool CefExtensionFunctionDetails::CanAccessBrowser(
    CefRefPtr<CefBrowserHostImpl> target) const {
  DCHECK(target);

  // Start with the browser hosting the extension.
  CefRefPtr<CefBrowserHostImpl> browser = GetSenderBrowser();
  if (browser == target) {
    // A sender can always access itself.
    return true;
  }

  if (browser && browser->client()) {
    CefRefPtr<CefExtensionHandler> handler = GetCefExtension()->GetHandler();
    if (handler) {
      return handler->CanAccessBrowser(GetCefExtension(), browser.get(),
                                       function_->include_incognito(), target);
    }
  }

  // Default to allowing access.
  return true;
}

CefRefPtr<CefBrowserHostImpl>
CefExtensionFunctionDetails::GetBrowserForTabIdFirstTime(
    int tab_id,
    std::string* error_message) const {
  DCHECK_GE(tab_id, -1);
  DCHECK(!get_browser_called_first_time_);
  get_browser_called_first_time_ = true;

  CefRefPtr<CefBrowserHostImpl> browser;

  if (tab_id != -1) {
    // May be an invalid tabId or in the wrong BrowserContext.
    browser = GetBrowserForTabId(tab_id, function_->browser_context());
    if (!browser || !browser->web_contents() || !CanAccessBrowser(browser)) {
      if (error_message) {
        *error_message = ErrorUtils::FormatErrorMessage(
            keys::kTabNotFoundError, base::IntToString(tab_id));
      }
      return nullptr;
    }
  } else {
    // May return NULL during shutdown.
    browser = GetCurrentBrowser();
    if (!browser || !browser->web_contents()) {
      if (error_message) {
        *error_message = keys::kNoCurrentWindowError;
      }
      return nullptr;
    }
  }

  return browser;
}

CefRefPtr<CefBrowserHostImpl>
CefExtensionFunctionDetails::GetBrowserForTabIdAgain(
    int tab_id,
    std::string* error_message) const {
  DCHECK_GE(tab_id, 0);
  DCHECK(get_browser_called_first_time_);

  // May return NULL during shutdown.
  CefRefPtr<CefBrowserHostImpl> browser =
      GetBrowserForTabId(tab_id, function_->browser_context());
  if (!browser || !browser->web_contents()) {
    if (error_message) {
      *error_message = ErrorUtils::FormatErrorMessage(
          keys::kTabNotFoundError, base::IntToString(tab_id));
    }
  }
  return browser;
}

bool CefExtensionFunctionDetails::LoadFile(const std::string& file,
                                           LoadFileCallback callback) const {
  // Start with the browser hosting the extension.
  CefRefPtr<CefBrowserHostImpl> browser = GetSenderBrowser();
  if (browser && browser->client()) {
    CefRefPtr<CefExtensionHandler> handler = GetCefExtension()->GetHandler();
    if (handler) {
      CefRefPtr<CefGetExtensionLoadFileCallbackImpl> cef_callback(
          new CefGetExtensionLoadFileCallbackImpl(file, std::move(callback)));
      if (handler->GetExtensionResource(GetCefExtension(), browser.get(), file,
                                        cef_callback)) {
        return true;
      }
      cef_callback->Disconnect();
    }
  }

  return false;
}

CefRefPtr<CefExtension> CefExtensionFunctionDetails::GetCefExtension() const {
  if (!cef_extension_) {
    cef_extension_ =
        static_cast<CefBrowserContext*>(function_->browser_context())
            ->extension_system()
            ->GetExtension(function_->extension_id());
    DCHECK(cef_extension_);
  }
  return cef_extension_;
}

}  // namespace extensions
