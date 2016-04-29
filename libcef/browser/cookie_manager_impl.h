// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_COOKIE_MANAGER_IMPL_H_
#define CEF_LIBCEF_BROWSER_COOKIE_MANAGER_IMPL_H_

#include <set>

#include "include/cef_cookie.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "net/cookies/cookie_monster.h"

// Implementation of the CefCookieManager interface.
class CefCookieManagerImpl : public CefCookieManager {
 public:
  CefCookieManagerImpl();
  ~CefCookieManagerImpl() override;

  // Must be called immediately after this object is created.
  void Initialize(CefRefPtr<CefRequestContextImpl> request_context,
                  const CefString& path,
                  bool persist_session_cookies,
                  CefRefPtr<CefCompletionCallback> callback);

  // Executes |callback| either synchronously or asynchronously with the
  // CookieStoreGetter when the cookie store object is available. If
  // |task_runner| is NULL the callback will be executed on the originating
  // thread. CookieStoreGetter can only be executed on, and the resulting cookie
  // store object can only be accessed on, the IO thread.
  typedef base::Callback<net::CookieStore*()> CookieStoreGetter;
  typedef base::Callback<void(const CookieStoreGetter&)> CookieStoreCallback;
  void GetCookieStore(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const CookieStoreCallback& callback);

  // Returns the existing cookie store object. Logs an error if the cookie
  // store does not yet exist. Must be called on the IO thread.
  net::CookieStore* GetExistingCookieStore();

  // CefCookieManager methods.
  void SetSupportedSchemes(const std::vector<CefString>& schemes,
                           CefRefPtr<CefCompletionCallback> callback) override;
  bool VisitAllCookies(CefRefPtr<CefCookieVisitor> visitor) override;
  bool VisitUrlCookies(const CefString& url, bool includeHttpOnly,
                       CefRefPtr<CefCookieVisitor> visitor) override;
  bool SetCookie(const CefString& url,
                 const CefCookie& cookie,
                 CefRefPtr<CefSetCookieCallback> callback) override;
  bool DeleteCookies(const CefString& url,
                     const CefString& cookie_name,
                     CefRefPtr<CefDeleteCookiesCallback> callback) override;
  bool SetStoragePath(const CefString& path,
                      bool persist_session_cookies,
                      CefRefPtr<CefCompletionCallback> callback) override;
  bool FlushStore(CefRefPtr<CefCompletionCallback> callback) override;

  static bool GetCefCookie(const net::CanonicalCookie& cc, CefCookie& cookie);
  static bool GetCefCookie(const GURL& url, const std::string& cookie_line,
                           CefCookie& cookie);

  // Set the schemes supported by |cookie_monster|. Default schemes will always
  // be supported.
  static void SetCookieMonsterSchemes(net::CookieMonster* cookie_monster,
                                      const std::vector<std::string>& schemes);

 private:
  // Returns true if a context is or will be available.
  bool HasContext();

  // Execute |method| on the IO thread once the request context is available.
  void RunMethodWithContext(
      const CefRequestContextImpl::RequestContextCallback& method);

  void InitWithContext(
      CefRefPtr<CefCompletionCallback> callback,
      scoped_refptr<CefURLRequestContextGetterImpl> request_context);
  void SetStoragePathWithContext(
      const CefString& path,
      bool persist_session_cookies,
      CefRefPtr<CefCompletionCallback> callback,
      scoped_refptr<CefURLRequestContextGetterImpl> request_context);
  void SetSupportedSchemesWithContext(
      const std::vector<std::string>& schemes,
      CefRefPtr<CefCompletionCallback> callback,
      scoped_refptr<CefURLRequestContextGetterImpl> request_context);
  void GetCookieStoreWithContext(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const CookieStoreCallback& callback,
      scoped_refptr<CefURLRequestContextGetterImpl> request_context);

  void SetSupportedSchemesInternal(
      const std::vector<std::string>& schemes,
      CefRefPtr<CefCompletionCallback> callback);
  void VisitAllCookiesInternal(
      CefRefPtr<CefCookieVisitor> visitor,
      const CookieStoreGetter& cookie_store_getter);
  void VisitUrlCookiesInternal(
      const CefString& url,
      bool includeHttpOnly,
      CefRefPtr<CefCookieVisitor> visitor,
      const CookieStoreGetter& cookie_store_getter);
  void SetCookieInternal(
      const GURL& url,
      const CefCookie& cookie,
      CefRefPtr<CefSetCookieCallback> callback,
      const CookieStoreGetter& cookie_store_getter);
  void DeleteCookiesInternal(
      const GURL& url,
      const CefString& cookie_name,
      CefRefPtr<CefDeleteCookiesCallback> callback,
      const CookieStoreGetter& cookie_store_getter);
  void FlushStoreInternal(
      CefRefPtr<CefCompletionCallback> callback,
      const CookieStoreGetter& cookie_store_getter);

  // Used for cookie monsters owned by the context.
  CefRefPtr<CefRequestContextImpl> request_context_;
  scoped_refptr<CefURLRequestContextGetterImpl> request_context_impl_;

  // Used for cookie monsters owned by this object.
  base::FilePath storage_path_;
  std::vector<std::string> supported_schemes_;
  std::unique_ptr<net::CookieMonster> cookie_store_;

  // Must be the last member.
  base::WeakPtrFactory<CefCookieManagerImpl> weak_ptr_factory_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_IOT(CefCookieManagerImpl);
};

#endif  // CEF_LIBCEF_BROWSER_COOKIE_MANAGER_IMPL_H_
