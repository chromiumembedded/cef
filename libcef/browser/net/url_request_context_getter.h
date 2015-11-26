// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_H_
#define CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_H_
#pragma once

#include "net/url_request/url_request_context_getter.h"

namespace net {
class HostResolver;
}

// Responsible for creating and owning the URLRequestContext and all network-
// related functionality. Life span is primarily controlled by
// CefResourceContext*. See browser_context.h for an object relationship
// diagram.
class CefURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  CefURLRequestContextGetter() {}

  // Called from CefResourceContext::GetHostResolver().
  virtual net::HostResolver* GetHostResolver() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefURLRequestContextGetter);
};

#endif  // CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_H_
