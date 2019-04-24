// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net/network_delegate.h"

#include <string>
#include <utility>

#include "include/cef_urlrequest.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/net/cookie_manager_old_impl.h"
#include "libcef/browser/net/net_util.h"
#include "libcef/browser/net/source_stream.h"
#include "libcef/browser/net/url_request_user_data.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/net_service/net_service_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/response_impl.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "chrome/common/net/safe_search_util.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"

namespace {

const void* kAuthCallbackHolderUserDataKey =
    static_cast<const void*>(&kAuthCallbackHolderUserDataKey);

class CefBeforeResourceLoadCallbackImpl : public CefRequestCallback {
 public:
  typedef net::CompletionOnceCallback CallbackType;

  CefBeforeResourceLoadCallbackImpl(CefRefPtr<CefRequestImpl> cef_request,
                                    GURL* new_url,
                                    net::URLRequest* url_request,
                                    bool force_google_safesearch,
                                    CallbackType callback)
      : cef_request_(cef_request),
        new_url_(new_url),
        url_request_(url_request),
        force_google_safesearch_(force_google_safesearch),
        callback_(std::move(callback)) {
    DCHECK(new_url);
    DCHECK(url_request_);

    // Add an association between the URLRequest and this object.
    url_request_->SetUserData(UserDataKey(),
                              base::WrapUnique(new Disconnector(this)));
  }

  ~CefBeforeResourceLoadCallbackImpl() {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_IOT()) {
        RunNow(cef_request_, new_url_, url_request_, std::move(callback_),
               force_google_safesearch_, false);
      } else {
        CEF_POST_TASK(CEF_IOT,
                      base::Bind(&CefBeforeResourceLoadCallbackImpl::RunNow,
                                 cef_request_, new_url_, url_request_,
                                 base::Passed(std::move(callback_)),
                                 force_google_safesearch_, false));
      }
    }
  }

  void Continue(bool allow) override {
    // Always continue asynchronously.
    CEF_POST_TASK(CEF_IOT,
                  base::Bind(&CefBeforeResourceLoadCallbackImpl::ContinueNow,
                             this, allow));
  }

  void Cancel() override { Continue(false); }

  void ContinueNow(bool allow) {
    CEF_REQUIRE_IOT();
    if (!callback_.is_null()) {
      RunNow(cef_request_, new_url_, url_request_, std::move(callback_),
             force_google_safesearch_, allow);
      Disconnect();
    }
  }

 private:
  void Disconnect() {
    CEF_REQUIRE_IOT();
    cef_request_ = nullptr;
    new_url_ = nullptr;
    url_request_ = nullptr;
    callback_.Reset();
  }

  // Used to disconnect the callback when the associated URLRequest is
  // destroyed.
  class Disconnector : public base::SupportsUserData::Data {
   public:
    explicit Disconnector(CefBeforeResourceLoadCallbackImpl* callback)
        : callback_(callback) {}
    ~Disconnector() override {
      if (callback_)
        callback_->Disconnect();
    }

    void Disconnect() { callback_ = NULL; }

   private:
    CefBeforeResourceLoadCallbackImpl* callback_;
  };

  static void RunNow(CefRefPtr<CefRequestImpl> cef_request,
                     GURL* new_url,
                     net::URLRequest* request,
                     CallbackType callback,
                     bool force_google_safesearch,
                     bool allow) {
    CEF_REQUIRE_IOT();

    if (allow) {
      // Update the URLRequest with only the values that have been changed by
      // the client.
      cef_request->Get(request, true);

      if (!!(cef_request->GetChanges() & CefRequestImpl::kChangedUrl)) {
        // If the URL was changed then redirect the request.
        GURL url = GURL(cef_request->GetURL().ToString());
        DCHECK_NE(url, request->url());
        new_url->Swap(&url);
      }
    }

    // Remove the association between the URLRequest and this object.
    Disconnector* disconnector =
        static_cast<Disconnector*>(request->GetUserData(UserDataKey()));
    DCHECK(disconnector);
    disconnector->Disconnect();
    request->RemoveUserData(UserDataKey());

    // Only execute the callback if the request has not been canceled.
    if (request->status().status() != net::URLRequestStatus::CANCELED) {
      if (force_google_safesearch && allow && new_url->is_empty())
        safe_search_util::ForceGoogleSafeSearch(request->url(), new_url);

      std::move(callback).Run(allow ? net::OK : net::ERR_ABORTED);
    }
  }

  static inline void* UserDataKey() { return &kLocatorKey; }

  CefRefPtr<CefRequestImpl> cef_request_;
  const GURL old_url_;
  GURL* new_url_;
  net::URLRequest* url_request_;
  bool force_google_safesearch_;
  CallbackType callback_;

  // The user data key.
  static int kLocatorKey;

  IMPLEMENT_REFCOUNTING(CefBeforeResourceLoadCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefBeforeResourceLoadCallbackImpl);
};

