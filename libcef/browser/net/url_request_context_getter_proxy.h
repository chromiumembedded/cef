// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_PROXY_H_
#define CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_PROXY_H_
#pragma once

#include "include/cef_request_context_handler.h"
#include "libcef/browser/net/url_request_context_getter.h"
#include "libcef/browser/net/url_request_context_getter_impl.h"


class CefURLRequestContextProxy;

// URLRequestContextGetter implementation for a particular CefRequestContext.
// Life span is primarily controlled by CefResourceContext. Created on the UI
// thread but accessed and destroyed on the IO thread. See browser_context.h for
// an object relationship diagram.
class CefURLRequestContextGetterProxy : public CefURLRequestContextGetter {
 public:
  CefURLRequestContextGetterProxy(
      CefRefPtr<CefRequestContextHandler> handler,
      scoped_refptr<CefURLRequestContextGetterImpl> parent);
  ~CefURLRequestContextGetterProxy() override;

  // Called when the StoragePartitionProxy is destroyed.
  void ShutdownOnUIThread();

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const override;

  // CefURLRequestContextGetter implementation.
  net::HostResolver* GetHostResolver() const override;

  CefRefPtr<CefRequestContextHandler> handler() const { return handler_; }

 private:
  void ShutdownOnIOThread();

  CefRefPtr<CefRequestContextHandler> handler_;

  // The CefURLRequestContextImpl owned by |parent_| is passed as a raw pointer
  // to CefURLRequestContextProxy and CefCookieStoreProxy. This reference is
  // necessary to keep it alive.
  scoped_refptr<CefURLRequestContextGetterImpl> parent_;

  std::unique_ptr<CefURLRequestContextProxy> context_proxy_;

  bool shutting_down_ = false;

  DISALLOW_COPY_AND_ASSIGN(CefURLRequestContextGetterProxy);
};

#endif  // CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_PROXY_H_
