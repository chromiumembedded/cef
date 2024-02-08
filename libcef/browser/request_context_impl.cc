// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/context.h"
#include "libcef/browser/prefs/pref_helper.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/task_runner_impl.h"
#include "libcef/common/values_impl.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/host_resolver.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

using content::BrowserThread;

namespace {

base::AtomicSequenceNumber g_next_id;

class ResolveHostHelper : public network::ResolveHostClientBase {
 public:
  explicit ResolveHostHelper(CefRefPtr<CefResolveCallback> callback)
      : callback_(callback), receiver_(this) {}

  ResolveHostHelper(const ResolveHostHelper&) = delete;
  ResolveHostHelper& operator=(const ResolveHostHelper&) = delete;

  void Start(CefBrowserContext* browser_context, const CefString& origin) {
    CEF_REQUIRE_UIT();

    browser_context->GetNetworkContext()->CreateHostResolver(
        net::DnsConfigOverrides(), host_resolver_.BindNewPipeAndPassReceiver());

    host_resolver_.set_disconnect_handler(base::BindOnce(
        &ResolveHostHelper::OnComplete, base::Unretained(this), net::ERR_FAILED,
        net::ResolveErrorInfo(net::ERR_FAILED), absl::nullopt, absl::nullopt));

    host_resolver_->ResolveHost(
        network::mojom::HostResolverHost::NewHostPortPair(
            net::HostPortPair::FromURL(GURL(origin.ToString()))),
        net::NetworkAnonymizationKey::CreateTransient(), nullptr,
        receiver_.BindNewPipeAndPassRemote());
  }

 private:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& resolved_addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadat) override {
    CEF_REQUIRE_UIT();

    host_resolver_.reset();
    receiver_.reset();

    std::vector<CefString> resolved_ips;

    if (result == net::OK && resolved_addresses.has_value()) {
      DCHECK(!resolved_addresses->empty());
      for (const auto& value : *resolved_addresses) {
        resolved_ips.push_back(value.ToStringWithoutPort());
      }
    }

    callback_->OnResolveCompleted(static_cast<cef_errorcode_t>(result),
                                  resolved_ips);
    delete this;
  }

  CefRefPtr<CefResolveCallback> callback_;

  mojo::Remote<network::mojom::HostResolver> host_resolver_;
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
};

CefBrowserContext* GetCefBrowserContext(
    CefRefPtr<CefRequestContext> request_context) {
  CEF_REQUIRE_UIT();
  CefRefPtr<CefRequestContextImpl> request_context_impl =
      CefRequestContextImpl::GetOrCreateForRequestContext(request_context);
  CHECK(request_context_impl);
  auto* cef_browser_context = request_context_impl->GetBrowserContext();
  CHECK(cef_browser_context);
  return cef_browser_context;
}

}  // namespace

// CefBrowserContext

// static
CefRefPtr<CefRequestContext> CefRequestContext::GetGlobalContext() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return nullptr;
  }

  CefRequestContextImpl::Config config;
  config.is_global = true;
  return CefRequestContextImpl::GetOrCreateRequestContext(std::move(config));
}

// static
CefRefPtr<CefRequestContext> CefRequestContext::CreateContext(
    const CefRequestContextSettings& settings,
    CefRefPtr<CefRequestContextHandler> handler) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return nullptr;
  }

  CefRequestContextImpl::Config config;
  config.settings = settings;
  config.handler = handler;
  config.unique_id = g_next_id.GetNext();
  return CefRequestContextImpl::GetOrCreateRequestContext(std::move(config));
}

// static
CefRefPtr<CefRequestContext> CefRequestContext::CreateContext(
    CefRefPtr<CefRequestContext> other,
    CefRefPtr<CefRequestContextHandler> handler) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return nullptr;
  }

  if (!other.get()) {
    return nullptr;
  }

  CefRequestContextImpl::Config config;
  config.other = static_cast<CefRequestContextImpl*>(other.get());
  config.handler = handler;
  config.unique_id = g_next_id.GetNext();
  return CefRequestContextImpl::GetOrCreateRequestContext(std::move(config));
}

// CefRequestContextImpl

CefRequestContextImpl::~CefRequestContextImpl() {
  CEF_REQUIRE_UIT();

  if (browser_context_) {
    // May result in |browser_context_| being deleted if no other
    // CefRequestContextImpl are referencing it.
    browser_context_->RemoveCefRequestContext(this);
  }
}

