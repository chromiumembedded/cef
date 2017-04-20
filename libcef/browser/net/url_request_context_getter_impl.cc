// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net/url_request_context_getter_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "libcef/browser/cookie_manager_impl.h"
#include "libcef/browser/net/network_delegate.h"
#include "libcef/browser/net/scheme_handler.h"
#include "libcef/browser/net/url_request_interceptor.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/content_client.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/net_log/chrome_net_log.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_known_logs.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/dns/host_resolver.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_cache.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "url/url_constants.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_job_manager.h"

#if defined(OS_WIN)
#include <winhttp.h>
#endif

#if defined(USE_NSS_CERTS)
#include "net/cert_net/nss_ocsp.h"
#endif

using content::BrowserThread;

#if defined(OS_WIN)
#pragma comment(lib, "winhttp.lib")
#endif

namespace {

// An implementation of |HttpUserAgentSettings| that provides a static
// HTTP Accept-Language header value and uses |content::GetUserAgent|
// to provide the HTTP User-Agent header value.
class CefHttpUserAgentSettings : public net::HttpUserAgentSettings {
 public:
  explicit CefHttpUserAgentSettings(const std::string& raw_language_list)
      : http_accept_language_(net::HttpUtil::GenerateAcceptLanguageHeader(
            raw_language_list)) {
    CEF_REQUIRE_IOT();
  }

  // net::HttpUserAgentSettings implementation
  std::string GetAcceptLanguage() const override {
    CEF_REQUIRE_IOT();
    return http_accept_language_;
  }

  std::string GetUserAgent() const override {
    CEF_REQUIRE_IOT();
    return CefContentClient::Get()->GetUserAgent();
  }

 private:
  const std::string http_accept_language_;

  DISALLOW_COPY_AND_ASSIGN(CefHttpUserAgentSettings);
};

}  // namespace

CefURLRequestContextGetterImpl::CefURLRequestContextGetterImpl(
    const CefRequestContextSettings& settings,
    PrefService* pref_service,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    content::ProtocolHandlerMap* protocol_handlers,
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    content::URLRequestInterceptorScopedVector request_interceptors)
    : settings_(settings),
      io_state_(base::MakeUnique<IOState>()) {
  // Must first be created on the UI thread.
  CEF_REQUIRE_UIT();

  io_state_->net_log_ = g_browser_process->net_log(),
  DCHECK(io_state_->net_log_);
  io_state_->io_task_runner_ = std::move(io_task_runner);
  io_state_->file_task_runner_ = std::move(file_task_runner);
  io_state_->proxy_config_service_ = std::move(proxy_config_service);
  io_state_->request_interceptors_ = std::move(request_interceptors);

  std::swap(io_state_->protocol_handlers_, *protocol_handlers);

  auto io_thread_proxy =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);

  quick_check_enabled_.Init(prefs::kQuickCheckEnabled, pref_service);
  quick_check_enabled_.MoveToThread(io_thread_proxy);

  pac_https_url_stripping_enabled_.Init(prefs::kPacHttpsUrlStrippingEnabled,
                                        pref_service);
  pac_https_url_stripping_enabled_.MoveToThread(io_thread_proxy);

  force_google_safesearch_.Init(prefs::kForceGoogleSafeSearch, pref_service);
  force_google_safesearch_.MoveToThread(io_thread_proxy);

#if defined(OS_POSIX) && !defined(OS_ANDROID)
  io_state_->gsapi_library_name_ =
      pref_service->GetString(prefs::kGSSAPILibraryName);
#endif

  auth_server_whitelist_.Init(
      prefs::kAuthServerWhitelist, pref_service,
      base::Bind(&CefURLRequestContextGetterImpl::UpdateServerWhitelist,
      base::Unretained(this)));
  auth_server_whitelist_.MoveToThread(io_thread_proxy);

  auth_negotiate_delegate_whitelist_.Init(
      prefs::kAuthNegotiateDelegateWhitelist, pref_service,
      base::Bind(&CefURLRequestContextGetterImpl::UpdateDelegateWhitelist,
      base::Unretained(this)));
  auth_negotiate_delegate_whitelist_.MoveToThread(io_thread_proxy);
}

CefURLRequestContextGetterImpl::~CefURLRequestContextGetterImpl() {
  CEF_REQUIRE_IOT();
  // This destructor may not be called during shutdown. Perform any required
  // shutdown in ShutdownOnIOThread() instead.
}

