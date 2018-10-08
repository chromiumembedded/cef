// Copyright (c) 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_COOKIE_STORE_SOURCE_H_
#define CEF_LIBCEF_BROWSER_COOKIE_STORE_SOURCE_H_
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "include/cef_request_context_handler.h"

#include "base/macros.h"

namespace base {
class FilePath;
}

namespace net {
class CookieStore;
class NetLog;
}  // namespace net

class CefURLRequestContextImpl;

// Abstract base class for CookieStore sources. Only accessed on the IO thread.
class CefCookieStoreSource {
 public:
  virtual net::CookieStore* GetCookieStore() = 0;
  virtual ~CefCookieStoreSource() {}
};

// Sources a cookie store that is created/owned by a CefCookieManager or the
// parent context. Life span is controlled by CefURLRequestContextProxy. See
// browser_context.h for an object relationship diagram.
class CefCookieStoreHandlerSource : public CefCookieStoreSource {
 public:
  CefCookieStoreHandlerSource(CefURLRequestContextImpl* parent,
                              CefRefPtr<CefRequestContextHandler> handler);

  net::CookieStore* GetCookieStore() override;

 private:
  // The |parent_| pointer is kept alive by CefURLRequestContextGetterProxy
  // which has a ref to the owning CefURLRequestContextGetterImpl.
  CefURLRequestContextImpl* parent_;
  CefRefPtr<CefRequestContextHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(CefCookieStoreHandlerSource);
};

// Sources a cookie store that is created/owned by this object. Life span is
// controlled by the owning URLRequestContext.
class CefCookieStoreOwnerSource : public CefCookieStoreSource {
 public:
  CefCookieStoreOwnerSource();

  void SetCookieStoragePath(const base::FilePath& path,
                            bool persist_session_cookies,
                            net::NetLog* net_log);
  void SetCookieSupportedSchemes(const std::vector<std::string>& schemes);

  net::CookieStore* GetCookieStore() override;

 private:
  std::unique_ptr<net::CookieStore> cookie_store_;
  base::FilePath path_;
  std::vector<std::string> supported_schemes_;

  DISALLOW_COPY_AND_ASSIGN(CefCookieStoreOwnerSource);
};

#endif  // CEF_LIBCEF_BROWSER_COOKIE_STORE_SOURCE_H_
