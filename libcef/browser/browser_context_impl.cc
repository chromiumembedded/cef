// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_context_impl.h"

#include <map>
#include <utility>

#include "libcef/browser/browser_context_proxy.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/permissions/permission_manager.h"
#include "libcef/browser/prefs/browser_prefs.h"
#include "libcef/browser/ssl_host_state_delegate.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/font_family_cache.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/prefs/pref_service.h"
#include "components/visitedlink/browser/visitedlink_event_listener.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/common/constants.h"
#include "net/proxy/proxy_config_service.h"

using content::BrowserThread;

namespace {

// Manages the global list of Impl instances.
class ImplManager {
 public:
  typedef std::vector<CefBrowserContextImpl*> Vector;

  ImplManager() {}
  ~ImplManager() {
    DCHECK(all_.empty());
    DCHECK(map_.empty());
  }

  void AddImpl(CefBrowserContextImpl* impl) {
    CEF_REQUIRE_UIT();
    DCHECK(!IsValidImpl(impl));
    all_.push_back(impl);
  }

  void RemoveImpl(CefBrowserContextImpl* impl, const base::FilePath& path) {
    CEF_REQUIRE_UIT();

    Vector::iterator it = GetImplPos(impl);
    DCHECK(it != all_.end());
    all_.erase(it);

    if (!path.empty()) {
      PathMap::iterator it = map_.find(path);
      DCHECK(it != map_.end());
      if (it != map_.end())
        map_.erase(it);
    }
  }

  bool IsValidImpl(const CefBrowserContextImpl* impl) {
    CEF_REQUIRE_UIT();
    return GetImplPos(impl) != all_.end();
  }

  CefBrowserContextImpl* GetImplForContext(
      const content::BrowserContext* context) {
    CEF_REQUIRE_UIT();
    if (!context)
      return NULL;

    const CefBrowserContext* cef_context =
        static_cast<const CefBrowserContext*>(context);
    const CefBrowserContextImpl* cef_context_impl = nullptr;
    if (cef_context->is_proxy()) {
      cef_context_impl =
          static_cast<const CefBrowserContextProxy*>(cef_context)->parent();
    } else {
      cef_context_impl = static_cast<const CefBrowserContextImpl*>(cef_context);
    }

    Vector::iterator it = all_.begin();
    for (; it != all_.end(); ++it) {
      if (*it == cef_context_impl)
        return *it;
    }
    return NULL;
  }

  void SetImplPath(CefBrowserContextImpl* impl, const base::FilePath& path) {
    CEF_REQUIRE_UIT();
    DCHECK(!path.empty());
    DCHECK(IsValidImpl(impl));
    DCHECK(GetImplForPath(path) == NULL);
    map_.insert(std::make_pair(path, impl));
  }

  CefBrowserContextImpl* GetImplForPath(const base::FilePath& path) {
    CEF_REQUIRE_UIT();
    DCHECK(!path.empty());
    PathMap::const_iterator it = map_.find(path);
    if (it != map_.end())
      return it->second;
    return NULL;
  }

  const Vector GetAllImpl() const { return all_; }

 private:
  Vector::iterator GetImplPos(const CefBrowserContextImpl* impl) {
    Vector::iterator it = all_.begin();
    for (; it != all_.end(); ++it) {
      if (*it == impl)
        return it;
    }
    return all_.end();
  }

  typedef std::map<base::FilePath, CefBrowserContextImpl*> PathMap;
  PathMap map_;

  Vector all_;

  DISALLOW_COPY_AND_ASSIGN(ImplManager);
};

#if DCHECK_IS_ON()
// Because of DCHECK()s in the object destructor.
base::LazyInstance<ImplManager>::DestructorAtExit g_manager =
    LAZY_INSTANCE_INITIALIZER;
#else
base::LazyInstance<ImplManager>::Leaky g_manager =
    LAZY_INSTANCE_INITIALIZER;
#endif

}  // namespace

// Creates and manages VisitedLinkEventListener objects for each
// CefBrowserContext sharing the same VisitedLinkMaster.
class CefVisitedLinkListener : public visitedlink::VisitedLinkMaster::Listener {
 public:
  CefVisitedLinkListener() {
    DCHECK(listener_map_.empty());
  }

