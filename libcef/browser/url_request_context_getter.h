// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_URL_REQUEST_CONTEXT_GETTER_H_
#define CEF_LIBCEF_BROWSER_URL_REQUEST_CONTEXT_GETTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_context_getter.h"

class CefRequestInterceptor;
class MessageLoop;

namespace net {
class HostResolver;
class ProxyConfigService;
class URLRequestContextStorage;
class URLRequestJobFactory;
class URLSecurityManager;
}

class CefURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  CefURLRequestContextGetter(
      const FilePath& base_path_,
      MessageLoop* io_loop,
      MessageLoop* file_loop);
  virtual ~CefURLRequestContextGetter();

  // net::URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy>
      GetIOMessageLoopProxy() const OVERRIDE;

  net::HostResolver* host_resolver();

  void SetCookieStoragePath(const FilePath& path);

 private:
  void CreateProxyConfigService();

  FilePath base_path_;
  MessageLoop* io_loop_;
  MessageLoop* file_loop_;

  scoped_ptr<net::ProxyConfigService> proxy_config_service_;

  scoped_refptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<net::URLSecurityManager> url_security_manager_;
  scoped_ptr<net::URLRequestContextStorage> storage_;
  scoped_ptr<CefRequestInterceptor> request_interceptor_;

  FilePath cookie_store_path_;

  DISALLOW_COPY_AND_ASSIGN(CefURLRequestContextGetter);
};

#endif  // CEF_LIBCEF_BROWSER_URL_REQUEST_CONTEXT_GETTER_H_
