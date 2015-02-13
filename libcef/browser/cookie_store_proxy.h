// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_COOKIE_STORE_PROXY_H_
#define CEF_LIBCEF_BROWSER_COOKIE_STORE_PROXY_H_
#pragma once

#include "include/cef_request_context_handler.h"

#include "net/cookies/cookie_store.h"

class CefURLRequestContextImpl;

// Proxies cookie requests to the CefRequestContextHandler or global cookie
// store. Life span is controlled by CefURLRequestContextProxy. Only accessed on
// the IO thread. See browser_context.h for an object relationship diagram.
class CefCookieStoreProxy : public net::CookieStore {
 public:
  CefCookieStoreProxy(CefURLRequestContextImpl* parent,
                      CefRefPtr<CefRequestContextHandler> handler);
  ~CefCookieStoreProxy() override;

  // The |parent_| object may no longer be valid after this method is called.
  void Detach();

  // net::CookieStore methods.
  void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const net::CookieOptions& options,
      const SetCookiesCallback& callback) override;
  void GetCookiesWithOptionsAsync(
      const GURL& url, const net::CookieOptions& options,
      const GetCookiesCallback& callback) override;
  void DeleteCookieAsync(const GURL& url,
                         const std::string& cookie_name,
                         const base::Closure& callback) override;
  void GetAllCookiesForURLAsync(
      const GURL& url,
      const GetCookieListCallback& callback) override;
  void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                    const base::Time& delete_end,
                                    const DeleteCallback& callback) override;
  void DeleteAllCreatedBetweenForHostAsync(
      const base::Time delete_begin,
      const base::Time delete_end,
      const GURL& url,
      const DeleteCallback& callback) override;
  void DeleteSessionCookiesAsync(const DeleteCallback& callback) override;
  net::CookieMonster* GetCookieMonster() override;
  scoped_ptr<CookieChangedSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      const CookieChangedCallback& callback) override;

 private:
  net::CookieStore* GetCookieStore();

  // The |parent_| pointer is kept alive by CefURLRequestContextGetterProxy
  // which has a ref to the owning CefURLRequestContextGetterImpl. Detach() will
  // be called when the CefURLRequestContextGetterProxy is destroyed.
  CefURLRequestContextImpl* parent_;
  CefRefPtr<CefRequestContextHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(CefCookieStoreProxy);
};

#endif  // CEF_LIBCEF_BROWSER_COOKIE_STORE_PROXY_H_
