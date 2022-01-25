// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/task_runner_impl.h"
#include "libcef/common/values_impl.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/common/child_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/host_resolver.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/network_context.mojom.h"

using content::BrowserThread;

namespace {

base::AtomicSequenceNumber g_next_id;

const char* GetTypeString(base::Value::Type type) {
  switch (type) {
    case base::Value::Type::NONE:
      return "NULL";
    case base::Value::Type::BOOLEAN:
      return "BOOLEAN";
    case base::Value::Type::INTEGER:
      return "INTEGER";
    case base::Value::Type::DOUBLE:
      return "DOUBLE";
    case base::Value::Type::STRING:
      return "STRING";
    case base::Value::Type::BINARY:
      return "BINARY";
    case base::Value::Type::DICTIONARY:
      return "DICTIONARY";
    case base::Value::Type::LIST:
      return "LIST";
  }

  NOTREACHED();
  return "UNKNOWN";
}

class ResolveHostHelper : public network::ResolveHostClientBase {
 public:
  explicit ResolveHostHelper(CefRefPtr<CefResolveCallback> callback)
      : callback_(callback), receiver_(this) {}

  ResolveHostHelper(const ResolveHostHelper&) = delete;
  ResolveHostHelper& operator=(const ResolveHostHelper&) = delete;

  void Start(CefBrowserContext* browser_context, const CefString& origin) {
    CEF_REQUIRE_UIT();

    browser_context->GetNetworkContext()->CreateHostResolver(
        absl::nullopt, host_resolver_.BindNewPipeAndPassReceiver());

    host_resolver_.set_disconnect_handler(base::BindOnce(
        &ResolveHostHelper::OnComplete, base::Unretained(this), net::ERR_FAILED,
        net::ResolveErrorInfo(net::ERR_FAILED), absl::nullopt));

    host_resolver_->ResolveHost(
        net::HostPortPair::FromURL(GURL(origin.ToString())),
        net::NetworkIsolationKey::CreateTransient(), nullptr,
        receiver_.BindNewPipeAndPassRemote());
  }

 private:
  void OnComplete(
      int32_t result,
      const ::net::ResolveErrorInfo& resolve_error_info,
      const absl::optional<net::AddressList>& resolved_addresses) override {
    CEF_REQUIRE_UIT();

    host_resolver_.reset();
    receiver_.reset();

    std::vector<CefString> resolved_ips;

    if (result == net::OK) {
      DCHECK(resolved_addresses && !resolved_addresses->empty());
      for (const auto& value : resolved_addresses.value()) {
        resolved_ips.push_back(value.ToStringWithoutPort());
      }
    }

    callback_->OnResolveCompleted(static_cast<cef_errorcode_t>(result),
                                  resolved_ips);
    delete this;
  }

  CefRefPtr<CefResolveCallback> callback_;

  mojo::Remote<network::mojom::HostResolver> host_resolver_;
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_;
};

}  // namespace

// CefBrowserContext

// static
CefRefPtr<CefRequestContext> CefRequestContext::GetGlobalContext() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return nullptr;
  }

  CefRequestContextImpl::Config config;
  config.is_global = true;
  return CefRequestContextImpl::GetOrCreateRequestContext(config);
}

// static
CefRefPtr<CefRequestContext> CefRequestContext::CreateContext(
    const CefRequestContextSettings& settings,
    CefRefPtr<CefRequestContextHandler> handler) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return nullptr;
  }

  CefRequestContextImpl::Config config;
  config.settings = settings;
  config.handler = handler;
  config.unique_id = g_next_id.GetNext();
  return CefRequestContextImpl::GetOrCreateRequestContext(config);
}

// static
CefRefPtr<CefRequestContext> CefRequestContext::CreateContext(
    CefRefPtr<CefRequestContext> other,
    CefRefPtr<CefRequestContextHandler> handler) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return nullptr;
  }

  if (!other.get())
    return nullptr;

  CefRequestContextImpl::Config config;
  config.other = static_cast<CefRequestContextImpl*>(other.get());
  config.handler = handler;
  config.unique_id = g_next_id.GetNext();
  return CefRequestContextImpl::GetOrCreateRequestContext(config);
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
  CefRefPtr<CefRequestContextImpl> impl = new CefRequestContextImpl(config);
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
  return CefRequestContextImpl::GetOrCreateRequestContext(config);
}

