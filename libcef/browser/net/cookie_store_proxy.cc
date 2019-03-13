// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net/cookie_store_proxy.h"

#include "include/cef_request_context.h"
#include "libcef/browser/net/cookie_store_source.h"
#include "libcef/browser/thread_util.h"

#include "base/logging.h"
#include "net/cookies/cookie_change_dispatcher.h"

namespace {

class NullCookieChangeDispatcher : public net::CookieChangeDispatcher {
 public:
  NullCookieChangeDispatcher() {}
  ~NullCookieChangeDispatcher() override {}

  // net::CookieChangeDispatcher
  std::unique_ptr<net::CookieChangeSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      net::CookieChangeCallback callback) override {
    return nullptr;
  }

  std::unique_ptr<net::CookieChangeSubscription> AddCallbackForUrl(
      const GURL& url,
      net::CookieChangeCallback callback) override {
    return nullptr;
  }

  std::unique_ptr<net::CookieChangeSubscription> AddCallbackForAllChanges(
      net::CookieChangeCallback callback) override {
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCookieChangeDispatcher);
};

}  // namespace

CefCookieStoreProxy::CefCookieStoreProxy(
    std::unique_ptr<CefCookieStoreSource> source)
    : source_(std::move(source)) {
  CEF_REQUIRE_IOT();
  DCHECK(source_);
}

CefCookieStoreProxy::~CefCookieStoreProxy() {
  CEF_REQUIRE_IOT();
}

void CefCookieStoreProxy::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    SetCookiesCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->SetCookieWithOptionsAsync(url, cookie_line, options,
                                            std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(
        net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
  }
}

void CefCookieStoreProxy::SetCanonicalCookieAsync(
    std::unique_ptr<net::CanonicalCookie> cookie,
    std::string source_scheme,
    bool modify_http_only,
    SetCookiesCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->SetCanonicalCookieAsync(std::move(cookie), source_scheme,
                                          modify_http_only,
                                          std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(
        net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
  }
}

void CefCookieStoreProxy::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookieListCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->GetCookieListWithOptionsAsync(url, options,
                                                std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(net::CookieList(), net::CookieStatusList());
  }
}

void CefCookieStoreProxy::GetAllCookiesAsync(GetCookieListCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->GetAllCookiesAsync(std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(net::CookieList(), net::CookieStatusList());
  }
}

void CefCookieStoreProxy::DeleteCanonicalCookieAsync(
    const net::CanonicalCookie& cookie,
    DeleteCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->DeleteCanonicalCookieAsync(cookie, std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(0);
  }
}

void CefCookieStoreProxy::DeleteAllCreatedInTimeRangeAsync(
    const net::CookieDeletionInfo::TimeRange& creation_range,
    DeleteCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->DeleteAllCreatedInTimeRangeAsync(creation_range,
                                                   std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(0);
  }
}

void CefCookieStoreProxy::DeleteAllMatchingInfoAsync(
    net::CookieDeletionInfo delete_info,
    DeleteCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->DeleteAllMatchingInfoAsync(delete_info, std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(0);
  }
}

void CefCookieStoreProxy::DeleteSessionCookiesAsync(DeleteCallback callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->DeleteSessionCookiesAsync(std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run(0);
  }
}

void CefCookieStoreProxy::FlushStore(base::OnceClosure callback) {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store) {
    cookie_store->FlushStore(std::move(callback));
  } else if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

net::CookieChangeDispatcher& CefCookieStoreProxy::GetChangeDispatcher() {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store)
    return cookie_store->GetChangeDispatcher();

  if (!null_dispatcher_)
    null_dispatcher_.reset(new NullCookieChangeDispatcher());
  return *null_dispatcher_;
}

bool CefCookieStoreProxy::IsEphemeral() {
  net::CookieStore* cookie_store = GetCookieStore();
  if (cookie_store)
    return cookie_store->IsEphemeral();
  return true;
}

net::CookieStore* CefCookieStoreProxy::GetCookieStore() {
  CEF_REQUIRE_IOT();
  return source_->GetCookieStore();
}
