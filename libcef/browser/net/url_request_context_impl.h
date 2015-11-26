// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_IMPL_H_
#define CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_IMPL_H_
#pragma once

#include "libcef/browser/net/url_request_context.h"

// Isolated URLRequestContext implementation. Life span is controlled by
// CefURLRequestContextGetterImpl. Only accessed on the IO thread. See
// browser_context.h for an object relationship diagram.
class CefURLRequestContextImpl : public CefURLRequestContext {
 public:
  CefURLRequestContextImpl() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CefURLRequestContextImpl);
};

#endif  // CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_IMPL_H_
