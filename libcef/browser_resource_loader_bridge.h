// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_RESOURCE_LOADER_BRIDGE_H
#define _BROWSER_RESOURCE_LOADER_BRIDGE_H

#include <string>
#include "base/message_loop_proxy.h"

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

  static scoped_refptr<base::MessageLoopProxy> GetCacheThread();
};

#endif  // _BROWSER_RESOURCE_LOADER_BRIDGE_H

