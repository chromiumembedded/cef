// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_IMPL_H_
#define CEF_LIBCEF_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_IMPL_H_
#pragma once

#include "include/cef_media_router.h"
#include "libcef/browser/browser_context.h"

#include "components/media_router/common/mojom/media_router.mojom.h"

class CefRegistrationImpl;

// Implementation of the CefMediaRouter interface. May be created on any thread.
class CefMediaRouterImpl : public CefMediaRouter {
 public:
  CefMediaRouterImpl();

  CefMediaRouterImpl(const CefMediaRouterImpl&) = delete;
  CefMediaRouterImpl& operator=(const CefMediaRouterImpl&) = delete;

  // Called on the UI thread after object creation and before any other object
  // methods are executed on the UI thread.
  void Initialize(const CefBrowserContext::Getter& browser_context_getter,
                  CefRefPtr<CefCompletionCallback> callback);

  // CefMediaRouter methods.
  CefRefPtr<CefRegistration> AddObserver(
      CefRefPtr<CefMediaObserver> observer) override;
  CefRefPtr<CefMediaSource> GetSource(const CefString& urn) override;
  void NotifyCurrentSinks() override;
  void CreateRoute(CefRefPtr<CefMediaSource> source,
                   CefRefPtr<CefMediaSink> sink,
                   CefRefPtr<CefMediaRouteCreateCallback> callback) override;
  void NotifyCurrentRoutes() override;

 private:
  void InitializeRegistrationInternal(
      CefRefPtr<CefRegistrationImpl> registration);
  void NotifyCurrentSinksInternal();
  void CreateRouteInternal(CefRefPtr<CefMediaSource> source,
                           CefRefPtr<CefMediaSink> sink,
                           CefRefPtr<CefMediaRouteCreateCallback> callback);
  void NotifyCurrentRoutesInternal();

  void CreateRouteCallback(CefRefPtr<CefMediaRouteCreateCallback> callback,
                           const media_router::RouteRequestResult& result);

  // If the context is fully initialized execute |callback|, otherwise
  // store it until the context is fully initialized.
  void StoreOrTriggerInitCallback(base::OnceClosure callback);

  bool ValidContext() const;

  // Only accessed on the UI thread. Will be non-null after Initialize().
  CefBrowserContext::Getter browser_context_getter_;

  bool initialized_ = false;
  std::vector<base::OnceClosure> init_callbacks_;

  IMPLEMENT_REFCOUNTING(CefMediaRouterImpl);
};

#endif  // CEF_LIBCEF_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_IMPL_H_