  void CreateListenerForContext(const CefBrowserContext* context) {
    CEF_REQUIRE_UIT();
    auto listener = base::MakeUnique<visitedlink::VisitedLinkEventListener>(
        const_cast<CefBrowserContext*>(context));
    listener_map_.insert(std::make_pair(context, std::move(listener)));
  }

  void RemoveListenerForContext(const CefBrowserContext* context) {
    CEF_REQUIRE_UIT();
    ListenerMap::iterator it = listener_map_.find(context);
    DCHECK(it != listener_map_.end());
    listener_map_.erase(it);
  }

  // visitedlink::VisitedLinkMaster::Listener methods.

  void NewTable(mojo::SharedBufferHandle table) override {
    CEF_REQUIRE_UIT();
    ListenerMap::iterator it = listener_map_.begin();
    for (; it != listener_map_.end(); ++it)
      it->second->NewTable(table);
  }

  void Add(visitedlink::VisitedLinkCommon::Fingerprint fingerprint) override {
    CEF_REQUIRE_UIT();
    ListenerMap::iterator it = listener_map_.begin();
    for (; it != listener_map_.end(); ++it)
      it->second->Add(fingerprint);
  }

  void Reset(bool invalidate_hashes) override {
    CEF_REQUIRE_UIT();
    ListenerMap::iterator it = listener_map_.begin();
    for (; it != listener_map_.end(); ++it)
      it->second->Reset(invalidate_hashes);
  }

 private:
  // Map of CefBrowserContext to the associated VisitedLinkEventListener.
  typedef std::map<const CefBrowserContext*,
                   std::unique_ptr<visitedlink::VisitedLinkEventListener> >
      ListenerMap;
  ListenerMap listener_map_;

  DISALLOW_COPY_AND_ASSIGN(CefVisitedLinkListener);
};

CefBrowserContextImpl::CefBrowserContextImpl(
    const CefRequestContextSettings& settings)
    : CefBrowserContext(false),
      settings_(settings) {
  g_manager.Get().AddImpl(this);
}

CefBrowserContextImpl::~CefBrowserContextImpl() {
  CEF_REQUIRE_UIT();

  // No CefRequestContextImpl should be referencing this object any longer.
  DCHECK_EQ(request_context_count_, 0);

  // Unregister the context first to avoid re-entrancy during shutdown.
  g_manager.Get().RemoveImpl(this, cache_path_);

  Shutdown();

  visitedlink_listener_->RemoveListenerForContext(this);

  // The FontFamilyCache references the ProxyService so delete it before the
  // ProxyService is deleted.
  SetUserData(&kFontFamilyCacheKey, NULL);

  pref_proxy_config_tracker_->DetachFromPrefService();

  if (url_request_getter_)
    url_request_getter_->ShutdownOnUIThread();
  if (host_content_settings_map_)
    host_content_settings_map_->ShutdownOnUIThread();

  // Delete the download manager delegate here because otherwise we'll crash
  // when it's accessed from the content::BrowserContext destructor.
  if (download_manager_delegate_)
    download_manager_delegate_.reset(NULL);
}

void CefBrowserContextImpl::Initialize() {
  cache_path_ = base::FilePath(CefString(&settings_.cache_path));
  if (!cache_path_.empty()) {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (!base::DirectoryExists(cache_path_) &&
        !base::CreateDirectory(cache_path_)) {
      LOG(ERROR) << "The cache_path directory could not be created: " <<
          cache_path_.value();
      cache_path_ = base::FilePath();
      CefString(&settings_.cache_path).clear();
    }
  }

  if (!cache_path_.empty())
    g_manager.Get().SetImplPath(this, cache_path_);

  if (settings_.accept_language_list.length == 0) {
    // Use the global language list setting.
    CefString(&settings_.accept_language_list) =
        CefString(&CefContext::Get()->settings().accept_language_list);
  }

  // Initialize a temporary PrefService object that may be referenced during
  // BrowserContextServices initialization.
  pref_service_ = browser_prefs::CreatePrefService(
      this, base::FilePath(), false, true);

  CefBrowserContext::Initialize();

  // Initialize the real PrefService object.
  pref_service_ = browser_prefs::CreatePrefService(
      this, cache_path_, !!settings_.persist_user_preferences, false);

  // Initialize visited links management.
  base::FilePath visited_link_path;
  if (!cache_path_.empty())
     visited_link_path = cache_path_.Append(FILE_PATH_LITERAL("Visited Links"));
  visitedlink_listener_ = new CefVisitedLinkListener;
  visitedlink_master_.reset(
      new visitedlink::VisitedLinkMaster(visitedlink_listener_, this,
                                         !visited_link_path.empty(), false,
                                         visited_link_path, 0));
  visitedlink_listener_->CreateListenerForContext(this);
  visitedlink_master_->Init();

  // Initialize proxy configuration tracker.
  pref_proxy_config_tracker_.reset(
      ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
          GetPrefs()));

  CefBrowserContext::PostInitialize();

  // Create the CefURLRequestContextGetterImpl via an indirect call to
  // CreateRequestContext. Triggers a call to CefURLRequestContextGetterImpl::
  // GetURLRequestContext() on the IO thread which creates the
  // CefURLRequestContextImpl.
  GetRequestContext();
  DCHECK(url_request_getter_.get());

  // Create the StoragePartitionImplMap and StoragePartitionImpl for this
  // object. This must be done before the first WebContents is created using a
  // CefBrowserContextProxy of this object, otherwise the StoragePartitionProxy
  // will not be created (in that case
  // CefBrowserContextProxy::CreateRequestContext will be called, which is
  // incorrect).
  GetDefaultStoragePartition(this);
}

