// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_REQUEST_CONTEXT_IMPL_H_
#define CEF_LIBCEF_BROWSER_REQUEST_CONTEXT_IMPL_H_
#pragma once

#include "base/memory/raw_ptr.h"
#include "cef/include/cef_request_context.h"
#include "cef/libcef/browser/browser_context.h"
#include "cef/libcef/browser/media_router/media_router_impl.h"
#include "cef/libcef/browser/net_service/cookie_manager_impl.h"
#include "cef/libcef/browser/thread_util.h"

namespace content {
struct GlobalRenderFrameHostId;
}

namespace pref_helper {
class Registrar;
}

namespace setting_helper {
class Registrar;
}

class CefBrowserContext;

// Implementation of the CefRequestContext interface. All methods are thread-
// safe unless otherwise indicated. Will be deleted on the UI thread.
class CefRequestContextImpl : public CefRequestContext {
 public:
  CefRequestContextImpl(const CefRequestContextImpl&) = delete;
  CefRequestContextImpl& operator=(const CefRequestContextImpl&) = delete;

  ~CefRequestContextImpl() override;

  // Creates the singleton global RequestContext. Called from
  // AlloyBrowserMainParts::PreMainMessageLoopRun and
  // ChromeBrowserMainExtraPartsCef::PostProfileInit.
  static CefRefPtr<CefRequestContextImpl> CreateGlobalRequestContext(
      const CefRequestContextSettings& settings);

  // Returns a CefRequestContextImpl for the specified |request_context|.
  // Will return the global context if |request_context| is NULL.
  static CefRefPtr<CefRequestContextImpl> GetOrCreateForRequestContext(
      CefRefPtr<CefRequestContext> request_context);

  // Returns a CefRequestContextImpl for the specified |browser_context| and
  // optional |handler|. If |handler| is nullptr, and a CefRequestContextImpl
  // without a handler currently exists for |browser_context|, then that
  // existing CefRequestContextImpl will be returned. Otherwise, a new
  // CefRequestContextImpl will be created with the specified |handler|. Must be
  // called on the UI thread.
  static CefRefPtr<CefRequestContextImpl> GetOrCreateForBrowserContext(
      CefBrowserContext* browser_context,
      CefRefPtr<CefRequestContextHandler> handler);

  // Returns the BrowserContext for the specified |request_context|. Will return
  // the global BrowserContext if |request_context| is NULL.
  static content::BrowserContext* GetBrowserContext(
      CefRefPtr<CefRequestContext> request_context);

  // Returns the Profile for the specified |request_context|. Will return the
  // global Profile if |request_context| is NULL.
  static Profile* GetProfile(CefRefPtr<CefRequestContext> request_context);

  // Verify that the browser context can be directly accessed (e.g. on the UI
  // thread and initialized).
  bool VerifyBrowserContext() const;

  // Returns the browser context object. Can only be called on the UI thread
  // after the browser context has been initialized.
  CefBrowserContext* GetBrowserContext();

  // If the context is fully initialized execute |callback|, otherwise
  // store it until the context is fully initialized.
  void ExecuteWhenBrowserContextInitialized(base::OnceClosure callback);

  // Executes |callback| either synchronously or asynchronously after the
  // browser context object has been initialized. If |task_runner| is NULL the
  // callback will be executed on the originating thread. The resulting getter
  // can only be executed on the UI thread.
  using BrowserContextCallback =
      base::OnceCallback<void(CefBrowserContext::Getter)>;
  void GetBrowserContext(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      BrowserContextCallback callback);

  // CefPreferenceManager methods.
  bool HasPreference(const CefString& name) override;
  CefRefPtr<CefValue> GetPreference(const CefString& name) override;
  CefRefPtr<CefDictionaryValue> GetAllPreferences(
      bool include_defaults) override;
  bool CanSetPreference(const CefString& name) override;
  bool SetPreference(const CefString& name,
                     CefRefPtr<CefValue> value,
                     CefString& error) override;
  CefRefPtr<CefRegistration> AddPreferenceObserver(
      const CefString& name,
      CefRefPtr<CefPreferenceObserver> observer) override;