// static
CefRefPtr<CefRequestContextImpl>
CefRequestContextImpl::CreateGlobalRequestContext(
    const CefRequestContextSettings& settings) {
  // Create and initialize the global context immediately.
  Config config;
  config.is_global = true;
  config.settings = settings;
  CefRefPtr<CefRequestContextImpl> impl =
      new CefRequestContextImpl(std::move(config));
  impl->Initialize();
  return impl;
}

// static
CefRefPtr<CefRequestContextImpl>
CefRequestContextImpl::GetOrCreateForRequestContext(
    CefRefPtr<CefRequestContext> request_context) {
  if (request_context.get()) {
    // Use the context from the provided CefRequestContext.
    return static_cast<CefRequestContextImpl*>(request_context.get());
  }

  // Use the global context.
  Config config;
  config.is_global = true;
  return CefRequestContextImpl::GetOrCreateRequestContext(std::move(config));
}

// static
CefRefPtr<CefRequestContextImpl>
CefRequestContextImpl::GetOrCreateForBrowserContext(
    CefBrowserContext* browser_context,
    CefRefPtr<CefRequestContextHandler> handler) {
  DCHECK(browser_context);

  Config config;
  config.browser_context = browser_context;
  config.handler = handler;
  config.unique_id = g_next_id.GetNext();
  return CefRequestContextImpl::GetOrCreateRequestContext(std::move(config));
}

content::BrowserContext* CefRequestContextImpl::GetBrowserContext(
    CefRefPtr<CefRequestContext> request_context) {
  auto* browser_context =
      GetCefBrowserContext(request_context)->AsBrowserContext();
  CHECK(browser_context);
  return browser_context;
}

Profile* CefRequestContextImpl::GetProfile(
    CefRefPtr<CefRequestContext> request_context) {
  auto* profile = GetCefBrowserContext(request_context)->AsProfile();
  CHECK(profile);
  return profile;
}

bool CefRequestContextImpl::VerifyBrowserContext() const {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  if (!browser_context() || !browser_context()->IsInitialized()) {
    DCHECK(false) << "Uninitialized context";
    return false;
  }

  return true;
}

CefBrowserContext* CefRequestContextImpl::GetBrowserContext() {
  CHECK(VerifyBrowserContext());
  return browser_context();
}

void CefRequestContextImpl::ExecuteWhenBrowserContextInitialized(
    base::OnceClosure callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(
            &CefRequestContextImpl::ExecuteWhenBrowserContextInitialized, this,
            std::move(callback)));
    return;
  }

  browser_context()->StoreOrTriggerInitCallback(std::move(callback));
}

void CefRequestContextImpl::GetBrowserContext(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    BrowserContextCallback callback) {
  if (!task_runner.get()) {
    task_runner = CefTaskRunnerImpl::GetCurrentTaskRunner();
  }

  ExecuteWhenBrowserContextInitialized(base::BindOnce(
      [](CefRefPtr<CefRequestContextImpl> context,
         scoped_refptr<base::SingleThreadTaskRunner> task_runner,
         BrowserContextCallback callback) {
        CEF_REQUIRE_UIT();

        auto browser_context = context->browser_context();
        DCHECK(browser_context->IsInitialized());

        if (task_runner->BelongsToCurrentThread()) {
          // Execute the callback immediately.
          std::move(callback).Run(browser_context->getter());
        } else {
          // Execute the callback on the target thread.
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), browser_context->getter()));
        }
      },
      CefRefPtr<CefRequestContextImpl>(this), task_runner,
      std::move(callback)));
}

bool CefRequestContextImpl::IsSame(CefRefPtr<CefRequestContext> other) {
  CefRequestContextImpl* other_impl =
      static_cast<CefRequestContextImpl*>(other.get());
  if (!other_impl) {
    return false;
  }

  // Compare whether both are the global context.
  if (config_.is_global && other_impl->config_.is_global) {
    return true;
  }

  // Compare CefBrowserContext pointers if one has been associated.
  if (browser_context() && other_impl->browser_context()) {
    return (browser_context() == other_impl->browser_context());
  } else if (browser_context() || other_impl->browser_context()) {
    return false;
  }

  // Otherwise compare unique IDs.
  return (config_.unique_id == other_impl->config_.unique_id);
}

