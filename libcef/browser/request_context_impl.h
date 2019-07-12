// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
#define CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
#pragma once

#include "include/cef_request_context.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/thread_util.h"

class CefBrowserContext;

// Implementation of the CefRequestContext interface. All methods are thread-
// safe unless otherwise indicated. Will be deleted on the UI thread.
class CefRequestContextImpl : public CefRequestContext {
 public:
  ~CefRequestContextImpl() override;

  // Creates the singleton global RequestContext. Called from
  // CefBrowserMainParts::PreMainMessageLoopRun.
  static CefRefPtr<CefRequestContextImpl> CreateGlobalRequestContext(
      const CefRequestContextSettings& settings);

  // Returns a CefRequestContextImpl for the specified |request_context|.
  // Will return the global context if |request_context| is NULL.
  static CefRefPtr<CefRequestContextImpl> GetOrCreateForRequestContext(
      CefRefPtr<CefRequestContext> request_context);

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
  typedef base::Callback<void(scoped_refptr<CefURLRequestContextGetter>)>
      RequestContextCallback;
  void GetRequestContextImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const RequestContextCallback& callback);

  bool IsSame(CefRefPtr<CefRequestContext> other) override;
  bool IsSharingWith(CefRefPtr<CefRequestContext> other) override;
  bool IsGlobal() override;
  CefRefPtr<CefRequestContextHandler> GetHandler() override;
  CefString GetCachePath() override;
  CefRefPtr<CefCookieManager> GetCookieManager(
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
  void ClearHttpAuthCredentials(
      CefRefPtr<CefCompletionCallback> callback) override;
  void CloseAllConnections(CefRefPtr<CefCompletionCallback> callback) override;
  void ResolveHost(const CefString& origin,
                   CefRefPtr<CefResolveCallback> callback) override;
  void LoadExtension(const CefString& root_directory,
                     CefRefPtr<CefDictionaryValue> manifest,
                     CefRefPtr<CefExtensionHandler> handler) override;
  bool DidLoadExtension(const CefString& extension_id) override;
  bool HasExtension(const CefString& extension_id) override;
  bool GetExtensions(std::vector<CefString>& extension_ids) override;
  CefRefPtr<CefExtension> GetExtension(const CefString& extension_id) override;

  const CefRequestContextSettings& settings() const { return config_.settings; }

  // Called from CefBrowserHostImpl::RenderFrameCreated or
  // CefMimeHandlerViewGuestDelegate::OnGuestAttached when a render frame is
  // created.
  void OnRenderFrameCreated(int render_process_id,
                            int render_frame_id,
                            int frame_tree_node_id,
                            bool is_main_frame,
                            bool is_guest_view);

  // Called from CefBrowserHostImpl::FrameDeleted or
  // CefMimeHandlerViewGuestDelegate::OnGuestDetached when a render frame is
  // deleted.
  void OnRenderFrameDeleted(int render_process_id,
                            int render_frame_id,
                            int frame_tree_node_id,
                            bool is_main_frame,
                            bool is_guest_view);

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

    // Optionally use this handler.
    CefRefPtr<CefRequestContextHandler> handler;

    // Used to uniquely identify CefRequestContext objects before an associated
    // CefBrowserContext has been created. Should be set when a new
    // CefRequestContext via the API.
    int unique_id = -1;
  };

  static CefRefPtr<CefRequestContextImpl> GetOrCreateRequestContext(
      const Config& config);

  explicit CefRequestContextImpl(const Config& config);

  void Initialize();

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
      scoped_refptr<CefURLRequestContextGetter> request_context);
  void ClearSchemeHandlerFactoriesInternal(
      scoped_refptr<CefURLRequestContextGetter> request_context);
  void PurgePluginListCacheInternal(bool reload_pages,
                                    CefBrowserContext* browser_context);
  void ClearCertificateExceptionsInternal(
      CefRefPtr<CefCompletionCallback> callback,
      CefBrowserContext* browser_context);
  void ClearHttpAuthCredentialsInternal(
      CefRefPtr<CefCompletionCallback> callback,
      CefBrowserContext* browser_context);
  void ClearHttpAuthCredentialsInternalOld(
      CefRefPtr<CefCompletionCallback> callback,
      scoped_refptr<CefURLRequestContextGetter> request_context);
  void CloseAllConnectionsInternal(CefRefPtr<CefCompletionCallback> callback,
                                   CefBrowserContext* browser_context);
  void CloseAllConnectionsInternalOld(
      CefRefPtr<CefCompletionCallback> callback,
      scoped_refptr<CefURLRequestContextGetter> request_context);
  void ResolveHostInternal(const CefString& origin,
                           CefRefPtr<CefResolveCallback> callback,
                           CefBrowserContext* browser_context);
  void ResolveHostInternalOld(
      const CefString& origin,
      CefRefPtr<CefResolveCallback> callback,
      scoped_refptr<CefURLRequestContextGetter> request_context);

  CefBrowserContext* browser_context() const;

  // We must disassociate from this on destruction.
  CefBrowserContext* browser_context_ = nullptr;

  Config config_;

  // Owned by the CefBrowserContext.
  CefURLRequestContextGetter* request_context_getter_ = nullptr;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefRequestContextImpl);
  DISALLOW_COPY_AND_ASSIGN(CefRequestContextImpl);
};

#endif  // CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