  // CefRequestContext methods.
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
  void ClearCertificateExceptions(
      CefRefPtr<CefCompletionCallback> callback) override;
  void ClearHttpAuthCredentials(
      CefRefPtr<CefCompletionCallback> callback) override;
  void CloseAllConnections(CefRefPtr<CefCompletionCallback> callback) override;
  void ResolveHost(const CefString& origin,
                   CefRefPtr<CefResolveCallback> callback) override;
  CefRefPtr<CefMediaRouter> GetMediaRouter(
      CefRefPtr<CefCompletionCallback> callback) override;
  CefRefPtr<CefValue> GetWebsiteSetting(
      const CefString& requesting_url,
      const CefString& top_level_url,
      cef_content_setting_types_t content_type) override;
  void SetWebsiteSetting(const CefString& requesting_url,
                         const CefString& top_level_url,
                         cef_content_setting_types_t content_type,
                         CefRefPtr<CefValue> value) override;
  cef_content_setting_values_t GetContentSetting(
      const CefString& requesting_url,
      const CefString& top_level_url,
      cef_content_setting_types_t content_type) override;
  void SetContentSetting(const CefString& requesting_url,
                         const CefString& top_level_url,
                         cef_content_setting_types_t content_type,
                         cef_content_setting_values_t value) override;
  CefRefPtr<CefRegistration> AddSettingObserver(
      CefRefPtr<CefSettingObserver> observer) override;
  void SetChromeColorScheme(cef_color_variant_t variant,
                            cef_color_t user_color) override;
  cef_color_variant_t GetChromeColorSchemeMode() override;
  cef_color_t GetChromeColorSchemeColor() override;
  cef_color_variant_t GetChromeColorSchemeVariant() override;

  const CefRequestContextSettings& settings() const { return config_.settings; }

  // Called from CefBrowserContentsDelegate::RenderFrameCreated when a render
  // frame is created.
  void OnRenderFrameCreated(const content::GlobalRenderFrameHostId& global_id,
                            bool is_main_frame);

  // Called from CefBrowserContentsDelegate::RenderFrameDeleted when a render
  // frame is deleted.
  void OnRenderFrameDeleted(const content::GlobalRenderFrameHostId& global_id,
                            bool is_main_frame);

 private:
  friend class CefRequestContext;

  struct Config {
    // True if wrapping the global context.
    bool is_global = false;

    // Wrap an existing (non-global) browser context. When specifying this value
    // GetOrCreateRequestContext() must be called on the UI thread.
    raw_ptr<CefBrowserContext> browser_context = nullptr;

    // |settings| or |other| will be set when creating a new CefRequestContext
    // via the API.
    CefRequestContextSettings settings;
    CefRefPtr<CefRequestContextImpl> other;

    // Optionally use this handler.
    CefRefPtr<CefRequestContextHandler> handler;

    // Used to uniquely identify CefRequestContext objects before an associated
    // CefBrowserContext has been created. Should be set when creating a new
    // CefRequestContext via the API.
    int unique_id = -1;
  };

  static CefRefPtr<CefRequestContextImpl> GetOrCreateRequestContext(
      Config&& config);

  explicit CefRequestContextImpl(Config&& config);

  void Initialize();
  void BrowserContextInitialized();

  void ClearCertificateExceptionsInternal(
      CefRefPtr<CefCompletionCallback> callback,
      CefBrowserContext::Getter browser_context_getter);
  void ClearHttpAuthCredentialsInternal(
      CefRefPtr<CefCompletionCallback> callback,
      CefBrowserContext::Getter browser_context_getter);
  void CloseAllConnectionsInternal(
      CefRefPtr<CefCompletionCallback> callback,
      CefBrowserContext::Getter browser_context_getter);
  void ResolveHostInternal(const CefString& origin,
                           CefRefPtr<CefResolveCallback> callback,
                           CefBrowserContext::Getter browser_context_getter);
  void SetWebsiteSettingInternal(
      const CefString& requesting_url,
      const CefString& top_level_url,
      cef_content_setting_types_t content_type,
      CefRefPtr<CefValue> value,
      CefBrowserContext::Getter browser_context_getter);
  void SetContentSettingInternal(
      const CefString& requesting_url,
      const CefString& top_level_url,
      cef_content_setting_types_t content_type,
      cef_content_setting_values_t value,
      CefBrowserContext::Getter browser_context_getter);
  void SetChromeColorSchemeInternal(
      cef_color_variant_t variant,
      cef_color_t user_color,
      CefBrowserContext::Getter browser_context_getter);

  void InitializeCookieManagerInternal(
      CefRefPtr<CefCookieManagerImpl> cookie_manager,
      CefRefPtr<CefCompletionCallback> callback);
  void InitializeMediaRouterInternal(CefRefPtr<CefMediaRouterImpl> media_router,
                                     CefRefPtr<CefCompletionCallback> callback);

  CefBrowserContext* browser_context() const;

  // We must disassociate from this on destruction.
  raw_ptr<CefBrowserContext> browser_context_ = nullptr;

  Config config_;

  std::unique_ptr<pref_helper::Registrar> pref_registrar_;
  std::unique_ptr<setting_helper::Registrar> setting_registrar_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefRequestContextImpl);
};

#endif  // CEF_LIBCEF_BROWSER_REQUEST_CONTEXT_IMPL_H_
