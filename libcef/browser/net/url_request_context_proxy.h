// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_PROXY_H_
#define CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_PROXY_H_
#pragma once

#include "include/cef_request_context_handler.h"
#include "libcef/browser/net/cookie_store_proxy.h"
#include "libcef/browser/net/url_request_context.h"

#include "base/memory/scoped_ptr.h"

class CefBrowserHostImpl;
class CefURLRequestContextImpl;

// URLRequestContext implementation for a particular CefRequestContext. Life
// span is controlled by CefURLRequestContextGetterProxy. Only accessed on the
// IO thread. See browser_context.h for an object relationship diagram.
class CefURLRequestContextProxy : public CefURLRequestContext {
 public:
  // The |parent| pointer is kept alive by CefURLRequestContextGetterProxy
  // which has a ref to the owning CefURLRequestContextGetterImpl. It is
  // guaranteed to outlive this object.
  CefURLRequestContextProxy(CefURLRequestContextImpl* parent,
                            CefRefPtr<CefRequestContextHandler> handler);
  ~CefURLRequestContextProxy() override;

 private:
  scoped_refptr<CefCookieStoreProxy> cookie_store_proxy_;

  DISALLOW_COPY_AND_ASSIGN(CefURLRequestContextProxy);
};

#endif  // CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_PROXY_H_