bool CefRequestContextImpl::IsSharingWith(CefRefPtr<CefRequestContext> other) {
  CefRequestContextImpl* other_impl =
      static_cast<CefRequestContextImpl*>(other.get());
  if (!other_impl) {
    return false;
  }

  if (IsSame(other)) {
    return true;
  }

  CefRefPtr<CefRequestContext> pending_other = config_.other;
  if (pending_other.get()) {
    // This object is not initialized but we know what context this object will
    // share with. Compare to that other context instead.
    return pending_other->IsSharingWith(other);
  }

  pending_other = other_impl->config_.other;
  if (pending_other.get()) {
    // The other object is not initialized but we know what context that object
    // will share with. Compare to that other context instead.
    return pending_other->IsSharingWith(this);
  }

  // This or the other object is not initialized. Compare the cache path values.
  // If both are non-empty and the same then they'll share the same storage.
  if (config_.settings.cache_path.length > 0 &&
      other_impl->config_.settings.cache_path.length > 0) {
    return (
        base::FilePath(CefString(&config_.settings.cache_path)) ==
        base::FilePath(CefString(&other_impl->config_.settings.cache_path)));
  }

  return false;
}

bool CefRequestContextImpl::IsGlobal() {
  return config_.is_global;
}

CefRefPtr<CefRequestContextHandler> CefRequestContextImpl::GetHandler() {
  return config_.handler;
}

CefString CefRequestContextImpl::GetCachePath() {
  return CefString(&config_.settings.cache_path);
}

CefRefPtr<CefCookieManager> CefRequestContextImpl::GetCookieManager(
    CefRefPtr<CefCompletionCallback> callback) {
  CefRefPtr<CefCookieManagerImpl> cookie_manager = new CefCookieManagerImpl();
  InitializeCookieManagerInternal(cookie_manager, callback);
  return cookie_manager.get();
}

bool CefRequestContextImpl::RegisterSchemeHandlerFactory(
    const CefString& scheme_name,
    const CefString& domain_name,
    CefRefPtr<CefSchemeHandlerFactory> factory) {
  GetBrowserContext(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(
          [](const CefString& scheme_name, const CefString& domain_name,
             CefRefPtr<CefSchemeHandlerFactory> factory,
             CefBrowserContext::Getter browser_context_getter) {
            auto browser_context = browser_context_getter.Run();
            if (browser_context) {
              browser_context->RegisterSchemeHandlerFactory(
                  scheme_name, domain_name, factory);
            }
          },
          scheme_name, domain_name, factory));

  return true;
}

bool CefRequestContextImpl::ClearSchemeHandlerFactories() {
  GetBrowserContext(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce([](CefBrowserContext::Getter browser_context_getter) {
        auto browser_context = browser_context_getter.Run();
        if (browser_context) {
          browser_context->ClearSchemeHandlerFactories();
        }
      }));

  return true;
}

bool CefRequestContextImpl::HasPreference(const CefString& name) {
  if (!VerifyBrowserContext()) {
    return false;
  }

  PrefService* pref_service = browser_context()->AsProfile()->GetPrefs();
  return pref_helper::HasPreference(pref_service, name);
}

CefRefPtr<CefValue> CefRequestContextImpl::GetPreference(
    const CefString& name) {
  if (!VerifyBrowserContext()) {
    return nullptr;
  }

  PrefService* pref_service = browser_context()->AsProfile()->GetPrefs();
  return pref_helper::GetPreference(pref_service, name);
}

CefRefPtr<CefDictionaryValue> CefRequestContextImpl::GetAllPreferences(
    bool include_defaults) {
  if (!VerifyBrowserContext()) {
    return nullptr;
  }

  PrefService* pref_service = browser_context()->AsProfile()->GetPrefs();
  return pref_helper::GetAllPreferences(pref_service, include_defaults);
}

bool CefRequestContextImpl::CanSetPreference(const CefString& name) {
  if (!VerifyBrowserContext()) {
    return false;
  }

  PrefService* pref_service = browser_context()->AsProfile()->GetPrefs();
  return pref_helper::CanSetPreference(pref_service, name);
}

