// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_request_context.h"
#include "browser_file_system.h"
#include "browser_persistent_cookie_store.h"
#include "browser_resource_loader_bridge.h"
#include "build/build_config.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_monster.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service.h"
#include "net/base/static_cookie_policy.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitClient.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)

#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

namespace {

// ProxyConfigService implementation that does nothing.
class ProxyConfigServiceNull : public net::ProxyConfigService {
public:
  ProxyConfigServiceNull() {}
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual bool GetLatestProxyConfig(net::ProxyConfig* config) { return true; }
  virtual void OnLazyPoll() {}
};

} // namespace

#endif // defined(OS_WIN)

BrowserRequestContext::BrowserRequestContext() {
  Init(FilePath(), net::HttpCache::NORMAL, false);
}

BrowserRequestContext::BrowserRequestContext(
    const FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) {
  Init(cache_path, cache_mode, no_proxy);
}

void BrowserRequestContext::Init(
    const FilePath& cache_path,
    net::HttpCache::Mode cache_mode,
    bool no_proxy) {
  // Create the |cache_path| directory if necessary.
  bool cache_path_valid = false;
  if (!cache_path.empty()) {
    if (file_util::CreateDirectory(cache_path))
      cache_path_valid = true;
    else
      NOTREACHED() << "The cache_path directory could not be created";
  }

  scoped_refptr<BrowserPersistentCookieStore> persistent_store;
  if (cache_path_valid) {
    const FilePath& cookie_path = cache_path.AppendASCII("Cookies");
    persistent_store = new BrowserPersistentCookieStore(cookie_path);
  }

  set_cookie_store(new net::CookieMonster(persistent_store.get(), NULL));
  set_cookie_policy(new net::StaticCookiePolicy());

  // hard-code A-L and A-C for test shells
  set_accept_language("en-us,en");
  set_accept_charset("iso-8859-1,*,utf-8");

#if defined(OS_WIN)
  // Using the system proxy resolver on Windows when "Automatically detect
  // settings" (auto-detection) is checked under LAN Settings can hurt resource
  // loading performance because the call to WinHttpGetProxyForUrl in
  // proxy_resolver_winhttp.cc will block the IO thread.  This is especially
  // true for Windows 7 where auto-detection is checked by default. To avoid
  // slow resource loading on Windows we only use the system proxy resolver if
  // auto-detection is unchecked.
  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_config = {0};
  if (WinHttpGetIEProxyConfigForCurrentUser(&ie_config)) {
    if (ie_config.fAutoDetect == TRUE) {
      set_proxy_service(net::ProxyService::CreateWithoutProxyResolver(
          new ProxyConfigServiceNull(), NULL));
    }

    if (ie_config.lpszAutoConfigUrl)
      GlobalFree(ie_config.lpszAutoConfigUrl);
    if (ie_config.lpszProxy)
      GlobalFree(ie_config.lpszProxy);
    if (ie_config.lpszProxyBypass)
      GlobalFree(ie_config.lpszProxyBypass);
  }
#endif // defined(OS_WIN)
  
  if (!proxy_service()) {
    // Use the system proxy resolver.
    scoped_ptr<net::ProxyConfigService> proxy_config_service(
        net::ProxyService::CreateSystemProxyConfigService(
            MessageLoop::current(), NULL));
    set_proxy_service(net::ProxyService::CreateUsingSystemProxyResolver(
        proxy_config_service.release(), 0, NULL));
  }

  set_host_resolver(
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    NULL, NULL));
  set_cert_verifier(new net::CertVerifier);
  set_ssl_config_service(net::SSLConfigService::CreateSystemSSLConfigService());

  // Add support for single sign-on.
  url_security_manager_.reset(net::URLSecurityManager::Create(NULL, NULL));

  std::vector<std::string> supported_schemes;
  supported_schemes.push_back("basic");
  supported_schemes.push_back("digest");
  supported_schemes.push_back("ntlm");
  supported_schemes.push_back("negotiate");

  set_http_auth_handler_factory(
      net::HttpAuthHandlerRegistryFactory::Create(supported_schemes,
                                                  url_security_manager_.get(),
                                                  host_resolver(),
                                                  std::string(),
                                                  false,
                                                  false));

  net::HttpCache::DefaultBackend* backend = new net::HttpCache::DefaultBackend(
      cache_path_valid ? net::DISK_CACHE : net::MEMORY_CACHE,
      cache_path, 0, BrowserResourceLoaderBridge::GetCacheThread());

  net::HttpCache* cache =
      new net::HttpCache(host_resolver(), cert_verifier(), NULL, NULL,
                         proxy_service(), ssl_config_service(),
                         http_auth_handler_factory(), NULL, NULL, backend);

  cache->set_mode(cache_mode);
  set_http_transaction_factory(cache);

  set_ftp_transaction_factory(new net::FtpNetworkLayer(host_resolver()));

  blob_storage_controller_.reset(new webkit_blob::BlobStorageController());
  file_system_context_ = static_cast<BrowserFileSystem*>(
      WebKit::webKitClient()->fileSystem())->file_system_context();
}

BrowserRequestContext::~BrowserRequestContext() {
  delete ftp_transaction_factory();
  delete http_transaction_factory();
  delete http_auth_handler_factory();
  delete cookie_policy();
  delete cert_verifier();
  delete host_resolver();
}

void BrowserRequestContext::SetAcceptAllCookies(bool accept_all_cookies) {
  net::StaticCookiePolicy::Type policy_type = accept_all_cookies ?
      net::StaticCookiePolicy::ALLOW_ALL_COOKIES :
      net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES;
  static_cast<net::StaticCookiePolicy*>(cookie_policy())->set_type(policy_type);
}

const std::string& BrowserRequestContext::GetUserAgent(
    const GURL& url) const {
  return webkit_glue::GetUserAgent(url);
}

