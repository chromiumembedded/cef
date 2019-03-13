// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_COOKIE_STORE_PROXY_H_
#define CEF_LIBCEF_BROWSER_COOKIE_STORE_PROXY_H_
#pragma once

#include "net/cookies/cookie_store.h"

class CefCookieStoreSource;

// Proxies cookie requests to a CefCookieStoreSource (see comments on the
// implementation classes for details). Only accessed on the IO thread.
class CefCookieStoreProxy : public net::CookieStore {
 public:
  explicit CefCookieStoreProxy(std::unique_ptr<CefCookieStoreSource> source);
  ~CefCookieStoreProxy() override;

  // net::CookieStore methods.
  void SetCookieWithOptionsAsync(const GURL& url,
                                 const std::string& cookie_line,
                                 const net::CookieOptions& options,
                                 SetCookiesCallback callback) override;
  void SetCanonicalCookieAsync(std::unique_ptr<net::CanonicalCookie> cookie,
                               std::string source_scheme,
                               bool modify_http_only,
                               SetCookiesCallback callback) override;
  void GetCookieListWithOptionsAsync(const GURL& url,
                                     const net::CookieOptions& options,
                                     GetCookieListCallback callback) override;
  void GetAllCookiesAsync(GetCookieListCallback callback) override;
  void DeleteCanonicalCookieAsync(const net::CanonicalCookie& cookie,
                                  DeleteCallback callback) override;
  void DeleteAllCreatedInTimeRangeAsync(
      const net::CookieDeletionInfo::TimeRange& creation_range,
      DeleteCallback callback) override;
  void DeleteAllMatchingInfoAsync(net::CookieDeletionInfo delete_info,
                                  DeleteCallback callback) override;
  void DeleteSessionCookiesAsync(DeleteCallback callback) override;
  void FlushStore(base::OnceClosure callback) override;
  net::CookieChangeDispatcher& GetChangeDispatcher() override;
  bool IsEphemeral() override;

 private:
  net::CookieStore* GetCookieStore();

  std::unique_ptr<CefCookieStoreSource> const source_;
  std::unique_ptr<net::CookieChangeDispatcher> null_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(CefCookieStoreProxy);
};

#endif  // CEF_LIBCEF_BROWSER_COOKIE_STORE_PROXY_H_
