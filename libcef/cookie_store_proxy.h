// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_COOKIE_STORE_PROXY_H_
#define CEF_LIBCEF_COOKIE_STORE_PROXY_H_
#pragma once

#include "net/base/cookie_store.h"

class CefBrowserImpl;

// Handles cookie requests from the network stack.
class CefCookieStoreProxy : public net::CookieStore {
 public:
  explicit CefCookieStoreProxy(CefBrowserImpl* browser);

  // net::CookieStore methods.
  virtual void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const net::CookieOptions& options,
      const SetCookiesCallback& callback) OVERRIDE;
  virtual void GetCookiesWithOptionsAsync(
      const GURL& url, const net::CookieOptions& options,
      const GetCookiesCallback& callback) OVERRIDE;
  void GetCookiesWithInfoAsync(
      const GURL& url,
      const net::CookieOptions& options,
      const GetCookieInfoCallback& callback) OVERRIDE;
  virtual void DeleteCookieAsync(const GURL& url,
                                 const std::string& cookie_name,
                                 const base::Closure& callback) OVERRIDE;
  virtual void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                            const base::Time& delete_end,
                                            const DeleteCallback& callback)
                                            OVERRIDE;
  virtual net::CookieMonster* GetCookieMonster() OVERRIDE;

 private:
  net::CookieStore* GetCookieStore();

  CefBrowserImpl* browser_;
};

#endif  // CEF_LIBCEF_COOKIE_STORE_PROXY_H_