// static
void CefURLRequestContextGetterImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  // Based on IOThread::RegisterPrefs.
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  registry->RegisterStringPref(prefs::kGSSAPILibraryName, std::string());
#endif
  registry->RegisterBooleanPref(prefs::kQuickCheckEnabled, true);
  registry->RegisterBooleanPref(prefs::kPacHttpsUrlStrippingEnabled, true);

  // Based on ProfileImpl::RegisterProfilePrefs.
  registry->RegisterBooleanPref(prefs::kForceGoogleSafeSearch, false);
}

void CefURLRequestContextGetterImpl::ShutdownOnUIThread() {
  CEF_REQUIRE_UIT();
  quick_check_enabled_.Destroy();
  pac_https_url_stripping_enabled_.Destroy();
  force_google_safesearch_.Destroy();
  auth_server_whitelist_.Destroy();
  auth_negotiate_delegate_whitelist_.Destroy();

  CEF_POST_TASK(CEF_IOT,
      base::Bind(&CefURLRequestContextGetterImpl::ShutdownOnIOThread, this));
}

void CefURLRequestContextGetterImpl::ShutdownOnIOThread() {
  CEF_REQUIRE_IOT();

  shutting_down_ = true;

  // Delete the ProxyService object here so that any pending requests will be
  // canceled before the URLRequestContext is destroyed.
  io_state_->storage_->set_proxy_service(NULL);

  io_state_.reset();

  NotifyContextShuttingDown();
}

