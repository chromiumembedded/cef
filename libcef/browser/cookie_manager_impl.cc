// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/cookie_manager_impl.h"

#include <set>
#include <string>
#include <vector>

#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/net/cookie_store_source.h"
#include "libcef/browser/net/network_delegate.h"
#include "libcef/common/task_runner_impl.h"
#include "libcef/common/time_util.h"

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "components/net_log/chrome_net_log.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

// Callback class for visiting cookies.
class VisitCookiesCallback : public base::RefCounted<VisitCookiesCallback> {
 public:
  explicit VisitCookiesCallback(
      const CefCookieManagerImpl::CookieStoreGetter& cookie_store_getter,
      CefRefPtr<CefCookieVisitor> visitor)
      : cookie_store_getter_(cookie_store_getter), visitor_(visitor) {}

  void Run(const net::CookieList& list,
           const net::CookieStatusList& excluded_list) {
    CEF_REQUIRE_IOT();

    int total = list.size(), count = 0;

    net::CookieList::const_iterator it = list.begin();
    for (; it != list.end(); ++it, ++count) {
      CefCookie cookie;
      const net::CanonicalCookie& cc = *(it);
      CefCookieManagerImpl::GetCefCookie(cc, cookie);

      bool deleteCookie = false;
      bool keepLooping = visitor_->Visit(cookie, count, total, deleteCookie);
      if (deleteCookie) {
        net::CookieStore* cookie_store = cookie_store_getter_.Run();
        if (cookie_store) {
          cookie_store->DeleteCanonicalCookieAsync(
              cc, net::CookieMonster::DeleteCallback());
        }
      }
      if (!keepLooping)
        break;
    }
  }

 private:
  friend class base::RefCounted<VisitCookiesCallback>;

  ~VisitCookiesCallback() {}

  CefCookieManagerImpl::CookieStoreGetter cookie_store_getter_;
  CefRefPtr<CefCookieVisitor> visitor_;
};

// Methods extracted from net/cookies/cookie_store.cc

// Determine the cookie domain to use for setting the specified cookie.
bool GetCookieDomain(const GURL& url,
                     const net::ParsedCookie& pc,
                     std::string* result) {
  std::string domain_string;
  if (pc.HasDomain())
    domain_string = pc.Domain();
  return net::cookie_util::GetCookieDomainWithString(url, domain_string,
                                                     result);
}

// Always execute the callback asynchronously.
void RunAsyncCompletionOnIOThread(CefRefPtr<CefCompletionCallback> callback) {
  if (!callback.get())
    return;
  CEF_POST_TASK(CEF_IOT,
                base::Bind(&CefCompletionCallback::OnComplete, callback.get()));
}

// Always execute the callback asynchronously.
void DeleteCookiesCallbackImpl(CefRefPtr<CefDeleteCookiesCallback> callback,
                               uint32_t num_deleted) {
  if (!callback.get())
    return;
  CEF_POST_TASK(CEF_IOT, base::Bind(&CefDeleteCookiesCallback::OnComplete,
                                    callback.get(), num_deleted));
}

// Always execute the callback asynchronously.
void SetCookieCallbackImpl(CefRefPtr<CefSetCookieCallback> callback,
                           net::CanonicalCookie::CookieInclusionStatus status) {
  if (!callback.get())
    return;
  CEF_POST_TASK(
      CEF_IOT,
      base::Bind(
          &CefSetCookieCallback::OnComplete, callback.get(),
          status == net::CanonicalCookie::CookieInclusionStatus::INCLUDE));
}

net::CookieStore* GetExistingCookieStoreHelper(
    base::WeakPtr<CefCookieManagerImpl> cookie_manager) {
  if (cookie_manager.get())
    return cookie_manager->GetExistingCookieStore();
  return nullptr;
}

}  // namespace

CefCookieManagerImpl::CefCookieManagerImpl(bool is_blocking)
    : is_blocking_(is_blocking), weak_ptr_factory_(this) {}

CefCookieManagerImpl::~CefCookieManagerImpl() {
  CEF_REQUIRE_IOT();
}

void CefCookieManagerImpl::Initialize(
    CefRefPtr<CefRequestContextImpl> request_context,
    const CefString& path,
    bool persist_session_cookies,
    CefRefPtr<CefCompletionCallback> callback) {
  CHECK(!is_blocking_);
  if (request_context.get()) {
    request_context_ = request_context;
    request_context_->GetRequestContextImpl(
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
        base::Bind(&CefCookieManagerImpl::InitWithContext, this, callback));
  } else {
    SetStoragePath(path, persist_session_cookies, callback);
  }
}