bool CefRequestContextImpl::SetPreference(const CefString& name,
                                          CefRefPtr<CefValue> value,
                                          CefString& error) {
  if (!VerifyBrowserContext()) {
    return false;
  }

  PrefService* pref_service = browser_context()->AsProfile()->GetPrefs();
  return pref_helper::SetPreference(pref_service, name, value, error);
}

void CefRequestContextImpl::ClearCertificateExceptions(
    CefRefPtr<CefCompletionCallback> callback) {
  GetBrowserContext(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&CefRequestContextImpl::ClearCertificateExceptionsInternal,
                     this, callback));
}

void CefRequestContextImpl::ClearHttpAuthCredentials(
    CefRefPtr<CefCompletionCallback> callback) {
  GetBrowserContext(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&CefRequestContextImpl::ClearHttpAuthCredentialsInternal,
                     this, callback));
}

void CefRequestContextImpl::CloseAllConnections(
    CefRefPtr<CefCompletionCallback> callback) {
  GetBrowserContext(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&CefRequestContextImpl::CloseAllConnectionsInternal, this,
                     callback));
}

void CefRequestContextImpl::ResolveHost(
    const CefString& origin,
    CefRefPtr<CefResolveCallback> callback) {
  GetBrowserContext(content::GetUIThreadTaskRunner({}),
                    base::BindOnce(&CefRequestContextImpl::ResolveHostInternal,
                                   this, origin, callback));
}

void CefRequestContextImpl::LoadExtension(
    const CefString& root_directory,
    CefRefPtr<CefDictionaryValue> manifest,
    CefRefPtr<CefExtensionHandler> handler) {
  GetBrowserContext(content::GetUIThreadTaskRunner({}),
                    base::BindOnce(
                        [](const CefString& root_directory,
                           CefRefPtr<CefDictionaryValue> manifest,
                           CefRefPtr<CefExtensionHandler> handler,
                           CefRefPtr<CefRequestContextImpl> self,
                           CefBrowserContext::Getter browser_context_getter) {
                          auto browser_context = browser_context_getter.Run();
                          if (browser_context) {
                            browser_context->LoadExtension(
                                root_directory, manifest, handler, self);
                          }
                        },
                        root_directory, manifest, handler,
                        CefRefPtr<CefRequestContextImpl>(this)));
}

bool CefRequestContextImpl::DidLoadExtension(const CefString& extension_id) {
  CefRefPtr<CefExtension> extension = GetExtension(extension_id);
  // GetLoaderContext() will return NULL for internal extensions.
  return extension && IsSame(extension->GetLoaderContext());
}

bool CefRequestContextImpl::HasExtension(const CefString& extension_id) {
  return !!GetExtension(extension_id);
}

bool CefRequestContextImpl::GetExtensions(
    std::vector<CefString>& extension_ids) {
  extension_ids.clear();

  if (!VerifyBrowserContext()) {
    return false;
  }

  return browser_context()->GetExtensions(extension_ids);
}

CefRefPtr<CefExtension> CefRequestContextImpl::GetExtension(
    const CefString& extension_id) {
  if (!VerifyBrowserContext()) {
    return nullptr;
  }

  return browser_context()->GetExtension(extension_id);
}

CefRefPtr<CefMediaRouter> CefRequestContextImpl::GetMediaRouter(
    CefRefPtr<CefCompletionCallback> callback) {
  CefRefPtr<CefMediaRouterImpl> media_router = new CefMediaRouterImpl();
  InitializeMediaRouterInternal(media_router, callback);
  return media_router.get();
}

CefRefPtr<CefValue> CefRequestContextImpl::GetWebsiteSetting(
    const CefString& requesting_url,
    const CefString& top_level_url,
    cef_content_setting_types_t content_type) {
  if (!VerifyBrowserContext()) {
    return nullptr;
  }

  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
      browser_context()->AsProfile());
  if (!settings_map) {
    return nullptr;
  }

  // Either or both URLs may be invalid.
  GURL requesting_gurl(requesting_url.ToString());
  GURL top_level_gurl(top_level_url.ToString());

  content_settings::SettingInfo info;
  base::Value value = settings_map->GetWebsiteSetting(
      requesting_gurl, top_level_gurl,
      static_cast<ContentSettingsType>(content_type), &info);
  if (value.is_none()) {
    return nullptr;
  }

  return new CefValueImpl(std::move(value));
}

