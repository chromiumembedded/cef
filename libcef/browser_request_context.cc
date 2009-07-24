// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "browser_request_context.h"

#include "net/base/cookie_monster.h"
#include "net/base/host_resolver.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/proxy/proxy_service.h"
#include "webkit/glue/webkit_glue.h"

BrowserRequestContext::BrowserRequestContext() {
  Init(std::wstring(), net::HttpCache::NORMAL, false);
}

BrowserRequestContext::BrowserRequestContext(
    const std::wstring& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) {
  Init(cache_path, cache_mode, no_proxy);
}

void BrowserRequestContext::Init(
    const std::wstring& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) {
  cookie_store_ = new net::CookieMonster();

  // hard-code A-L and A-C for test shells
  accept_language_ = "en-us,en";
  accept_charset_ = "iso-8859-1,*,utf-8";

  net::ProxyConfig proxy_config;
  host_resolver_ = net::CreateSystemHostResolver();
  proxy_service_ = net::ProxyService::Create(no_proxy ? &proxy_config : NULL,
                                             false, NULL, NULL);

  net::HttpCache *cache;
  if (cache_path.empty()) {
    cache = new net::HttpCache(host_resolver_, proxy_service_, 0);
  } else {
    cache = new net::HttpCache(host_resolver_, proxy_service_, cache_path, 0);
  }
  cache->set_mode(cache_mode);
  http_transaction_factory_ = cache;

  ftp_transaction_factory_ = new net::FtpNetworkLayer(host_resolver_);
}

BrowserRequestContext::~BrowserRequestContext() {
  delete cookie_store_;
  delete ftp_transaction_factory_;
  delete http_transaction_factory_;
  delete proxy_service_;
}

const std::string& BrowserRequestContext::GetUserAgent(
    const GURL& url) const {
  return webkit_glue::GetUserAgent(url);
}