int CefBeforeResourceLoadCallbackImpl::kLocatorKey = 0;

class CefAuthCallbackImpl : public CefAuthCallback {
 public:
  typedef net::NetworkDelegate::AuthCallback CallbackType;

  CefAuthCallbackImpl(CallbackType callback, net::AuthCredentials* credentials)
      : callback_(std::move(callback)), credentials_(credentials) {}
  ~CefAuthCallbackImpl() override {
    if (!callback_.is_null()) {
      // The auth callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_IOT()) {
        CancelNow(std::move(callback_));
      } else {
        CEF_POST_TASK(CEF_IOT, base::Bind(&CefAuthCallbackImpl::CancelNow,
                                          base::Passed(std::move(callback_))));
      }
    }
  }

  void Continue(const CefString& username, const CefString& password) override {
    if (CEF_CURRENTLY_ON_IOT()) {
      if (!callback_.is_null()) {
        credentials_->Set(username, password);
        std::move(callback_).Run(
            net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH);
      }
    } else {
      CEF_POST_TASK(CEF_IOT, base::Bind(&CefAuthCallbackImpl::Continue, this,
                                        username, password));
    }
  }

  void Cancel() override {
    if (CEF_CURRENTLY_ON_IOT()) {
      if (!callback_.is_null()) {
        CancelNow(std::move(callback_));
      }
    } else {
      CEF_POST_TASK(CEF_IOT, base::Bind(&CefAuthCallbackImpl::Cancel, this));
    }
  }

  CallbackType Disconnect() WARN_UNUSED_RESULT { return std::move(callback_); }

 private:
  static void CancelNow(CallbackType callback) {
    CEF_REQUIRE_IOT();
    std::move(callback).Run(
        net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION);
  }

  CallbackType callback_;
  net::AuthCredentials* credentials_;

  IMPLEMENT_REFCOUNTING(CefAuthCallbackImpl);
};

// Invalidates CefAuthCallbackImpl::callback_ if the URLRequest is deleted
// before the callback.
class AuthCallbackHolder : public base::SupportsUserData::Data {
 public:
  explicit AuthCallbackHolder(CefRefPtr<CefAuthCallbackImpl> callback)
      : callback_(callback) {}

  ~AuthCallbackHolder() override { callback_->Disconnect().Reset(); }

 private:
  CefRefPtr<CefAuthCallbackImpl> callback_;
};

}  // namespace

CefNetworkDelegate::CefNetworkDelegate() : force_google_safesearch_(nullptr) {}

CefNetworkDelegate::~CefNetworkDelegate() {}

