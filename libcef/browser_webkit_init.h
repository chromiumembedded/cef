// Copyright (c) 2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_WEBKIT_INIT_H
#define _BROWSER_WEBKIT_INIT_H

#include "base/string_util.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitclient_impl.h"
#include "browser_resource_loader_bridge.h"

#include "WebKit.h"
#include "WebString.h"
#include "WebURL.h"

class BrowserWebKitInit : public webkit_glue::WebKitClientImpl {
 public:
  BrowserWebKitInit() {
    WebKit::initialize(this);
    WebKit::setLayoutTestMode(false);
    WebKit::registerURLSchemeAsLocal(
        ASCIIToUTF16(webkit_glue::GetUIResourceProtocol()));
  }

  ~BrowserWebKitInit() {
    WebKit::shutdown();
  }

  virtual WebKit::WebMimeRegistry* mimeRegistry() {
    return &mime_registry_;
  }

  virtual void setCookies(
      const WebKit::WebURL& url, const WebKit::WebURL& policy_url,
      const WebKit::WebString& value) {
    BrowserResourceLoaderBridge::SetCookie(url, policy_url, UTF16ToUTF8(value));
  }

  virtual WebKit::WebString cookies(
      const WebKit::WebURL& url, const WebKit::WebURL& policy_url) {
    return UTF8ToUTF16(BrowserResourceLoaderBridge::GetCookies(url, policy_url));
  }

  virtual void prefetchHostName(const WebKit::WebString&) {
  }

  virtual WebKit::WebString defaultLocale() {
    return ASCIIToUTF16("en-US");
  }

 private:
  webkit_glue::SimpleWebMimeRegistryImpl mime_registry_;
};

#endif  // _BROWSER_WEBKIT_INIT_H
