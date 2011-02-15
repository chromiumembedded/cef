// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_webcookiejar_impl.h"
#include "browser_resource_loader_bridge.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"

using WebKit::WebString;
using WebKit::WebURL;

void BrowserWebCookieJarImpl::setCookie(const WebURL& url,
                                       const WebURL& first_party_for_cookies,
                                       const WebString& value) {
  BrowserResourceLoaderBridge::SetCookie(
      url, first_party_for_cookies, value.utf8());
}

WebString BrowserWebCookieJarImpl::cookies(
    const WebURL& url,
    const WebURL& first_party_for_cookies) {
  return WebString::fromUTF8(
      BrowserResourceLoaderBridge::GetCookies(url, first_party_for_cookies));
}
