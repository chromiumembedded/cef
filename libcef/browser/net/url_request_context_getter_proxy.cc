// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net/url_request_context_getter_proxy.h"

#include "libcef/browser/net/url_request_context_getter.h"
#include "libcef/browser/net/url_request_context_proxy.h"
#include "libcef/browser/thread_util.h"

CefURLRequestContextGetterProxy::CefURLRequestContextGetterProxy(
    CefRefPtr<CefRequestContextHandler> handler,
    scoped_refptr<CefURLRequestContextGetterImpl> parent)
    : handler_(handler),
      parent_(parent) {
  DCHECK(handler_.get());
  DCHECK(parent_.get());
}

CefURLRequestContextGetterProxy::~CefURLRequestContextGetterProxy() {
  CEF_REQUIRE_IOT();
}

void CefURLRequestContextGetterProxy::ShutdownOnUIThread() {
  CEF_REQUIRE_UIT();
  CEF_POST_TASK(CEF_IOT,
      base::Bind(&CefURLRequestContextGetterProxy::ShutdownOnIOThread, this));
}

void CefURLRequestContextGetterProxy::ShutdownOnIOThread() {
  CEF_REQUIRE_IOT();
  shutting_down_ = true;
  context_proxy_.reset();
  NotifyContextShuttingDown();
}

net::URLRequestContext*
    CefURLRequestContextGetterProxy::GetURLRequestContext() {
  CEF_REQUIRE_IOT();

  if (shutting_down_)
    return nullptr;

  if (!context_proxy_) {
    context_proxy_.reset(
        new CefURLRequestContextProxy(static_cast<CefURLRequestContextImpl*>(
                                          parent_->GetURLRequestContext()),
                                      handler_));
  }
  return context_proxy_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
    CefURLRequestContextGetterProxy::GetNetworkTaskRunner() const {
  return parent_->GetNetworkTaskRunner();
}

net::HostResolver* CefURLRequestContextGetterProxy::GetHostResolver() const {
  return parent_->GetHostResolver();
}
