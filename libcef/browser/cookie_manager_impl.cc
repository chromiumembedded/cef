// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/cookie_manager_impl.h"

#include <string>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/url_request_context_getter.h"
#include "libcef/common/time_util.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context.h"

namespace {

// Callback class for visiting cookies.
class VisitCookiesCallback : public base::RefCounted<VisitCookiesCallback> {
 public:
  explicit VisitCookiesCallback(net::CookieMonster* cookie_monster,
                                CefRefPtr<CefCookieVisitor> visitor)
    : cookie_monster_(cookie_monster),
      visitor_(visitor) {
  }

  void Run(const net::CookieList& list) {
    CEF_REQUIRE_IOT();

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
        cookie_monster_->DeleteCanonicalCookieAsync(cc,
            net::CookieMonster::DeleteCookieCallback());
      }
      if (!keepLooping)
        break;
    }
  }

 private:
  scoped_refptr<net::CookieMonster> cookie_monster_;
  CefRefPtr<CefCookieVisitor> visitor_;
};

}  // namespace


CefCookieManagerImpl::CefCookieManagerImpl(bool is_global)
  : is_global_(is_global) {
}

CefCookieManagerImpl::~CefCookieManagerImpl() {
}

void CefCookieManagerImpl::Initialize(const CefString& path) {
  if (is_global_)
    SetGlobal();
  else
    SetStoragePath(path);
}

bool CefCookieManagerImpl::VisitAllCookies(
    CefRefPtr<CefCookieVisitor> visitor) {
  if (CEF_CURRENTLY_ON_IOT()) {
    scoped_refptr<VisitCookiesCallback> callback(
      new VisitCookiesCallback(cookie_monster_, visitor));

    cookie_monster_->GetAllCookiesAsync(
        base::Bind(&VisitCookiesCallback::Run, callback.get()));
  } else {
    // Execute on the IO thread.
    CEF_POST_TASK(CEF_IOT,
        base::Bind(base::IgnoreResult(&CefCookieManagerImpl::VisitAllCookies),
                   this, visitor));
  }

  return true;
}

bool CefCookieManagerImpl::VisitUrlCookies(
    const CefString& url, bool includeHttpOnly,
    CefRefPtr<CefCookieVisitor> visitor) {
  if (CEF_CURRENTLY_ON_IOT()) {
    net::CookieOptions options;
    if (includeHttpOnly)
      options.set_include_httponly();

    scoped_refptr<VisitCookiesCallback> callback(
        new VisitCookiesCallback(cookie_monster_, visitor));

    GURL gurl = GURL(url.ToString());
    cookie_monster_->GetAllCookiesForURLWithOptionsAsync(gurl, options,
        base::Bind(&VisitCookiesCallback::Run, callback.get()));
  } else {
    // Execute on the IO thread.
    CEF_POST_TASK(CEF_IOT,
        base::Bind(base::IgnoreResult(&CefCookieManagerImpl::VisitUrlCookies),
                   this, url, includeHttpOnly, visitor));
  }

  return true;
}

bool CefCookieManagerImpl::SetCookie(const CefString& url,
                                     const CefCookie& cookie) {
  CEF_REQUIRE_IOT_RETURN(false);

  GURL gurl = GURL(url.ToString());
  if (!gurl.is_valid())
    return false;

  std::string name = CefString(&cookie.name).ToString();
  std::string value = CefString(&cookie.value).ToString();
  std::string domain = CefString(&cookie.domain).ToString();
  std::string path = CefString(&cookie.path).ToString();

  base::Time expiration_time;
  if (cookie.has_expires)
    cef_time_to_basetime(cookie.expires, expiration_time);

  cookie_monster_->SetCookieWithDetailsAsync(gurl, name, value, domain, path,
      expiration_time, cookie.secure, cookie.httponly,
      net::CookieStore::SetCookiesCallback());
  return true;
}

bool CefCookieManagerImpl::DeleteCookies(const CefString& url,
                                         const CefString& cookie_name) {
  CEF_REQUIRE_IOT_RETURN(false);

  if (url.empty()) {
    // Delete all cookies.
    cookie_monster_->DeleteAllAsync(net::CookieMonster::DeleteCallback());
    return true;
  }

  GURL gurl = GURL(url.ToString());
  if (!gurl.is_valid())
    return false;

  if (cookie_name.empty()) {
    // Delete all matching host cookies.
    cookie_monster_->DeleteAllForHostAsync(gurl,
        net::CookieMonster::DeleteCallback());
  } else {
    // Delete all matching host and domain cookies.
    cookie_monster_->DeleteCookieAsync(gurl, cookie_name, base::Closure());
  }
  return true;
}

bool CefCookieManagerImpl::SetStoragePath(const CefString& path) {
  if (CEF_CURRENTLY_ON_IOT()) {
    FilePath new_path;
    if (!path.empty())
      new_path = FilePath(path);

    if (is_global_) {
      // Global path changes are handled by the request context.
      CefURLRequestContextGetter* getter =
          static_cast<CefURLRequestContextGetter*>(
              _Context->browser_context()->GetRequestContext());
      getter->SetCookieStoragePath(new_path);
      cookie_monster_ = getter->GetURLRequestContext()->cookie_store()->
          GetCookieMonster();
      return true;
    }
    
    if (cookie_monster_ && ((storage_path_.empty() && path.empty()) ||
                            storage_path_ == new_path)) {
      // The path has not changed so don't do anything.
      return true;
    }

    scoped_refptr<SQLitePersistentCookieStore> persistent_store;
    if (!new_path.empty()) {
      if (file_util::CreateDirectory(new_path)) {
        const FilePath& cookie_path = new_path.AppendASCII("Cookies");
        persistent_store = new SQLitePersistentCookieStore(cookie_path, false);
      } else {
        NOTREACHED() << "The cookie storage directory could not be created";
        storage_path_.clear();
      }
    }

    // Set the new cookie store that will be used for all new requests. The old
    // cookie store, if any, will be automatically flushed and closed when no
    // longer referenced.
    cookie_monster_ = new net::CookieMonster(persistent_store.get(), NULL);
    storage_path_ = new_path;
  } else {
    // Execute on the IO thread.
    CEF_POST_TASK(CEF_IOT,
        base::Bind(base::IgnoreResult(&CefCookieManagerImpl::SetStoragePath),
                   this, path));
  }

  return true;
}

void CefCookieManagerImpl::SetGlobal() {
  if (CEF_CURRENTLY_ON_IOT()) {
    cookie_monster_ = _Context->browser_context()->GetRequestContext()->
        GetURLRequestContext()->cookie_store()->GetCookieMonster();
    DCHECK(cookie_monster_);
  } else {
    // Execute on the IO thread.
    CEF_POST_TASK(CEF_IOT, base::Bind(&CefCookieManagerImpl::SetGlobal, this));
  }
}


// CefCookieManager methods ----------------------------------------------------

// static
CefRefPtr<CefCookieManager> CefCookieManager::GetGlobalManager() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return NULL;
  }

  CefRefPtr<CefCookieManagerImpl> manager(new CefCookieManagerImpl(true));
  manager->Initialize(CefString());
  return manager.get();
}

// static
CefRefPtr<CefCookieManager> CefCookieManager::CreateManager(
    const CefString& path) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return NULL;
  }

  CefRefPtr<CefCookieManagerImpl> manager(new CefCookieManagerImpl(false));
  manager->Initialize(path);
  return manager.get();
}
