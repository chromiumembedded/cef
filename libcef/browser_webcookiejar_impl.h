// Copyright (c) 2012 the Chromium Embedded Framework authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_WEBCOOKIEJAR_IMPL_H_
#define CEF_LIBCEF_BROWSER_WEBCOOKIEJAR_IMPL_H_
#pragma once

#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCookieJar.h"

namespace net {
class CookieStore;
}

class CefBrowserImpl;

// Handles cookie requests from the renderer.
class BrowserWebCookieJarImpl : public WebKit::WebCookieJar {
 public:
  BrowserWebCookieJarImpl();
  explicit BrowserWebCookieJarImpl(CefBrowserImpl* browser);

  // WebKit::WebCookieJar methods.
  virtual void setCookie(
      const WebKit::WebURL& url, const WebKit::WebURL& first_party_for_cookies,
      const WebKit::WebString& cookie);
  virtual WebKit::WebString cookies(
      const WebKit::WebURL& url, const WebKit::WebURL& first_party_for_cookies);

 private:
  // May be NULL for the global implementation.
  CefBrowserImpl* browser_;
};

#endif  // CEF_LIBCEF_BROWSER_WEBCOOKIEJAR_IMPL_H_