net::URLRequestContext* CefURLRequestContextGetterImpl::GetURLRequestContext() {
  CEF_REQUIRE_IOT();

  if (shutting_down_)
    return nullptr;

  if (!io_state_->url_request_context_.get()) {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();

    base::FilePath cache_path;
    if (settings_.cache_path.length > 0)
      cache_path = base::FilePath(CefString(&settings_.cache_path));

    io_state_->url_request_context_.reset(new CefURLRequestContextImpl());
    io_state_->url_request_context_->set_net_log(io_state_->net_log_);

    io_state_->storage_.reset(new net::URLRequestContextStorage(
        io_state_->url_request_context_.get()));

    SetCookieStoragePath(cache_path,
                         settings_.persist_session_cookies ? true : false);

    std::unique_ptr<CefNetworkDelegate> network_delegate(
        new CefNetworkDelegate());
    network_delegate->set_force_google_safesearch(&force_google_safesearch_);
    io_state_->storage_->set_network_delegate(std::move(network_delegate));

    const std::string& accept_language =
        settings_.accept_language_list.length > 0 ?
            CefString(&settings_.accept_language_list): "en-US,en";
    io_state_->storage_->set_http_user_agent_settings(base::WrapUnique(
        new CefHttpUserAgentSettings(accept_language)));

    io_state_->storage_->set_host_resolver(
        net::HostResolver::CreateDefaultResolver(io_state_->net_log_));
    io_state_->storage_->set_cert_verifier(net::CertVerifier::CreateDefault());

    std::unique_ptr<net::TransportSecurityState> transport_security_state(
        new net::TransportSecurityState);
    transport_security_state->set_enforce_net_security_expiration(
        settings_.enable_net_security_expiration ? true : false);
    io_state_->storage_->set_transport_security_state(
        std::move(transport_security_state));

    std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs(
        net::ct::CreateLogVerifiersForKnownLogs());
    std::unique_ptr<net::MultiLogCTVerifier> ct_verifier(
        new net::MultiLogCTVerifier());
    ct_verifier->AddLogs(ct_logs);
    io_state_->storage_->set_cert_transparency_verifier(std::move(ct_verifier));

    std::unique_ptr<net::CTPolicyEnforcer> ct_policy_enforcer(
        new net::CTPolicyEnforcer);
    ct_policy_enforcer->set_enforce_net_security_expiration(
        settings_.enable_net_security_expiration ? true : false);
    io_state_->storage_->set_ct_policy_enforcer(std::move(ct_policy_enforcer));

    std::unique_ptr<net::ProxyService> system_proxy_service =
        ProxyServiceFactory::CreateProxyService(
            io_state_->net_log_,
            io_state_->url_request_context_.get(),
            io_state_->url_request_context_->network_delegate(),
            std::move(io_state_->proxy_config_service_),
            *command_line,
            quick_check_enabled_.GetValue(),
            pac_https_url_stripping_enabled_.GetValue());
    io_state_->storage_->set_proxy_service(std::move(system_proxy_service));

    io_state_->storage_->set_ssl_config_service(
        new net::SSLConfigServiceDefaults);

    std::vector<std::string> supported_schemes;
    supported_schemes.push_back("basic");
    supported_schemes.push_back("digest");
    supported_schemes.push_back("ntlm");
    supported_schemes.push_back("negotiate");

    io_state_->http_auth_preferences_.reset(
        new net::HttpAuthPreferences(supported_schemes
#if defined(OS_POSIX) && !defined(OS_ANDROID)
                                     , io_state_->gsapi_library_name_
#endif
        ));

    io_state_->storage_->set_http_auth_handler_factory(
        net::HttpAuthHandlerRegistryFactory::Create(
            io_state_->http_auth_preferences_.get(),
            io_state_->url_request_context_->host_resolver()));
    io_state_->storage_->set_http_server_properties(base::WrapUnique(
        new net::HttpServerPropertiesImpl));

    base::FilePath http_cache_path;
    if (!cache_path.empty())
      http_cache_path = cache_path.Append(FILE_PATH_LITERAL("Cache"));

    UpdateServerWhitelist();
    UpdateDelegateWhitelist();

    std::unique_ptr<net::HttpCache::DefaultBackend> main_backend(
        new net::HttpCache::DefaultBackend(
            cache_path.empty() ? net::MEMORY_CACHE : net::DISK_CACHE,
            net::CACHE_BACKEND_DEFAULT,
            http_cache_path,
            0,
            BrowserThread::GetTaskRunnerForThread(
                BrowserThread::CACHE)));

    net::HttpNetworkSession::Params network_session_params;
    network_session_params.host_resolver =
        io_state_->url_request_context_->host_resolver();
    network_session_params.cert_verifier =
        io_state_->url_request_context_->cert_verifier();
    network_session_params.transport_security_state =
        io_state_->url_request_context_->transport_security_state();
    network_session_params.cert_transparency_verifier =
        io_state_->url_request_context_->cert_transparency_verifier();
    network_session_params.ct_policy_enforcer =
        io_state_->url_request_context_->ct_policy_enforcer();
    network_session_params.proxy_service =
        io_state_->url_request_context_->proxy_service();
    network_session_params.ssl_config_service =
        io_state_->url_request_context_->ssl_config_service();
    network_session_params.http_auth_handler_factory =
        io_state_->url_request_context_->http_auth_handler_factory();
    network_session_params.http_server_properties =
        io_state_->url_request_context_->http_server_properties();
    network_session_params.ignore_certificate_errors =
        settings_.ignore_certificate_errors ? true : false;
    network_session_params.net_log = io_state_->net_log_;

    io_state_->storage_->set_http_network_session(
        base::WrapUnique(new net::HttpNetworkSession(network_session_params)));
    io_state_->storage_->set_http_transaction_factory(base::WrapUnique(
        new net::HttpCache(io_state_->storage_->http_network_session(),
                           std::move(main_backend),
                           true /* set_up_quic_server_info */)));

    std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory(
        new net::URLRequestJobFactoryImpl());
    io_state_->url_request_manager_.reset(
        new CefURLRequestManager(job_factory.get()));

    // Install internal scheme handlers that cannot be overridden.
    scheme::InstallInternalProtectedHandlers(
        job_factory.get(),
        io_state_->url_request_manager_.get(),
        &io_state_->protocol_handlers_,
        network_session_params.host_resolver);
    io_state_->protocol_handlers_.clear();

    // Register internal scheme handlers that can be overridden.
    scheme::RegisterInternalHandlers(io_state_->url_request_manager_.get());

    io_state_->request_interceptors_.push_back(
        base::MakeUnique<CefRequestInterceptor>());

    // Set up interceptors in the reverse order.
    std::unique_ptr<net::URLRequestJobFactory> top_job_factory =
        std::move(job_factory);
    for (auto i = io_state_->request_interceptors_.rbegin();
         i != io_state_->request_interceptors_.rend(); ++i) {
      top_job_factory.reset(new net::URLRequestInterceptingJobFactory(
          std::move(top_job_factory), std::move(*i)));
    }
    io_state_->request_interceptors_.clear();

    io_state_->storage_->set_job_factory(std::move(top_job_factory));

#if defined(USE_NSS_CERTS)
    // Only do this for the first (global) request context.
    static bool request_context_for_nss_set = false;
    if (!request_context_for_nss_set) {
      net::SetURLRequestContextForNSSHttpIO(
          io_state_->url_request_context_.get());
      request_context_for_nss_set = true;
    }
#endif
  }

  return io_state_->url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
    CefURLRequestContextGetterImpl::GetNetworkTaskRunner() const {
  return BrowserThread::GetTaskRunnerForThread(CEF_IOT);
}

net::HostResolver* CefURLRequestContextGetterImpl::GetHostResolver() const {
  return io_state_->url_request_context_->host_resolver();
}

void CefURLRequestContextGetterImpl::SetCookieStoragePath(
    const base::FilePath& path,
    bool persist_session_cookies) {
  CEF_REQUIRE_IOT();

  if (io_state_->url_request_context_->cookie_store() &&
      ((io_state_->cookie_store_path_.empty() && path.empty()) ||
       io_state_->cookie_store_path_ == path)) {
    // The path has not changed so don't do anything.
    return;
  }

  scoped_refptr<net::SQLitePersistentCookieStore> persistent_store;
  if (!path.empty()) {
    // TODO(cef): Move directory creation to the blocking pool instead of
    // allowing file IO on this thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (base::DirectoryExists(path) ||
        base::CreateDirectory(path)) {
      const base::FilePath& cookie_path = path.AppendASCII("Cookies");
      persistent_store =
          new net::SQLitePersistentCookieStore(
              cookie_path,
              BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
              BrowserThread::GetTaskRunnerForThread(BrowserThread::DB),
              persist_session_cookies,
              NULL);
    } else {
      NOTREACHED() << "The cookie storage directory could not be created";
    }
  }

  // Set the new cookie store that will be used for all new requests. The old
  // cookie store, if any, will be automatically flushed and closed when no
  // longer referenced.
  std::unique_ptr<net::CookieMonster> cookie_monster(
      new net::CookieMonster(persistent_store.get(), NULL));
  if (persistent_store.get() && persist_session_cookies)
      cookie_monster->SetPersistSessionCookies(true);
  io_state_->cookie_store_path_ = path;

  // Restore the previously supported schemes.
  CefCookieManagerImpl::SetCookieMonsterSchemes(
      cookie_monster.get(), io_state_->cookie_supported_schemes_);

  io_state_->storage_->set_cookie_store(std::move(cookie_monster));
}

