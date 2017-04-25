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
#include "components/prefs/pref_member.h"
#include "content/public/browser/browser_context.h"
#include "net/url_request/url_request_job_factory.h"

class PrefRegistrySimple;
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
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      content::ProtocolHandlerMap* protocol_handlers,
      std::unique_ptr<net::ProxyConfigService> proxy_config_service,
      content::URLRequestInterceptorScopedVector request_interceptors);
  ~CefURLRequestContextGetterImpl() override;

  // Register preferences. Called from browser_prefs::CreatePrefService().
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Called when the BrowserContextImpl is destroyed.
  void ShutdownOnUIThread();

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const override;

  // CefURLRequestContextGetter implementation.
  net::HostResolver* GetHostResolver() const override;

  void SetCookieStoragePath(const base::FilePath& path,
                            bool persist_session_cookies);
  void SetCookieSupportedSchemes(const std::vector<std::string>& schemes);

  // Keep a reference to all handlers sharing this context so that they'll be
  // kept alive until the context is destroyed.
  void AddHandler(CefRefPtr<CefRequestContextHandler> handler);

  // Returns the existing cookie store object. Logs an error if the cookie
  // store does not yet exist. Must be called on the IO thread.
  net::CookieStore* GetExistingCookieStore() const;

  CefURLRequestManager* request_manager() const {
    return io_state_->url_request_manager_.get();
  }

 private:
  void CreateProxyConfigService();
  void UpdateServerWhitelist();
  void UpdateDelegateWhitelist();

  void ShutdownOnIOThread();

  const CefRequestContextSettings settings_;

  bool shutting_down_ = false;

  // State that is only accessed on the IO thread and will be reset in
  // ShutdownOnIOThread().
  struct IOState {
    net::NetLog* net_log_ = nullptr;  // Guaranteed to outlive this object.

    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

#if defined(OS_POSIX) && !defined(OS_ANDROID)
    std::string gsapi_library_name_;
#endif

    std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
    std::unique_ptr<net::URLRequestContextStorage> storage_;
    std::unique_ptr<net::HttpAuthPreferences> http_auth_preferences_;
    std::unique_ptr<CefURLRequestContextImpl> url_request_context_;
    std::unique_ptr<CefURLRequestManager> url_request_manager_;
    content::ProtocolHandlerMap protocol_handlers_;
    content::URLRequestInterceptorScopedVector request_interceptors_;

    base::FilePath cookie_store_path_;
    std::vector<std::string> cookie_supported_schemes_;

    std::vector<CefRefPtr<CefRequestContextHandler> > handler_list_;
  };
  std::unique_ptr<IOState> io_state_;

  BooleanPrefMember quick_check_enabled_;
  BooleanPrefMember pac_https_url_stripping_enabled_;

  // Member variables which are pointed to by the various context objects.
  mutable BooleanPrefMember force_google_safesearch_;

  StringPrefMember auth_server_whitelist_;
  StringPrefMember auth_negotiate_delegate_whitelist_;

  DISALLOW_COPY_AND_ASSIGN(CefURLRequestContextGetterImpl);
};

#endif  // CEF_LIBCEF_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_IMPL_H_