void CefCookieManagerImpl::GetCookieStore(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const CookieStoreCallback& callback) {
  if (!task_runner.get())
    task_runner = CefTaskRunnerImpl::GetCurrentTaskRunner();

  if (!CEF_CURRENTLY_ON_IOT()) {
    CEF_POST_TASK(CEF_IOT, base::Bind(&CefCookieManagerImpl::GetCookieStore,
                                      this, task_runner, callback));
    return;
  }

  if (HasContext()) {
    RunMethodWithContext(
        base::Bind(&CefCookieManagerImpl::GetCookieStoreWithContext, this,
                   task_runner, callback));
    return;
  }

  DCHECK(is_blocking_ || cookie_source_);

  // Binding ref-counted |this| to CookieStoreGetter may result in
  // heap-use-after-free if (a) the CookieStoreGetter contains the last
  // CefCookieManagerImpl reference and (b) that reference is released during
  // execution of a CookieMonster callback (which then results in the
  // CookieManager being deleted). Use WeakPtr instead of |this| so that, in
  // that case, the CookieStoreGetter will return nullptr instead of keeping
  // the CefCookieManagerImpl alive (see issue #1882).
  const CookieStoreGetter& cookie_store_getter =
      base::Bind(GetExistingCookieStoreHelper, weak_ptr_factory_.GetWeakPtr());

  if (task_runner->BelongsToCurrentThread()) {
    // Execute the callback immediately.
    callback.Run(cookie_store_getter);
  } else {
    // Execute the callback on the target thread.
    task_runner->PostTask(FROM_HERE, base::Bind(callback, cookie_store_getter));
  }
}

net::CookieStore* CefCookieManagerImpl::GetExistingCookieStore() {
  CEF_REQUIRE_IOT();
  if (cookie_source_) {
    return cookie_source_->GetCookieStore();
  } else if (request_context_impl_.get()) {
    net::CookieStore* cookie_store =
        request_context_impl_->GetExistingCookieStore();
    DCHECK(cookie_store);
    return cookie_store;
  }

  DCHECK(is_blocking_);
  if (!is_blocking_)
    LOG(ERROR) << "Cookie store does not exist";
  return nullptr;
}

void CefCookieManagerImpl::SetSupportedSchemes(
    const std::vector<CefString>& schemes,
    CefRefPtr<CefCompletionCallback> callback) {
  if (!CEF_CURRENTLY_ON_IOT()) {
    CEF_POST_TASK(CEF_IOT,
                  base::Bind(&CefCookieManagerImpl::SetSupportedSchemes, this,
                             schemes, callback));
    return;
  }

  std::vector<std::string> scheme_set;
  std::vector<CefString>::const_iterator it = schemes.begin();
  for (; it != schemes.end(); ++it)
    scheme_set.push_back(*it);

  SetSupportedSchemesInternal(scheme_set, callback);
}

bool CefCookieManagerImpl::VisitAllCookies(
    CefRefPtr<CefCookieVisitor> visitor) {
  GetCookieStore(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
      base::Bind(&CefCookieManagerImpl::VisitAllCookiesInternal, this,
                 visitor));
  return true;
}

bool CefCookieManagerImpl::VisitUrlCookies(
    const CefString& url,
    bool includeHttpOnly,
    CefRefPtr<CefCookieVisitor> visitor) {
  GetCookieStore(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
      base::Bind(&CefCookieManagerImpl::VisitUrlCookiesInternal, this, url,
                 includeHttpOnly, visitor));
  return true;
}

bool CefCookieManagerImpl::SetCookie(const CefString& url,
                                     const CefCookie& cookie,
                                     CefRefPtr<CefSetCookieCallback> callback) {
  GURL gurl = GURL(url.ToString());
  if (!gurl.is_valid())
    return false;

  GetCookieStore(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
      base::Bind(&CefCookieManagerImpl::SetCookieInternal, this, gurl, cookie,
                 callback));
  return true;
}

bool CefCookieManagerImpl::DeleteCookies(
    const CefString& url,
    const CefString& cookie_name,
    CefRefPtr<CefDeleteCookiesCallback> callback) {
  // Empty URLs are allowed but not invalid URLs.
  GURL gurl = GURL(url.ToString());
  if (!gurl.is_empty() && !gurl.is_valid())
    return false;

  GetCookieStore(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
      base::Bind(&CefCookieManagerImpl::DeleteCookiesInternal, this, gurl,
                 cookie_name, callback));
  return true;
}