bool CefRequestContextImpl::VerifyBrowserContext() const {
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  if (!browser_context() || !browser_context()->IsInitialized()) {
    NOTREACHED() << "Uninitialized context";
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

  EnsureBrowserContext();
  browser_context()->StoreOrTriggerInitCallback(std::move(callback));
}

void CefRequestContextImpl::GetBrowserContext(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    BrowserContextCallback callback) {
  if (!task_runner.get())
    task_runner = CefTaskRunnerImpl::GetCurrentTaskRunner();

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
  if (!other_impl)
    return false;

  // Compare whether both are the global context.
  if (config_.is_global && other_impl->config_.is_global)
    return true;

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
  if (!other_impl)
    return false;

  if (IsSame(other))
    return true;

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
        if (browser_context)
          browser_context->ClearSchemeHandlerFactories();
      }));

  return true;
}

bool CefRequestContextImpl::HasPreference(const CefString& name) {
  if (!VerifyBrowserContext())
    return false;

  PrefService* pref_service = browser_context()->AsProfile()->GetPrefs();
  return (pref_service->FindPreference(name) != nullptr);
}

CefRefPtr<CefValue> CefRequestContextImpl::GetPreference(
    const CefString& name) {
  if (!VerifyBrowserContext())
    return nullptr;

  PrefService* pref_service = browser_context()->AsProfile()->GetPrefs();
  const PrefService::Preference* pref = pref_service->FindPreference(name);
  if (!pref)
    return nullptr;
  return new CefValueImpl(pref->GetValue()->CreateDeepCopy().release());
}

CefRefPtr<CefDictionaryValue> CefRequestContextImpl::GetAllPreferences(
    bool include_defaults) {
  if (!VerifyBrowserContext())
    return nullptr;

  PrefService* pref_service = browser_context()->AsProfile()->GetPrefs();

  base::Value values = pref_service->GetPreferenceValues(
      include_defaults ? PrefService::INCLUDE_DEFAULTS
                       : PrefService::EXCLUDE_DEFAULTS);

  // CefDictionaryValueImpl takes ownership of |values|.
  return new CefDictionaryValueImpl(
      base::DictionaryValue::From(
          base::Value::ToUniquePtrValue(std::move(values)))
          .release(),
      true, false);
}

bool CefRequestContextImpl::CanSetPreference(const CefString& name) {
  if (!VerifyBrowserContext())
    return false;

  PrefService* pref_service = browser_context()->AsProfile()->GetPrefs();
  const PrefService::Preference* pref = pref_service->FindPreference(name);
  return (pref && pref->IsUserModifiable());
}

bool CefRequestContextImpl::SetPreference(const CefString& name,
                                          CefRefPtr<CefValue> value,
                                          CefString& error) {
  if (!VerifyBrowserContext())
    return false;

  PrefService* pref_service = browser_context()->AsProfile()->GetPrefs();

  // The below validation logic should match PrefService::SetUserPrefValue.

  const PrefService::Preference* pref = pref_service->FindPreference(name);
  if (!pref) {
    error = "Trying to modify an unregistered preference";
    return false;
  }

  if (!pref->IsUserModifiable()) {
    error = "Trying to modify a preference that is not user modifiable";
    return false;
  }

  if (!value.get()) {
    // Reset the preference to its default value.
    pref_service->ClearPref(name);
    return true;
  }

  if (!value->IsValid()) {
    error = "A valid value is required";
    return false;
  }

  CefValueImpl* impl = static_cast<CefValueImpl*>(value.get());

  CefValueImpl::ScopedLockedValue scoped_locked_value(impl);
  base::Value* impl_value = impl->GetValueUnsafe();

  if (pref->GetType() != impl_value->type()) {
    error = base::StringPrintf(
        "Trying to set a preference of type %s to value of type %s",
        GetTypeString(pref->GetType()), GetTypeString(impl_value->type()));
    return false;
  }

  // PrefService will make a DeepCopy of |impl_value|.
  pref_service->Set(name, *impl_value);
  return true;
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

  if (!VerifyBrowserContext())
    return false;

  return browser_context()->GetExtensions(extension_ids);
}