std::unique_ptr<net::SourceStream> CefNetworkDelegate::CreateSourceStream(
    net::URLRequest* request,
    std::unique_ptr<net::SourceStream> upstream) {
  if (net_util::IsInternalRequest(request))
    return upstream;

  CefRefPtr<CefRequestImpl> requestPtr;
  CefRefPtr<CefBrowser> browser;
  CefRefPtr<CefFrame> frame;
  CefRefPtr<CefResourceRequestHandler> handler =
      net_util::GetResourceRequestHandler(request, requestPtr, browser, frame);
  if (handler) {
    CefRefPtr<CefResponseImpl> responsePtr = new CefResponseImpl();
    responsePtr->Set(request);
    responsePtr->SetReadOnly(true);

    CefRefPtr<CefResponseFilter> cef_filter =
        handler->GetResourceResponseFilter(browser, frame, requestPtr.get(),
                                           responsePtr.get());
    if (cef_filter && cef_filter->InitFilter()) {
      return std::make_unique<CefSourceStream>(cef_filter, std::move(upstream));
    }
  }

  return upstream;
}

int CefNetworkDelegate::OnBeforeURLRequest(net::URLRequest* request,
                                           net::CompletionOnceCallback callback,
                                           GURL* new_url) {
  if (net_util::IsInternalRequest(request))
    return net::OK;

  const bool force_google_safesearch =
      (force_google_safesearch_ && force_google_safesearch_->GetValue());

  CefRefPtr<CefRequestImpl> requestPtr;
  CefRefPtr<CefBrowser> browser;
  CefRefPtr<CefFrame> frame;
  CefRefPtr<CefResourceRequestHandler> handler =
      net_util::GetResourceRequestHandler(request, requestPtr, browser, frame);

  if (handler) {
    // The following callback allows modification of the request object.
    requestPtr->SetReadOnly(false);

    CefBrowserHostImpl* browser_impl =
        static_cast<CefBrowserHostImpl*>(browser.get());
    if (browser_impl) {
      const CefBrowserSettings& browser_settings = browser_impl->settings();
      if (browser_settings.accept_language_list.length > 0) {
        const std::string& accept_language =
            net::HttpUtil::GenerateAcceptLanguageHeader(
                CefString(&browser_settings.accept_language_list));
        request->SetExtraRequestHeaderByName(
            net::HttpRequestHeaders::kAcceptLanguage, accept_language, false);
        requestPtr->SetHeaderByName(net::HttpRequestHeaders::kAcceptLanguage,
                                    accept_language, false);
      }
    }

    requestPtr->SetTrackChanges(true);

    CefRefPtr<CefBeforeResourceLoadCallbackImpl> callbackImpl(
        new CefBeforeResourceLoadCallbackImpl(requestPtr, new_url, request,
                                              force_google_safesearch,
                                              std::move(callback)));

    // Give the client an opportunity to evaluate the request.
    cef_return_value_t retval = handler->OnBeforeResourceLoad(
        browser, frame, requestPtr.get(), callbackImpl.get());
    if (retval == RV_CANCEL) {
      // Cancel the request.
      callbackImpl->Continue(false);
    } else if (retval == RV_CONTINUE) {
      // Continue the request.
      callbackImpl->Continue(true);
    }

    // Continue or cancel the request asynchronously.
    return net::ERR_IO_PENDING;
  }

  if (force_google_safesearch && new_url->is_empty())
    safe_search_util::ForceGoogleSafeSearch(request->url(), new_url);

  // Continue the request immediately.
  return net::OK;
}

void CefNetworkDelegate::OnCompleted(net::URLRequest* request,
                                     bool started,
                                     int net_error) {
  if (net_util::IsInternalRequest(request))
    return;

  if (!started)
    return;

  CefRefPtr<CefRequestImpl> requestPtr;
  CefRefPtr<CefBrowser> browser;
  CefRefPtr<CefFrame> frame;
  CefRefPtr<CefResourceRequestHandler> handler =
      net_util::GetResourceRequestHandler(request, requestPtr, browser, frame);
  if (!handler)
    return;

  CefRefPtr<CefResponseImpl> responsePtr = new CefResponseImpl();
  responsePtr->Set(request);
  responsePtr->SetReadOnly(true);

  cef_urlrequest_status_t status = UR_UNKNOWN;
  switch (request->status().status()) {
    case net::URLRequestStatus::SUCCESS:
      status = UR_SUCCESS;
      break;
    case net::URLRequestStatus::CANCELED:
      status = UR_CANCELED;
      break;
    case net::URLRequestStatus::FAILED:
      status = UR_FAILED;
      break;
    default:
      NOTREACHED();
      break;
  }

  const int64 received_content_length =
      request->received_response_content_length();
  handler->OnResourceLoadComplete(browser, frame, requestPtr.get(),
                                  responsePtr.get(), status,
                                  received_content_length);
}

