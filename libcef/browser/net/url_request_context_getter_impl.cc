// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net/url_request_context_getter_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/cookie_manager_impl.h"
#include "libcef/browser/net/cookie_store_proxy.h"
#include "libcef/browser/net/cookie_store_source.h"
#include "libcef/browser/net/network_delegate.h"
#include "libcef/browser/net/scheme_handler.h"
#include "libcef/browser/net/url_request_interceptor.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/content_client.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"
#include "components/certificate_transparency/ct_known_logs.h"
#include "components/net_log/chrome_net_log.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/host_resolver.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_cache.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/proxy_resolution/dhcp_pac_file_fetcher_factory.h"
#include "net/proxy_resolution/pac_file_fetcher_impl.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_job_manager.h"
#include "services/network/proxy_service_mojo.h"
#include "url/url_constants.h"

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
      : http_accept_language_(
            net::HttpUtil::GenerateAcceptLanguageHeader(raw_language_list)) {
    CEF_REQUIRE_IOT();
  }

  // net::HttpUserAgentSettings implementation
  std::string GetAcceptLanguage() const override {
    CEF_REQUIRE_IOT();
    return http_accept_language_;
  }

  std::string GetUserAgent() const override {
    CEF_REQUIRE_IOT();
    return CefContentClient::Get()->browser()->GetUserAgent();
  }

 private:
  const std::string http_accept_language_;

  DISALLOW_COPY_AND_ASSIGN(CefHttpUserAgentSettings);
};

// Based on ProxyResolutionServiceFactory::CreateProxyResolutionService which
// was deleted in http://crrev.com/1c261ff4.
std::unique_ptr<net::ProxyResolutionService> CreateProxyResolutionService(
    net::NetLog* net_log,
    net::URLRequestContext* context,
    net::NetworkDelegate* network_delegate,
    proxy_resolver::mojom::ProxyResolverFactoryPtr proxy_resolver_factory,
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    const base::CommandLine& command_line,
    bool quick_check_enabled,
    bool pac_https_url_stripping_enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool use_v8 = !command_line.HasSwitch(switches::kWinHttpProxyResolver);
  // TODO(eroman): Figure out why this doesn't work in single-process mode.
  // Should be possible now that a private isolate is used.
  // http://crbug.com/474654
  if (use_v8 && command_line.HasSwitch(switches::kSingleProcess)) {
    LOG(ERROR) << "Cannot use V8 Proxy resolver in single process mode.";
    use_v8 = false;  // Fallback to non-v8 implementation.
  }

  std::unique_ptr<net::ProxyResolutionService> proxy_service;
  if (use_v8) {
    std::unique_ptr<net::DhcpPacFileFetcher> dhcp_pac_file_fetcher;
    net::DhcpPacFileFetcherFactory dhcp_factory;
    dhcp_pac_file_fetcher = dhcp_factory.Create(context);

    proxy_service = network::CreateProxyResolutionServiceUsingMojoFactory(
        std::move(proxy_resolver_factory), std::move(proxy_config_service),
        net::PacFileFetcherImpl::Create(context),
        std::move(dhcp_pac_file_fetcher), context->host_resolver(), net_log,
        network_delegate);
  } else {
    proxy_service = net::ProxyResolutionService::CreateUsingSystemProxyResolver(
        std::move(proxy_config_service), net_log);
  }

  proxy_service->set_quick_check_enabled(quick_check_enabled);
  proxy_service->set_sanitize_url_policy(
      pac_https_url_stripping_enabled
          ? net::ProxyResolutionService::SanitizeUrlPolicy::SAFE
          : net::ProxyResolutionService::SanitizeUrlPolicy::UNSAFE);

  return proxy_service;
}

// Based on net::ct::CreateLogVerifiersForKnownLogs which was deleted in
// https://crrev.com/24711fe395.
std::vector<scoped_refptr<const net::CTLogVerifier>>
CreateLogVerifiersForKnownLogs() {
  std::vector<scoped_refptr<const net::CTLogVerifier>> verifiers;

  for (const auto& log : certificate_transparency::GetKnownLogs()) {
    scoped_refptr<const net::CTLogVerifier> log_verifier =
        net::CTLogVerifier::Create(
            base::StringPiece(log.log_key, log.log_key_length), log.log_name,
            log.log_dns_domain);
    // Make sure no null logs enter verifiers. Parsing of all statically
    // configured logs should always succeed, unless there has been binary or
    // memory corruption.
    CHECK(log_verifier);
    verifiers.push_back(std::move(log_verifier));
  }

  return verifiers;
}

}  // namespace

