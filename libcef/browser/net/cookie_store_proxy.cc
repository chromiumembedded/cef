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

void CefCookieStoreProxy::Detach() {
  CEF_REQUIRE_IOT();
  parent_ = NULL;
}

void CefCookieStoreProxy::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    const SetCookiesCallback& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  if (cookie_store.get()) {
    cookie_store->SetCookieWithOptionsAsync(url, cookie_line, options,
                                            callback);
  }
}

void CefCookieStoreProxy::GetCookiesWithOptionsAsync(
    const GURL& url, const net::CookieOptions& options,
    const GetCookiesCallback& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  if (cookie_store.get())
    cookie_store->GetCookiesWithOptionsAsync(url, options, callback);
}

void CefCookieStoreProxy::DeleteCookieAsync(
    const GURL& url,
    const std::string& cookie_name,
    const base::Closure& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  if (cookie_store.get())
    cookie_store->DeleteCookieAsync(url, cookie_name, callback);
}

void CefCookieStoreProxy::GetAllCookiesForURLAsync(
    const GURL& url,
    const GetCookieListCallback& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  if (cookie_store.get())
    cookie_store->GetAllCookiesForURLAsync(url, callback);
}

void CefCookieStoreProxy::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const DeleteCallback& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  if (cookie_store.get()) {
    cookie_store->DeleteAllCreatedBetweenAsync(delete_begin, delete_end,
                                               callback);
  }
}

void CefCookieStoreProxy::DeleteAllCreatedBetweenForHostAsync(
    const base::Time delete_begin,
    const base::Time delete_end,
    const GURL& url,
    const DeleteCallback& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  if (cookie_store.get()) {
    cookie_store->DeleteAllCreatedBetweenForHostAsync(delete_begin, delete_end,
                                                      url, callback);
  }
}

void CefCookieStoreProxy::DeleteSessionCookiesAsync(
    const DeleteCallback& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  if (cookie_store.get())
    cookie_store->DeleteSessionCookiesAsync(callback);
}

net::CookieMonster* CefCookieStoreProxy::GetCookieMonster() {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  if (cookie_store.get())
    return cookie_store->GetCookieMonster();
  return NULL;
}

scoped_ptr<net::CookieStore::CookieChangedSubscription>
CefCookieStoreProxy::AddCallbackForCookie(
    const GURL& url,
    const std::string& name,
    const CookieChangedCallback& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  if (cookie_store.get())
    return cookie_store->AddCallbackForCookie(url, name, callback);
  return NULL;
}

net::CookieStore* CefCookieStoreProxy::GetCookieStore() {
  CEF_REQUIRE_IOT();

  scoped_refptr<net::CookieStore> cookie_store;

  CefRefPtr<CefCookieManager> manager = handler_->GetCookieManager();
  if (manager.get()) {
    // Use the cookie store provided by the manager.
    cookie_store = reinterpret_cast<CefCookieManagerImpl*>(manager.get())->
        GetExistingCookieMonster();
    DCHECK(cookie_store.get());
    return cookie_store.get();
  }

  DCHECK(parent_);
  if (parent_) {
    // Use the cookie store from the parent.
    cookie_store = parent_->cookie_store();
    DCHECK(cookie_store.get());
  }

  return cookie_store.get();
}
