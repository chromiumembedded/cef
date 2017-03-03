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

  // net::CookieStore methods.
  void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const net::CookieOptions& options,
      const SetCookiesCallback& callback) override;
  void SetCookieWithDetailsAsync(
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
      const SetCookiesCallback& callback) override;
  void GetCookiesWithOptionsAsync(
      const GURL& url,
      const net::CookieOptions& options,
      const GetCookiesCallback& callback) override;
  void GetCookieListWithOptionsAsync(
      const GURL& url,
      const net::CookieOptions& options,
      const GetCookieListCallback& callback) override;
  void GetAllCookiesAsync(const GetCookieListCallback& callback) override;
  void DeleteCookieAsync(const GURL& url,
                         const std::string& cookie_name,
                         const base::Closure& callback) override;
  void DeleteCanonicalCookieAsync(const net::CanonicalCookie& cookie,
                                  const DeleteCallback& callback) override;
  void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                    const base::Time& delete_end,
                                    const DeleteCallback& callback) override;
  void DeleteAllCreatedBetweenWithPredicateAsync(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      const CookiePredicate& predicate,
      const DeleteCallback& callback) override;
  void DeleteSessionCookiesAsync(const DeleteCallback& callback) override;
  void FlushStore(const base::Closure& callback) override;
  std::unique_ptr<CookieChangedSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      const CookieChangedCallback& callback) override;
  bool IsEphemeral() override;

 private:
  net::CookieStore* GetCookieStore();

  // The |parent_| pointer is kept alive by CefURLRequestContextGetterProxy
  // which has a ref to the owning CefURLRequestContextGetterImpl.
  CefURLRequestContextImpl* parent_;
  CefRefPtr<CefRequestContextHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(CefCookieStoreProxy);
};

#endif  // CEF_LIBCEF_BROWSER_COOKIE_STORE_PROXY_H_
