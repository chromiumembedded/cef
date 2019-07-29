// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/net_service/cookie_manager_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/task_runner_impl.h"
#include "libcef/common/values_impl.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/cpp/resolve_host_client_base.h"

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
    case base::Value::Type::DEAD:
      return "DEAD";
  }

  NOTREACHED();
  return "UNKNOWN";
}

// Helper for HostResolver::Resolve.
struct ResolveHostHelperOld {
  explicit ResolveHostHelperOld(CefRefPtr<CefResolveCallback> callback)
      : callback_(callback) {}

  void OnResolveCompleted(int result) {
    std::vector<CefString> resolved_ips;
    base::Optional<net::AddressList> maybe_address_list =
        request_->GetAddressResults();
    if (maybe_address_list) {
      net::AddressList::const_iterator iter = maybe_address_list->begin();
      for (; iter != maybe_address_list->end(); ++iter)
        resolved_ips.push_back(iter->ToStringWithoutPort());
    }
    CEF_POST_TASK(
        CEF_UIT,
        base::Bind(&CefResolveCallback::OnResolveCompleted, callback_,
                   static_cast<cef_errorcode_t>(result), resolved_ips));

    delete this;
  }

  CefRefPtr<CefResolveCallback> callback_;
  std::unique_ptr<net::HostResolver::ResolveHostRequest> request_;
};

class ResolveHostHelper : public network::ResolveHostClientBase {
 public:
  explicit ResolveHostHelper(CefRefPtr<CefResolveCallback> callback)
      : callback_(callback), binding_(this) {}

  void Start(CefBrowserContext* browser_context, const CefString& origin) {
    CEF_REQUIRE_UIT();

    network::mojom::HostResolverPtrInfo host_resolver_info;
    browser_context->GetNetworkContext()->CreateHostResolver(
        base::nullopt, mojo::MakeRequest(&host_resolver_info));

    network::mojom::ResolveHostClientPtr client_ptr;
    binding_.Bind(mojo::MakeRequest(&client_ptr));
    binding_.set_connection_error_handler(
        base::BindOnce(&ResolveHostHelper::OnComplete, base::Unretained(this),
                       net::ERR_FAILED, base::nullopt));
    host_resolver_ =
        network::mojom::HostResolverPtr(std::move(host_resolver_info));
    host_resolver_->ResolveHost(
        net::HostPortPair::FromURL(GURL(origin.ToString())), nullptr,
        std::move(client_ptr));
  }

 private:
  void OnComplete(
      int result,
      const base::Optional<net::AddressList>& resolved_addresses) override {
    CEF_REQUIRE_UIT();

    host_resolver_.reset();
    binding_.Close();

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

  network::mojom::HostResolverPtr host_resolver_;
  mojo::Binding<network::mojom::ResolveHostClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(ResolveHostHelper);
};

}  // namespace

// CefBrowserContext

// static
CefRefPtr<CefRequestContext> CefRequestContext::GetGlobalContext() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return NULL;
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
    return NULL;
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
    return NULL;
  }

  if (!other.get())
    return NULL;

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

CefBrowserContext* CefRequestContextImpl::GetBrowserContext() {
  EnsureBrowserContext();
  return browser_context();
}

void CefRequestContextImpl::GetBrowserContext(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const BrowserContextCallback& callback) {
  if (!task_runner.get())
    task_runner = CefTaskRunnerImpl::GetCurrentTaskRunner();
  GetBrowserContextOnUIThread(task_runner, callback);
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
  cookie_manager->Initialize(this, callback);
  return cookie_manager.get();
}

bool CefRequestContextImpl::RegisterSchemeHandlerFactory(
    const CefString& scheme_name,
    const CefString& domain_name,
    CefRefPtr<CefSchemeHandlerFactory> factory) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(
                      base::IgnoreResult(
                          &CefRequestContextImpl::RegisterSchemeHandlerFactory),
                      this, scheme_name, domain_name, factory));
    return true;
  }

  // Make sure the browser context exists.
  EnsureBrowserContext();

  browser_context()->RegisterSchemeHandlerFactory(scheme_name, domain_name,
                                                  factory);
  return true;
}

bool CefRequestContextImpl::ClearSchemeHandlerFactories() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(base::IgnoreResult(
                           &CefRequestContextImpl::ClearSchemeHandlerFactories),
                       this));
    return true;
  }

  // Make sure the browser context exists.
  EnsureBrowserContext();

  browser_context()->ClearSchemeHandlerFactories();
  return true;
}

void CefRequestContextImpl::PurgePluginListCache(bool reload_pages) {
  GetBrowserContext(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
      base::Bind(&CefRequestContextImpl::PurgePluginListCacheInternal, this,
                 reload_pages));
}

bool CefRequestContextImpl::HasPreference(const CefString& name) {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  // Make sure the browser context exists.
  EnsureBrowserContext();

  PrefService* pref_service = browser_context()->GetPrefs();
  return (pref_service->FindPreference(name) != NULL);
}

