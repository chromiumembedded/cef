// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/media_router/media_router_impl.h"

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

class CefRegistrationImpl : public CefRegistration,
                            public CefMediaRouterManager::Observer {
 public:
  explicit CefRegistrationImpl(CefRefPtr<CefMediaObserver> observer)
      : observer_(observer) {
    DCHECK(observer_);
  }

  CefRegistrationImpl(const CefRegistrationImpl&) = delete;
  CefRegistrationImpl& operator=(const CefRegistrationImpl&) = delete;

  ~CefRegistrationImpl() override {
    CEF_REQUIRE_UIT();

    // May be null if OnMediaRouterDestroyed was called.
    if (browser_context_getter_.is_null()) {
      return;
    }

    auto browser_context = GetBrowserContext(browser_context_getter_);
    if (!browser_context) {
      return;
    }

    browser_context->GetMediaRouterManager()->RemoveObserver(this);
  }

  void Initialize(const CefBrowserContext::Getter& browser_context_getter) {
    CEF_REQUIRE_UIT();
    DCHECK(!browser_context_getter.is_null());
    DCHECK(browser_context_getter_.is_null());
    browser_context_getter_ = browser_context_getter;

    auto browser_context = GetBrowserContext(browser_context_getter_);
    if (!browser_context) {
      return;
    }

    browser_context->GetMediaRouterManager()->AddObserver(this);
  }

 private:
  // CefMediaRouterManager::Observer methods:
  void OnMediaRouterDestroyed() override { browser_context_getter_.Reset(); }

  void OnMediaSinks(
      const CefMediaRouterManager::MediaSinkVector& sinks) override {
    std::vector<CefRefPtr<CefMediaSink>> cef_sinks;
    for (const auto& sink : sinks) {
      cef_sinks.push_back(new CefMediaSinkImpl(sink.sink));
    }
    observer_->OnSinks(cef_sinks);
  }

  void OnMediaRoutes(
      const CefMediaRouterManager::MediaRouteVector& routes) override {
    std::vector<CefRefPtr<CefMediaRoute>> cef_routes;
    for (const auto& route : routes) {
      cef_routes.push_back(MakeCefRoute(route));
    }
    observer_->OnRoutes(cef_routes);
  }

  void OnMediaRouteMessages(
      const media_router::MediaRoute& route,
      const CefMediaRouterManager::MediaMessageVector& messages) override {
    CefRefPtr<CefMediaRoute> cef_route = MakeCefRoute(route);
    for (const auto& message : messages) {
      if (message->type == media_router::mojom::RouteMessage::Type::TEXT) {
        if (message->message.has_value()) {
          const std::string& str = *(message->message);
          observer_->OnRouteMessageReceived(cef_route, str.c_str(), str.size());
        }
      } else if (message->type ==
                 media_router::mojom::RouteMessage::Type::BINARY) {
        if (message->data.has_value()) {
          const std::vector<uint8_t>& data = *(message->data);
          observer_->OnRouteMessageReceived(cef_route, data.data(),
                                            data.size());
        }
      }
    }
  }

  void OnMediaRouteStateChange(
      const media_router::MediaRoute& route,
      const content::PresentationConnectionStateChangeInfo& info) override {
    observer_->OnRouteStateChanged(MakeCefRoute(route),
                                   ToConnectionState(info.state));
  }

  CefRefPtr<CefMediaRoute> MakeCefRoute(const media_router::MediaRoute& route) {
    return new CefMediaRouteImpl(route, browser_context_getter_);
  }

  static CefMediaObserver::ConnectionState ToConnectionState(
      blink::mojom::PresentationConnectionState state) {
    switch (state) {
      case blink::mojom::PresentationConnectionState::CONNECTING:
        return CEF_MRCS_CONNECTING;
      case blink::mojom::PresentationConnectionState::CONNECTED:
        return CEF_MRCS_CONNECTED;
      case blink::mojom::PresentationConnectionState::CLOSED:
        return CEF_MRCS_CLOSED;
      case blink::mojom::PresentationConnectionState::TERMINATED:
        return CEF_MRCS_TERMINATED;
    }
    DCHECK(false);
    return CEF_MRCS_UNKNOWN;
  }

  CefRefPtr<CefMediaObserver> observer_;
  CefBrowserContext::Getter browser_context_getter_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefRegistrationImpl);
};

CefMediaRouterImpl::CefMediaRouterImpl() = default;

void CefMediaRouterImpl::Initialize(
    const CefBrowserContext::Getter& browser_context_getter,
    CefRefPtr<CefCompletionCallback> callback) {
  CEF_REQUIRE_UIT();
  DCHECK(!initialized_);
  DCHECK(!browser_context_getter.is_null());
  DCHECK(browser_context_getter_.is_null());
  browser_context_getter_ = browser_context_getter;

  initialized_ = true;
  if (!init_callbacks_.empty()) {
    for (auto& init_callback : init_callbacks_) {
      std::move(init_callback).Run();
    }
    init_callbacks_.clear();
  }

  if (callback) {
    // Execute client callback asynchronously for consistency.
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefCompletionCallback::OnComplete,
                                          callback.get()));
  }
}

