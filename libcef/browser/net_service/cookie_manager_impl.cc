// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net_service/cookie_manager_impl.h"

#include "libcef/common/net_service/net_service_util.h"
#include "libcef/common/time_util.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

using network::mojom::CookieManager;

namespace {

// Do not keep a reference to the object returned by this method.
CefBrowserContext* GetBrowserContext(const CefBrowserContext::Getter& getter) {
  CEF_REQUIRE_UIT();
  DCHECK(!getter.is_null());

  // Will return nullptr if the BrowserContext has been shut down.
  return getter.Run();
}

// Do not keep a reference to the object returned by this method.
CookieManager* GetCookieManager(CefBrowserContext* browser_context) {
  CEF_REQUIRE_UIT();
  return browser_context->AsBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
}

// Always execute the callback asynchronously.
void RunAsyncCompletionOnUIThread(CefRefPtr<CefCompletionCallback> callback) {
  if (!callback.get()) {
    return;
  }
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefCompletionCallback::OnComplete,
                                        callback.get()));
}

// Always execute the callback asynchronously.
void SetCookieCallbackImpl(CefRefPtr<CefSetCookieCallback> callback,
                           net::CookieAccessResult access_result) {
  if (!callback.get()) {
    return;
  }
  const bool is_include = access_result.status.IsInclude();
  if (!is_include) {
    LOG(WARNING) << "SetCookie failed with reason: "
                 << access_result.status.GetDebugString();
  }
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefSetCookieCallback::OnComplete,
                                        callback.get(), is_include));
}

// Always execute the callback asynchronously.
void DeleteCookiesCallbackImpl(CefRefPtr<CefDeleteCookiesCallback> callback,
                               uint32_t num_deleted) {
  if (!callback.get()) {
    return;
  }
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefDeleteCookiesCallback::OnComplete,
                                        callback.get(), num_deleted));
}

void ExecuteVisitor(CefRefPtr<CefCookieVisitor> visitor,
                    const CefBrowserContext::Getter& browser_context_getter,
                    const std::vector<net::CanonicalCookie>& cookies) {
  CEF_REQUIRE_UIT();

  auto browser_context = GetBrowserContext(browser_context_getter);
  if (!browser_context) {
    return;
  }

  auto cookie_manager = GetCookieManager(browser_context);

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
    if (!keepLooping) {
      break;
    }
    count++;
  }
}

// Always execute the callback asynchronously.
void GetAllCookiesCallbackImpl(
    CefRefPtr<CefCookieVisitor> visitor,
    const CefBrowserContext::Getter& browser_context_getter,
    const net::CookieList& cookies) {
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&ExecuteVisitor, visitor,
                                        browser_context_getter, cookies));
}

void GetCookiesCallbackImpl(
    CefRefPtr<CefCookieVisitor> visitor,
    const CefBrowserContext::Getter& browser_context_getter,
    const net::CookieAccessResultList& include_cookies,
    const net::CookieAccessResultList&) {
  net::CookieList cookies;
  for (const auto& status : include_cookies) {
    cookies.push_back(status.cookie);
  }
  GetAllCookiesCallbackImpl(visitor, browser_context_getter, cookies);
}

}  // namespace

CefCookieManagerImpl::CefCookieManagerImpl() = default;

void CefCookieManagerImpl::Initialize(
    CefBrowserContext::Getter browser_context_getter,
    CefRefPtr<CefCompletionCallback> callback) {
  CEF_REQUIRE_UIT();
  DCHECK(!initialized_);
  DCHECK(!browser_context_getter.is_null());
  DCHECK(browser_context_getter_.is_null());
  browser_context_getter_ = browser_context_getter;

  initialized_ = true;
  if (!init_callbacks_.empty()) {
    for (auto& init_callback : init_callbacks_) {
      std::move(init_callback).Run();
    }
    init_callbacks_.clear();
  }

  RunAsyncCompletionOnUIThread(callback);
}

bool CefCookieManagerImpl::VisitAllCookies(
    CefRefPtr<CefCookieVisitor> visitor) {
  if (!visitor.get()) {
    return false;
  }

  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::VisitAllCookiesInternal),
        this, visitor));
    return true;
  }

  return VisitAllCookiesInternal(visitor);
}

bool CefCookieManagerImpl::VisitUrlCookies(
    const CefString& url,
    bool includeHttpOnly,
    CefRefPtr<CefCookieVisitor> visitor) {
  if (!visitor.get()) {
    return false;
  }

  GURL gurl = GURL(url.ToString());
  if (!gurl.is_valid()) {
    return false;
  }

  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::VisitUrlCookiesInternal),
        this, gurl, includeHttpOnly, visitor));
    return true;
  }

  return VisitUrlCookiesInternal(gurl, includeHttpOnly, visitor);
}

bool CefCookieManagerImpl::SetCookie(const CefString& url,
                                     const CefCookie& cookie,
                                     CefRefPtr<CefSetCookieCallback> callback) {
  GURL gurl = GURL(url.ToString());
  if (!gurl.is_valid()) {
    return false;
  }

  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::SetCookieInternal), this,
        gurl, cookie, callback));
    return true;
  }

  return SetCookieInternal(gurl, cookie, callback);
}

bool CefCookieManagerImpl::DeleteCookies(
    const CefString& url,
    const CefString& cookie_name,
    CefRefPtr<CefDeleteCookiesCallback> callback) {
  // Empty URLs are allowed but not invalid URLs.
  GURL gurl = GURL(url.ToString());
  if (!gurl.is_empty() && !gurl.is_valid()) {
    return false;
  }

  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::DeleteCookiesInternal), this,
        gurl, cookie_name, callback));
    return true;
  }

  return DeleteCookiesInternal(gurl, cookie_name, callback);
}