bool CefCookieManagerImpl::SetStoragePath(
    const CefString& path,
    bool persist_session_cookies,
    CefRefPtr<CefCompletionCallback> callback) {
  if (!CEF_CURRENTLY_ON_IOT()) {
    CEF_POST_TASK(
        CEF_IOT,
        base::Bind(base::IgnoreResult(&CefCookieManagerImpl::SetStoragePath),
                   this, path, persist_session_cookies, callback));
    return true;
  }

  if (HasContext()) {
    RunMethodWithContext(
        base::Bind(&CefCookieManagerImpl::SetStoragePathWithContext, this, path,
                   persist_session_cookies, callback));
    return true;
  }

  base::FilePath new_path;
  if (!path.empty())
    new_path = base::FilePath(path);

  if (!cookie_source_) {
    cookie_source_.reset(new CefCookieStoreOwnerSource());
  }

  cookie_source_->SetCookieStoragePath(new_path, persist_session_cookies,
                                       g_browser_process->net_log());

  RunAsyncCompletionOnIOThread(callback);
  return true;
}

bool CefCookieManagerImpl::FlushStore(
    CefRefPtr<CefCompletionCallback> callback) {
  GetCookieStore(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
      base::Bind(&CefCookieManagerImpl::FlushStoreInternal, this, callback));
  return true;
}

// static
bool CefCookieManagerImpl::GetCefCookie(const net::CanonicalCookie& cc,
                                        CefCookie& cookie) {
  CefString(&cookie.name).FromString(cc.Name());
  CefString(&cookie.value).FromString(cc.Value());
  CefString(&cookie.domain).FromString(cc.Domain());
  CefString(&cookie.path).FromString(cc.Path());
  cookie.secure = cc.IsSecure();
  cookie.httponly = cc.IsHttpOnly();
  cef_time_from_basetime(cc.CreationDate(), cookie.creation);
  cef_time_from_basetime(cc.LastAccessDate(), cookie.last_access);
  cookie.has_expires = cc.IsPersistent();
  if (cookie.has_expires)
    cef_time_from_basetime(cc.ExpiryDate(), cookie.expires);

  return true;
}

// static
bool CefCookieManagerImpl::GetCefCookie(const GURL& url,
                                        const std::string& cookie_line,
                                        CefCookie& cookie) {
  // Parse the cookie.
  net::ParsedCookie pc(cookie_line);
  if (!pc.IsValid())
    return false;

  std::string cookie_domain;
  if (!GetCookieDomain(url, pc, &cookie_domain))
    return false;

  std::string path_string;
  if (pc.HasPath())
    path_string = pc.Path();
  std::string cookie_path =
      net::CanonicalCookie::CanonPathWithString(url, path_string);
  base::Time creation_time = base::Time::Now();
  base::Time cookie_expires =
      net::CanonicalCookie::CanonExpiration(pc, creation_time, creation_time);

  CefString(&cookie.name).FromString(pc.Name());
  CefString(&cookie.value).FromString(pc.Value());
  CefString(&cookie.domain).FromString(cookie_domain);
  CefString(&cookie.path).FromString(cookie_path);
  cookie.secure = pc.IsSecure();
  cookie.httponly = pc.IsHttpOnly();
  cef_time_from_basetime(creation_time, cookie.creation);
  cef_time_from_basetime(creation_time, cookie.last_access);
  cookie.has_expires = !cookie_expires.is_null();
  if (cookie.has_expires)
    cef_time_from_basetime(cookie_expires, cookie.expires);

  return true;
}

// static
void CefCookieManagerImpl::SetCookieMonsterSchemes(
    net::CookieMonster* cookie_monster,
    const std::vector<std::string>& schemes) {
  CEF_REQUIRE_IOT();

  std::vector<std::string> all_schemes = schemes;

  // Add default schemes that should always support cookies.
  all_schemes.push_back("http");
  all_schemes.push_back("https");
  all_schemes.push_back("ws");
  all_schemes.push_back("wss");

  cookie_monster->SetCookieableSchemes(all_schemes);
}

bool CefCookieManagerImpl::HasContext() {
  CEF_REQUIRE_IOT();
  return (request_context_impl_.get() || request_context_.get());
}

