// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/cookie_store_proxy.h"

#include <string>

#include "libcef/browser_impl.h"
#include "libcef/cef_context.h"
#include "libcef/cookie_manager_impl.h"

#include "base/logging.h"


CefCookieStoreProxy::CefCookieStoreProxy(CefBrowserImpl* browser)
  : browser_(browser) {
  DCHECK(browser_);
}

void CefCookieStoreProxy::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    const SetCookiesCallback& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  cookie_store->SetCookieWithOptionsAsync(url, cookie_line, options, callback);
}

void CefCookieStoreProxy::GetCookiesWithOptionsAsync(
    const GURL& url, const net::CookieOptions& options,
    const GetCookiesCallback& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  cookie_store->GetCookiesWithOptionsAsync(url, options, callback);
}

void CefCookieStoreProxy::GetCookiesWithInfoAsync(
    const GURL& url,
    const net::CookieOptions& options,
    const GetCookieInfoCallback& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  cookie_store->GetCookiesWithInfoAsync(url, options, callback);
}

void CefCookieStoreProxy::DeleteCookieAsync(
    const GURL& url,
    const std::string& cookie_name,
    const base::Closure& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  cookie_store->DeleteCookieAsync(url, cookie_name, callback);
}

void CefCookieStoreProxy::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const DeleteCallback& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  cookie_store->DeleteAllCreatedBetweenAsync(delete_begin, delete_end,
                                             callback);
}

void CefCookieStoreProxy::DeleteSessionCookiesAsync(
    const DeleteCallback& callback) {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  cookie_store->DeleteSessionCookiesAsync(callback);
}

net::CookieMonster* CefCookieStoreProxy::GetCookieMonster() {
  scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
  return cookie_store->GetCookieMonster();
}

net::CookieStore* CefCookieStoreProxy::GetCookieStore() {
  scoped_refptr<net::CookieStore> cookie_store;

  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
    if (handler.get()) {
      // Get the manager from the handler.
      CefRefPtr<CefCookieManager> manager =
          handler->GetCookieManager(browser_, browser_->pending_url().spec());
      if (manager.get()) {
        cookie_store =
          reinterpret_cast<CefCookieManagerImpl*>(
              manager.get())->cookie_monster();
      }
    }
  }

  if (!cookie_store) {
    // Use the global cookie store.
    cookie_store = _Context->request_context()->cookie_store();
  }

  DCHECK(cookie_store);
  return cookie_store;
}
