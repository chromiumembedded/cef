// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
#define CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
#pragma once

#include "include/cef_request_context.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/thread_util.h"

class CefBrowserContextImpl;
class CefBrowserContextProxy;

// Implementation of the CefRequestContext interface. All methods are thread-
// safe unless otherwise indicated. Will be deleted on the UI thread.
class CefRequestContextImpl : public CefRequestContext {
 public:
  ~CefRequestContextImpl() override;

  // Returns a CefRequestContextImpl for the specified |request_context|.
  // Will return the global context if |request_context| is NULL.
  static CefRefPtr<CefRequestContextImpl> GetOrCreateForRequestContext(
      CefRefPtr<CefRequestContext> request_context);

  // Returns a CefRequestContextImpl for the specified |browser_context|.
  // |browser_context| must be non-NULL.
  static CefRefPtr<CefRequestContextImpl> CreateForBrowserContext(
      CefBrowserContext* browser_context);

  // Returns the browser context object. Can only be called on the UI thread.
  CefBrowserContext* GetBrowserContext();

  // Executes |callback| either synchronously or asynchronously with the browser
  // context object when it's available. If |task_runner| is NULL the callback
  // will be executed on the originating thread. The resulting context object
  // can only be accessed on the UI thread.
  typedef base::Callback<void(CefBrowserContext*)> BrowserContextCallback;
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

  const CefRequestContextSettings& settings() const { return config_.settings; }

 private:
  friend class CefRequestContext;

  struct Config {
    // True if wrapping the global context.
    bool is_global = false;

    // |settings| or |other| will be set when creating a new CefRequestContext
    // via the API. When wrapping an existing CefBrowserContext* both will be
    // empty and Initialize(CefBrowserContext*) will be called immediately after
    // CefRequestContextImpl construction.
    CefRequestContextSettings settings;
    CefRefPtr<CefRequestContextImpl> other;

    // Optionally use this handler, in which case a CefBrowserContextProxy will
    // be created.
    CefRefPtr<CefRequestContextHandler> handler;

    // Used to uniquely identify CefRequestContext objects before an associated
    // CefBrowserContext has been created. Should be set when a new
    // CefRequestContext via the API.
    int unique_id = -1;
  };

  explicit CefRequestContextImpl(const Config& config);

  void Initialize();
  void Initialize(CefBrowserContext* other_browser_context);

  // Make sure the browser context exists. Only called on the UI thread.
  void EnsureBrowserContext();

  void GetBrowserContextOnUIThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const BrowserContextCallback& callback);
  void GetRequestContextImplOnIOThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const RequestContextCallback& callback,
      CefBrowserContext* browser_context);

  void RegisterSchemeHandlerFactoryInternal(
      const CefString& scheme_name,
      const CefString& domain_name,
      CefRefPtr<CefSchemeHandlerFactory> factory,
      scoped_refptr<CefURLRequestContextGetterImpl> request_context);
  void ClearSchemeHandlerFactoriesInternal(
      scoped_refptr<CefURLRequestContextGetterImpl> request_context);
  void PurgePluginListCacheInternal(
      bool reload_pages,
      CefBrowserContext* browser_context);
  void ClearCertificateExceptionsInternal(
      CefRefPtr<CefCompletionCallback> callback,
      CefBrowserContext* browser_context);
  void CloseAllConnectionsInternal(
      CefRefPtr<CefCompletionCallback> callback,
      scoped_refptr<CefURLRequestContextGetterImpl> request_context);
  void ResolveHostInternal(
      const CefString& origin,
      CefRefPtr<CefResolveCallback> callback,
      scoped_refptr<CefURLRequestContextGetterImpl> request_context);

  CefBrowserContext* browser_context() const;

  // If *Impl then we must disassociate from it on destruction.
  CefBrowserContextImpl* browser_context_impl_ = nullptr;
  // If *Proxy then we own it.
  std::unique_ptr<CefBrowserContextProxy> browser_context_proxy_;

  Config config_;

  // Owned by the CefBrowserContext.
  CefURLRequestContextGetterImpl* request_context_impl_ = nullptr;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefRequestContextImpl);
  DISALLOW_COPY_AND_ASSIGN(CefRequestContextImpl);
};

#endif  // CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
