// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/cookie_manager_impl.h"

#include <string>

#include "libcef/cef_context.h"
#include "libcef/cef_thread.h"
#include "libcef/cef_time_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"

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
    REQUIRE_IOT();

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
  if (is_global) {
    cookie_monster_ =
        static_cast<net::CookieMonster*>(
            _Context->request_context()->cookie_store());
    DCHECK(cookie_monster_);
  }
}

CefCookieManagerImpl::~CefCookieManagerImpl() {
}

void CefCookieManagerImpl::SetSupportedSchemes(
    const std::vector<CefString>& schemes) {
  if (CefThread::CurrentlyOn(CefThread::IO)) {
    if (schemes.empty())
      return;

    std::set<std::string> scheme_set;
    std::vector<CefString>::const_iterator it = schemes.begin();
    for (; it != schemes.end(); ++it)
      scheme_set.insert(*it);

    const char** arr = new const char*[scheme_set.size()];
    std::set<std::string>::const_iterator it2 = scheme_set.begin();
    for (int i = 0; it2 != scheme_set.end(); ++it2, ++i)
      arr[i] = it2->c_str();

    cookie_monster_->SetCookieableSchemes(arr, scheme_set.size());

    delete [] arr;
  } else {
    // Execute on the IO thread.
    CefThread::PostTask(CefThread::IO, FROM_HERE,
        base::Bind(&CefCookieManagerImpl::SetSupportedSchemes,
                   this, schemes));
  }
}

bool CefCookieManagerImpl::VisitAllCookies(
    CefRefPtr<CefCookieVisitor> visitor) {
  if (CefThread::CurrentlyOn(CefThread::IO)) {
    scoped_refptr<VisitCookiesCallback> callback(
      new VisitCookiesCallback(cookie_monster_, visitor));

    cookie_monster_->GetAllCookiesAsync(
        base::Bind(&VisitCookiesCallback::Run, callback.get()));
  } else {
    // Execute on the IO thread.
    CefThread::PostTask(CefThread::IO, FROM_HERE,
        base::Bind(base::IgnoreResult(&CefCookieManagerImpl::VisitAllCookies),
                   this, visitor));
  }

  return true;
}

bool CefCookieManagerImpl::VisitUrlCookies(
    const CefString& url, bool includeHttpOnly,
    CefRefPtr<CefCookieVisitor> visitor) {
  if (CefThread::CurrentlyOn(CefThread::IO)) {
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
    CefThread::PostTask(CefThread::IO, FROM_HERE,
        base::Bind(base::IgnoreResult(&CefCookieManagerImpl::VisitUrlCookies),
                   this, url, includeHttpOnly, visitor));
  }

  return true;
}

bool CefCookieManagerImpl::SetCookie(const CefString& url,
                                     const CefCookie& cookie) {
  // Verify that this function is being called on the IO thread.
  if (!CefThread::CurrentlyOn(CefThread::IO)) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

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
  // Verify that this function is being called on the IO thread.
  if (!CefThread::CurrentlyOn(CefThread::IO)) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

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
  if (CefThread::CurrentlyOn(CefThread::IO)) {
    FilePath new_path;
    if (!path.empty())
      new_path = FilePath(path);

    if (is_global_) {
      // Global path changes are handled by the request context.
      _Context->request_context()->SetCookieStoragePath(new_path);
      cookie_monster_ =
        static_cast<net::CookieMonster*>(
            _Context->request_context()->cookie_store());
      return true;
    }

    if (cookie_monster_ && ((storage_path_.empty() && path.empty()) ||
                            storage_path_ == new_path)) {
      // The path has not changed so don't do anything.
      return true;
    }

    scoped_refptr<SQLitePersistentCookieStore> persistent_store;
    if (!new_path.empty()) {
      if (!file_util::PathExists(new_path) &&
          !file_util::CreateDirectory(new_path)) {
        NOTREACHED() << "Failed to create cookie storage directory";
        new_path.clear();
      } else {
        FilePath cookie_path = new_path.Append(FILE_PATH_LITERAL("Cookies"));
        persistent_store =
            new SQLitePersistentCookieStore(cookie_path, false, NULL);
      }
    }

    // Set the new cookie store that will be used for all new requests. The old
    // cookie store, if any, will be automatically flushed and closed when no
    // longer referenced.
    cookie_monster_ = new net::CookieMonster(persistent_store.get(), NULL);
    storage_path_ = new_path;
  } else {
    // Execute on the IO thread.
    CefThread::PostTask(CefThread::IO, FROM_HERE,
        base::Bind(base::IgnoreResult(&CefCookieManagerImpl::SetStoragePath),
                   this, path));
  }

  return true;
}


// CefCookieManager methods ----------------------------------------------------

// static
CefRefPtr<CefCookieManager> CefCookieManager::GetGlobalManager() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return NULL;
  }

  return new CefCookieManagerImpl(true);
}

// static
CefRefPtr<CefCookieManager> CefCookieManager::CreateManager(
    const CefString& path) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return NULL;
  }

  CefRefPtr<CefCookieManager> manager(new CefCookieManagerImpl(false));
  manager->SetStoragePath(path);
  return manager;
}
