// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser_request_context_proxy.h"
#include "libcef/browser_impl.h"
#include "libcef/browser_request_context.h"
#include "libcef/cookie_store_proxy.h"

BrowserRequestContextProxy::BrowserRequestContextProxy(
    BrowserRequestContext* context,
    CefBrowserImpl* browser)
  : context_(context),
    browser_(browser) {
  DCHECK(context_);
  DCHECK(browser_);

  // Cookie store that proxies to the browser implementation.
  set_cookie_store(new CefCookieStoreProxy(browser_));

  // All other values refer to the global request context.
  set_net_log(context->net_log());
  set_host_resolver(context->host_resolver());
  set_cert_verifier(context->cert_verifier());
  set_server_bound_cert_service(context->server_bound_cert_service());
  set_fraudulent_certificate_reporter(
      context->fraudulent_certificate_reporter());
  set_proxy_service(context->proxy_service());
  set_ssl_config_service(context->ssl_config_service());
  set_http_auth_handler_factory(context->http_auth_handler_factory());
  set_http_transaction_factory(context->http_transaction_factory());
  set_ftp_transaction_factory(context->ftp_transaction_factory());
  set_network_delegate(context->network_delegate());
  set_http_server_properties(context->http_server_properties());
  set_transport_security_state(context->transport_security_state());
  set_accept_charset(context->accept_charset());
  set_accept_language(context->accept_language());
  set_referrer_charset(context->referrer_charset());
  set_job_factory(context->job_factory());
}

const std::string&
    BrowserRequestContextProxy::GetUserAgent(const GURL& url) const {
  return context_->GetUserAgent(url);
}
