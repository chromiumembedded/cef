// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MEDIA_ROUTER_MEDIA_ROUTE_IMPL_H_
#define CEF_LIBCEF_BROWSER_MEDIA_ROUTER_MEDIA_ROUTE_IMPL_H_
#pragma once

#include "include/cef_media_router.h"
#include "libcef/browser/browser_context.h"

#include "components/media_router/common/media_route.h"

// Implementation of the CefMediaRoute interface. Only created on the UI thread.
class CefMediaRouteImpl : public CefMediaRoute {
 public:
  CefMediaRouteImpl(const media_router::MediaRoute& route,
                    const CefBrowserContext::Getter& browser_context_getter);

  CefMediaRouteImpl(const CefMediaRouteImpl&) = delete;
  CefMediaRouteImpl& operator=(const CefMediaRouteImpl&) = delete;

  // CefMediaRoute methods.
  CefString GetId() override;
  CefRefPtr<CefMediaSource> GetSource() override;
  CefRefPtr<CefMediaSink> GetSink() override;
  void SendRouteMessage(const void* message, size_t message_size) override;
  void Terminate() override;

  const media_router::MediaRoute& route() const { return route_; }

 private:
  void SendRouteMessageInternal(std::string message);

  // Read-only after creation.
  const media_router::MediaRoute route_;
  const CefBrowserContext::Getter browser_context_getter_;

  IMPLEMENT_REFCOUNTING(CefMediaRouteImpl);
};

#endif  // CEF_LIBCEF_BROWSER_MEDIA_ROUTER_MEDIA_ROUTE_IMPL_H_
