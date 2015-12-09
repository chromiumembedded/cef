// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_IMPL_H_
#define CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_IMPL_H_
#pragma once

#include <set>
#include <string>

#include "include/internal/cef_types_wrappers.h"
#include "libcef/browser/net/url_request_context_getter.h"
#include "libcef/browser/net/url_request_context_impl.h"
#include "libcef/browser/net/url_request_manager.h"

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "net/url_request/url_request_job_factory.h"

class PrefService;

namespace base {
class MessageLoop;
}

namespace net {
class CookieMonster;
class FtpTransactionFactory;
class HttpAuthPreferences;
class ProxyConfigService;
class URLRequestContextStorage;
class URLRequestJobFactory;
class URLRequestJobFactoryImpl;
}

// Isolated URLRequestContextGetter implementation. Life span is primarily
// controlled by CefResourceContext and (for the global context)
// CefBrowserMainParts. Created on the UI thread but accessed and destroyed on
// the IO thread. See browser_context.h for an object relationship diagram.
class CefURLRequestContextGetterImpl : public CefURLRequestContextGetter {
 public:
  CefURLRequestContextGetterImpl(
      const CefRequestContextSettings& settings,
      PrefService* pref_service,
      base::MessageLoop* io_loop,
      base::MessageLoop* file_loop,
      content::ProtocolHandlerMap* protocol_handlers,
      scoped_ptr<net::ProxyConfigService> proxy_config_service,
      content::URLRequestInterceptorScopedVector request_interceptors);
  ~CefURLRequestContextGetterImpl() override;

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const override;

  // CefURLRequestContextGetter implementation.
  net::HostResolver* GetHostResolver() const override;

  void SetCookieStoragePath(const base::FilePath& path,
                            bool persist_session_cookies);
  void SetCookieSupportedSchemes(const std::set<std::string>& schemes);

  // Keep a reference to all handlers sharing this context so that they'll be
  // kept alive until the context is destroyed.
  void AddHandler(CefRefPtr<CefRequestContextHandler> handler);

  net::CookieMonster* GetCookieMonster() const;

  CefURLRequestManager* request_manager() const {
    return url_request_manager_.get();
  }

 private:
  void CreateProxyConfigService();

  const CefRequestContextSettings settings_;

  base::MessageLoop* io_loop_;
  base::MessageLoop* file_loop_;

#if defined(OS_POSIX) && !defined(OS_ANDROID)
  std::string gsapi_library_name_;
#endif

  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_ptr<net::URLRequestContextStorage> storage_;
  scoped_ptr<net::HttpAuthPreferences> http_auth_preferences_;
  scoped_ptr<CefURLRequestContextImpl> url_request_context_;
  scoped_ptr<CefURLRequestManager> url_request_manager_;
  scoped_ptr<net::FtpTransactionFactory> ftp_transaction_factory_;
  content::ProtocolHandlerMap protocol_handlers_;
  content::URLRequestInterceptorScopedVector request_interceptors_;

  base::FilePath cookie_store_path_;
  std::set<std::string> cookie_supported_schemes_;

  std::vector<CefRefPtr<CefRequestContextHandler> > handler_list_;

  DISALLOW_COPY_AND_ASSIGN(CefURLRequestContextGetterImpl);
};

#endif  // CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_IMPL_H_