void CefBrowserContextImpl::AddProxy(const CefBrowserContextProxy* proxy) {
  CEF_REQUIRE_UIT();
  visitedlink_listener_->CreateListenerForContext(proxy);
}

void CefBrowserContextImpl::RemoveProxy(const CefBrowserContextProxy* proxy) {
  CEF_REQUIRE_UIT();
  visitedlink_listener_->RemoveListenerForContext(proxy);
}

void CefBrowserContextImpl::AddRequestContext() {
  CEF_REQUIRE_UIT();
  request_context_count_++;
}

void CefBrowserContextImpl::RemoveRequestContext() {
  CEF_REQUIRE_UIT();
  request_context_count_--;
  DCHECK_GE(request_context_count_, 0);

  // Delete non-global contexts when the reference count reaches zero.
  if (request_context_count_ == 0 &&
      this != CefContentBrowserClient::Get()->browser_context()) {
    delete this;
  }
}

// static
CefBrowserContextImpl* CefBrowserContextImpl::GetForCachePath(
    const base::FilePath& cache_path) {
  return g_manager.Get().GetImplForPath(cache_path);
}

// static
CefBrowserContextImpl* CefBrowserContextImpl::GetForContext(
    content::BrowserContext* context) {
  return g_manager.Get().GetImplForContext(context);
}

// static
std::vector<CefBrowserContextImpl*> CefBrowserContextImpl::GetAll() {
  return g_manager.Get().GetAllImpl();
}

base::FilePath CefBrowserContextImpl::GetPath() const {
  return cache_path_;
}

std::unique_ptr<content::ZoomLevelDelegate>
    CefBrowserContextImpl::CreateZoomLevelDelegate(
        const base::FilePath& partition_path) {
  if (cache_path_.empty())
    return std::unique_ptr<content::ZoomLevelDelegate>();

  return base::WrapUnique(new ChromeZoomLevelPrefs(
      GetPrefs(), cache_path_, partition_path,
      zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr()));
}

bool CefBrowserContextImpl::IsOffTheRecord() const {
  return cache_path_.empty();
}

content::DownloadManagerDelegate*
    CefBrowserContextImpl::GetDownloadManagerDelegate() {
  DCHECK(!download_manager_delegate_.get());

  content::DownloadManager* manager = BrowserContext::GetDownloadManager(this);
  download_manager_delegate_.reset(new CefDownloadManagerDelegate(manager));
  return download_manager_delegate_.get();
}

content::BrowserPluginGuestManager* CefBrowserContextImpl::GetGuestManager() {
  DCHECK(extensions::ExtensionsEnabled());
  return guest_view::GuestViewManager::FromBrowserContext(this);
}

storage::SpecialStoragePolicy*
    CefBrowserContextImpl::GetSpecialStoragePolicy() {
  return NULL;
}

content::PushMessagingService*
    CefBrowserContextImpl::GetPushMessagingService() {
  return NULL;
}

content::SSLHostStateDelegate*
    CefBrowserContextImpl::GetSSLHostStateDelegate() {
  if (!ssl_host_state_delegate_.get())
    ssl_host_state_delegate_.reset(new CefSSLHostStateDelegate());
  return ssl_host_state_delegate_.get();
}