void CefURLRequestContextGetterImpl::SetCookieSupportedSchemes(
    const std::vector<std::string>& schemes) {
  CEF_REQUIRE_IOT();

  io_state_->cookie_supported_schemes_ = schemes;
  CefCookieManagerImpl::SetCookieMonsterSchemes(
      static_cast<net::CookieMonster*>(GetExistingCookieStore()),
      io_state_->cookie_supported_schemes_);
}

void CefURLRequestContextGetterImpl::AddHandler(
    CefRefPtr<CefRequestContextHandler> handler) {
  if (!CEF_CURRENTLY_ON_IOT()) {
    CEF_POST_TASK(CEF_IOT,
        base::Bind(&CefURLRequestContextGetterImpl::AddHandler, this, handler));
    return;
  }
  io_state_->handler_list_.push_back(handler);
}

net::CookieStore*
    CefURLRequestContextGetterImpl::GetExistingCookieStore() const {
  CEF_REQUIRE_IOT();
  if (io_state_->url_request_context_ &&
      io_state_->url_request_context_->cookie_store()) {
    return io_state_->url_request_context_->cookie_store();
  }

  LOG(ERROR) << "Cookie store does not exist";
  return nullptr;
}

void CefURLRequestContextGetterImpl::CreateProxyConfigService() {
  if (io_state_->proxy_config_service_.get())
    return;

  io_state_->proxy_config_service_ =
      net::ProxyService::CreateSystemProxyConfigService(
          io_state_->io_task_runner_, io_state_->file_task_runner_);
}

void CefURLRequestContextGetterImpl::UpdateServerWhitelist() {
  io_state_->http_auth_preferences_->set_server_whitelist(
      auth_server_whitelist_.GetValue());
}

void CefURLRequestContextGetterImpl::UpdateDelegateWhitelist() {
  io_state_->http_auth_preferences_->set_delegate_whitelist(
      auth_negotiate_delegate_whitelist_.GetValue());
}
