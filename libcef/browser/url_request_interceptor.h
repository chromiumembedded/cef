// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_URL_REQUEST_INTERCEPTOR_H_
#define CEF_LIBCEF_BROWSER_URL_REQUEST_INTERCEPTOR_H_
#pragma once

#include "net/url_request/url_request.h"

// Used for intercepting resource requests, redirects and responses. The single
// instance of this class is managed by CefURLRequestContextGetter.
class CefRequestInterceptor : public net::URLRequest::Interceptor {
 public:
  CefRequestInterceptor();
  ~CefRequestInterceptor() override;

  // net::URLRequest::Interceptor methods.
  net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) override;
  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) override;
  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) override;

  DISALLOW_COPY_AND_ASSIGN(CefRequestInterceptor);
};

#endif  // CEF_LIBCEF_BROWSER_URL_REQUEST_INTERCEPTOR_H_
