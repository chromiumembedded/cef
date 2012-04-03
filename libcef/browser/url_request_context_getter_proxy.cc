// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/url_request_context_getter_proxy.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/cookie_manager_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"

namespace {

class CefCookieStoreProxy : public net::CookieStore {
 public:
  explicit CefCookieStoreProxy(CefBrowserHostImpl* browser,
                               net::URLRequestContext* parent)
      : parent_(parent),
        browser_(browser) {
  }

  // net::CookieStore methods.
  virtual void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const net::CookieOptions& options,
      const SetCookiesCallback& callback) OVERRIDE {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    cookie_store->SetCookieWithOptionsAsync(url, cookie_line, options,
                                            callback);
  }

  virtual void GetCookiesWithOptionsAsync(
      const GURL& url, const net::CookieOptions& options,
      const GetCookiesCallback& callback) OVERRIDE {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    cookie_store->GetCookiesWithOptionsAsync(url, options, callback);
  }

  void GetCookiesWithInfoAsync(
      const GURL& url,
      const net::CookieOptions& options,
      const GetCookieInfoCallback& callback) OVERRIDE {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    cookie_store->GetCookiesWithInfoAsync(url, options, callback);
  }

  virtual void DeleteCookieAsync(const GURL& url,
                                 const std::string& cookie_name,
                                 const base::Closure& callback) OVERRIDE {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    cookie_store->DeleteCookieAsync(url, cookie_name, callback);
  }

  virtual void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                            const base::Time& delete_end,
                                            const DeleteCallback& callback)
                                            OVERRIDE {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    cookie_store->DeleteAllCreatedBetweenAsync(delete_begin, delete_end,
                                               callback);
  }

  virtual net::CookieMonster* GetCookieMonster() OVERRIDE {
    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore();
    return cookie_store->GetCookieMonster();
  }

 private:
  net::CookieStore* GetCookieStore() {
    CEF_REQUIRE_IOT();

    scoped_refptr<net::CookieStore> cookie_store;

    CefRefPtr<CefClient> client = browser_->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        // Get the manager from the handler.
        CefRefPtr<CefCookieManager> manager =
            handler->GetCookieManager(browser_,
                                      browser_->GetLoadingURL().spec());
        if (manager.get()) {
          cookie_store =
            reinterpret_cast<CefCookieManagerImpl*>(
                manager.get())->cookie_monster();
          DCHECK(cookie_store);
        }
      }
    }

    if (!cookie_store) {
      // Use the global cookie store.
      cookie_store = parent_->cookie_store();
    }

    DCHECK(cookie_store);
    return cookie_store;
  }

  // This pointer is guaranteed by the CefRequestContextProxy object.
  net::URLRequestContext* parent_;
  CefBrowserHostImpl* browser_;

  DISALLOW_COPY_AND_ASSIGN(CefCookieStoreProxy);
};

class CefRequestContextProxy : public net::URLRequestContext {
 public:
  CefRequestContextProxy(CefBrowserHostImpl* browser,
                         net::URLRequestContext* parent)
      : parent_(parent) {
    // Cookie store that proxies to the browser implementation.
    cookie_store_proxy_ = new CefCookieStoreProxy(browser, parent_);
    set_cookie_store(cookie_store_proxy_);

    // All other values refer to the parent request context.
    set_net_log(parent_->net_log());
    set_host_resolver(parent_->host_resolver());
    set_cert_verifier(parent_->cert_verifier());
    set_server_bound_cert_service(parent_->server_bound_cert_service());
    set_fraudulent_certificate_reporter(
        parent_->fraudulent_certificate_reporter());
    set_proxy_service(parent_->proxy_service());
    set_ssl_config_service(parent_->ssl_config_service());
    set_http_auth_handler_factory(parent_->http_auth_handler_factory());
    set_http_transaction_factory(parent_->http_transaction_factory());
    set_ftp_transaction_factory(parent_->ftp_transaction_factory());
    set_network_delegate(parent_->network_delegate());
    set_http_server_properties(parent_->http_server_properties());
    set_transport_security_state(parent_->transport_security_state());
    set_accept_charset(parent_->accept_charset());
    set_accept_language(parent_->accept_language());
    set_referrer_charset(parent_->referrer_charset());
    set_job_factory(parent_->job_factory());
  }

  virtual const std::string& GetUserAgent(const GURL& url) const OVERRIDE {
    return parent_->GetUserAgent(url);
  }

 private:
  scoped_refptr<net::URLRequestContext> parent_;
  scoped_refptr<net::CookieStore> cookie_store_proxy_;

  DISALLOW_COPY_AND_ASSIGN(CefRequestContextProxy);
};

}  // namespace


CefURLRequestContextGetterProxy::CefURLRequestContextGetterProxy(
    CefBrowserHostImpl* browser,
    net::URLRequestContextGetter* parent)
    : browser_(browser),
      parent_(parent) {
  DCHECK(browser);
  DCHECK(parent);
}

net::URLRequestContext*
    CefURLRequestContextGetterProxy::GetURLRequestContext() {
  CEF_REQUIRE_IOT();
  if (!context_proxy_) {
    context_proxy_ =
        new CefRequestContextProxy(browser_, parent_->GetURLRequestContext());
  }
  return context_proxy_;
}

scoped_refptr<base::MessageLoopProxy>
    CefURLRequestContextGetterProxy::GetIOMessageLoopProxy() const {
  return parent_->GetIOMessageLoopProxy();
}
