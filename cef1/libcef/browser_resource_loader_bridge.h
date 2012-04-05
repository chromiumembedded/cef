// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_RESOURCE_LOADER_BRIDGE_H_
#define CEF_LIBCEF_BROWSER_RESOURCE_LOADER_BRIDGE_H_
#pragma once

#include <string>

#include "include/cef_base.h"
#include "base/message_loop_proxy.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/resource_loader_bridge.h"

class CefBrowserImpl;
class GURL;

class BrowserResourceLoaderBridge {
 public:
  // Return the CefBrowser associated with the specified request. The browser
  // will be NULL in cases where the request was initiated using the
  // CefWebURLRequest API.
  static CefRefPtr<CefBrowserImpl> GetBrowserForRequest(
      net::URLRequest* request);

  // Creates a ResourceLoaderBridge instance.
  static webkit_glue::ResourceLoaderBridge* Create(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info);

  static scoped_refptr<base::MessageLoopProxy> GetCacheThread();

  // Used for intercepting URL redirects. Only one interceptor will be created
  // and its lifespan is controlled by the BrowserRequestContext.
  static net::URLRequest::Interceptor* CreateRequestInterceptor();
};

#endif  // CEF_LIBCEF_BROWSER_RESOURCE_LOADER_BRIDGE_H_

