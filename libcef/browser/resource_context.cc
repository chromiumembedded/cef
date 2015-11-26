// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/resource_context.h"

#include "libcef/browser/net/url_request_context_getter.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

#if defined(USE_NSS_CERTS)
#include "net/ssl/client_cert_store_nss.h"
#endif

#if defined(OS_WIN)
#include "net/ssl/client_cert_store_win.h"
#endif

#if defined(OS_MACOSX)
#include "net/ssl/client_cert_store_mac.h"
#endif

CefResourceContext::CefResourceContext(
    bool is_off_the_record,
    extensions::InfoMap* extension_info_map,
    CefRefPtr<CefRequestContextHandler> handler)
    : is_off_the_record_(is_off_the_record),
      extension_info_map_(extension_info_map),
      handler_(handler) {
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

scoped_ptr<net::ClientCertStore> CefResourceContext::CreateClientCertStore() {
#if defined(USE_NSS_CERTS)
  return scoped_ptr<net::ClientCertStore>(new net::ClientCertStoreNSS(
      net::ClientCertStoreNSS::PasswordDelegateFactory()));
#elif defined(OS_WIN)
  return scoped_ptr<net::ClientCertStore>(new net::ClientCertStoreWin());
#elif defined(OS_MACOSX)
  return scoped_ptr<net::ClientCertStore>(new net::ClientCertStoreMac());
#elif defined(USE_OPENSSL)
  // OpenSSL does not use the ClientCertStore infrastructure. On Android client
  // cert matching is done by the OS as part of the call to show the cert
  // selection dialog.
  return scoped_ptr<net::ClientCertStore>();
#else
#error Unknown platform.
#endif
}

void CefResourceContext::set_url_request_context_getter(
    scoped_refptr<CefURLRequestContextGetter> getter) {
  DCHECK(!getter_.get());
  getter_ = getter;
}
