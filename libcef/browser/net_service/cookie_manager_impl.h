// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_MANAGER_IMPL_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_MANAGER_IMPL_H_

#include "include/cef_cookie.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/files/file_path.h"

// Implementation of the CefCookieManager interface.
class CefCookieManagerImpl : public CefCookieManager {
 public:
  CefCookieManagerImpl();

  // Must be called immediately after this object is created when |is_blocking|
  // is false.
  void Initialize(CefRefPtr<CefRequestContextImpl> request_context,
                  CefRefPtr<CefCompletionCallback> callback);

  // CefCookieManager methods.
  void SetSupportedSchemes(const std::vector<CefString>& schemes,
                           CefRefPtr<CefCompletionCallback> callback) override;
  bool VisitAllCookies(CefRefPtr<CefCookieVisitor> visitor) override;
  bool VisitUrlCookies(const CefString& url,
                       bool includeHttpOnly,
                       CefRefPtr<CefCookieVisitor> visitor) override;
  bool SetCookie(const CefString& url,
                 const CefCookie& cookie,
                 CefRefPtr<CefSetCookieCallback> callback) override;
  bool DeleteCookies(const CefString& url,
                     const CefString& cookie_name,
                     CefRefPtr<CefDeleteCookiesCallback> callback) override;
  bool FlushStore(CefRefPtr<CefCompletionCallback> callback) override;

 private:
  // Context that owns the cookie manager.
  CefRefPtr<CefRequestContextImpl> request_context_;

  IMPLEMENT_REFCOUNTING(CefCookieManagerImpl);
};

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_MANAGER_IMPL_H_
