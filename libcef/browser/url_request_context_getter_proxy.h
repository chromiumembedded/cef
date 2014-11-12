// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_URL_REQUEST_CONTEXT_GETTER_PROXY_H_
#define CEF_LIBCEF_BROWSER_URL_REQUEST_CONTEXT_GETTER_PROXY_H_
#pragma once

#include "include/cef_request_context_handler.h"

#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_context_getter.h"

class CefURLRequestContextGetter;
class CefURLRequestContextProxy;

namespace net {
class HostResolver;
}

class CefURLRequestContextGetterProxy : public net::URLRequestContextGetter {
 public:
  CefURLRequestContextGetterProxy(CefRefPtr<CefRequestContextHandler> handler,
                                  CefURLRequestContextGetter* parent);
  ~CefURLRequestContextGetterProxy() override;

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const override;

  net::HostResolver* GetHostResolver() const;

  CefRefPtr<CefRequestContextHandler> handler() const { return handler_; }

 private:
  CefRefPtr<CefRequestContextHandler> handler_;
  scoped_refptr<CefURLRequestContextGetter> parent_;

  // The |context_proxy_| object is owned by |parent_|.
  CefURLRequestContextProxy* context_proxy_;

  DISALLOW_COPY_AND_ASSIGN(CefURLRequestContextGetterProxy);
};

#endif  // CEF_LIBCEF_BROWSER_URL_REQUEST_CONTEXT_GETTER_PROXY_H_
