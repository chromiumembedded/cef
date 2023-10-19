// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/media_router/media_router_manager.h"

#include "libcef/browser/browser_context.h"
#include "libcef/browser/thread_util.h"

#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/browser/presentation_connection_message_observer.h"
#include "components/media_router/browser/route_message_util.h"

namespace {

const int kTimeoutMs = 5 * 1000;
const char kDefaultPresentationUrl[] = "https://google.com";

}  // namespace

class CefMediaRoutesObserver : public media_router::MediaRoutesObserver {
 public:
  explicit CefMediaRoutesObserver(CefMediaRouterManager* manager)
      : media_router::MediaRoutesObserver(manager->GetMediaRouter()),
        manager_(manager) {}

  CefMediaRoutesObserver(const CefMediaRoutesObserver&) = delete;
  CefMediaRoutesObserver& operator=(const CefMediaRoutesObserver&) = delete;

  void OnRoutesUpdated(
      const std::vector<media_router::MediaRoute>& routes) override {
    manager_->routes_ = routes;
    manager_->NotifyCurrentRoutes();
  }

 private:
  CefMediaRouterManager* const manager_;
};

// Used to receive messages if PresentationConnection is not supported.
class CefPresentationConnectionMessageObserver
    : public media_router::PresentationConnectionMessageObserver {
 public:
  CefPresentationConnectionMessageObserver(
      CefMediaRouterManager* manager,
      const media_router::MediaRoute& route)
      : media_router::PresentationConnectionMessageObserver(
            manager->GetMediaRouter(),
            route.media_route_id()),
        manager_(manager),
        route_(route) {}

  CefPresentationConnectionMessageObserver(
      const CefPresentationConnectionMessageObserver&) = delete;
  CefPresentationConnectionMessageObserver& operator=(
      const CefPresentationConnectionMessageObserver&) = delete;

  void OnMessagesReceived(
      CefMediaRouterManager::MediaMessageVector messages) override {
    manager_->OnMessagesReceived(route_, messages);
  }

 private:
  CefMediaRouterManager* const manager_;
  const media_router::MediaRoute route_;
};

// Used for messaging and route status notifications with Cast.
class CefPresentationConnection : public blink::mojom::PresentationConnection {
 public:
  explicit CefPresentationConnection(
      CefMediaRouterManager* manager,
      const media_router::MediaRoute& route,
      media_router::mojom::RoutePresentationConnectionPtr connections)
      : manager_(manager),
        route_(route),
        connection_receiver_(this, std::move(connections->connection_receiver)),
        connection_remote_(std::move(connections->connection_remote)) {}

  CefPresentationConnection(const CefPresentationConnection&) = delete;
  CefPresentationConnection& operator=(const CefPresentationConnection&) =
      delete;

  void OnMessage(
      blink::mojom::PresentationConnectionMessagePtr message) override {
    CefMediaRouterManager::MediaMessageVector messages;
    if (message->is_message()) {
      messages.push_back(media_router::message_util::RouteMessageFromString(
          message->get_message()));
    } else if (message->is_data()) {
      messages.push_back(media_router::message_util::RouteMessageFromData(
          message->get_data()));
    }
    if (!messages.empty()) {
      manager_->OnMessagesReceived(route_, messages);
    }
  }

  void DidChangeState(
      blink::mojom::PresentationConnectionState state) override {
    // May result in |this| being deleted, so post async and allow the call
    // stack to unwind.
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefMediaRouterManager::OnRouteStateChange,
                       manager_->weak_ptr_factory_.GetWeakPtr(), route_,
                       content::PresentationConnectionStateChangeInfo(state)));
  }

  void DidClose(
      blink::mojom::PresentationConnectionCloseReason reason) override {
    DidChangeState(blink::mojom::PresentationConnectionState::CLOSED);
  }

  void SendRouteMessage(const std::string& message) {
    connection_remote_->OnMessage(
        blink::mojom::PresentationConnectionMessage::NewMessage(message));
  }

 private:
  CefMediaRouterManager* const manager_;
  const media_router::MediaRoute route_;

  // Used to receive messages from the MRP.
  mojo::Receiver<blink::mojom::PresentationConnection> connection_receiver_;

  // Used to send messages to the MRP.
  mojo::Remote<blink::mojom::PresentationConnection> connection_remote_;
};

CefMediaRouterManager::CefMediaRouterManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      query_result_manager_(GetMediaRouter()),
      weak_ptr_factory_(this) {
  // Perform initialization.
  GetMediaRouter()->OnUserGesture();

  query_result_manager_.AddObserver(this);

  // A non-empty presentation URL to required for discovery of Cast devices.
  query_result_manager_.SetSourcesForCastMode(
      media_router::MediaCastMode::PRESENTATION,
      {media_router::MediaSource::ForPresentationUrl(
          GURL(kDefaultPresentationUrl))},
      url::Origin());

  routes_observer_ = std::make_unique<CefMediaRoutesObserver>(this);
}