CefRefPtr<CefExtension> CefRequestContextImpl::GetExtension(
    const CefString& extension_id) {
  if (!VerifyBrowserContext())
    return nullptr;

  return browser_context()->GetExtension(extension_id);
}

CefRefPtr<CefMediaRouter> CefRequestContextImpl::GetMediaRouter(
    CefRefPtr<CefCompletionCallback> callback) {
  CefRefPtr<CefMediaRouterImpl> media_router = new CefMediaRouterImpl();
  InitializeMediaRouterInternal(media_router, callback);
  return media_router.get();
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
CefRequestContextImpl::GetOrCreateRequestContext(const Config& config) {
  if (config.is_global ||
      (config.other && config.other->IsGlobal() && !config.handler)) {
    // Return the singleton global context.
    return static_cast<CefRequestContextImpl*>(
        CefAppManager::Get()->GetGlobalRequestContext().get());
  }

  // The new context will be initialized later by EnsureBrowserContext().
  CefRefPtr<CefRequestContextImpl> context = new CefRequestContextImpl(config);

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
    const CefRequestContextImpl::Config& config)
    : config_(config) {}

void CefRequestContextImpl::Initialize() {
  CEF_REQUIRE_UIT();

  DCHECK(!browser_context_);

  if (config_.other) {
    // Share storage with |config_.other|.
    browser_context_ = config_.other->browser_context();
    CHECK(browser_context_);
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
    // Clear the reference to |config_.other| after setting
    // |request_context_getter_|. This is the reverse order of checks in
    // IsSharedWith().
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

void CefRequestContextImpl::EnsureBrowserContext() {
  CEF_REQUIRE_UIT();
  if (!browser_context())
    Initialize();
  DCHECK(browser_context());
}

void CefRequestContextImpl::ClearCertificateExceptionsInternal(
    CefRefPtr<CefCompletionCallback> callback,
    CefBrowserContext::Getter browser_context_getter) {
  auto browser_context = browser_context_getter.Run();
  if (!browser_context)
    return;

  content::SSLHostStateDelegate* ssl_delegate =
      browser_context->AsBrowserContext()->GetSSLHostStateDelegate();
  if (ssl_delegate)
    ssl_delegate->Clear(base::NullCallback());

  if (callback) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefCompletionCallback::OnComplete, callback));
  }
}

void CefRequestContextImpl::ClearHttpAuthCredentialsInternal(
    CefRefPtr<CefCompletionCallback> callback,
    CefBrowserContext::Getter browser_context_getter) {
  auto browser_context = browser_context_getter.Run();
  if (!browser_context)
    return;

  browser_context->GetNetworkContext()->ClearHttpAuthCache(
      /*start_time=*/base::Time(), /*end_time=*/base::Time::Max(),
      base::BindOnce(&CefCompletionCallback::OnComplete, callback));
}

void CefRequestContextImpl::CloseAllConnectionsInternal(
    CefRefPtr<CefCompletionCallback> callback,
    CefBrowserContext::Getter browser_context_getter) {
  auto browser_context = browser_context_getter.Run();
  if (!browser_context)
    return;

  browser_context->GetNetworkContext()->CloseAllConnections(
      base::BindOnce(&CefCompletionCallback::OnComplete, callback));
}

void CefRequestContextImpl::ResolveHostInternal(
    const CefString& origin,
    CefRefPtr<CefResolveCallback> callback,
    CefBrowserContext::Getter browser_context_getter) {
  auto browser_context = browser_context_getter.Run();
  if (!browser_context)
    return;

  // |helper| will be deleted in ResolveHostHelper::OnComplete().
  ResolveHostHelper* helper = new ResolveHostHelper(callback);
  helper->Start(browser_context, origin);
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
