// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net_service/cookie_manager_impl.h"

#include "libcef/common/net_service/net_service_util.h"
#include "libcef/common/time_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"

using network::mojom::CookieManager;

namespace {

// Do not keep a reference to the CookieManager returned by this method.
CookieManager* GetCookieManager(CefRequestContextImpl* request_context) {
  CEF_REQUIRE_UIT();
  return content::BrowserContext::GetDefaultStoragePartition(
             request_context->GetBrowserContext())
      ->GetCookieManagerForBrowserProcess();
}

// Always execute the callback asynchronously.
void RunAsyncCompletionOnUIThread(CefRefPtr<CefCompletionCallback> callback) {
  if (!callback.get())
    return;
  CEF_POST_TASK(CEF_UIT,
                base::Bind(&CefCompletionCallback::OnComplete, callback.get()));
}

// Always execute the callback asynchronously.
void SetCookieCallbackImpl(CefRefPtr<CefSetCookieCallback> callback,
                           bool success) {
  if (!callback.get())
    return;
  CEF_POST_TASK(CEF_UIT, base::Bind(&CefSetCookieCallback::OnComplete,
                                    callback.get(), success));
}

// Always execute the callback asynchronously.
void DeleteCookiesCallbackImpl(CefRefPtr<CefDeleteCookiesCallback> callback,
                               uint32_t num_deleted) {
  if (!callback.get())
    return;
  CEF_POST_TASK(CEF_UIT, base::Bind(&CefDeleteCookiesCallback::OnComplete,
                                    callback.get(), num_deleted));
}

void ExecuteVisitor(CefRefPtr<CefCookieVisitor> visitor,
                    CefRefPtr<CefRequestContextImpl> request_context,
                    const std::vector<net::CanonicalCookie>& cookies) {
  CEF_REQUIRE_UIT();

  CookieManager* cookie_manager = GetCookieManager(request_context.get());

  int total = cookies.size(), count = 0;
  for (const auto& cc : cookies) {
    CefCookie cookie;
    net_service::MakeCefCookie(cc, cookie);

    bool deleteCookie = false;
    bool keepLooping = visitor->Visit(cookie, count, total, deleteCookie);
    if (deleteCookie) {
      cookie_manager->DeleteCanonicalCookie(
          cc, CookieManager::DeleteCanonicalCookieCallback());
    }
    if (!keepLooping)
      break;
    count++;
  }
}

// Always execute the callback asynchronously.
void GetCookiesCallbackImpl(CefRefPtr<CefCookieVisitor> visitor,
                            CefRefPtr<CefRequestContextImpl> request_context,
                            const std::vector<net::CanonicalCookie>& cookies) {
  CEF_POST_TASK(CEF_UIT,
                base::Bind(&ExecuteVisitor, visitor, request_context, cookies));
}

}  // namespace

CefCookieManagerImpl::CefCookieManagerImpl() {}

void CefCookieManagerImpl::Initialize(
    CefRefPtr<CefRequestContextImpl> request_context,
    CefRefPtr<CefCompletionCallback> callback) {
  DCHECK(request_context);
  DCHECK(!request_context_);
  request_context_ = request_context;
  RunAsyncCompletionOnUIThread(callback);
}

void CefCookieManagerImpl::SetSupportedSchemes(
    const std::vector<CefString>& schemes,
    CefRefPtr<CefCompletionCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::Bind(&CefCookieManagerImpl::SetSupportedSchemes, this,
                             schemes, callback));
    return;
  }

  std::vector<std::string> all_schemes;
  for (const auto& scheme : schemes)
    all_schemes.push_back(scheme);

  // This list should match CookieMonster::kDefaultCookieableSchemes.
  all_schemes.push_back("http");
  all_schemes.push_back("https");
  all_schemes.push_back("ws");
  all_schemes.push_back("wss");

  // This will be forwarded to the CookieMonster that lives in the
  // NetworkService process when the NetworkContext is created via
  // CefContentBrowserClient::CreateNetworkContext.
  request_context_->GetBrowserContext()->set_cookieable_schemes(all_schemes);
  RunAsyncCompletionOnUIThread(callback);
}

bool CefCookieManagerImpl::VisitAllCookies(
    CefRefPtr<CefCookieVisitor> visitor) {
  if (!visitor.get())
    return false;

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::Bind(base::IgnoreResult(&CefCookieManagerImpl::VisitAllCookies),
                   this, visitor));
    return true;
  }

  GetCookieManager(request_context_.get())
      ->GetAllCookies(
          base::Bind(&GetCookiesCallbackImpl, visitor, request_context_));
  return true;
}