void CefRequestContextImpl::SetWebsiteSetting(
    const CefString& requesting_url,
    const CefString& top_level_url,
    cef_content_setting_types_t content_type,
    CefRefPtr<CefValue> value) {
  GetBrowserContext(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&CefRequestContextImpl::SetWebsiteSettingInternal, this,
                     requesting_url, top_level_url, content_type, value));
}

cef_content_setting_values_t CefRequestContextImpl::GetContentSetting(
    const CefString& requesting_url,
    const CefString& top_level_url,
    cef_content_setting_types_t content_type) {
  // Verify that our enums match Chromium's values.
  static_assert(static_cast<int>(CEF_CONTENT_SETTING_TYPE_NUM_TYPES) ==
                    static_cast<int>(ContentSettingsType::NUM_TYPES),
                "Mismatched enum found for CEF_CONTENT_SETTING_TYPE_NUM_TYPES");
  static_assert(
      static_cast<int>(CEF_CONTENT_SETTING_VALUE_NUM_VALUES) ==
          static_cast<int>(CONTENT_SETTING_NUM_SETTINGS),
      "Mismatched enum found for CEF_CONTENT_SETTING_VALUE_NUM_VALUES");

  if (!VerifyBrowserContext()) {
    return CEF_CONTENT_SETTING_VALUE_DEFAULT;
  }

  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
      browser_context()->AsProfile());
  if (!settings_map) {
    return CEF_CONTENT_SETTING_VALUE_DEFAULT;
  }

  ContentSetting value = ContentSetting::CONTENT_SETTING_DEFAULT;

  if (requesting_url.empty() && top_level_url.empty()) {
    value = settings_map->GetDefaultContentSetting(
        static_cast<ContentSettingsType>(content_type),
        /*provider_id=*/nullptr);
  } else {
    GURL requesting_gurl(requesting_url.ToString());
    GURL top_level_gurl(top_level_url.ToString());
    if (requesting_gurl.is_valid() || top_level_gurl.is_valid()) {
      value = settings_map->GetContentSetting(
          requesting_gurl, top_level_gurl,
          static_cast<ContentSettingsType>(content_type));
    }
  }

  return static_cast<cef_content_setting_values_t>(value);
}

void CefRequestContextImpl::SetContentSetting(
    const CefString& requesting_url,
    const CefString& top_level_url,
    cef_content_setting_types_t content_type,
    cef_content_setting_values_t value) {
  GetBrowserContext(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&CefRequestContextImpl::SetContentSettingInternal, this,
                     requesting_url, top_level_url, content_type, value));
}

void CefRequestContextImpl::OnRenderFrameCreated(
    const content::GlobalRenderFrameHostId& global_id,
    bool is_main_frame,
    bool is_guest_view) {
  browser_context_->OnRenderFrameCreated(this, global_id, is_main_frame,
                                         is_guest_view);
}

void CefRequestContextImpl::OnRenderFrameDeleted(
    const content::GlobalRenderFrameHostId& global_id,
    bool is_main_frame,
    bool is_guest_view) {
  browser_context_->OnRenderFrameDeleted(this, global_id, is_main_frame,
                                         is_guest_view);
}

// static
CefRefPtr<CefRequestContextImpl>
CefRequestContextImpl::GetOrCreateRequestContext(Config&& config) {
  if (config.browser_context) {
    // CefBrowserContext is only accessed on the UI thread.
    CEF_REQUIRE_UIT();
    DCHECK(!config.is_global);
    DCHECK(!config.other);

    // Retrieve any request context that currently exists for the browser
    // context. If |config.handler| is nullptr, and the returned request context
    // does not have a handler, then we can just return that existing context.
    // Otherwise, we'll need to create a new request context with
    // |config.handler|.
    if (auto other = config.browser_context->GetAnyRequestContext(
            /*prefer_no_handler=*/!config.handler)) {
      if (!config.handler && !other->GetHandler()) {
        // Safe to return the existing context.
        return other;
      }

      // Use the existing request context as a starting point. It may be the
      // global context. This is functionally equivalent to calling
      // `CefRequestContext::CreateContext(other, handler)`.
      config.other = other;
    }
  }

  if (config.is_global ||
      (config.other && config.other->IsGlobal() && !config.handler)) {
    // Return the singleton global context.
    return static_cast<CefRequestContextImpl*>(
        CefAppManager::Get()->GetGlobalRequestContext().get());
  }

  CefRefPtr<CefRequestContextImpl> context =
      new CefRequestContextImpl(std::move(config));

  // Initialize ASAP so that any tasks blocked on initialization will execute.
  if (CEF_CURRENTLY_ON_UIT()) {
    context->Initialize();
  } else {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefRequestContextImpl::Initialize, context));
  }

  return context;
}