CefRefPtr<CefValue> CefRequestContextImpl::GetPreference(
    const CefString& name) {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return NULL;
  }

  // Make sure the browser context exists.
  EnsureBrowserContext();

  PrefService* pref_service = browser_context()->GetPrefs();
  const PrefService::Preference* pref = pref_service->FindPreference(name);
  if (!pref)
    return NULL;
  return new CefValueImpl(pref->GetValue()->DeepCopy());
}

CefRefPtr<CefDictionaryValue> CefRequestContextImpl::GetAllPreferences(
    bool include_defaults) {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return NULL;
  }

  // Make sure the browser context exists.
  EnsureBrowserContext();

  PrefService* pref_service = browser_context()->GetPrefs();

  std::unique_ptr<base::DictionaryValue> values =
      pref_service->GetPreferenceValues(include_defaults
                                            ? PrefService::INCLUDE_DEFAULTS
                                            : PrefService::EXCLUDE_DEFAULTS);

  // CefDictionaryValueImpl takes ownership of |values|.
  return new CefDictionaryValueImpl(values.release(), true, false);
}

bool CefRequestContextImpl::CanSetPreference(const CefString& name) {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  // Make sure the browser context exists.
  EnsureBrowserContext();

  PrefService* pref_service = browser_context()->GetPrefs();
  const PrefService::Preference* pref = pref_service->FindPreference(name);
  return (pref && pref->IsUserModifiable());
}

bool CefRequestContextImpl::SetPreference(const CefString& name,
                                          CefRefPtr<CefValue> value,
                                          CefString& error) {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  // Make sure the browser context exists.
  EnsureBrowserContext();

  PrefService* pref_service = browser_context()->GetPrefs();

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
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
      base::Bind(&CefRequestContextImpl::ClearCertificateExceptionsInternal,
                 this, callback));
}

void CefRequestContextImpl::ClearHttpAuthCredentials(
    CefRefPtr<CefCompletionCallback> callback) {
  GetBrowserContext(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
      base::Bind(&CefRequestContextImpl::ClearHttpAuthCredentialsInternal, this,
                 callback));
}

void CefRequestContextImpl::CloseAllConnections(
    CefRefPtr<CefCompletionCallback> callback) {
  GetBrowserContext(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
      base::Bind(&CefRequestContextImpl::CloseAllConnectionsInternal, this,
                 callback));
}

void CefRequestContextImpl::ResolveHost(
    const CefString& origin,
    CefRefPtr<CefResolveCallback> callback) {
  GetBrowserContext(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
      base::Bind(&CefRequestContextImpl::ResolveHostInternal, this, origin,
                 callback));
}

void CefRequestContextImpl::LoadExtension(
    const CefString& root_directory,
    CefRefPtr<CefDictionaryValue> manifest,
    CefRefPtr<CefExtensionHandler> handler) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefRequestContextImpl::LoadExtension, this,
                                 root_directory, manifest, handler));
    return;
  }

  if (!extensions::ExtensionsEnabled()) {
    if (handler)
      handler->OnExtensionLoadFailed(ERR_ABORTED);
    return;
  }

  if (manifest && manifest->GetSize() > 0) {
    CefDictionaryValueImpl* value_impl =
        static_cast<CefDictionaryValueImpl*>(manifest.get());
    GetBrowserContext()->extension_system()->LoadExtension(
        base::WrapUnique(value_impl->CopyValue()), root_directory,
        false /* builtin */, this, handler);
  } else {
    GetBrowserContext()->extension_system()->LoadExtension(
        root_directory, false /* builtin */, this, handler);
  }
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

  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  if (!extensions::ExtensionsEnabled())
    return false;

  extensions::CefExtensionSystem::ExtensionMap extension_map =
      GetBrowserContext()->extension_system()->GetExtensions();
  extensions::CefExtensionSystem::ExtensionMap::const_iterator it =
      extension_map.begin();
  for (; it != extension_map.end(); ++it)
    extension_ids.push_back(it->second->GetIdentifier());

  return true;
}

CefRefPtr<CefExtension> CefRequestContextImpl::GetExtension(
    const CefString& extension_id) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return nullptr;
  }

  if (!extensions::ExtensionsEnabled())
    return nullptr;

  return GetBrowserContext()->extension_system()->GetExtension(extension_id);
}

void CefRequestContextImpl::OnRenderFrameCreated(int render_process_id,
                                                 int render_frame_id,
                                                 int frame_tree_node_id,
                                                 bool is_main_frame,
                                                 bool is_guest_view) {
  browser_context_->OnRenderFrameCreated(this, render_process_id,
                                         render_frame_id, frame_tree_node_id,
                                         is_main_frame, is_guest_view);
}

void CefRequestContextImpl::OnRenderFrameDeleted(int render_process_id,
                                                 int render_frame_id,
                                                 int frame_tree_node_id,
                                                 bool is_main_frame,
                                                 bool is_guest_view) {
  browser_context_->OnRenderFrameDeleted(this, render_process_id,
                                         render_frame_id, frame_tree_node_id,
                                         is_main_frame, is_guest_view);
}