bool CefCookieManagerImpl::FlushStore(
    CefRefPtr<CefCompletionCallback> callback) {
  if (!ValidContext()) {
    StoreOrTriggerInitCallback(base::BindOnce(
        base::IgnoreResult(&CefCookieManagerImpl::FlushStoreInternal), this,
        callback));
    return true;
  }

  return FlushStoreInternal(callback);
}

bool CefCookieManagerImpl::VisitAllCookiesInternal(
    CefRefPtr<CefCookieVisitor> visitor) {
  DCHECK(ValidContext());
  DCHECK(visitor);

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return false;
  }

  GetCookieManager(browser_context)
      ->GetAllCookies(base::BindOnce(&GetAllCookiesCallbackImpl, visitor,
                                     browser_context_getter_));
  return true;
}

bool CefCookieManagerImpl::VisitUrlCookiesInternal(
    const GURL& url,
    bool includeHttpOnly,
    CefRefPtr<CefCookieVisitor> visitor) {
  DCHECK(ValidContext());
  DCHECK(visitor);
  DCHECK(url.is_valid());

  net::CookieOptions options;
  if (includeHttpOnly) {
    options.set_include_httponly();
  }
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return false;
  }

  GetCookieManager(browser_context)
      ->GetCookieList(url, options, net::CookiePartitionKeyCollection(),
                      base::BindOnce(&GetCookiesCallbackImpl, visitor,
                                     browser_context_getter_));
  return true;
}

bool CefCookieManagerImpl::SetCookieInternal(
    const GURL& url,
    const CefCookie& cookie,
    CefRefPtr<CefSetCookieCallback> callback) {
  DCHECK(ValidContext());
  DCHECK(url.is_valid());

  std::string name = CefString(&cookie.name).ToString();
  std::string value = CefString(&cookie.value).ToString();
  std::string domain = CefString(&cookie.domain).ToString();
  std::string path = CefString(&cookie.path).ToString();

  base::Time expiration_time;
  if (cookie.has_expires) {
    expiration_time = CefBaseTime(cookie.expires);
  }

  net::CookieSameSite same_site =
      net_service::MakeCookieSameSite(cookie.same_site);
  net::CookiePriority priority =
      net_service::MakeCookiePriority(cookie.priority);

  auto canonical_cookie = net::CanonicalCookie::CreateSanitizedCookie(
      url, name, value, domain, path,
      /*creation_time=*/base::Time(), expiration_time,
      /*last_access_time=*/base::Time(), cookie.secure ? true : false,
      cookie.httponly ? true : false, same_site, priority,
      /*partition_key=*/absl::nullopt);

  if (!canonical_cookie) {
    SetCookieCallbackImpl(
        callback, net::CookieAccessResult(net::CookieInclusionStatus(
                      net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR)));
    return true;
  }

  net::CookieOptions options;
  if (cookie.httponly) {
    options.set_include_httponly();
  }
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return false;
  }

  GetCookieManager(browser_context)
      ->SetCanonicalCookie(*canonical_cookie, url, options,
                           base::BindOnce(SetCookieCallbackImpl, callback));
  return true;
}

bool CefCookieManagerImpl::DeleteCookiesInternal(
    const GURL& url,
    const CefString& cookie_name,
    CefRefPtr<CefDeleteCookiesCallback> callback) {
  DCHECK(ValidContext());
  DCHECK(url.is_empty() || url.is_valid());

  network::mojom::CookieDeletionFilterPtr deletion_filter =
      network::mojom::CookieDeletionFilter::New();

  if (url.is_empty()) {
    // Delete all cookies.
  } else if (cookie_name.empty()) {
    // Delete all matching host cookies.
    deletion_filter->host_name = url.host();
  } else {
    // Delete all matching host and domain cookies.
    deletion_filter->url = url;
    deletion_filter->cookie_name = cookie_name;
  }

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return false;
  }

  GetCookieManager(browser_context)
      ->DeleteCookies(std::move(deletion_filter),
                      base::BindOnce(DeleteCookiesCallbackImpl, callback));
  return true;
}

bool CefCookieManagerImpl::FlushStoreInternal(
    CefRefPtr<CefCompletionCallback> callback) {
  DCHECK(ValidContext());

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return false;
  }

  GetCookieManager(browser_context)
      ->FlushCookieStore(
          base::BindOnce(RunAsyncCompletionOnUIThread, callback));
  return true;
}

void CefCookieManagerImpl::StoreOrTriggerInitCallback(
    base::OnceClosure callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefCookieManagerImpl::StoreOrTriggerInitCallback, this,
                       std::move(callback)));
    return;
  }

  if (initialized_) {
    std::move(callback).Run();
  } else {
    init_callbacks_.emplace_back(std::move(callback));
  }
}

bool CefCookieManagerImpl::ValidContext() const {
  return CEF_CURRENTLY_ON_UIT() && initialized_;
}

// CefCookieManager methods ----------------------------------------------------

// static
CefRefPtr<CefCookieManager> CefCookieManager::GetGlobalManager(
    CefRefPtr<CefCompletionCallback> callback) {
  CefRefPtr<CefRequestContext> context = CefRequestContext::GetGlobalContext();
  return context ? context->GetCookieManager(callback) : nullptr;
}
