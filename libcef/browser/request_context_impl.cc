// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/browser_context_proxy.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/cookie_manager_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/values_impl.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction_factory.h"

using content::BrowserThread;

namespace {

base::StaticAtomicSequenceNumber g_next_id;

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

// Helper for HostResolver::Resolve.
struct ResolveHostHelper {
  explicit ResolveHostHelper(CefRefPtr<CefResolveCallback> callback)
      : callback_(callback) {
  }

  void OnResolveCompleted(int result) {
    std::vector<CefString> resolved_ips;

    net::AddressList::const_iterator iter = address_list_.begin();
    for (; iter != address_list_.end(); ++iter)
      resolved_ips.push_back(iter->ToStringWithoutPort());

    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefResolveCallback::OnResolveCompleted, callback_,
                   static_cast<cef_errorcode_t>(result), resolved_ips));

    delete this;
  }

  CefRefPtr<CefResolveCallback> callback_;
  net::AddressList address_list_;
  std::unique_ptr<net::HostResolver::Request> request_;
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
  return new CefRequestContextImpl(config);
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
  return new CefRequestContextImpl(config);
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
  return new CefRequestContextImpl(config);
}


// CefRequestContextImpl

CefRequestContextImpl::~CefRequestContextImpl() {
  // Delete the proxy first because it also references |browser_context_impl_|.
  if (browser_context_proxy_)
    browser_context_proxy_.reset(nullptr);

  if (browser_context_impl_) {
    // May result in |browser_context_impl_| being deleted if it's not the
    // global context and no other CefRequestContextImpl are referencing it.
    browser_context_impl_->RemoveRequestContext();
  }
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
  CefRequestContextImpl::Config config;
  config.is_global = true;
  return new CefRequestContextImpl(config);
}

// static
CefRefPtr<CefRequestContextImpl> CefRequestContextImpl::CreateForBrowserContext(
      CefBrowserContext* browser_context) {
  DCHECK(browser_context);
  CefRequestContextImpl::Config config;
  config.handler = browser_context->GetHandler();
  CefRefPtr<CefRequestContextImpl> impl = new CefRequestContextImpl(config);
  // Force immediate initialization because it's not safe to keep a raw pointer
  // to |browser_context|.
  impl->Initialize(browser_context);
  return impl;
}

CefBrowserContext* CefRequestContextImpl::GetBrowserContext() {
  EnsureBrowserContext();
  return browser_context();
}

void CefRequestContextImpl::GetBrowserContext(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const BrowserContextCallback& callback) {
  if (!task_runner.get())
    task_runner = base::MessageLoop::current()->task_runner();
  GetBrowserContextOnUIThread(task_runner, callback);
}