// static
CefRefPtr<CefRequestContextImpl>
CefRequestContextImpl::GetOrCreateRequestContext(const Config& config) {
  if (config.is_global ||
      (config.other && config.other->IsGlobal() && !config.handler)) {
    // Return the singleton global context.
    return CefContentBrowserClient::Get()->request_context();
  }

  // The new context will be initialized later by EnsureBrowserContext().
  return new CefRequestContextImpl(config);
}

CefRequestContextImpl::CefRequestContextImpl(
    const CefRequestContextImpl::Config& config)
    : config_(config) {}

void CefRequestContextImpl::Initialize() {
  CEF_REQUIRE_UIT();

  DCHECK(!browser_context_);

  if (config_.other) {
    // Share storage with |config_.other|.
    browser_context_ =
        CefBrowserContext::GetForContext(config_.other->GetBrowserContext());
  }

  if (!browser_context_) {
    const base::FilePath& cache_path =
        base::FilePath(CefString(&config_.settings.cache_path));
    if (!cache_path.empty()) {
      // Check if a CefBrowserContext is already globally registered for
      // the specified cache path. If so then use it.
      browser_context_ = CefBrowserContext::GetForCachePath(cache_path);
    }
  }

  if (!browser_context_) {
    // Create a new CefBrowserContext instance. If the cache path is non-
    // empty then this new instance will become the globally registered
    // CefBrowserContext for that path. Otherwise, this new instance will
    // be a completely isolated "incongento mode" context.
    browser_context_ = new CefBrowserContext(config_.settings);
    browser_context_->Initialize();
  }

  // We'll disassociate from |browser_context_| on destruction.
  browser_context_->AddCefRequestContext(this);

  // Force our settings to match |browser_context_|.
  config_.settings = browser_context_->GetSettings();

  if (config_.other) {
    // Clear the reference to |config_.other| after setting
    // |request_context_getter_|. This is the reverse order of checks in
    // IsSharedWith().
    config_.other = NULL;
  }

  if (config_.handler)
    config_.handler->OnRequestContextInitialized(this);
}

void CefRequestContextImpl::EnsureBrowserContext() {
  CEF_REQUIRE_UIT();
  if (!browser_context())
    Initialize();
  DCHECK(browser_context());
}

void CefRequestContextImpl::GetBrowserContextOnUIThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const BrowserContextCallback& callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::Bind(&CefRequestContextImpl::GetBrowserContextOnUIThread,
                            this, task_runner, callback));
    return;
  }

  // Make sure the browser context exists.
  EnsureBrowserContext();

  if (task_runner->BelongsToCurrentThread()) {
    // Execute the callback immediately.
    callback.Run(browser_context());
  } else {
    // Execute the callback on the target thread.
    task_runner->PostTask(FROM_HERE, base::Bind(callback, browser_context()));
  }
}

void CefRequestContextImpl::PurgePluginListCacheInternal(
    bool reload_pages,
    CefBrowserContext* browser_context) {
  CEF_REQUIRE_UIT();
  browser_context->OnPurgePluginListCache();
  content::PluginService::GetInstance()->PurgePluginListCache(browser_context,
                                                              false);
}

void CefRequestContextImpl::ClearCertificateExceptionsInternal(
    CefRefPtr<CefCompletionCallback> callback,
    CefBrowserContext* browser_context) {
  CEF_REQUIRE_UIT();

  content::SSLHostStateDelegate* ssl_delegate =
      browser_context->GetSSLHostStateDelegate();
  if (ssl_delegate)
    ssl_delegate->Clear(base::Callback<bool(const std::string&)>());

  if (callback) {
    CEF_POST_TASK(CEF_UIT,
                  base::Bind(&CefCompletionCallback::OnComplete, callback));
  }
}

void CefRequestContextImpl::ClearHttpAuthCredentialsInternal(
    CefRefPtr<CefCompletionCallback> callback,
    CefBrowserContext* browser_context) {
  CEF_REQUIRE_UIT();

  browser_context->GetNetworkContext()->ClearHttpAuthCache(
      base::Time(), base::Bind(&CefCompletionCallback::OnComplete, callback));
}

void CefRequestContextImpl::CloseAllConnectionsInternal(
    CefRefPtr<CefCompletionCallback> callback,
    CefBrowserContext* browser_context) {
  CEF_REQUIRE_UIT();

  browser_context->GetNetworkContext()->CloseAllConnections(
      base::Bind(&CefCompletionCallback::OnComplete, callback));
}

void CefRequestContextImpl::ResolveHostInternal(
    const CefString& origin,
    CefRefPtr<CefResolveCallback> callback,
    CefBrowserContext* browser_context) {
  CEF_REQUIRE_UIT();

  // |helper| will be deleted in ResolveHostHelper::OnComplete().
  ResolveHostHelper* helper = new ResolveHostHelper(callback);
  helper->Start(browser_context, origin);
}

CefBrowserContext* CefRequestContextImpl::browser_context() const {
  return browser_context_;
}
