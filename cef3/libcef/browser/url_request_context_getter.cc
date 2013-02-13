// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/url_request_context_getter.h"

#if defined(OS_WIN)
#include <winhttp.h>
#endif
#include <string>
#include <vector>

#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/url_network_delegate.h"
#include "libcef/browser/url_request_context_proxy.h"
#include "libcef/browser/url_request_interceptor.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/cert_verifier.h"
#include "net/base/default_server_bound_cert_store.h"
#include "net/base/host_resolver.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/cookies/cookie_monster.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_job_manager.h"

using content::BrowserThread;

#if defined(OS_WIN)
#pragma comment(lib, "winhttp.lib")
#endif

CefURLRequestContextGetter::CefURLRequestContextGetter(
    bool ignore_certificate_errors,
    const FilePath& base_path,
    MessageLoop* io_loop,
    MessageLoop* file_loop)
    : ignore_certificate_errors_(ignore_certificate_errors),
      base_path_(base_path),
      io_loop_(io_loop),
      file_loop_(file_loop) {
  // Must first be created on the UI thread.
  CEF_REQUIRE_UIT();
}

CefURLRequestContextGetter::~CefURLRequestContextGetter() {
  CEF_REQUIRE_IOT();
  STLDeleteElements(&url_request_context_proxies_);
}

