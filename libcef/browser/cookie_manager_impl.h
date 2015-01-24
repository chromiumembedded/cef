// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_COOKIE_MANAGER_IMPL_H_
#define CEF_LIBCEF_BROWSER_COOKIE_MANAGER_IMPL_H_

#include "include/cef_cookie.h"
#include "base/files/file_path.h"
#include "net/cookies/cookie_monster.h"

// Implementation of the CefCookieManager interface.
class CefCookieManagerImpl : public CefCookieManager {
 public:
  CefCookieManagerImpl(bool is_global);
  ~CefCookieManagerImpl() override;

  // Initialize the cookie manager.
  void Initialize(const CefString& path,
                  bool persist_session_cookies);

  // CefCookieManager methods.
  void SetSupportedSchemes(const std::vector<CefString>& schemes) override;
  bool VisitAllCookies(CefRefPtr<CefCookieVisitor> visitor) override;
  bool VisitUrlCookies(const CefString& url, bool includeHttpOnly,
                       CefRefPtr<CefCookieVisitor> visitor) override;
  bool SetCookie(const CefString& url,
                 const CefCookie& cookie) override;
  bool DeleteCookies(const CefString& url,
                     const CefString& cookie_name) override;
  bool SetStoragePath(const CefString& path,
                      bool persist_session_cookies) override;
  bool FlushStore(CefRefPtr<CefCompletionCallback> callback) override;

  net::CookieMonster* cookie_monster() { return cookie_monster_.get(); }

  static bool GetCefCookie(const net::CanonicalCookie& cc, CefCookie& cookie);
  static bool GetCefCookie(const GURL& url, const std::string& cookie_line,
                           CefCookie& cookie);

 private:
  void SetGlobal();

  scoped_refptr<net::CookieMonster> cookie_monster_;
  bool is_global_;
  base::FilePath storage_path_;
  std::vector<CefString> supported_schemes_;

  IMPLEMENT_REFCOUNTING(CefCookieManagerImpl);
};

#endif  // CEF_LIBCEF_BROWSER_COOKIE_MANAGER_IMPL_H_