void CefRequestContextImpl::GetRequestContextImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const RequestContextCallback& callback) {
  if (!task_runner.get())
    task_runner = base::MessageLoop::current()->task_runner();
  if (request_context_impl_) {
    // The browser context already exists.
    DCHECK(browser_context());
    GetRequestContextImplOnIOThread(task_runner, callback, browser_context());
  } else {
    // Need to initialize the browser context first.
    GetBrowserContextOnUIThread(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
        base::Bind(&CefRequestContextImpl::GetRequestContextImplOnIOThread,
                   this, task_runner, callback));
  }
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
    if (browser_context()->is_proxy() &&
        other_impl->browser_context()->is_proxy()) {
      CefBrowserContextProxy* proxy =
          static_cast<CefBrowserContextProxy*>(browser_context());
      CefBrowserContextProxy* other_proxy =
          static_cast<CefBrowserContextProxy*>(other_impl->browser_context());
      return (proxy->parent() == other_proxy->parent() &&
              proxy->GetHandler() == other_proxy->GetHandler());
    }
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

  if (request_context_impl_ && other_impl->request_context_impl_) {
    // Both objects are initialized. Compare the request context objects.
    return (request_context_impl_ == other_impl->request_context_impl_);
  }

  // This or the other object is not initialized. Compare the cache path values.
  // If both are non-empty and the same then they'll share the same storage.
  if (config_.settings.cache_path.length > 0 &&
      other_impl->config_.settings.cache_path.length > 0) {
    return (base::FilePath(CefString(&config_.settings.cache_path)) ==
            base::FilePath(
                CefString(&other_impl->config_.settings.cache_path)));
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

CefRefPtr<CefCookieManager> CefRequestContextImpl::GetDefaultCookieManager(
    CefRefPtr<CefCompletionCallback> callback) {
  CefRefPtr<CefCookieManagerImpl> cookie_manager = new CefCookieManagerImpl();
  cookie_manager->Initialize(this, CefString(), false, callback);
  return cookie_manager.get();
}

bool CefRequestContextImpl::RegisterSchemeHandlerFactory(
    const CefString& scheme_name,
    const CefString& domain_name,
    CefRefPtr<CefSchemeHandlerFactory> factory) {
  GetRequestContextImpl(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
      base::Bind(&CefRequestContextImpl::RegisterSchemeHandlerFactoryInternal,
                 this, scheme_name, domain_name, factory));
  return true;
}

bool CefRequestContextImpl::ClearSchemeHandlerFactories() {
  GetRequestContextImpl(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
      base::Bind(&CefRequestContextImpl::ClearSchemeHandlerFactoriesInternal,
                 this));
  return true;
}

void CefRequestContextImpl::PurgePluginListCache(bool reload_pages) {
  GetBrowserContext(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
      base::Bind(&CefRequestContextImpl::PurgePluginListCacheInternal,
                 this, reload_pages));
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

  std::unique_ptr<base::DictionaryValue> values;
  if (include_defaults)
    values = pref_service->GetPreferenceValues();
  else
    values = pref_service->GetPreferenceValuesOmitDefaults();

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

  if (pref->GetType() != impl_value->GetType()) {
    error = base::StringPrintf(
        "Trying to set a preference of type %s to value of type %s",
        GetTypeString(pref->GetType()), GetTypeString(impl_value->GetType()));
    return false;
  }

  // PrefService will make a DeepCopy of |impl_value|.
  pref_service->Set(name, *impl_value);
  return true;
}

void CefRequestContextImpl::ClearCertificateExceptions(
    CefRefPtr<CefCompletionCallback> callback) {
  GetBrowserContext(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
      base::Bind(&CefRequestContextImpl::ClearCertificateExceptionsInternal,
                 this, callback));
}

void CefRequestContextImpl::CloseAllConnections(
    CefRefPtr<CefCompletionCallback> callback) {
  GetRequestContextImpl(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
      base::Bind(&CefRequestContextImpl::CloseAllConnectionsInternal, this,
                 callback));
}

void CefRequestContextImpl::ResolveHost(
    const CefString& origin,
    CefRefPtr<CefResolveCallback> callback) {
  GetRequestContextImpl(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
      base::Bind(&CefRequestContextImpl::ResolveHostInternal, this, origin,
                 callback));
}

cef_errorcode_t CefRequestContextImpl::ResolveHostCached(
    const CefString& origin,
    std::vector<CefString>& resolved_ips) {
  resolved_ips.clear();

  if (!CEF_CURRENTLY_ON_IOT()) {
    NOTREACHED() << "called on invalid thread";
    return ERR_FAILED;
  }

  if (!request_context_impl_)
    return ERR_FAILED;

  int retval = ERR_FAILED;

  net::HostResolver* host_resolver = request_context_impl_->GetHostResolver();
  if (host_resolver) {
    net::HostResolver::RequestInfo request_info(
        net::HostPortPair::FromURL(GURL(origin.ToString())));
    net::AddressList address_list;
    retval = host_resolver->ResolveFromCache(request_info, &address_list,
                                             net::NetLogWithSource());
    if (retval == net::OK) {
      net::AddressList::const_iterator iter = address_list.begin();
      for (; iter != address_list.end(); ++iter)
        resolved_ips.push_back(iter->ToString());
    }
  }

  return static_cast<cef_errorcode_t>(retval);
}

CefRequestContextImpl::CefRequestContextImpl(
    const CefRequestContextImpl::Config& config)
    : config_(config) {
}

void CefRequestContextImpl::Initialize() {
  CefBrowserContext* other_browser_context = nullptr;
  if (config_.is_global)
    other_browser_context = CefContentBrowserClient::Get()->browser_context();
  else if (config_.other.get())
    other_browser_context = config_.other->GetBrowserContext();

  Initialize(other_browser_context);

  if (config_.other.get()) {
    // Clear the reference to |other_| after setting |request_context_impl_|.
    // This is the reverse order of checks in IsSharedWith().
    config_.other = NULL;
  }
}

void CefRequestContextImpl::Initialize(
    CefBrowserContext* other_browser_context) {
  CEF_REQUIRE_UIT();

  DCHECK(!browser_context_impl_);
  DCHECK(!request_context_impl_);

  if (other_browser_context) {
    // Share storage with |other_browser_context|.
    browser_context_impl_ = CefBrowserContextImpl::GetForContext(
        other_browser_context);
  }

  if (!browser_context_impl_) {
    const base::FilePath& cache_path =
        base::FilePath(CefString(&config_.settings.cache_path));
    if (!cache_path.empty()) {
      // Check if a CefBrowserContextImpl is already globally registered for
      // the specified cache path. If so then use it.
      browser_context_impl_ =
          CefBrowserContextImpl::GetForCachePath(cache_path);
    }
  }

  if (!browser_context_impl_) {
    // Create a new CefBrowserContextImpl instance. If the cache path is non-
    // empty then this new instance will become the globally registered
    // CefBrowserContextImpl for that path. Otherwise, this new instance will
    // be a completely isolated "incongento mode" context.
    browser_context_impl_ = new CefBrowserContextImpl(config_.settings);
    browser_context_impl_->Initialize();
  }

  // We'll disassociate from |browser_context_impl_| on destruction.
  browser_context_impl_->AddRequestContext();

  // Force our settings to match |browser_context_impl_|.
  config_.settings = browser_context_impl_->GetSettings();

  if (config_.handler.get()) {
    // Use a proxy that will execute handler callbacks where appropriate and
    // otherwise forward all requests to |browser_context_impl_|.
    browser_context_proxy_.reset(
        new CefBrowserContextProxy(config_.handler, browser_context_impl_));
    browser_context_proxy_->Initialize();
    DCHECK(!config_.is_global);
  } else {
    config_.is_global = (browser_context_impl_ ==
                         CefContentBrowserClient::Get()->browser_context());
  }

  request_context_impl_ = browser_context_impl_->request_context().get();
  DCHECK(request_context_impl_);

  if (config_.handler.get()) {
    // Keep the handler alive until the associated request context is
    // destroyed.
    request_context_impl_->AddHandler(config_.handler);
  }
}

void CefRequestContextImpl::EnsureBrowserContext() {
  if (!browser_context())
    Initialize();
  DCHECK(browser_context());
  DCHECK(request_context_impl_);
}

void CefRequestContextImpl::GetBrowserContextOnUIThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const BrowserContextCallback& callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefRequestContextImpl::GetBrowserContextOnUIThread,
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

void CefRequestContextImpl::GetRequestContextImplOnIOThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const RequestContextCallback& callback,
    CefBrowserContext* browser_context) {
  if (!CEF_CURRENTLY_ON_IOT()) {
    CEF_POST_TASK(CEF_IOT,
        base::Bind(&CefRequestContextImpl::GetRequestContextImplOnIOThread,
                   this, task_runner, callback, browser_context));
    return;
  }

  DCHECK(request_context_impl_);

  // Make sure the request context exists.
  request_context_impl_->GetURLRequestContext();

  if (task_runner->BelongsToCurrentThread()) {
    // Execute the callback immediately.
    callback.Run(request_context_impl_);
  } else {
    // Execute the callback on the target thread.
    task_runner->PostTask(FROM_HERE,
        base::Bind(callback, make_scoped_refptr(request_context_impl_)));
  }
}

void CefRequestContextImpl::RegisterSchemeHandlerFactoryInternal(
    const CefString& scheme_name,
    const CefString& domain_name,
    CefRefPtr<CefSchemeHandlerFactory> factory,
    scoped_refptr<CefURLRequestContextGetterImpl> request_context) {
  CEF_REQUIRE_IOT();
  request_context->request_manager()->AddFactory(scheme_name, domain_name,
                                                 factory);
}

void CefRequestContextImpl::ClearSchemeHandlerFactoriesInternal(
    scoped_refptr<CefURLRequestContextGetterImpl> request_context) {
  CEF_REQUIRE_IOT();
  request_context->request_manager()->ClearFactories();
}

void CefRequestContextImpl::PurgePluginListCacheInternal(
    bool reload_pages,
    CefBrowserContext* browser_context) {
  CEF_REQUIRE_UIT();
  browser_context->OnPurgePluginListCache();
  content::PluginService::GetInstance()->PurgePluginListCache(
      browser_context, false);
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

void CefRequestContextImpl::CloseAllConnectionsInternal(
    CefRefPtr<CefCompletionCallback> callback,
    scoped_refptr<CefURLRequestContextGetterImpl> request_context) {
  CEF_REQUIRE_IOT();

  net::URLRequestContext* url_context = request_context->GetURLRequestContext();
  if (url_context) {
    net::HttpTransactionFactory* http_factory =
        url_context->http_transaction_factory();
    if (http_factory) {
      net::HttpCache* cache = http_factory->GetCache();
      if (cache)
        cache->CloseAllConnections();
    }
  }

  if (callback) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefCompletionCallback::OnComplete, callback));
  }
}