CefRequestContextImpl::CefRequestContextImpl(
    CefRequestContextImpl::Config&& config)
    : config_(std::move(config)) {}

void CefRequestContextImpl::Initialize() {
  CEF_REQUIRE_UIT();

  DCHECK(!browser_context_);

  if (config_.other) {
    // Share storage with |config_.other|.
    browser_context_ = config_.other->browser_context();
    CHECK(browser_context_);
  } else if (config_.browser_context) {
    browser_context_ = config_.browser_context;
    config_.browser_context = nullptr;
  }

  if (!browser_context_) {
    if (!config_.is_global) {
      // User-specified settings need to be normalized.
      CefContext::Get()->NormalizeRequestContextSettings(&config_.settings);
    }

    const base::FilePath& cache_path =
        base::FilePath(CefString(&config_.settings.cache_path));
    if (!cache_path.empty()) {
      // Check if a CefBrowserContext is already globally registered for
      // the specified cache path. If so then use it.
      browser_context_ = CefBrowserContext::FromCachePath(cache_path);
    }
  }

  auto initialized_cb =
      base::BindOnce(&CefRequestContextImpl::BrowserContextInitialized, this);

  if (!browser_context_) {
    // Create a new CefBrowserContext instance. If the cache path is non-
    // empty then this new instance will become the globally registered
    // CefBrowserContext for that path. Otherwise, this new instance will
    // be a completely isolated "incognito mode" context.
    browser_context_ = CefAppManager::Get()->CreateNewBrowserContext(
        config_.settings, std::move(initialized_cb));
  } else {
    // Share the same settings as the existing context.
    config_.settings = browser_context_->settings();
    browser_context_->StoreOrTriggerInitCallback(std::move(initialized_cb));
  }

  // We'll disassociate from |browser_context_| on destruction.
  browser_context_->AddCefRequestContext(this);

  if (config_.other) {
    // Clear the reference to |config_.other| after adding the new assocation
    // with |browser_context_| as this may result in |other| being released
    // and we don't want the browser context to be destroyed prematurely.
    // This is the also the reverse order of checks in IsSharingWith().
    config_.other = nullptr;
  }
}

void CefRequestContextImpl::BrowserContextInitialized() {
  if (config_.handler) {
    // Always execute asynchronously so the current call stack can unwind.
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefRequestContextHandler::OnRequestContextInitialized,
                       config_.handler, CefRefPtr<CefRequestContext>(this)));
  }
}

void CefRequestContextImpl::ClearCertificateExceptionsInternal(
    CefRefPtr<CefCompletionCallback> callback,
    CefBrowserContext::Getter browser_context_getter) {
  auto browser_context = browser_context_getter.Run();
  if (!browser_context) {
    return;
  }

  content::SSLHostStateDelegate* ssl_delegate =
      browser_context->AsBrowserContext()->GetSSLHostStateDelegate();
  if (ssl_delegate) {
    ssl_delegate->Clear(base::NullCallback());
  }

  if (callback) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefCompletionCallback::OnComplete, callback));
  }
}

void CefRequestContextImpl::ClearHttpAuthCredentialsInternal(
    CefRefPtr<CefCompletionCallback> callback,
    CefBrowserContext::Getter browser_context_getter) {
  auto browser_context = browser_context_getter.Run();
  if (!browser_context) {
    return;
  }

  browser_context->GetNetworkContext()->ClearHttpAuthCache(
      /*start_time=*/base::Time(), /*end_time=*/base::Time::Max(),
      /*filter=*/nullptr,
      base::BindOnce(&CefCompletionCallback::OnComplete, callback));
}

void CefRequestContextImpl::CloseAllConnectionsInternal(
    CefRefPtr<CefCompletionCallback> callback,
    CefBrowserContext::Getter browser_context_getter) {
  auto browser_context = browser_context_getter.Run();
  if (!browser_context) {
    return;
  }

  browser_context->GetNetworkContext()->CloseAllConnections(
      base::BindOnce(&CefCompletionCallback::OnComplete, callback));
}

