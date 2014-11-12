// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/url_network_delegate.h"

#include <string>

#include "include/cef_urlrequest.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/url_request_user_data.h"
#include "libcef/common/request_impl.h"

#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

namespace {

class CefAuthCallbackImpl : public CefAuthCallback {
 public:
  CefAuthCallbackImpl(const net::NetworkDelegate::AuthCallback& callback,
                      net::AuthCredentials* credentials)
      : callback_(callback),
        credentials_(credentials) {
  }
  ~CefAuthCallbackImpl() override {
    if (!callback_.is_null()) {
      // The auth callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_IOT()) {
        CancelNow(callback_);
      } else {
        CEF_POST_TASK(CEF_IOT,
            base::Bind(&CefAuthCallbackImpl::CancelNow, callback_));
      }
    }
  }

  void Continue(const CefString& username,
                const CefString& password) override {
    if (CEF_CURRENTLY_ON_IOT()) {
      if (!callback_.is_null()) {
        credentials_->Set(username, password);
        callback_.Run(net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_IOT,
          base::Bind(&CefAuthCallbackImpl::Continue, this, username, password));
    }
  }

  void Cancel() override {
    if (CEF_CURRENTLY_ON_IOT()) {
      if (!callback_.is_null()) {
        CancelNow(callback_);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_IOT, base::Bind(&CefAuthCallbackImpl::Cancel, this));
    }
  }

  void Disconnect() {
    callback_.Reset();
  }

 private:
  static void CancelNow(const net::NetworkDelegate::AuthCallback& callback) {
    CEF_REQUIRE_IOT();
    callback.Run(net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION);
  }

  net::NetworkDelegate::AuthCallback callback_;
  net::AuthCredentials* credentials_;

  IMPLEMENT_REFCOUNTING(CefAuthCallbackImpl);
};

}  // namespace

CefNetworkDelegate::CefNetworkDelegate() {
}

CefNetworkDelegate::~CefNetworkDelegate() {
}

int CefNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForRequest(request);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame = browser->GetFrameForRequest(request);

        GURL old_url = request->url();

        // Populate the request data.
        CefRefPtr<CefRequestImpl> requestPtr(new CefRequestImpl());
        requestPtr->Set(request);

        // Give the client an opportunity to cancel the request.
        if (handler->OnBeforeResourceLoad(browser.get(), frame,
            requestPtr.get())) {
          return net::ERR_ABORTED;
        }

        GURL url = GURL(std::string(requestPtr->GetURL()));
        if (old_url != url)
          new_url ->Swap(&url);

        requestPtr->Get(request);
      }
    }
  }

  return net::OK;
}

net::NetworkDelegate::AuthRequiredResponse CefNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForRequest(request);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame = browser->GetFrameForRequest(request);

        CefRefPtr<CefAuthCallbackImpl> callbackPtr(
            new CefAuthCallbackImpl(callback, credentials));
        if (handler->GetAuthCredentials(browser.get(),
                                        frame,
                                        auth_info.is_proxy,
                                        auth_info.challenger.host(),
                                        auth_info.challenger.port(),
                                        auth_info.realm,
                                        auth_info.scheme,
                                        callbackPtr.get())) {
          return AUTH_REQUIRED_RESPONSE_IO_PENDING;
        } else {
          callbackPtr->Disconnect();
        }
      }
    }
  }

  CefURLRequestUserData* user_data =
      (CefURLRequestUserData*)request->GetUserData(
          CefURLRequestUserData::kUserDataKey);
  if (user_data) {
    CefRefPtr<CefURLRequestClient> client = user_data->GetClient();
    if (client.get()) {
      CefRefPtr<CefAuthCallbackImpl> callbackPtr(
          new CefAuthCallbackImpl(callback, credentials));
      if (client->GetAuthCredentials(auth_info.is_proxy,
                                     auth_info.challenger.host(),
                                     auth_info.challenger.port(),
                                     auth_info.realm,
                                     auth_info.scheme,
                                     callbackPtr.get())) {
        return AUTH_REQUIRED_RESPONSE_IO_PENDING;
      } else {
        callbackPtr->Disconnect();
      }
    }
  }

  return AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool CefNetworkDelegate::OnCanAccessFile(const net::URLRequest& request,
                                         const base::FilePath& path) const {
  return true;
}
