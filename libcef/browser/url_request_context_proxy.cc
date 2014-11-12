// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/url_request_context_proxy.h"

#include <string>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/cookie_manager_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

class CefCookieStoreProxy : public net::CookieStore {
 public:
  CefCookieStoreProxy(net::URLRequestContext* parent,
                      CefRefPtr<CefRequestContextHandler> handler)
      : parent_(parent),
        handler_(handler) {
  }
  ~CefCookieStoreProxy() override {
    CEF_REQUIRE_IOT();
  }

  // net::CookieStore methods.
  void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const net::CookieOptions& options,
      const SetCookiesCallback& callback) override {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    cookie_store->SetCookieWithOptionsAsync(url, cookie_line, options,
                                            callback);
  }

  void GetCookiesWithOptionsAsync(
      const GURL& url, const net::CookieOptions& options,
      const GetCookiesCallback& callback) override {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    cookie_store->GetCookiesWithOptionsAsync(url, options, callback);
  }

  void DeleteCookieAsync(const GURL& url,
                         const std::string& cookie_name,
                         const base::Closure& callback) override {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    cookie_store->DeleteCookieAsync(url, cookie_name, callback);
  }

  void GetAllCookiesForURLAsync(
      const GURL& url,
      const GetCookieListCallback& callback) override {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    cookie_store->GetAllCookiesForURLAsync(url, callback);
  }

  void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                    const base::Time& delete_end,
                                    const DeleteCallback& callback) override {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    cookie_store->DeleteAllCreatedBetweenAsync(delete_begin, delete_end,
                                               callback);
  }

  void DeleteAllCreatedBetweenForHostAsync(
      const base::Time delete_begin,
      const base::Time delete_end,
      const GURL& url,
      const DeleteCallback& callback) override {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    cookie_store->DeleteAllCreatedBetweenForHostAsync(delete_begin, delete_end,
                                                      url, callback);
  }

  void DeleteSessionCookiesAsync(const DeleteCallback& callback) override {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    cookie_store->DeleteSessionCookiesAsync(callback);
  }

  net::CookieMonster* GetCookieMonster() override {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    return cookie_store->GetCookieMonster();
  }

  scoped_ptr<CookieChangedSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      const CookieChangedCallback& callback) override {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    return cookie_store->AddCallbackForCookie(url, name, callback);
  }

 private:
  net::CookieStore* GetCookieStore() {
    CEF_REQUIRE_IOT();

    scoped_refptr<net::CookieStore> cookie_store;

    if (handler_.get()) {
      // Get the manager from the handler.
      CefRefPtr<CefCookieManager> manager = handler_->GetCookieManager();
      if (manager.get()) {
        cookie_store =
          reinterpret_cast<CefCookieManagerImpl*>(
              manager.get())->cookie_monster();
        DCHECK(cookie_store.get());
      }
    }

    if (!cookie_store.get()) {
      // Use the global cookie store.
      cookie_store = parent_->cookie_store();
    }

    DCHECK(cookie_store.get());
    return cookie_store.get();
  }

  // This pointer is guaranteed by the CefRequestContextProxy object.
  net::URLRequestContext* parent_;
  CefRefPtr<CefRequestContextHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(CefCookieStoreProxy);
};

}  // namespace


CefURLRequestContextProxy::CefURLRequestContextProxy(
    net::URLRequestContextGetter* parent)
    : parent_(parent),
      delete_try_count_(0) {
}

CefURLRequestContextProxy::~CefURLRequestContextProxy() {
  CEF_REQUIRE_IOT();
}

void CefURLRequestContextProxy::Initialize(
    CefRefPtr<CefRequestContextHandler> handler) {
  CEF_REQUIRE_IOT();

  net::URLRequestContext* context = parent_->GetURLRequestContext();

  // Cookie store that proxies to the browser implementation.
  cookie_store_proxy_ = new CefCookieStoreProxy(context, handler);
  set_cookie_store(cookie_store_proxy_.get());

  // All other values refer to the parent request context.
  set_net_log(context->net_log());
  set_host_resolver(context->host_resolver());
  set_cert_verifier(context->cert_verifier());
  set_transport_security_state(context->transport_security_state());
  set_channel_id_service(context->channel_id_service());
  set_fraudulent_certificate_reporter(
      context->fraudulent_certificate_reporter());
  set_proxy_service(context->proxy_service());
  set_ssl_config_service(context->ssl_config_service());
  set_http_auth_handler_factory(context->http_auth_handler_factory());
  set_http_transaction_factory(context->http_transaction_factory());
  set_network_delegate(context->network_delegate());
  set_http_server_properties(context->http_server_properties());
  set_transport_security_state(context->transport_security_state());
  set_http_user_agent_settings(const_cast<net::HttpUserAgentSettings*>(
      context->http_user_agent_settings()));
  set_job_factory(context->job_factory());
}
