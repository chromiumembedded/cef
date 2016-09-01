// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_H_
#define CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_H_
#pragma once

#include "base/logging.h"
#include "net/url_request/url_request_context.h"

// Owns URLRequest instances and provides access to network-related
// functionality. Life span is controlled by CefURLRequestContextGetter*. Only
// accessed on the IO thread. URLRequest objects are created via ResourceContext
// and URLFetcher. All URLRequest objects must be destroyed before this object
// is destroyed. See browser_context.h for an object relationship diagram.
class CefURLRequestContext : public net::URLRequestContext {
 public:
  CefURLRequestContext();
  ~CefURLRequestContext() override;

#if DCHECK_IS_ON()
  // Simple tracking of allocated objects.
  static base::AtomicRefCount DebugObjCt;  // NOLINT(runtime/int)
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(CefURLRequestContext);
};

#endif  // CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_H_