CefURLRequestContextGetterImpl::CefURLRequestContextGetterImpl(
    const CefRequestContextSettings& settings,
    PrefService* pref_service,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    content::ProtocolHandlerMap* protocol_handlers,
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    content::URLRequestInterceptorScopedVector request_interceptors)
    : settings_(settings), io_state_(std::make_unique<IOState>()) {
  // Must first be created on the UI thread.
  CEF_REQUIRE_UIT();

  io_state_->net_log_ = g_browser_process->net_log(),
  DCHECK(io_state_->net_log_);
  io_state_->io_task_runner_ = std::move(io_task_runner);
  io_state_->proxy_resolver_factory_ =
      ChromeMojoProxyResolverFactory::CreateWithStrongBinding();
  io_state_->proxy_config_service_ = std::move(proxy_config_service);
  io_state_->request_interceptors_ = std::move(request_interceptors);

  std::swap(io_state_->protocol_handlers_, *protocol_handlers);

  auto io_thread_proxy =
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});

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

  CEF_POST_TASK(
      CEF_IOT,
      base::Bind(&CefURLRequestContextGetterImpl::ShutdownOnIOThread, this));
}

void CefURLRequestContextGetterImpl::ShutdownOnIOThread() {
  CEF_REQUIRE_IOT();

  shutting_down_ = true;

  // Delete the ProxyResolutionService object here so that any pending requests
  // will be canceled before the URLRequestContext is destroyed.
  io_state_->storage_->set_proxy_resolution_service(NULL);

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
    io_state_->url_request_context_->set_enable_brotli(true);

    io_state_->storage_.reset(new net::URLRequestContextStorage(
        io_state_->url_request_context_.get()));

    SetCookieStoragePath(cache_path,
                         settings_.persist_session_cookies ? true : false);

    std::unique_ptr<CefNetworkDelegate> network_delegate(
        new CefNetworkDelegate());
    network_delegate->set_force_google_safesearch(&force_google_safesearch_);
    io_state_->storage_->set_network_delegate(std::move(network_delegate));

    const std::string& accept_language =
        settings_.accept_language_list.length > 0
            ? CefString(&settings_.accept_language_list)
            : "en-US,en";
    io_state_->storage_->set_http_user_agent_settings(
        base::WrapUnique(new CefHttpUserAgentSettings(accept_language)));

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
        CreateLogVerifiersForKnownLogs());
    std::unique_ptr<net::MultiLogCTVerifier> ct_verifier(
        new net::MultiLogCTVerifier());
    ct_verifier->AddLogs(ct_logs);
    io_state_->storage_->set_cert_transparency_verifier(std::move(ct_verifier));

    std::unique_ptr<certificate_transparency::ChromeCTPolicyEnforcer>
        ct_policy_enforcer(
            new certificate_transparency::ChromeCTPolicyEnforcer);
    ct_policy_enforcer->set_enforce_net_security_expiration(
        settings_.enable_net_security_expiration ? true : false);
    io_state_->storage_->set_ct_policy_enforcer(std::move(ct_policy_enforcer));

    std::unique_ptr<net::ProxyResolutionService> system_proxy_service =
        CreateProxyResolutionService(
            io_state_->net_log_, io_state_->url_request_context_.get(),
            io_state_->url_request_context_->network_delegate(),
            std::move(io_state_->proxy_resolver_factory_),
            std::move(io_state_->proxy_config_service_), *command_line,
            quick_check_enabled_.GetValue(),
            pac_https_url_stripping_enabled_.GetValue());
    io_state_->storage_->set_proxy_resolution_service(
        std::move(system_proxy_service));

    io_state_->storage_->set_ssl_config_service(
        std::make_unique<net::SSLConfigServiceDefaults>());

    std::vector<std::string> supported_schemes;
    supported_schemes.push_back("basic");
    supported_schemes.push_back("digest");
    supported_schemes.push_back("ntlm");
    supported_schemes.push_back("negotiate");

    io_state_->http_auth_preferences_.reset(new net::HttpAuthPreferences());

    io_state_->storage_->set_http_auth_handler_factory(
        net::HttpAuthHandlerRegistryFactory::Create(
            io_state_->url_request_context_->host_resolver(),
            io_state_->http_auth_preferences_.get(), supported_schemes
#if defined(OS_POSIX) && !defined(OS_ANDROID)
            ,
            io_state_->gsapi_library_name_
#endif
            ));
    io_state_->storage_->set_http_server_properties(
        base::WrapUnique(new net::HttpServerPropertiesImpl));

    base::FilePath http_cache_path;
    if (!cache_path.empty())
      http_cache_path = cache_path.Append(FILE_PATH_LITERAL("Cache"));

    UpdateServerWhitelist();
    UpdateDelegateWhitelist();

    std::unique_ptr<net::HttpCache::DefaultBackend> main_backend(
        new net::HttpCache::DefaultBackend(
            cache_path.empty() ? net::MEMORY_CACHE : net::DISK_CACHE,
            net::CACHE_BACKEND_DEFAULT, http_cache_path, 0));

    net::HttpNetworkSession::Context network_session_context;
    network_session_context.host_resolver =
        io_state_->url_request_context_->host_resolver();
    network_session_context.cert_verifier =
        io_state_->url_request_context_->cert_verifier();
    network_session_context.transport_security_state =
        io_state_->url_request_context_->transport_security_state();
    network_session_context.cert_transparency_verifier =
        io_state_->url_request_context_->cert_transparency_verifier();
    network_session_context.ct_policy_enforcer =
        io_state_->url_request_context_->ct_policy_enforcer();
    network_session_context.proxy_resolution_service =
        io_state_->url_request_context_->proxy_resolution_service();
    network_session_context.ssl_config_service =
        io_state_->url_request_context_->ssl_config_service();
    network_session_context.http_auth_handler_factory =
        io_state_->url_request_context_->http_auth_handler_factory();
    network_session_context.http_server_properties =
        io_state_->url_request_context_->http_server_properties();
    network_session_context.net_log = io_state_->net_log_;

    net::HttpNetworkSession::Params network_session_params;
    network_session_configurator::ParseCommandLineAndFieldTrials(
        *base::CommandLine::ForCurrentProcess(),
        false /* is_quic_force_disabled */,
        CefContentClient::Get()
            ->browser()
            ->GetUserAgent() /* quic_user_agent_id */,
        &network_session_params);
    network_session_params.ignore_certificate_errors =
        settings_.ignore_certificate_errors ? true : false;

    io_state_->storage_->set_http_network_session(
        base::WrapUnique(new net::HttpNetworkSession(network_session_params,
                                                     network_session_context)));
    io_state_->storage_->set_http_transaction_factory(
        base::WrapUnique(new net::HttpCache(
            io_state_->storage_->http_network_session(),
            std::move(main_backend), true /* set_up_quic_server_info */)));

    std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory(
        new net::URLRequestJobFactoryImpl());
    io_state_->url_request_manager_.reset(
        new CefURLRequestManager(job_factory.get()));

    // Install internal scheme handlers that cannot be overridden.
    scheme::InstallInternalProtectedHandlers(
        job_factory.get(), io_state_->url_request_manager_.get(),
        &io_state_->protocol_handlers_, network_session_context.host_resolver);
    io_state_->protocol_handlers_.clear();

    // Register internal scheme handlers that can be overridden.
    scheme::RegisterInternalHandlers(io_state_->url_request_manager_.get());

    io_state_->request_interceptors_.push_back(
        std::make_unique<CefRequestInterceptor>());

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
  return base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});
}

