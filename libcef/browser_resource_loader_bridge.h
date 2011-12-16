// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_RESOURCE_LOADER_BRIDGE_H
#define _BROWSER_RESOURCE_LOADER_BRIDGE_H

#include "include/cef.h"
#include "base/message_loop_proxy.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/resource_loader_bridge.h"
#include <string>

class GURL;

class BrowserResourceLoaderBridge {
 public:
  // May only be called after Init.
  static void SetCookie(const GURL& url,
                        const GURL& first_party_for_cookies,
                        const std::string& cookie);
  static std::string GetCookies(const GURL& url,
                                const GURL& first_party_for_cookies);
  static void SetAcceptAllCookies(bool accept_all_cookies);

  // Return the CefBrowser associated with the specified request. The browser
  // will be NULL in cases where the request was initiated using the
  // CefWebURLRequest API.
  static CefRefPtr<CefBrowser> GetBrowserForRequest(net::URLRequest* request);

  // Creates a ResourceLoaderBridge instance.
  static webkit_glue::ResourceLoaderBridge* Create(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info);

  static scoped_refptr<base::MessageLoopProxy> GetCacheThread();

  // Used for intercepting URL redirects. Only one interceptor will be created
  // and its lifespan is controlled by the BrowserRequestContext.
  static net::URLRequest::Interceptor* CreateRequestInterceptor();
};

#endif  // _BROWSER_RESOURCE_LOADER_BRIDGE_H