bool CefCookieManagerImpl::VisitUrlCookies(
    const CefString& url,
    bool includeHttpOnly,
    CefRefPtr<CefCookieVisitor> visitor) {
  if (!visitor.get())
    return false;

  GURL gurl = GURL(url.ToString());
  if (!gurl.is_valid())
    return false;

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::Bind(base::IgnoreResult(&CefCookieManagerImpl::VisitUrlCookies),
                   this, url, includeHttpOnly, visitor));
    return true;
  }

  net::CookieOptions options;
  if (includeHttpOnly)
    options.set_include_httponly();

  GetCookieManager(request_context_.get())
      ->GetCookieList(
          gurl, options,
          base::Bind(&GetCookiesCallbackImpl, visitor, request_context_));
  return true;
}

bool CefCookieManagerImpl::SetCookie(const CefString& url,
                                     const CefCookie& cookie,
                                     CefRefPtr<CefSetCookieCallback> callback) {
  GURL gurl = GURL(url.ToString());
  if (!gurl.is_valid())
    return false;

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::Bind(base::IgnoreResult(&CefCookieManagerImpl::SetCookie), this,
                   url, cookie, callback));
    return true;
  }

  std::string name = CefString(&cookie.name).ToString();
  std::string value = CefString(&cookie.value).ToString();
  std::string domain = CefString(&cookie.domain).ToString();
  std::string path = CefString(&cookie.path).ToString();

  base::Time expiration_time;
  if (cookie.has_expires)
    cef_time_to_basetime(cookie.expires, expiration_time);

  auto canonical_cookie = net::CanonicalCookie::CreateSanitizedCookie(
      gurl, name, value, domain, path,
      base::Time(),  // Creation time.
      expiration_time,
      base::Time(),  // Last access time.
      cookie.secure ? true : false, cookie.httponly ? true : false,
      net::CookieSameSite::DEFAULT_MODE, net::COOKIE_PRIORITY_DEFAULT);

  net::CookieOptions options;
  if (cookie.httponly)
    options.set_include_httponly();

  GetCookieManager(request_context_.get())
      ->SetCanonicalCookie(*canonical_cookie, gurl.scheme(), options,
                           base::Bind(SetCookieCallbackImpl, callback));
  return true;
}

bool CefCookieManagerImpl::DeleteCookies(
    const CefString& url,
    const CefString& cookie_name,
    CefRefPtr<CefDeleteCookiesCallback> callback) {
  // Empty URLs are allowed but not invalid URLs.
  GURL gurl = GURL(url.ToString());
  if (!gurl.is_empty() && !gurl.is_valid())
    return false;

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::Bind(base::IgnoreResult(&CefCookieManagerImpl::DeleteCookies),
                   this, url, cookie_name, callback));
    return true;
  }

  network::mojom::CookieDeletionFilterPtr deletion_filter =
      network::mojom::CookieDeletionFilter::New();

  if (gurl.is_empty()) {
    // Delete all cookies.
  } else if (cookie_name.empty()) {
    // Delete all matching host cookies.
    deletion_filter->host_name = gurl.host();
  } else {
    // Delete all matching host and domain cookies.
    deletion_filter->url = gurl;
    deletion_filter->cookie_name = cookie_name;
  }

  GetCookieManager(request_context_.get())
      ->DeleteCookies(std::move(deletion_filter),
                      base::Bind(DeleteCookiesCallbackImpl, callback));
  return true;
}

bool CefCookieManagerImpl::FlushStore(
    CefRefPtr<CefCompletionCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::Bind(base::IgnoreResult(&CefCookieManagerImpl::FlushStore), this,
                   callback));
    return true;
  }

  GetCookieManager(request_context_.get())
      ->FlushCookieStore(base::Bind(RunAsyncCompletionOnUIThread, callback));
  return true;
}

// CefCookieManager methods ----------------------------------------------------

// static
CefRefPtr<CefCookieManager> CefCookieManager::GetGlobalManager(
    CefRefPtr<CefCompletionCallback> callback) {
  CefRefPtr<CefRequestContext> context = CefRequestContext::GetGlobalContext();
  return context ? context->GetCookieManager(callback) : nullptr;
}
