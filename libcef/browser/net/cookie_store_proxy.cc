// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net/cookie_store_proxy.h"

#include "libcef/browser/cookie_manager_impl.h"
#include "libcef/browser/net/url_request_context_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/logging.h"
#include "net/url_request/url_request_context.h"

CefCookieStoreProxy::CefCookieStoreProxy(
    CefURLRequestContextImpl* parent,
    CefRefPtr<CefRequestContextHandler> handler)
    : parent_(parent),
      handler_(handler) {
  CEF_REQUIRE_IOT();
  DCHECK(parent_);
  DCHECK(handler_.get());
}

CefCookieStoreProxy::~CefCookieStoreProxy() {
  CEF_REQUIRE_IOT();
}

void CefCookieStoreProxy::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    const SetCookiesCallback& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->SetCookieWithOptionsAsync(url, cookie_line, options,
                                            callback);
  }
}

void CefCookieStoreProxy::SetCookieWithDetailsAsync(
    const GURL& url,
    const std::string& name,
    const std::string& value,
    const std::string& domain,
    const std::string& path,
    base::Time creation_time,
    base::Time expiration_time,
    base::Time last_access_time,
    bool secure,
    bool http_only,
    net::CookieSameSite same_site,
    net::CookiePriority priority,
    const SetCookiesCallback& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->SetCookieWithDetailsAsync(url, name, value, domain, path,
                                            creation_time, expiration_time,
                                            last_access_time, secure, http_only,
                                            same_site, priority, callback);
  }
}

void CefCookieStoreProxy::GetCookiesWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    const GetCookiesCallback& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store)
    cookie_store->GetCookiesWithOptionsAsync(url, options, callback);
}

void CefCookieStoreProxy::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    const GetCookieListCallback& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store)
    cookie_store->GetCookieListWithOptionsAsync(url, options, callback);
}

void CefCookieStoreProxy::GetAllCookiesAsync(
    const GetCookieListCallback& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store)
    cookie_store->GetAllCookiesAsync(callback);
}

void CefCookieStoreProxy::DeleteCookieAsync(
    const GURL& url,
    const std::string& cookie_name,
    const base::Closure& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store)
    cookie_store->DeleteCookieAsync(url, cookie_name, callback);
}

void CefCookieStoreProxy::DeleteCanonicalCookieAsync(
    const net::CanonicalCookie& cookie,
    const DeleteCallback& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store)
    cookie_store->DeleteCanonicalCookieAsync(cookie, callback);
}

void CefCookieStoreProxy::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const DeleteCallback& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->DeleteAllCreatedBetweenAsync(delete_begin, delete_end,
                                               callback);
  }
}

void CefCookieStoreProxy::DeleteAllCreatedBetweenWithPredicateAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const CookiePredicate& predicate,
    const DeleteCallback& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->DeleteAllCreatedBetweenWithPredicateAsync(
        delete_begin, delete_end, predicate, callback);
  }
}

void CefCookieStoreProxy::DeleteSessionCookiesAsync(
    const DeleteCallback& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store)
    cookie_store->DeleteSessionCookiesAsync(callback);
}

void CefCookieStoreProxy::FlushStore(const base::Closure& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store)
    cookie_store->FlushStore(callback);
}

std::unique_ptr<net::CookieStore::CookieChangedSubscription>
CefCookieStoreProxy::AddCallbackForCookie(
    const GURL& url,
    const std::string& name,
    const CookieChangedCallback& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store)
    return cookie_store->AddCallbackForCookie(url, name, callback);
  return NULL;
}

bool CefCookieStoreProxy::IsEphemeral() {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store)
    return cookie_store->IsEphemeral();
  return true;
}

net::CookieStore* CefCookieStoreProxy::GetCookieStore() {
  CEF_REQUIRE_IOT();

  net::CookieStore* cookie_store = nullptr;

  CefRefPtr<CefCookieManager> manager = handler_->GetCookieManager();
  if (manager.get()) {
    // Use the cookie store provided by the manager.
    cookie_store = reinterpret_cast<CefCookieManagerImpl*>(manager.get())->
        GetExistingCookieStore();
    DCHECK(cookie_store);
    return cookie_store;
  }

  DCHECK(parent_);
  if (parent_) {
    // Use the cookie store from the parent.
    cookie_store = parent_->cookie_store();
    DCHECK(cookie_store);
    if (!cookie_store)
      LOG(ERROR) << "Cookie store does not exist";
  }

  return cookie_store;
}