CefMediaRouterManager::~CefMediaRouterManager() {
  CEF_REQUIRE_UIT();
  for (auto& observer : observers_) {
    observers_.RemoveObserver(&observer);
    observer.OnMediaRouterDestroyed();
  }

  query_result_manager_.RemoveObserver(this);
}

void CefMediaRouterManager::AddObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.AddObserver(observer);
}

void CefMediaRouterManager::RemoveObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.RemoveObserver(observer);
}

void CefMediaRouterManager::NotifyCurrentSinks() {
  CEF_REQUIRE_UIT();
  for (auto& observer : observers_) {
    observer.OnMediaSinks(sinks_);
  }
}

void CefMediaRouterManager::NotifyCurrentRoutes() {
  CEF_REQUIRE_UIT();
  for (auto& observer : observers_) {
    observer.OnMediaRoutes(routes_);
  }
}

void CefMediaRouterManager::CreateRoute(
    const media_router::MediaSource::Id& source_id,
    const media_router::MediaSink::Id& sink_id,
    const url::Origin& origin,
    CreateRouteResultCallback callback) {
  GetMediaRouter()->CreateRoute(
      source_id, sink_id, origin, /*web_contents=*/nullptr,
      base::BindOnce(&CefMediaRouterManager::OnCreateRoute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      base::Milliseconds(kTimeoutMs));
}

void CefMediaRouterManager::SendRouteMessage(
    const media_router::MediaRoute::Id& route_id,
    const std::string& message) {
  // Must use PresentationConnection to send messages if it exists.
  auto state = GetRouteState(route_id);
  if (state && state->presentation_connection_) {
    state->presentation_connection_->SendRouteMessage(message);
    return;
  }

  GetMediaRouter()->SendRouteMessage(route_id, message);
}

void CefMediaRouterManager::TerminateRoute(
    const media_router::MediaRoute::Id& route_id) {
  GetMediaRouter()->TerminateRoute(route_id);
}

void CefMediaRouterManager::OnSinksUpdated(const MediaSinkVector& sinks) {
  sinks_ = sinks;
  NotifyCurrentSinks();
}

media_router::MediaRouter* CefMediaRouterManager::GetMediaRouter() const {
  CEF_REQUIRE_UIT();
  return media_router::MediaRouterFactory::GetApiForBrowserContext(
      browser_context_);
}

void CefMediaRouterManager::OnCreateRoute(
    CreateRouteResultCallback callback,
    media_router::mojom::RoutePresentationConnectionPtr connection,
    const media_router::RouteRequestResult& result) {
  CEF_REQUIRE_UIT();
  if (result.route()) {
    CreateRouteState(*result.route(), std::move(connection));
  }

  std::move(callback).Run(result);
}

void CefMediaRouterManager::OnRouteStateChange(
    const media_router::MediaRoute& route,
    const content::PresentationConnectionStateChangeInfo& info) {
  CEF_REQUIRE_UIT();
  if (info.state == blink::mojom::PresentationConnectionState::CLOSED ||
      info.state == blink::mojom::PresentationConnectionState::TERMINATED) {
    RemoveRouteState(route.media_route_id());
  }

  for (auto& observer : observers_) {
    observer.OnMediaRouteStateChange(route, info);
  }
}

void CefMediaRouterManager::OnMessagesReceived(
    const media_router::MediaRoute& route,
    const MediaMessageVector& messages) {
  CEF_REQUIRE_UIT();
  for (auto& observer : observers_) {
    observer.OnMediaRouteMessages(route, messages);
  }
}

void CefMediaRouterManager::CreateRouteState(
    const media_router::MediaRoute& route,
    media_router::mojom::RoutePresentationConnectionPtr connection) {
  const auto route_id = route.media_route_id();
  auto state = std::make_unique<RouteState>();

  if (!connection.is_null()) {
    // PresentationConnection must be used for messaging and status
    // notifications if it exists.
    state->presentation_connection_ =
        std::make_unique<CefPresentationConnection>(this, route,
                                                    std::move(connection));
  } else {
    // Fallback if PresentationConnection is not supported.
    state->message_observer_ =
        std::make_unique<CefPresentationConnectionMessageObserver>(this, route);
    state->state_subscription_ =
        GetMediaRouter()->AddPresentationConnectionStateChangedCallback(
            route_id,
            base::BindRepeating(&CefMediaRouterManager::OnRouteStateChange,
                                weak_ptr_factory_.GetWeakPtr(), route));
  }

  route_state_map_.insert(std::make_pair(route_id, std::move(state)));
}

CefMediaRouterManager::RouteState* CefMediaRouterManager::GetRouteState(
    const media_router::MediaRoute::Id& route_id) {
  const auto it = route_state_map_.find(route_id);
  if (it != route_state_map_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void CefMediaRouterManager::RemoveRouteState(
    const media_router::MediaRoute::Id& route_id) {
  auto it = route_state_map_.find(route_id);
  if (it != route_state_map_.end()) {
    route_state_map_.erase(it);
  }
}
