// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net/url_request_context_proxy.h"

#include "libcef/browser/net/cookie_store_proxy.h"
#include "libcef/browser/net/url_request_context_impl.h"
#include "libcef/browser/thread_util.h"

CefURLRequestContextProxy::CefURLRequestContextProxy(
    CefURLRequestContextImpl* parent,
    CefRefPtr<CefRequestContextHandler> handler) {
  CEF_REQUIRE_IOT();
  DCHECK(parent);
  DCHECK(handler.get());

  // Cookie store that proxies to the browser implementation.
  cookie_store_proxy_.reset(new CefCookieStoreProxy(parent, handler));
  set_cookie_store(cookie_store_proxy_.get());

  // All other values refer to the parent request context.
  set_net_log(parent->net_log());
  set_host_resolver(parent->host_resolver());
  set_cert_verifier(parent->cert_verifier());
  set_transport_security_state(parent->transport_security_state());
  set_cert_transparency_verifier(parent->cert_transparency_verifier());
  set_ct_policy_enforcer(parent->ct_policy_enforcer());
  set_channel_id_service(parent->channel_id_service());
  set_proxy_service(parent->proxy_service());
  set_ssl_config_service(parent->ssl_config_service());
  set_http_auth_handler_factory(parent->http_auth_handler_factory());
  set_http_transaction_factory(parent->http_transaction_factory());
  set_network_delegate(parent->network_delegate());
  set_http_server_properties(parent->http_server_properties());
  set_transport_security_state(parent->transport_security_state());
  set_http_user_agent_settings(const_cast<net::HttpUserAgentSettings*>(
      parent->http_user_agent_settings()));
  set_job_factory(parent->job_factory());
}

CefURLRequestContextProxy::~CefURLRequestContextProxy() {
  CEF_REQUIRE_IOT();
}
