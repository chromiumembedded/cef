// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_RESOURCE_LOADER_BRIDGE_H
#define _BROWSER_RESOURCE_LOADER_BRIDGE_H

#include "include/cef.h"
#include "base/message_loop_proxy.h"
#include <string>

class GURL;

namespace net {
class URLRequest;
};

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

  static scoped_refptr<base::MessageLoopProxy> GetCacheThread();
};

#endif  // _BROWSER_RESOURCE_LOADER_BRIDGE_H

