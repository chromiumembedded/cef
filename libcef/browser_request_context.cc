// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_request_context.h"
#include "browser_resource_loader_bridge.h"
#include "build/build_config.h"

#include "base/file_path.h"
#include "net/base/cookie_monster.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service.h"
#include "net/base/static_cookie_policy.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "webkit/glue/webkit_glue.h"

BrowserRequestContext::BrowserRequestContext() {
  Init(FilePath(), net::HttpCache::NORMAL, false);
}

BrowserRequestContext::BrowserRequestContext(
    const FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) {
  Init(cache_path, cache_mode, no_proxy);
}

void BrowserRequestContext::Init(
    const FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) {
  cookie_store_ = new net::CookieMonster(NULL, NULL);
  cookie_policy_ = new net::StaticCookiePolicy();

  // hard-code A-L and A-C for test shells
  accept_language_ = "en-us,en";
  accept_charset_ = "iso-8859-1,*,utf-8";

  // Use the system proxy settings.
  scoped_ptr<net::ProxyConfigService> proxy_config_service(
      net::ProxyService::CreateSystemProxyConfigService(
          MessageLoop::current(), NULL));
  host_resolver_ =
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism);
  proxy_service_ = net::ProxyService::Create(proxy_config_service.release(),
                                             false, NULL, NULL, NULL, NULL);
  ssl_config_service_ = net::SSLConfigService::CreateSystemSSLConfigService();

  http_auth_handler_factory_ = net::HttpAuthHandlerFactory::CreateDefault();

  net::HttpCache::DefaultBackend* backend = new net::HttpCache::DefaultBackend(
      cache_path.empty() ? net::MEMORY_CACHE : net::DISK_CACHE,
      cache_path, 0, BrowserResourceLoaderBridge::GetCacheThread());

  net::HttpCache* cache =
      new net::HttpCache(host_resolver_, proxy_service_, ssl_config_service_,
                         http_auth_handler_factory_, NULL, NULL, backend);

  cache->set_mode(cache_mode);
  http_transaction_factory_ = cache;

  ftp_transaction_factory_ = new net::FtpNetworkLayer(host_resolver_);
}

BrowserRequestContext::~BrowserRequestContext() {
  delete ftp_transaction_factory_;
  delete http_transaction_factory_;
  delete http_auth_handler_factory_;
  delete static_cast<net::StaticCookiePolicy*>(cookie_policy_);
}

void BrowserRequestContext::SetAcceptAllCookies(bool accept_all_cookies) {
  net::StaticCookiePolicy::Type policy_type = accept_all_cookies ?
      net::StaticCookiePolicy::ALLOW_ALL_COOKIES :
      net::StaticCookiePolicy::BLOCK_THIRD_PARTY_COOKIES;
  static_cast<net::StaticCookiePolicy*>(cookie_policy())->set_type(policy_type);
}

const std::string& BrowserRequestContext::GetUserAgent(
    const GURL& url) const {
  return webkit_glue::GetUserAgent(url);
}

