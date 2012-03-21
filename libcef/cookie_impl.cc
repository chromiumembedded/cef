// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_cookie.h"
#include "libcef/cef_context.h"
#include "libcef/cef_thread.h"
#include "libcef/cef_time_util.h"

#include "base/bind.h"
#include "net/base/cookie_monster.h"

namespace {

// Callback class for visiting cookies.
class VisitCookiesCallback : public base::RefCounted<VisitCookiesCallback> {
 public:
  explicit VisitCookiesCallback(CefRefPtr<CefCookieVisitor> visitor)
    : visitor_(visitor) {
  }

  void Run(const net::CookieList& list) {
    REQUIRE_IOT();

    net::CookieMonster* cookie_monster = static_cast<net::CookieMonster*>(
        _Context->request_context()->cookie_store());
    if (!cookie_monster)
      return;

    int total = list.size(), count = 0;

    net::CookieList::const_iterator it = list.begin();
    for (; it != list.end(); ++it, ++count) {
      CefCookie cookie;
      const net::CookieMonster::CanonicalCookie& cc = *(it);

      CefString(&cookie.name).FromString(cc.Name());
      CefString(&cookie.value).FromString(cc.Value());
      CefString(&cookie.domain).FromString(cc.Domain());
      CefString(&cookie.path).FromString(cc.Path());
      cookie.secure = cc.IsSecure();
      cookie.httponly = cc.IsHttpOnly();
      cef_time_from_basetime(cc.CreationDate(), cookie.creation);
      cef_time_from_basetime(cc.LastAccessDate(), cookie.last_access);
      cookie.has_expires = cc.DoesExpire();
      if (cookie.has_expires)
        cef_time_from_basetime(cc.ExpiryDate(), cookie.expires);

      bool deleteCookie = false;
      bool keepLooping = visitor_->Visit(cookie, count, total, deleteCookie);
      if (deleteCookie) {
        cookie_monster->DeleteCanonicalCookieAsync(cc,
            net::CookieMonster::DeleteCookieCallback());
      }
      if (!keepLooping)
        break;
    }
  }

 private:
  CefRefPtr<CefCookieVisitor> visitor_;
};

void IOT_VisitAllCookies(CefRefPtr<CefCookieVisitor> visitor) {
  REQUIRE_IOT();

  net::CookieMonster* cookie_monster = static_cast<net::CookieMonster*>(
      _Context->request_context()->cookie_store());
  if (!cookie_monster)
    return;

  scoped_refptr<VisitCookiesCallback> callback(
      new VisitCookiesCallback(visitor));

  cookie_monster->GetAllCookiesAsync(
      base::Bind(&VisitCookiesCallback::Run, callback.get()));
}

void IOT_VisitUrlCookies(const GURL& url, bool includeHttpOnly,
                         CefRefPtr<CefCookieVisitor> visitor) {
  REQUIRE_IOT();

  net::CookieMonster* cookie_monster = static_cast<net::CookieMonster*>(
      _Context->request_context()->cookie_store());
  if (!cookie_monster)
    return;

  net::CookieOptions options;
  if (includeHttpOnly)
    options.set_include_httponly();

  scoped_refptr<VisitCookiesCallback> callback(
      new VisitCookiesCallback(visitor));

  cookie_monster->GetAllCookiesForURLWithOptionsAsync(url, options,
      base::Bind(&VisitCookiesCallback::Run, callback.get()));
}

void IOT_SetCookiePath(const CefString& path) {
  REQUIRE_IOT();

  FilePath cookie_path;
  if (!path.empty())
    cookie_path = FilePath(path);

  _Context->request_context()->SetCookieStoragePath(cookie_path);
}

}  // namespace

bool CefVisitAllCookies(CefRefPtr<CefCookieVisitor> visitor) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  return CefThread::PostTask(CefThread::IO, FROM_HERE,
      base::Bind(IOT_VisitAllCookies, visitor));
}

bool CefVisitUrlCookies(const CefString& url, bool includeHttpOnly,
                        CefRefPtr<CefCookieVisitor> visitor) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  std::string urlStr = url;
  GURL gurl = GURL(urlStr);
  if (!gurl.is_valid())
    return false;

  return CefThread::PostTask(CefThread::IO, FROM_HERE,
      base::Bind(IOT_VisitUrlCookies, gurl, includeHttpOnly, visitor));
}

bool CefSetCookie(const CefString& url, const CefCookie& cookie) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  // Verify that this function is being called on the IO thread.
  if (!CefThread::CurrentlyOn(CefThread::IO)) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  net::CookieMonster* cookie_monster = static_cast<net::CookieMonster*>(
      _Context->request_context()->cookie_store());
  if (!cookie_monster)
    return false;

  std::string urlStr = url;
  GURL gurl = GURL(urlStr);
  if (!gurl.is_valid())
    return false;

  std::string name = CefString(&cookie.name).ToString();
  std::string value = CefString(&cookie.value).ToString();
  std::string domain = CefString(&cookie.domain).ToString();
  std::string path = CefString(&cookie.path).ToString();

  base::Time expiration_time;
  if (cookie.has_expires)
    cef_time_to_basetime(cookie.expires, expiration_time);

  cookie_monster->SetCookieWithDetailsAsync(gurl, name, value, domain, path,
      expiration_time, cookie.secure, cookie.httponly,
      net::CookieStore::SetCookiesCallback());
  return true;
}

bool CefDeleteCookies(const CefString& url, const CefString& cookie_name) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  // Verify that this function is being called on the IO thread.
  if (!CefThread::CurrentlyOn(CefThread::IO)) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  net::CookieMonster* cookie_monster = static_cast<net::CookieMonster*>(
      _Context->request_context()->cookie_store());
  if (!cookie_monster)
    return false;

  if (url.empty()) {
    // Delete all cookies.
    cookie_monster->DeleteAllAsync(net::CookieMonster::DeleteCallback());
    return true;
  }

  std::string urlStr = url;
  GURL gurl = GURL(urlStr);
  if (!gurl.is_valid())
    return false;

  if (cookie_name.empty()) {
    // Delete all matching host cookies.
    cookie_monster->DeleteAllForHostAsync(gurl,
        net::CookieMonster::DeleteCallback());
  } else {
    // Delete all matching host and domain cookies.
    cookie_monster->DeleteCookieAsync(gurl, cookie_name, base::Closure());
  }
  return true;
}

bool CefSetCookiePath(const CefString& path) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  if (CefThread::CurrentlyOn(CefThread::IO)) {
    IOT_SetCookiePath(path);
  } else {
    CefThread::PostTask(CefThread::IO, FROM_HERE,
        base::Bind(&IOT_SetCookiePath, path));
  }

  return true;
}