void CefCookieManagerImpl::RunMethodWithContext(
    const CefRequestContextImpl::RequestContextCallback& method) {
  CEF_REQUIRE_IOT();
  if (request_context_impl_.get()) {
    method.Run(request_context_impl_);
  } else if (request_context_.get()) {
    // Try again after the request context is initialized.
    request_context_->GetRequestContextImpl(
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
        method);
  } else {
    NOTREACHED();
  }
}

void CefCookieManagerImpl::InitWithContext(
    CefRefPtr<CefCompletionCallback> callback,
    scoped_refptr<CefURLRequestContextGetterImpl> request_context) {
  CEF_REQUIRE_IOT();

  DCHECK(!request_context_impl_.get());
  request_context_impl_ = request_context;

  // Clear the CefRequestContextImpl reference here to avoid a potential
  // reference loop between CefRequestContextImpl (which has a reference to
  // CefRequestContextHandler), CefRequestContextHandler (which may keep a
  // reference to this object) and this object.
  request_context_ = NULL;

  RunAsyncCompletionOnIOThread(callback);
}

void CefCookieManagerImpl::SetStoragePathWithContext(
    const CefString& path,
    bool persist_session_cookies,
    CefRefPtr<CefCompletionCallback> callback,
    scoped_refptr<CefURLRequestContextGetterImpl> request_context) {
  CEF_REQUIRE_IOT();

  base::FilePath new_path;
  if (!path.empty())
    new_path = base::FilePath(path);

  request_context->SetCookieStoragePath(new_path, persist_session_cookies);

  RunAsyncCompletionOnIOThread(callback);
}

void CefCookieManagerImpl::SetSupportedSchemesWithContext(
    const std::vector<std::string>& schemes,
    CefRefPtr<CefCompletionCallback> callback,
    scoped_refptr<CefURLRequestContextGetterImpl> request_context) {
  CEF_REQUIRE_IOT();

  request_context->SetCookieSupportedSchemes(schemes);

  RunAsyncCompletionOnIOThread(callback);
}

void CefCookieManagerImpl::GetCookieStoreWithContext(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const CookieStoreCallback& callback,
    scoped_refptr<CefURLRequestContextGetterImpl> request_context) {
  CEF_REQUIRE_IOT();
  DCHECK(request_context->GetExistingCookieStore());

  const CookieStoreGetter& cookie_store_getter = base::Bind(
      &CefURLRequestContextGetterImpl::GetExistingCookieStore, request_context);

  if (task_runner->BelongsToCurrentThread()) {
    // Execute the callback immediately.
    callback.Run(cookie_store_getter);
  } else {
    // Execute the callback on the target thread.
    task_runner->PostTask(FROM_HERE, base::Bind(callback, cookie_store_getter));
  }
}

void CefCookieManagerImpl::SetSupportedSchemesInternal(
    const std::vector<std::string>& schemes,
    CefRefPtr<CefCompletionCallback> callback) {
  CEF_REQUIRE_IOT();

  if (HasContext()) {
    RunMethodWithContext(
        base::Bind(&CefCookieManagerImpl::SetSupportedSchemesWithContext, this,
                   schemes, callback));
    return;
  }

  DCHECK(is_blocking_ || cookie_source_);
  if (cookie_source_) {
    cookie_source_->SetCookieSupportedSchemes(schemes);
  }

  RunAsyncCompletionOnIOThread(callback);
}

void CefCookieManagerImpl::VisitAllCookiesInternal(
    CefRefPtr<CefCookieVisitor> visitor,
    const CookieStoreGetter& cookie_store_getter) {
  CEF_REQUIRE_IOT();

  net::CookieStore* cookie_store = cookie_store_getter.Run();
  if (!cookie_store)
    return;

  scoped_refptr<VisitCookiesCallback> callback(
      new VisitCookiesCallback(cookie_store_getter, visitor));

  cookie_store->GetAllCookiesAsync(
      base::Bind(&VisitCookiesCallback::Run, callback.get()));
}

void CefCookieManagerImpl::VisitUrlCookiesInternal(
    const CefString& url,
    bool includeHttpOnly,
    CefRefPtr<CefCookieVisitor> visitor,
    const CookieStoreGetter& cookie_store_getter) {
  CEF_REQUIRE_IOT();

  net::CookieStore* cookie_store = cookie_store_getter.Run();
  if (!cookie_store)
    return;

  net::CookieOptions options;
  if (includeHttpOnly)
    options.set_include_httponly();

  scoped_refptr<VisitCookiesCallback> callback(
      new VisitCookiesCallback(cookie_store_getter, visitor));

  GURL gurl = GURL(url.ToString());
  cookie_store->GetCookieListWithOptionsAsync(
      gurl, options, base::Bind(&VisitCookiesCallback::Run, callback.get()));
}

