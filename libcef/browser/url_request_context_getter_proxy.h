// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_URL_REQUEST_CONTEXT_GETTER_PROXY_H_
#define CEF_LIBCEF_BROWSER_URL_REQUEST_CONTEXT_GETTER_PROXY_H_
#pragma once

#include "net/url_request/url_request_context_getter.h"

class CefBrowserHostImpl;

class CefURLRequestContextGetterProxy : public net::URLRequestContextGetter {
 public:
  CefURLRequestContextGetterProxy(CefBrowserHostImpl* browser,
                                  net::URLRequestContextGetter* parent);

  // net::URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy>
      GetIOMessageLoopProxy() const OVERRIDE;

 private:
  CefBrowserHostImpl* browser_;
  scoped_refptr<net::URLRequestContextGetter> parent_;
  scoped_refptr<net::URLRequestContext> context_proxy_;

  DISALLOW_COPY_AND_ASSIGN(CefURLRequestContextGetterProxy);
};

#endif  // CEF_LIBCEF_BROWSER_URL_REQUEST_CONTEXT_GETTER_PROXY_H_
