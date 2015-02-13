// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_URL_REQUEST_CONTEXT_GETTER_IMPL_H_
#define CEF_LIBCEF_BROWSER_URL_REQUEST_CONTEXT_GETTER_IMPL_H_
#pragma once

#include <string>
#include <vector>

#include "libcef/browser/url_request_context_getter.h"
#include "libcef/browser/url_request_context_impl.h"

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "net/url_request/url_request_job_factory.h"

namespace base {
class MessageLoop;
}

namespace net {
class FtpTransactionFactory;
class ProxyConfigService;
class URLRequestContextStorage;
class URLRequestJobFactory;
class URLRequestJobFactoryImpl;
class URLSecurityManager;
}

// Global URLRequestContextGetter implementation. Life span is primarily
// controlled by CefResourceContext and CefBrowserMainParts. Created on the UI
// thread but accessed and destroyed on the IO thread. See browser_context.h
// for an object relationship diagram.
class CefURLRequestContextGetterImpl : public CefURLRequestContextGetter {
 public:
  CefURLRequestContextGetterImpl(
      base::MessageLoop* io_loop,
      base::MessageLoop* file_loop,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors);
  ~CefURLRequestContextGetterImpl() override;

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const override;

  // CefURLRequestContextGetter implementation.
  net::HostResolver* GetHostResolver() const override;

  net::URLRequestJobFactoryImpl* job_factory_impl() const {
    return job_factory_impl_;
  }

  void SetCookieStoragePath(const base::FilePath& path,
                            bool persist_session_cookies);
  void SetCookieSupportedSchemes(const std::vector<std::string>& schemes);

 private:
  void CreateProxyConfigService();

  base::MessageLoop* io_loop_;
  base::MessageLoop* file_loop_;

  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_ptr<net::URLRequestContextStorage> storage_;
  scoped_ptr<CefURLRequestContextImpl> url_request_context_;
  scoped_ptr<net::URLSecurityManager> url_security_manager_;
  scoped_ptr<net::FtpTransactionFactory> ftp_transaction_factory_;
  content::ProtocolHandlerMap protocol_handlers_;
  content::URLRequestInterceptorScopedVector request_interceptors_;
  net::URLRequestJobFactoryImpl* job_factory_impl_;

  base::FilePath cookie_store_path_;
  std::vector<std::string> cookie_supported_schemes_;

  DISALLOW_COPY_AND_ASSIGN(CefURLRequestContextGetterImpl);
};

#endif  // CEF_LIBCEF_BROWSER_URL_REQUEST_CONTEXT_GETTER_IMPL_H_