void CefRequestContextImpl::ResolveHostInternal(
    const CefString& origin,
    CefRefPtr<CefResolveCallback> callback,
    scoped_refptr<CefURLRequestContextGetterImpl> request_context) {
  CEF_REQUIRE_IOT();

  int retval = ERR_FAILED;

  // |helper| will be deleted in ResolveHostHelper::OnResolveCompleted().
  ResolveHostHelper* helper = new ResolveHostHelper(callback);

  net::HostResolver* host_resolver = request_context->GetHostResolver();
  if (host_resolver) {
    net::HostResolver::RequestInfo request_info(
        net::HostPortPair::FromURL(GURL(origin.ToString())));
    retval = host_resolver->Resolve(
        request_info, net::DEFAULT_PRIORITY,
        &helper->address_list_,
        base::Bind(&ResolveHostHelper::OnResolveCompleted,
                   base::Unretained(helper)),
        &helper->request_, net::NetLogWithSource());
    if (retval == net::ERR_IO_PENDING) {
      // The result will be delivered asynchronously via the callback.
      return;
    }
  }

  helper->OnResolveCompleted(retval);
}

CefBrowserContext* CefRequestContextImpl::browser_context() const {
  if (browser_context_proxy_)
    return browser_context_proxy_.get();
  return browser_context_impl_;
}

