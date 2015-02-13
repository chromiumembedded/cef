// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/resource_context.h"
#include "libcef/browser/url_request_context_getter.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

CefResourceContext::CefResourceContext() {
}

CefResourceContext::~CefResourceContext() {
  if (getter_.get()) {
    // When the parent object (ResourceContext) destructor executes all
    // associated URLRequests should be destroyed. If there are no other
    // references it should then be safe to destroy the URLRequestContextGetter
    // which owns the URLRequestContext.
    getter_->AddRef();
    CefURLRequestContextGetter* raw_getter = getter_.get();
    getter_ = NULL;
    content::BrowserThread::ReleaseSoon(
          content::BrowserThread::IO, FROM_HERE, raw_getter);
  }
}

net::HostResolver* CefResourceContext::GetHostResolver() {
  CHECK(getter_.get());
  return getter_->GetHostResolver();
}

net::URLRequestContext* CefResourceContext::GetRequestContext() {
  CHECK(getter_.get());
  return getter_->GetURLRequestContext();
}

void CefResourceContext::set_url_request_context_getter(
    scoped_refptr<CefURLRequestContextGetter> getter) {
  DCHECK(!getter_.get());
  getter_ = getter;
}