void CefRequestContextImpl::ResolveHostInternal(
    const CefString& origin,
    CefRefPtr<CefResolveCallback> callback,
    CefBrowserContext::Getter browser_context_getter) {
  auto browser_context = browser_context_getter.Run();
  if (!browser_context) {
    return;
  }

  // |helper| will be deleted in ResolveHostHelper::OnComplete().
  ResolveHostHelper* helper = new ResolveHostHelper(callback);
  helper->Start(browser_context, origin);
}

void CefRequestContextImpl::SetWebsiteSettingInternal(
    const CefString& requesting_url,
    const CefString& top_level_url,
    cef_content_setting_types_t content_type,
    CefRefPtr<CefValue> value,
    CefBrowserContext::Getter browser_context_getter) {
  auto browser_context = browser_context_getter.Run();
  if (!browser_context) {
    return;
  }

  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
      browser_context->AsProfile());
  if (!settings_map) {
    return;
  }

  // Starts as a NONE value.
  base::Value new_value;
  if (value && value->IsValid()) {
    new_value = static_cast<CefValueImpl*>(value.get())->CopyValue();
  }

  if (requesting_url.empty() && top_level_url.empty()) {
    settings_map->SetWebsiteSettingCustomScope(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        static_cast<ContentSettingsType>(content_type), std::move(new_value));
  } else {
    GURL requesting_gurl(requesting_url.ToString());
    GURL top_level_gurl(top_level_url.ToString());
    if (requesting_gurl.is_valid() || top_level_gurl.is_valid()) {
      settings_map->SetWebsiteSettingDefaultScope(
          requesting_gurl, top_level_gurl,
          static_cast<ContentSettingsType>(content_type), std::move(new_value));
    }
  }
}

void CefRequestContextImpl::SetContentSettingInternal(
    const CefString& requesting_url,
    const CefString& top_level_url,
    cef_content_setting_types_t content_type,
    cef_content_setting_values_t value,
    CefBrowserContext::Getter browser_context_getter) {
  auto browser_context = browser_context_getter.Run();
  if (!browser_context) {
    return;
  }

  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
      browser_context->AsProfile());
  if (!settings_map) {
    return;
  }

  if (requesting_url.empty() && top_level_url.empty()) {
    settings_map->SetDefaultContentSetting(
        static_cast<ContentSettingsType>(content_type),
        static_cast<ContentSetting>(value));
  } else {
    GURL requesting_gurl(requesting_url.ToString());
    GURL top_level_gurl(top_level_url.ToString());
    if (requesting_gurl.is_valid() || top_level_gurl.is_valid()) {
      settings_map->SetContentSettingDefaultScope(
          requesting_gurl, top_level_gurl,
          static_cast<ContentSettingsType>(content_type),
          static_cast<ContentSetting>(value));
    }
  }
}

void CefRequestContextImpl::InitializeCookieManagerInternal(
    CefRefPtr<CefCookieManagerImpl> cookie_manager,
    CefRefPtr<CefCompletionCallback> callback) {
  GetBrowserContext(content::GetUIThreadTaskRunner({}),
                    base::BindOnce(
                        [](CefRefPtr<CefCookieManagerImpl> cookie_manager,
                           CefRefPtr<CefCompletionCallback> callback,
                           CefBrowserContext::Getter browser_context_getter) {
                          cookie_manager->Initialize(browser_context_getter,
                                                     callback);
                        },
                        cookie_manager, callback));
}

void CefRequestContextImpl::InitializeMediaRouterInternal(
    CefRefPtr<CefMediaRouterImpl> media_router,
    CefRefPtr<CefCompletionCallback> callback) {
  GetBrowserContext(content::GetUIThreadTaskRunner({}),
                    base::BindOnce(
                        [](CefRefPtr<CefMediaRouterImpl> media_router,
                           CefRefPtr<CefCompletionCallback> callback,
                           CefBrowserContext::Getter browser_context_getter) {
                          media_router->Initialize(browser_context_getter,
                                                   callback);
                        },
                        media_router, callback));
}

CefBrowserContext* CefRequestContextImpl::browser_context() const {
  return browser_context_;
}