net::URLRequestContext* CefURLRequestContextGetter::GetURLRequestContext() {
  CEF_REQUIRE_IOT();

  if (!url_request_context_.get()) {
    const FilePath& cache_path = _Context->cache_path();
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    const CefSettings& settings = _Context->settings();

    url_request_context_.reset(new net::URLRequestContext());
    storage_.reset(
        new net::URLRequestContextStorage(url_request_context_.get()));

    bool persist_session_cookies =
        (settings.persist_session_cookies ||
         command_line.HasSwitch(switches::kPersistSessionCookies));
    SetCookieStoragePath(cache_path, persist_session_cookies);

    storage_->set_network_delegate(new CefNetworkDelegate);

    storage_->set_server_bound_cert_service(new net::ServerBoundCertService(
        new net::DefaultServerBoundCertStore(NULL),
        base::WorkerPool::GetTaskRunner(true)));
    storage_->set_http_user_agent_settings(
        new net::StaticHttpUserAgentSettings(
            "en-us,en", "iso-8859-1,*,utf-8", EmptyString()));

    storage_->set_host_resolver(net::HostResolver::CreateDefaultResolver(NULL));
    storage_->set_cert_verifier(net::CertVerifier::CreateDefault());

    scoped_ptr<net::ProxyService> system_proxy_service;
    system_proxy_service.reset(
        ProxyServiceFactory::CreateProxyService(
            NULL,
            url_request_context_.get(),
            _Context->proxy_config_service().release(),
            command_line));
    storage_->set_proxy_service(system_proxy_service.release());

    storage_->set_ssl_config_service(new net::SSLConfigServiceDefaults);

    // Add support for single sign-on.
    url_security_manager_.reset(net::URLSecurityManager::Create(NULL, NULL));

    std::vector<std::string> supported_schemes;
    supported_schemes.push_back("basic");
    supported_schemes.push_back("digest");
    supported_schemes.push_back("ntlm");
    supported_schemes.push_back("negotiate");

    storage_->set_http_auth_handler_factory(
        net::HttpAuthHandlerRegistryFactory::Create(
            supported_schemes,
            url_security_manager_.get(),
            url_request_context_->host_resolver(),
            std::string(),
            false,
            false));
    storage_->set_http_server_properties(new net::HttpServerPropertiesImpl);

    net::HttpCache::DefaultBackend* main_backend =
        new net::HttpCache::DefaultBackend(
            cache_path.empty() ? net::MEMORY_CACHE : net::DISK_CACHE,
            cache_path,
            0,
            BrowserThread::GetMessageLoopProxyForThread(
                BrowserThread::CACHE));

    net::HttpNetworkSession::Params network_session_params;
    network_session_params.host_resolver =
        url_request_context_->host_resolver();
    network_session_params.cert_verifier =
        url_request_context_->cert_verifier();
    network_session_params.server_bound_cert_service =
        url_request_context_->server_bound_cert_service();
    network_session_params.proxy_service =
        url_request_context_->proxy_service();
    network_session_params.ssl_config_service =
        url_request_context_->ssl_config_service();
    network_session_params.http_auth_handler_factory =
        url_request_context_->http_auth_handler_factory();
    network_session_params.network_delegate =
        url_request_context_->network_delegate();
    network_session_params.http_server_properties =
        url_request_context_->http_server_properties();
    network_session_params.ignore_certificate_errors =
        ignore_certificate_errors_;

    net::HttpCache* main_cache = new net::HttpCache(network_session_params,
                                                    main_backend);
    storage_->set_http_transaction_factory(main_cache);

    storage_->set_ftp_transaction_factory(
      new net::FtpNetworkLayer(url_request_context_->host_resolver()));

    storage_->set_job_factory(new net::URLRequestJobFactoryImpl);

    request_interceptor_.reset(new CefRequestInterceptor);
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
    CefURLRequestContextGetter::GetNetworkTaskRunner() const {
  return BrowserThread::GetMessageLoopProxyForThread(CEF_IOT);
}

net::HostResolver* CefURLRequestContextGetter::host_resolver() {
  return url_request_context_->host_resolver();
}

void CefURLRequestContextGetter::SetCookieStoragePath(
    const FilePath& path,
    bool persist_session_cookies) {
  CEF_REQUIRE_IOT();

  if (url_request_context_->cookie_store() &&
      ((cookie_store_path_.empty() && path.empty()) ||
       cookie_store_path_ == path)) {
    // The path has not changed so don't do anything.
    return;
  }

  scoped_refptr<SQLitePersistentCookieStore> persistent_store;
  if (!path.empty()) {
    // TODO(cef): Move directory creation to the blocking pool instead of
    // allowing file IO on this thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (file_util::DirectoryExists(path) ||
        file_util::CreateDirectory(path)) {
      const FilePath& cookie_path = path.AppendASCII("Cookies");
      persistent_store =
          new SQLitePersistentCookieStore(cookie_path,
                                          persist_session_cookies,
                                          NULL);
    } else {
      NOTREACHED() << "The cookie storage directory could not be created";
    }
  }

  // Set the new cookie store that will be used for all new requests. The old
  // cookie store, if any, will be automatically flushed and closed when no
  // longer referenced.
  scoped_refptr<net::CookieMonster> cookie_monster =
      new net::CookieMonster(persistent_store.get(), NULL);
  storage_->set_cookie_store(cookie_monster);
  if (persistent_store.get() && persist_session_cookies)
      cookie_monster->SetPersistSessionCookies(true);
  cookie_store_path_ = path;

  // Restore the previously supported schemes.
  SetCookieSupportedSchemes(cookie_supported_schemes_);
}

void CefURLRequestContextGetter::SetCookieSupportedSchemes(
    const std::vector<std::string>& schemes) {
  CEF_REQUIRE_IOT();

  cookie_supported_schemes_ = schemes;

  if (cookie_supported_schemes_.empty()) {
    cookie_supported_schemes_.push_back("http");
    cookie_supported_schemes_.push_back("https");
  }

  std::set<std::string> scheme_set;
  std::vector<std::string>::const_iterator it =
      cookie_supported_schemes_.begin();
  for (; it != cookie_supported_schemes_.end(); ++it)
    scheme_set.insert(*it);

  const char** arr = new const char*[scheme_set.size()];
  std::set<std::string>::const_iterator it2 = scheme_set.begin();
  for (int i = 0; it2 != scheme_set.end(); ++it2, ++i)
    arr[i] = it2->c_str();

  url_request_context_->cookie_store()->GetCookieMonster()->
      SetCookieableSchemes(arr, scheme_set.size());

  delete [] arr;
}

CefURLRequestContextProxy*
    CefURLRequestContextGetter::CreateURLRequestContextProxy() {
  CEF_REQUIRE_IOT();
  CefURLRequestContextProxy* proxy = new CefURLRequestContextProxy(this);
  url_request_context_proxies_.insert(proxy);
  return proxy;
}

void CefURLRequestContextGetter::ReleaseURLRequestContextProxy(
    CefURLRequestContextProxy* proxy) {
  CEF_REQUIRE_IOT();

  // Don't do anything if we're currently shutting down. The proxy objects will
  // be deleted when this object is destroyed.
  if (_Context->shutting_down())
    return;

  if (proxy->url_requests()->size() == 0) {
    // Safe to delete the proxy.
    RequestContextProxySet::iterator it =
        url_request_context_proxies_.find(proxy);
    DCHECK(it != url_request_context_proxies_.end());
    url_request_context_proxies_.erase(it);
    delete proxy;
  } else {
    proxy->increment_delete_try_count();
    if (proxy->delete_try_count() <= 1) {
      // Cancel the pending requests. This may result in additional tasks being
      // posted on the IO thread.
      std::set<const net::URLRequest*>::iterator it =
          proxy->url_requests()->begin();
      for (; it != proxy->url_requests()->end(); ++it)
        const_cast<net::URLRequest*>(*it)->Cancel();

      // Try to delete the proxy again later.
      CEF_POST_TASK(CEF_IOT,
          base::Bind(&CefURLRequestContextGetter::ReleaseURLRequestContextProxy,
                     this, proxy));
    } else {
      NOTREACHED() <<
          "too many retries to delete URLRequestContext proxy object";
    }
  }
}

void CefURLRequestContextGetter::CreateProxyConfigService() {
  if (proxy_config_service_.get())
    return;

  proxy_config_service_.reset(
      net::ProxyService::CreateSystemProxyConfigService(
          io_loop_->message_loop_proxy(), file_loop_));
}