CefRefPtr<CefRegistration> CefMediaRouterImpl::AddObserver(
    CefRefPtr<CefMediaObserver> observer) {
  if (!observer) {
    return nullptr;
  }
  CefRefPtr<CefRegistrationImpl> registration =
      new CefRegistrationImpl(observer);
  StoreOrTriggerInitCallback(base::BindOnce(
      &CefMediaRouterImpl::InitializeRegistrationInternal, this, registration));
  return registration.get();
}

CefRefPtr<CefMediaSource> CefMediaRouterImpl::GetSource(const CefString& urn) {
  if (urn.empty()) {
    return nullptr;
  }

  // Check for a valid URL and supported Cast/DIAL schemes.
  GURL presentation_url(urn.ToString());
  if (!media_router::IsValidPresentationUrl(presentation_url)) {
    return nullptr;
  }

  if (presentation_url.SchemeIsHTTPOrHTTPS()) {
    // We don't support tab/desktop mirroring, which is what Cast uses for
    // arbitrary HTTP/HTTPS URLs (see CastMediaSource).
    return nullptr;
  }

  return new CefMediaSourceImpl(presentation_url);
}

void CefMediaRouterImpl::NotifyCurrentSinks() {
  StoreOrTriggerInitCallback(
      base::BindOnce(&CefMediaRouterImpl::NotifyCurrentSinksInternal, this));
}

void CefMediaRouterImpl::CreateRoute(
    CefRefPtr<CefMediaSource> source,
    CefRefPtr<CefMediaSink> sink,
    CefRefPtr<CefMediaRouteCreateCallback> callback) {
  StoreOrTriggerInitCallback(base::BindOnce(
      &CefMediaRouterImpl::CreateRouteInternal, this, source, sink, callback));
}

void CefMediaRouterImpl::NotifyCurrentRoutes() {
  StoreOrTriggerInitCallback(
      base::BindOnce(&CefMediaRouterImpl::NotifyCurrentRoutesInternal, this));
}

void CefMediaRouterImpl::InitializeRegistrationInternal(
    CefRefPtr<CefRegistrationImpl> registration) {
  DCHECK(ValidContext());

  registration->Initialize(browser_context_getter_);
}

void CefMediaRouterImpl::NotifyCurrentSinksInternal() {
  DCHECK(ValidContext());

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return;
  }

  browser_context->GetMediaRouterManager()->NotifyCurrentSinks();
}

void CefMediaRouterImpl::CreateRouteInternal(
    CefRefPtr<CefMediaSource> source,
    CefRefPtr<CefMediaSink> sink,
    CefRefPtr<CefMediaRouteCreateCallback> callback) {
  DCHECK(ValidContext());

  std::string error;

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    error = "Context is not valid";
  } else if (!source) {
    error = "Source is empty or invalid";
  } else if (!sink) {
    error = "Sink is empty or invalid";
  } else if (!sink->IsCompatibleWith(source)) {
    error = "Sink is not compatible with source";
  }

  if (!error.empty()) {
    LOG(WARNING) << "Media route creation failed: " << error;
    if (callback) {
      callback->OnMediaRouteCreateFinished(CEF_MRCR_UNKNOWN_ERROR, error,
                                           nullptr);
    }
    return;
  }

  auto source_impl = static_cast<CefMediaSourceImpl*>(source.get());
  auto sink_impl = static_cast<CefMediaSinkImpl*>(sink.get());

  browser_context->GetMediaRouterManager()->CreateRoute(
      source_impl->source().id(), sink_impl->sink().id(), url::Origin(),
      base::BindOnce(&CefMediaRouterImpl::CreateRouteCallback, this, callback));
}

void CefMediaRouterImpl::NotifyCurrentRoutesInternal() {
  DCHECK(ValidContext());

  auto browser_context = GetBrowserContext(browser_context_getter_);
  if (!browser_context) {
    return;
  }

  browser_context->GetMediaRouterManager()->NotifyCurrentRoutes();
}

void CefMediaRouterImpl::CreateRouteCallback(
    CefRefPtr<CefMediaRouteCreateCallback> callback,
    const media_router::RouteRequestResult& result) {
  DCHECK(ValidContext());

  if (result.result_code() != media_router::mojom::RouteRequestResultCode::OK) {
    LOG(WARNING) << "Media route creation failed: " << result.error() << " ("
                 << result.result_code() << ")";
  }

  if (!callback) {
    return;
  }

  CefRefPtr<CefMediaRoute> route;
  if (result.result_code() == media_router::mojom::RouteRequestResultCode::OK &&
      result.route()) {
    route = new CefMediaRouteImpl(*result.route(), browser_context_getter_);
  }

  callback->OnMediaRouteCreateFinished(
      static_cast<cef_media_route_create_result_t>(result.result_code()),
      result.error(), route);
}

void CefMediaRouterImpl::StoreOrTriggerInitCallback(
    base::OnceClosure callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefMediaRouterImpl::StoreOrTriggerInitCallback,
                                this, std::move(callback)));
    return;
  }

  if (initialized_) {
    std::move(callback).Run();
  } else {
    init_callbacks_.emplace_back(std::move(callback));
  }
}

bool CefMediaRouterImpl::ValidContext() const {
  return CEF_CURRENTLY_ON_UIT() && initialized_;
}

// CefMediaRouter methods ------------------------------------------------------

// static
CefRefPtr<CefMediaRouter> CefMediaRouter::GetGlobalMediaRouter(
    CefRefPtr<CefCompletionCallback> callback) {
  return CefRequestContext::GetGlobalContext()->GetMediaRouter(callback);
}
