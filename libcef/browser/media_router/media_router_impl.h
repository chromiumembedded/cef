// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_IMPL_H_
#define CEF_LIBCEF_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_IMPL_H_
#pragma once

#include "include/cef_media_router.h"
#include "libcef/browser/browser_context.h"

#include "chrome/common/media_router/mojom/media_router.mojom.h"

class CefRegistrationImpl;

// Implementation of the CefMediaRouter interface. May be created on any thread.
class CefMediaRouterImpl : public CefMediaRouter {
 public:
  CefMediaRouterImpl();

  // Called on the UI thread after object creation and before any other object
  // methods are executed on the UI thread.
  void Initialize(const CefBrowserContext::Getter& browser_context_getter);

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
  void InitializeRegistrationOnUIThread(
      CefRefPtr<CefRegistrationImpl> registration);

  void CreateRouteCallback(CefRefPtr<CefMediaRouteCreateCallback> callback,
                           const media_router::RouteRequestResult& result);

  // Only accessed on the UI thread. Will be non-null after Initialize().
  CefBrowserContext::Getter browser_context_getter_;

  IMPLEMENT_REFCOUNTING(CefMediaRouterImpl);
  DISALLOW_COPY_AND_ASSIGN(CefMediaRouterImpl);
};

#endif  // CEF_LIBCEF_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_IMPL_H_
