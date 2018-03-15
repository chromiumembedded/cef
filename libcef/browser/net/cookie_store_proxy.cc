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
    : parent_(parent), handler_(handler) {
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
    SetCookiesCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->SetCookieWithOptionsAsync(url, cookie_line, options,
                                            std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(false);
  }
}

void CefCookieStoreProxy::SetCanonicalCookieAsync(
    std::unique_ptr<net::CanonicalCookie> cookie,
    bool secure_source,
    bool modify_http_only,
    SetCookiesCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->SetCanonicalCookieAsync(std::move(cookie), secure_source,
                                          modify_http_only,
                                          std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(false);
  }
}

void CefCookieStoreProxy::GetCookiesWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookiesCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->GetCookiesWithOptionsAsync(url, options, std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(std::string());
  }
}

void CefCookieStoreProxy::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookieListCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->GetCookieListWithOptionsAsync(url, options,
                                                std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(net::CookieList());
  }
}

void CefCookieStoreProxy::GetAllCookiesAsync(GetCookieListCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->GetAllCookiesAsync(std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(net::CookieList());
  }
}

void CefCookieStoreProxy::DeleteCookieAsync(const GURL& url,
                                            const std::string& cookie_name,
                                            base::OnceClosure callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->DeleteCookieAsync(url, cookie_name, std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

void CefCookieStoreProxy::DeleteCanonicalCookieAsync(
    const net::CanonicalCookie& cookie,
    DeleteCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->DeleteCanonicalCookieAsync(cookie, std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(0);
  }
}

void CefCookieStoreProxy::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    DeleteCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->DeleteAllCreatedBetweenAsync(delete_begin, delete_end,
                                               std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(0);
  }
}

void CefCookieStoreProxy::DeleteAllCreatedBetweenWithPredicateAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const CookiePredicate& predicate,
    DeleteCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->DeleteAllCreatedBetweenWithPredicateAsync(
        delete_begin, delete_end, predicate, std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(0);
  }
}

void CefCookieStoreProxy::DeleteSessionCookiesAsync(DeleteCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->DeleteSessionCookiesAsync(std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(0);
  }
}

void CefCookieStoreProxy::FlushStore(base::OnceClosure callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->FlushStore(std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

std::unique_ptr<net::CookieStore::CookieChangedSubscription>
CefCookieStoreProxy::AddCallbackForCookie(
    const GURL& url,
    const std::string& name,
    const CookieChangedCallback& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    return cookie_store->AddCallbackForCookie(url, name, callback);
  }
  return nullptr;
}

std::unique_ptr<net::CookieStore::CookieChangedSubscription>
CefCookieStoreProxy::AddCallbackForAllChanges(
    const CookieChangedCallback& callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    return cookie_store->AddCallbackForAllChanges(callback);
  }
  return nullptr;
}

bool CefCookieStoreProxy::IsEphemeral() {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store)
    return cookie_store->IsEphemeral();
  return true;
}

net::CookieStore* CefCookieStoreProxy::GetCookieStore() {
  CEF_REQUIRE_IOT();

  CefRefPtr<CefCookieManager> manager = handler_->GetCookieManager();
  if (manager.get()) {
    // Use the cookie store provided by the manager. May be nullptr if the
    // cookie manager is blocking.
    return reinterpret_cast<CefCookieManagerImpl*>(manager.get())
        ->GetExistingCookieStore();
  }

  DCHECK(parent_);
  if (parent_) {
    // Use the cookie store from the parent.
    net::CookieStore* cookie_store = parent_->cookie_store();
    DCHECK(cookie_store);
    if (!cookie_store)
      LOG(ERROR) << "Cookie store does not exist";
    return cookie_store;
  }

  return nullptr;
}