net::NetworkDelegate::AuthRequiredResponse CefNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    AuthCallback callback,
    net::AuthCredentials* credentials) {
  if (net_util::IsInternalRequest(request))
    return AUTH_REQUIRED_RESPONSE_NO_ACTION;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForRequest(request);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame = browser->GetFrameForRequest(request);

        CefRefPtr<CefAuthCallbackImpl> callbackPtr(
            new CefAuthCallbackImpl(std::move(callback), credentials));
        if (handler->GetAuthCredentials(
                browser.get(), frame, auth_info.is_proxy,
                auth_info.challenger.host(), auth_info.challenger.port(),
                auth_info.realm, auth_info.scheme, callbackPtr.get())) {
          request->SetUserData(
              kAuthCallbackHolderUserDataKey,
              std::make_unique<AuthCallbackHolder>(callbackPtr));
          return AUTH_REQUIRED_RESPONSE_IO_PENDING;
        } else {
          callback = callbackPtr->Disconnect();
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
          new CefAuthCallbackImpl(std::move(callback), credentials));
      if (client->GetAuthCredentials(
              auth_info.is_proxy, auth_info.challenger.host(),
              auth_info.challenger.port(), auth_info.realm, auth_info.scheme,
              callbackPtr.get())) {
        request->SetUserData(kAuthCallbackHolderUserDataKey,
                             std::make_unique<AuthCallbackHolder>(callbackPtr));
        return AUTH_REQUIRED_RESPONSE_IO_PENDING;
      } else {
        callback = callbackPtr->Disconnect();
      }
    }
  }

  return AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool CefNetworkDelegate::OnCanGetCookies(const net::URLRequest& request,
                                         const net::CookieList& cookie_list,
                                         bool allowed_from_caller) {
  if (!allowed_from_caller)
    return false;
  if (net_util::IsInternalRequest(&request))
    return true;

  CefRefPtr<CefRequestImpl> requestPtr;
  CefRefPtr<CefBrowser> browser;
  CefRefPtr<CefFrame> frame;
  CefRefPtr<CefResourceRequestHandler> handler =
      net_util::GetResourceRequestHandler(&request, requestPtr, browser, frame);
  if (!handler)
    return true;

  bool cookie_blocked = false;

  for (const auto& cookie : cookie_list) {
    CefCookie cef_cookie;
    if (!net_service::MakeCefCookie(cookie, cef_cookie))
      continue;
    if (!handler->CanSendCookie(browser, frame, requestPtr.get(), cef_cookie)) {
      if (!cookie_blocked)
        cookie_blocked = true;
    }
  }

  return !cookie_blocked;
}

bool CefNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                        const net::CanonicalCookie& cookie,
                                        net::CookieOptions* options,
                                        bool allowed_from_caller) {
  if (!allowed_from_caller)
    return false;
  if (net_util::IsInternalRequest(&request))
    return true;

  CefRefPtr<CefRequestImpl> requestPtr;
  CefRefPtr<CefBrowser> browser;
  CefRefPtr<CefFrame> frame;
  CefRefPtr<CefResourceRequestHandler> handler =
      net_util::GetResourceRequestHandler(&request, requestPtr, browser, frame);
  if (!handler)
    return true;

  CefCookie cef_cookie;
  if (!net_service::MakeCefCookie(cookie, cef_cookie))
    return true;

  CefRefPtr<CefResponseImpl> responsePtr = new CefResponseImpl();
  responsePtr->Set(&request);
  responsePtr->SetReadOnly(true);

  return handler->CanSaveCookie(browser, frame, requestPtr.get(),
                                responsePtr.get(), cef_cookie);
}

bool CefNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  return true;
}