net::HostResolver* CefURLRequestContextGetterImpl::GetHostResolver() const {
  return io_state_->url_request_context_->host_resolver();
}

void CefURLRequestContextGetterImpl::SetCookieStoragePath(
    const base::FilePath& path,
    bool persist_session_cookies) {
  CEF_REQUIRE_IOT();
  if (!io_state_->cookie_source_) {
    // Use a proxy because we can't change the URLRequestContext's CookieStore
    // during runtime.
    io_state_->cookie_source_ = new CefCookieStoreOwnerSource();
    io_state_->storage_->set_cookie_store(std::make_unique<CefCookieStoreProxy>(
        base::WrapUnique(io_state_->cookie_source_)));
  }
  io_state_->cookie_source_->SetCookieStoragePath(path, persist_session_cookies,
                                                  io_state_->net_log_);
}

void CefURLRequestContextGetterImpl::SetCookieSupportedSchemes(
    const std::vector<std::string>& schemes) {
  CEF_REQUIRE_IOT();
  io_state_->cookie_source_->SetCookieSupportedSchemes(schemes);
}

void CefURLRequestContextGetterImpl::AddHandler(
    CefRefPtr<CefRequestContextHandler> handler) {
  if (!CEF_CURRENTLY_ON_IOT()) {
    CEF_POST_TASK(
        CEF_IOT,
        base::Bind(&CefURLRequestContextGetterImpl::AddHandler, this, handler));
    return;
  }
  io_state_->handler_list_.push_back(handler);
}

net::CookieStore* CefURLRequestContextGetterImpl::GetExistingCookieStore()
    const {
  CEF_REQUIRE_IOT();
  if (io_state_->cookie_source_) {
    return io_state_->cookie_source_->GetCookieStore();
  }

  LOG(ERROR) << "Cookie store does not exist";
  return nullptr;
}

void CefURLRequestContextGetterImpl::UpdateServerWhitelist() {
  io_state_->http_auth_preferences_->SetServerWhitelist(
      auth_server_whitelist_.GetValue());
}

void CefURLRequestContextGetterImpl::UpdateDelegateWhitelist() {
  io_state_->http_auth_preferences_->SetDelegateWhitelist(
      auth_negotiate_delegate_whitelist_.GetValue());
}
