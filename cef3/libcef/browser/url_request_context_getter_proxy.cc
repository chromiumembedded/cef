// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/url_request_context_getter_proxy.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/url_request_context_getter.h"
#include "libcef/browser/url_request_context_proxy.h"

CefURLRequestContextGetterProxy::CefURLRequestContextGetterProxy(
    CefBrowserHostImpl* browser,
    CefURLRequestContextGetter* parent)
    : browser_(browser),
      parent_(parent),
      context_proxy_(NULL) {
  DCHECK(browser);
  DCHECK(parent);
}

CefURLRequestContextGetterProxy::~CefURLRequestContextGetterProxy() {
  CEF_REQUIRE_IOT();
  if (context_proxy_)
    parent_->ReleaseURLRequestContextProxy(context_proxy_);
}

net::URLRequestContext*
    CefURLRequestContextGetterProxy::GetURLRequestContext() {
  CEF_REQUIRE_IOT();
  if (!context_proxy_) {
    context_proxy_ = parent_->CreateURLRequestContextProxy();
    context_proxy_->Initialize(browser_);
  }
  return context_proxy_;
}

scoped_refptr<base::SingleThreadTaskRunner>
    CefURLRequestContextGetterProxy::GetNetworkTaskRunner() const {
  return parent_->GetNetworkTaskRunner();
}
