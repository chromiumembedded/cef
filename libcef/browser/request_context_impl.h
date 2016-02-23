// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
#define CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
#pragma once

#include "include/cef_request_context.h"
#include "libcef/browser/browser_context.h"

// Implementation of the CefRequestContext interface. All methods are thread-
// safe unless otherwise indicated.
class CefRequestContextImpl : public CefRequestContext {
 public:
  ~CefRequestContextImpl() override;

  // Returns a CefRequestContextImpl for the specified |request_context|.
  // Will return the global context if |request_context| is NULL.
  static CefRefPtr<CefRequestContextImpl> GetForRequestContext(
      CefRefPtr<CefRequestContext> request_context);

  // Returns a CefRequestContextImpl for the specified |browser_context|.
  // |browser_context| must be non-NULL.
  static CefRefPtr<CefRequestContextImpl> GetForBrowserContext(
      scoped_refptr<CefBrowserContext> browser_context);

  // Returns the browser context object. Can only be called on the UI thread.
  scoped_refptr<CefBrowserContext> GetBrowserContext();

  // Executes |callback| either synchronously or asynchronously with the browser
  // context object when it's available. If |task_runner| is NULL the callback
  // will be executed on the originating thread. The resulting context object
  // can only be accessed on the UI thread.
  typedef base::Callback<void(scoped_refptr<CefBrowserContext>)>
      BrowserContextCallback;
  void GetBrowserContext(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const BrowserContextCallback& callback);

  // Executes |callback| either synchronously or asynchronously with the request
  // context object when it's available. If |task_runner| is NULL the callback
  // will be executed on the originating thread. The resulting context object
  // can only be accessed on the IO thread.
  typedef base::Callback<void(scoped_refptr<CefURLRequestContextGetterImpl>)>
      RequestContextCallback;
  void GetRequestContextImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const RequestContextCallback& callback);

  bool IsSame(CefRefPtr<CefRequestContext> other) override;
  bool IsSharingWith(CefRefPtr<CefRequestContext> other) override;
  bool IsGlobal() override;
  CefRefPtr<CefRequestContextHandler> GetHandler() override;
  CefString GetCachePath() override;
  CefRefPtr<CefCookieManager> GetDefaultCookieManager(
      CefRefPtr<CefCompletionCallback> callback) override;
  bool RegisterSchemeHandlerFactory(
      const CefString& scheme_name,
      const CefString& domain_name,
      CefRefPtr<CefSchemeHandlerFactory> factory) override;
  bool ClearSchemeHandlerFactories() override;
  void PurgePluginListCache(bool reload_pages) override;
  bool HasPreference(const CefString& name) override;
  CefRefPtr<CefValue> GetPreference(const CefString& name) override;
  CefRefPtr<CefDictionaryValue> GetAllPreferences(
      bool include_defaults) override;
  bool CanSetPreference(const CefString& name) override;
  bool SetPreference(const CefString& name,
                     CefRefPtr<CefValue> value,
                     CefString& error) override;
  void ClearCertificateExceptions(
      CefRefPtr<CefCompletionCallback> callback) override;
  void CloseAllConnections(CefRefPtr<CefCompletionCallback> callback) override;
  void ResolveHost(
      const CefString& origin,
      CefRefPtr<CefResolveCallback> callback) override;
  cef_errorcode_t ResolveHostCached(
      const CefString& origin,
      std::vector<CefString>& resolved_ips) override;

  const CefRequestContextSettings& settings() const { return settings_; }

 private:
  friend class CefRequestContext;

  explicit CefRequestContextImpl(
      scoped_refptr<CefBrowserContext> browser_context);
  CefRequestContextImpl(const CefRequestContextSettings& settings,
                        CefRefPtr<CefRequestContextHandler> handler);
  CefRequestContextImpl(CefRefPtr<CefRequestContextImpl> other,
                        CefRefPtr<CefRequestContextHandler> handler);

  // Make sure the browser context exists. Only called on the UI thread.
  void EnsureBrowserContext();

  void GetBrowserContextOnUIThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const BrowserContextCallback& callback);
  void GetRequestContextImplOnIOThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const RequestContextCallback& callback,
      scoped_refptr<CefBrowserContext> browser_context);

  void RegisterSchemeHandlerFactoryInternal(
      const CefString& scheme_name,
      const CefString& domain_name,
      CefRefPtr<CefSchemeHandlerFactory> factory,
      scoped_refptr<CefURLRequestContextGetterImpl> request_context);
  void ClearSchemeHandlerFactoriesInternal(
      scoped_refptr<CefURLRequestContextGetterImpl> request_context);
  void PurgePluginListCacheInternal(
      bool reload_pages,
      scoped_refptr<CefBrowserContext> browser_context);
  void ClearCertificateExceptionsInternal(
      CefRefPtr<CefCompletionCallback> callback,
      scoped_refptr<CefBrowserContext> browser_context);
  void CloseAllConnectionsInternal(
      CefRefPtr<CefCompletionCallback> callback,
      scoped_refptr<CefURLRequestContextGetterImpl> request_context);
  void ResolveHostInternal(
      const CefString& origin,
      CefRefPtr<CefResolveCallback> callback,
      scoped_refptr<CefURLRequestContextGetterImpl> request_context);

  scoped_refptr<CefBrowserContext> browser_context_;
  CefRequestContextSettings settings_;
  CefRefPtr<CefRequestContextImpl> other_;
  CefRefPtr<CefRequestContextHandler> handler_;

  // Used to uniquely identify CefRequestContext objects before an associated
  // CefBrowserContext has been created.
  int unique_id_;

  // Owned by the CefBrowserContext.
  CefURLRequestContextGetterImpl* request_context_impl_;

  IMPLEMENT_REFCOUNTING(CefRequestContextImpl);
  DISALLOW_COPY_AND_ASSIGN(CefRequestContextImpl);
};

#endif  // CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