void CefCookieManagerImpl::SetCookieInternal(
    const GURL& url,
    const CefCookie& cookie,
    CefRefPtr<CefSetCookieCallback> callback,
    const CookieStoreGetter& cookie_store_getter) {
  CEF_REQUIRE_IOT();

  net::CookieStore* cookie_store = cookie_store_getter.Run();
  if (!cookie_store) {
    if (callback.get()) {
      CEF_POST_TASK(CEF_IOT, base::Bind(&CefSetCookieCallback::OnComplete,
                                        callback.get(), false));
    }
    return;
  }

  std::string name = CefString(&cookie.name).ToString();
  std::string value = CefString(&cookie.value).ToString();
  std::string domain = CefString(&cookie.domain).ToString();
  std::string path = CefString(&cookie.path).ToString();

  base::Time expiration_time;
  if (cookie.has_expires)
    cef_time_to_basetime(cookie.expires, expiration_time);

  cookie_store->SetCanonicalCookieAsync(
      net::CanonicalCookie::CreateSanitizedCookie(
          url, name, value, domain, path,
          base::Time(),  // Creation time.
          expiration_time,
          base::Time(),  // Last access time.
          cookie.secure ? true : false, cookie.httponly ? true : false,
          net::CookieSameSite::DEFAULT_MODE, net::COOKIE_PRIORITY_DEFAULT),
      url.scheme(), cookie.httponly ? true : false,
      base::Bind(SetCookieCallbackImpl, callback));
}

void CefCookieManagerImpl::DeleteCookiesInternal(
    const GURL& url,
    const CefString& cookie_name,
    CefRefPtr<CefDeleteCookiesCallback> callback,
    const CookieStoreGetter& cookie_store_getter) {
  CEF_REQUIRE_IOT();

  net::CookieStore* cookie_store = cookie_store_getter.Run();
  if (!cookie_store) {
    if (callback.get()) {
      CEF_POST_TASK(CEF_IOT, base::Bind(&CefDeleteCookiesCallback::OnComplete,
                                        callback.get(), 0));
    }
    return;
  }

  if (url.is_empty()) {
    // Delete all cookies.
    cookie_store->DeleteAllAsync(
        base::Bind(DeleteCookiesCallbackImpl, callback));
  } else if (cookie_name.empty()) {
    // Delete all matching host cookies.
    net::CookieDeletionInfo delete_info;
    delete_info.host = url.host();
    cookie_store->DeleteAllMatchingInfoAsync(
        delete_info, base::Bind(DeleteCookiesCallbackImpl, callback));
  } else {
    // Delete all matching host and domain cookies.
    net::CookieDeletionInfo delete_info;
    delete_info.url = url;
    delete_info.name = cookie_name;
    cookie_store->DeleteAllMatchingInfoAsync(
        delete_info, base::Bind(DeleteCookiesCallbackImpl, callback));
  }
}

void CefCookieManagerImpl::FlushStoreInternal(
    CefRefPtr<CefCompletionCallback> callback,
    const CookieStoreGetter& cookie_store_getter) {
  CEF_REQUIRE_IOT();

  net::CookieStore* cookie_store = cookie_store_getter.Run();
  if (!cookie_store) {
    RunAsyncCompletionOnIOThread(callback);
    return;
  }

  cookie_store->FlushStore(base::Bind(RunAsyncCompletionOnIOThread, callback));
}

// CefCookieManager methods ----------------------------------------------------

// static
CefRefPtr<CefCookieManager> CefCookieManager::GetGlobalManager(
    CefRefPtr<CefCompletionCallback> callback) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return NULL;
  }

  return CefRequestContext::GetGlobalContext()->GetDefaultCookieManager(
      callback);
}

// static
CefRefPtr<CefCookieManager> CefCookieManager::GetBlockingManager() {
  return new CefCookieManagerImpl(true);
}

// static
CefRefPtr<CefCookieManager> CefCookieManager::CreateManager(
    const CefString& path,
    bool persist_session_cookies,
    CefRefPtr<CefCompletionCallback> callback) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return NULL;
  }

  CefRefPtr<CefCookieManagerImpl> cookie_manager =
      new CefCookieManagerImpl(false);
  cookie_manager->Initialize(NULL, path, persist_session_cookies, callback);
  return cookie_manager.get();
}
