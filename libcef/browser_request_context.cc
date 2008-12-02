// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "browser_request_context.h"

#include "net/base/cookie_monster.h"
#include "webkit/glue/webkit_glue.h"

BrowserRequestContext::BrowserRequestContext() {
  Init(std::wstring(), net::HttpCache::NORMAL);
}

BrowserRequestContext::BrowserRequestContext(
    const std::wstring& cache_path,
    net::HttpCache::Mode cache_mode) {
  Init(cache_path, cache_mode);
}

void BrowserRequestContext::Init(
    const std::wstring& cache_path,
    net::HttpCache::Mode cache_mode) {
  cookie_store_ = new net::CookieMonster();

  user_agent_ = webkit_glue::GetUserAgent();

  // hard-code A-L and A-C for test shells
  accept_language_ = "en-us,en";
  accept_charset_ = "iso-8859-1,*,utf-8";

  net::HttpCache *cache;
  if (cache_path.empty()) {
    cache = new net::HttpCache(NULL, 0);
  } else {
    cache = new net::HttpCache(NULL, cache_path, 0);
  }
  cache->set_mode(cache_mode);
  http_transaction_factory_ = cache;
}

BrowserRequestContext::~BrowserRequestContext() {
  delete cookie_store_;
  delete http_transaction_factory_;
}

