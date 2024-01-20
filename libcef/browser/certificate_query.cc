// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/certificate_query.h"

#include "include/cef_request_handler.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/ssl_info_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "content/public/browser/web_contents.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace certificate_query {

namespace {

class CefAllowCertificateErrorCallbackImpl : public CefCallback {
 public:
  using CallbackType = CertificateErrorCallback;

  explicit CefAllowCertificateErrorCallbackImpl(CallbackType callback)
      : callback_(std::move(callback)) {}

  CefAllowCertificateErrorCallbackImpl(
      const CefAllowCertificateErrorCallbackImpl&) = delete;
  CefAllowCertificateErrorCallbackImpl& operator=(
      const CefAllowCertificateErrorCallbackImpl&) = delete;

  ~CefAllowCertificateErrorCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        RunNow(std::move(callback_), false);
      } else {
        CEF_POST_TASK(
            CEF_UIT,
            base::BindOnce(&CefAllowCertificateErrorCallbackImpl::RunNow,
                           std::move(callback_), false));
      }
    }
  }

  void Continue() override { ContinueNow(true); }

  void Cancel() override { ContinueNow(false); }

  [[nodiscard]] CallbackType Disconnect() { return std::move(callback_); }

 private:
  void ContinueNow(bool allow) {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        RunNow(std::move(callback_), allow);
      }
    } else {
      CEF_POST_TASK(
          CEF_UIT,
          base::BindOnce(&CefAllowCertificateErrorCallbackImpl::ContinueNow,
                         this, allow));
    }
  }

  static void RunNow(CallbackType callback, bool allow) {
    CEF_REQUIRE_UIT();
    std::move(callback).Run(
        allow ? content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE
              : content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefAllowCertificateErrorCallbackImpl);
};

}  // namespace

CertificateErrorCallback AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    CertificateErrorCallback callback,
    bool default_disallow) {
  CEF_REQUIRE_UIT();

  if (!is_main_frame_request) {
    // A sub-resource has a certificate error. The user doesn't really
    // have a context for making the right decision, so block the request
    // hard.
    std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
    return base::NullCallback();
  }

  if (auto browser = CefBrowserHostBase::GetBrowserForContents(web_contents)) {
    if (auto client = browser->GetClient()) {
      if (auto handler = client->GetRequestHandler()) {
        CefRefPtr<CefSSLInfo> sslInfo(new CefSSLInfoImpl(ssl_info));
        CefRefPtr<CefAllowCertificateErrorCallbackImpl> callbackImpl(
            new CefAllowCertificateErrorCallbackImpl(std::move(callback)));

        bool proceed = handler->OnCertificateError(
            browser.get(), static_cast<cef_errorcode_t>(cert_error),
            request_url.spec(), sslInfo, callbackImpl.get());
        if (!proceed) {
          callback = callbackImpl->Disconnect();
          LOG_IF(ERROR, callback.is_null())
              << "Should return true from OnCertificateError when executing "
                 "the callback";
        }
      }
    }
  }

  if (!callback.is_null() && default_disallow) {
    std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
    return base::NullCallback();
  }

  return callback;
}

}  // namespace certificate_query