content::PermissionManager* CefBrowserContextImpl::GetPermissionManager() {
  if (!permission_manager_.get())
    permission_manager_.reset(new CefPermissionManager(this));
  return permission_manager_.get();
}

content::BackgroundSyncController*
    CefBrowserContextImpl::GetBackgroundSyncController() {
  return nullptr;
}

net::URLRequestContextGetter* CefBrowserContextImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  CEF_REQUIRE_UIT();
  DCHECK(!url_request_getter_.get());

  // Initialize the proxy configuration service.
  std::unique_ptr<net::ProxyConfigService> proxy_config_service(
      ProxyServiceFactory::CreateProxyConfigService(
          pref_proxy_config_tracker_.get()));

  if (extensions::ExtensionsEnabled()) {
    // Handle only chrome-extension:// requests. CEF does not support
    // chrome-extension-resource:// requests (it does not store shared extension
    // data in its installation directory).
    extensions::InfoMap* extension_info_map =
        extension_system()->info_map();
    (*protocol_handlers)[extensions::kExtensionScheme] =
        linked_ptr<net::URLRequestJobFactory::ProtocolHandler>(
            extensions::CreateExtensionProtocolHandler(
                IsOffTheRecord(), extension_info_map).release());
  }

  url_request_getter_ = new CefURLRequestContextGetterImpl(
      settings_,
      GetPrefs(),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE),
      protocol_handlers,
      std::move(proxy_config_service),
      std::move(request_interceptors));
  resource_context()->set_url_request_context_getter(url_request_getter_.get());
  return url_request_getter_.get();
}

net::URLRequestContextGetter*
    CefBrowserContextImpl::CreateRequestContextForStoragePartition(
        const base::FilePath& partition_path,
        bool in_memory,
        content::ProtocolHandlerMap* protocol_handlers,
        content::URLRequestInterceptorScopedVector request_interceptors) {
  return nullptr;
}

content::StoragePartition* CefBrowserContextImpl::GetStoragePartitionProxy(
    content::BrowserContext* browser_context,
    content::StoragePartition* partition_impl) {
  CefBrowserContextProxy* proxy =
      static_cast<CefBrowserContextProxy*>(browser_context);
  return proxy->GetOrCreateStoragePartitionProxy(partition_impl);
}

PrefService* CefBrowserContextImpl::GetPrefs() {
  return pref_service_.get();
}

const PrefService* CefBrowserContextImpl::GetPrefs() const {
  return pref_service_.get();
}

const CefRequestContextSettings& CefBrowserContextImpl::GetSettings() const {
  return settings_;
}

CefRefPtr<CefRequestContextHandler> CefBrowserContextImpl::GetHandler() const {
  return NULL;
}

HostContentSettingsMap* CefBrowserContextImpl::GetHostContentSettingsMap() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!host_content_settings_map_.get()) {
    // The |is_incognito_profile| and |is_guest_profile| arguments are
    // intentionally set to false as they otherwise limit the types of values
    // that can be stored in the settings map (for example, default values set
    // via DefaultProvider::SetWebsiteSetting).
    host_content_settings_map_ =
        new HostContentSettingsMap(GetPrefs(), false, false);

    // Change the default plugin policy.
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    const std::string& plugin_policy_str =
      command_line->GetSwitchValueASCII(switches::kPluginPolicy);
    if (!plugin_policy_str.empty()) {
      ContentSetting plugin_policy = CONTENT_SETTING_ALLOW;
      if (base::LowerCaseEqualsASCII(plugin_policy_str,
                                     switches::kPluginPolicy_Detect)) {
        plugin_policy = CONTENT_SETTING_DETECT_IMPORTANT_CONTENT;
      } else if (base::LowerCaseEqualsASCII(plugin_policy_str,
                                            switches::kPluginPolicy_Block)) {
        plugin_policy = CONTENT_SETTING_BLOCK;
      }
      host_content_settings_map_->SetDefaultContentSetting(
          CONTENT_SETTINGS_TYPE_PLUGINS, plugin_policy);
    }
  }
  return host_content_settings_map_.get();
}

void CefBrowserContextImpl::AddVisitedURLs(const std::vector<GURL>& urls) {
  visitedlink_master_->AddURLs(urls);
}

void CefBrowserContextImpl::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  // Called when visited links will not or cannot be loaded from disk.
  enumerator->OnComplete(true);
}
