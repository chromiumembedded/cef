// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_SIMPLE_WEBCOOKIEJAR_IMPL_H
#define _BROWSER_SIMPLE_WEBCOOKIEJAR_IMPL_H

// TODO(darin): WebCookieJar.h is missing a WebString.h include!
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCookieJar.h"

class BrowserWebCookieJarImpl : public WebKit::WebCookieJar {
 public:
  // WebKit::WebCookieJar methods:
  virtual void setCookie(
      const WebKit::WebURL& url, const WebKit::WebURL& first_party_for_cookies,
      const WebKit::WebString& cookie);
  virtual WebKit::WebString cookies(
      const WebKit::WebURL& url, const WebKit::WebURL& first_party_for_cookies);
};

#endif  // _BROWSER_SIMPLE_WEBCOOKIEJAR_IMPL_H
