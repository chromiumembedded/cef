// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/media_router/media_route_impl.h"

#include "libcef/browser/media_router/media_router_manager.h"
#include "libcef/browser/media_router/media_sink_impl.h"
#include "libcef/browser/media_router/media_source_impl.h"
#include "libcef/browser/thread_util.h"

namespace {

// Do not keep a reference to the object returned by this method.
CefBrowserContext* GetBrowserContext(const CefBrowserContext::Getter& getter) {
  CEF_REQUIRE_UIT();
  DCHECK(!getter.is_null());

  // Will return nullptr if the BrowserContext has been shut down.
  return getter.Run();
}

}  // namespace

CefMediaRouteImpl::CefMediaRouteImpl(
    const media_router::MediaRoute& route,
    const CefBrowserContext::Getter& browser_context_getter)
    : route_(route), browser_context_getter_(browser_context_getter) {
  CEF_REQUIRE_UIT();
}

CefString CefMediaRouteImpl::GetId() {
  return route_.media_route_id();
}

CefRefPtr<CefMediaSource> CefMediaRouteImpl::GetSource() {
  return new CefMediaSourceImpl(route_.media_source().id());
}

CefRefPtr<CefMediaSink> CefMediaRouteImpl::GetSink() {
  return new CefMediaSinkImpl(
      route_.media_sink_id(), route_.media_sink_name(),
      route_.media_source().IsDialSource()
          ? media_router::mojom::MediaRouteProviderId::DIAL
          : media_router::mojom::MediaRouteProviderId::CAST);
}

void CefMediaRouteImpl::SendRouteMessage(const void* message,
                                         size_t message_size) {
  std::string message_str(reinterpret_cast<const char*>(message), message_size);

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(
            [](CefRefPtr<CefMediaRouteImpl> self, std::string message_str) {
              self->SendRouteMessageInternal(std::move(message_str));
            },
            CefRefPtr<CefMediaRouteImpl>(this), std::move(message_str)));
    return;
  }

  SendRouteMessageInternal(std::move(message_str));
}

void CefMediaRouteImpl::Terminate() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefMediaRouteImpl::Terminate, this));
    return;
  }

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return;
  }

  browser_context->GetMediaRouterManager()->TerminateRoute(
      route_.media_route_id());
}

void CefMediaRouteImpl::SendRouteMessageInternal(std::string message) {
  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return;
  }

  browser_context->GetMediaRouterManager()->SendRouteMessage(
      route_.media_route_id(), message);
}
