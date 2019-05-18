// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_context.h"

#include <map>
#include <utility>

#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/prefs/browser_prefs.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/ssl_host_state_delegate.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/net_service/util.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/font_family_cache.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/user_prefs/user_prefs.h"
#include "components/visitedlink/browser/visitedlink_event_listener.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_resolution_service.h"

using content::BrowserThread;

namespace {

// Manages the global list of Impl instances.
class ImplManager {
 public:
  typedef std::vector<CefBrowserContext*> Vector;

  ImplManager() {}
  ~ImplManager() {
    DCHECK(all_.empty());
    DCHECK(map_.empty());
  }

  void AddImpl(CefBrowserContext* impl) {
    CEF_REQUIRE_UIT();
    DCHECK(!IsValidImpl(impl));
    all_.push_back(impl);
  }

  void RemoveImpl(CefBrowserContext* impl, const base::FilePath& path) {
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

  bool IsValidImpl(const CefBrowserContext* impl) {
    CEF_REQUIRE_UIT();
    return GetImplPos(impl) != all_.end();
  }

  CefBrowserContext* GetImplForContext(const content::BrowserContext* context) {
    CEF_REQUIRE_UIT();
    if (!context)
      return NULL;

    Vector::iterator it = all_.begin();
    for (; it != all_.end(); ++it) {
      if (*it == context)
        return *it;
    }
    return NULL;
  }

  void SetImplPath(CefBrowserContext* impl, const base::FilePath& path) {
    CEF_REQUIRE_UIT();
    DCHECK(!path.empty());
    DCHECK(IsValidImpl(impl));
    DCHECK(GetImplForPath(path) == NULL);
    map_.insert(std::make_pair(path, impl));
  }

  CefBrowserContext* GetImplForPath(const base::FilePath& path) {
    CEF_REQUIRE_UIT();
    DCHECK(!path.empty());
    PathMap::const_iterator it = map_.find(path);
    if (it != map_.end())
      return it->second;
    return NULL;
  }

  const Vector GetAllImpl() const { return all_; }

 private:
  Vector::iterator GetImplPos(const CefBrowserContext* impl) {
    Vector::iterator it = all_.begin();
    for (; it != all_.end(); ++it) {
      if (*it == impl)
        return it;
    }
    return all_.end();
  }

  typedef std::map<base::FilePath, CefBrowserContext*> PathMap;
  PathMap map_;

  Vector all_;

  DISALLOW_COPY_AND_ASSIGN(ImplManager);
};

#if DCHECK_IS_ON()
// Because of DCHECK()s in the object destructor.
base::LazyInstance<ImplManager>::DestructorAtExit g_manager =
    LAZY_INSTANCE_INITIALIZER;
#else
base::LazyInstance<ImplManager>::Leaky g_manager = LAZY_INSTANCE_INITIALIZER;
#endif

}  // namespace

// Creates and manages VisitedLinkEventListener objects for each
// CefBrowserContext sharing the same VisitedLinkMaster.
class CefVisitedLinkListener : public visitedlink::VisitedLinkMaster::Listener {
 public:
  CefVisitedLinkListener() { DCHECK(listener_map_.empty()); }

  void CreateListenerForContext(const CefBrowserContext* context) {
    CEF_REQUIRE_UIT();
    auto listener = std::make_unique<visitedlink::VisitedLinkEventListener>(
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

  void NewTable(base::ReadOnlySharedMemoryRegion* table_region) override {
    CEF_REQUIRE_UIT();
    ListenerMap::iterator it = listener_map_.begin();
    for (; it != listener_map_.end(); ++it)
      it->second->NewTable(table_region);
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
                   std::unique_ptr<visitedlink::VisitedLinkEventListener>>
      ListenerMap;
  ListenerMap listener_map_;

  DISALLOW_COPY_AND_ASSIGN(CefVisitedLinkListener);
};

CefBrowserContext::CefBrowserContext(const CefRequestContextSettings& settings)
    : settings_(settings) {
  g_manager.Get().AddImpl(this);
}

CefBrowserContext::~CefBrowserContext() {
  CEF_REQUIRE_UIT();

  // No CefRequestContext should be referencing this object any longer.
  DCHECK(request_context_set_.empty());

  // Unregister the context first to avoid re-entrancy during shutdown.
  g_manager.Get().RemoveImpl(this, cache_path_);

  // Send notifications to clean up objects associated with this Profile.
  MaybeSendDestroyedNotification();

  ChromePluginServiceFilter::GetInstance()->UnregisterResourceContext(
      resource_context_.get());

  // Remove any BrowserContextKeyedServiceFactory associations. This must be
  // called before the ProxyService owned by CefBrowserContext is destroyed.
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);

  // Shuts down the storage partitions associated with this browser context.
  // This must be called before the browser context is actually destroyed
  // and before a clean-up task for its corresponding IO thread residents
  // (e.g. ResourceContext) is posted, so that the classes that hung on
  // StoragePartition can have time to do necessary cleanups on IO thread.
  ShutdownStoragePartitions();

  if (resource_context_.get()) {
    // Destruction of the ResourceContext will trigger destruction of all
    // associated URLRequests.
    content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                       resource_context_.release());
  }

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

void CefBrowserContext::Initialize() {
  CefContext* context = CefContext::Get();

  cache_path_ = base::FilePath(CefString(&settings_.cache_path));
  if (!context->ValidateCachePath(cache_path_)) {
    // Reset to in-memory storage.
    CefString(&settings_.cache_path).clear();
    cache_path_ = base::FilePath();
  }

  if (!cache_path_.empty())
    g_manager.Get().SetImplPath(this, cache_path_);

  if (settings_.accept_language_list.length == 0) {
    // Use the global language list setting.
    CefString(&settings_.accept_language_list) =
        CefString(&context->settings().accept_language_list);
  }

  // Initialize the PrefService object.
  pref_service_ = browser_prefs::CreatePrefService(
      this, cache_path_, !!settings_.persist_user_preferences);

  content::BrowserContext::Initialize(this, GetPath());

  resource_context_.reset(new CefResourceContext(IsOffTheRecord()));

  // This must be called before creating any services to avoid hitting
  // DependencyManager::AssertContextWasntDestroyed when creating/destroying
  // multiple browser contexts (due to pointer address reuse).
  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  const bool extensions_enabled = extensions::ExtensionsEnabled();
  if (extensions_enabled) {
    // Create the custom ExtensionSystem first because other KeyedServices
    // depend on it.
    extension_system_ = static_cast<extensions::CefExtensionSystem*>(
        extensions::ExtensionSystem::Get(this));
    extension_system_->InitForRegularProfile(true);
    resource_context_->set_extensions_info_map(extension_system_->info_map());

    // Make sure the ProcessManager is created so that it receives extension
    // load notifications. This is necessary for the proper initialization of
    // background/event pages.
    extensions::ProcessManager::Get(this);
  }

  // Initialize visited links management.
  base::FilePath visited_link_path;
  if (!cache_path_.empty())
    visited_link_path = cache_path_.Append(FILE_PATH_LITERAL("Visited Links"));
  visitedlink_listener_ = new CefVisitedLinkListener;
  visitedlink_master_.reset(new visitedlink::VisitedLinkMaster(
      visitedlink_listener_, this, !visited_link_path.empty(), false,
      visited_link_path, 0));
  visitedlink_listener_->CreateListenerForContext(this);
  visitedlink_master_->Init();

  // Initialize proxy configuration tracker.
  pref_proxy_config_tracker_.reset(new PrefProxyConfigTrackerImpl(
      GetPrefs(),
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})));

  // Spell checking support and possibly other subsystems retrieve the
  // PrefService associated with a BrowserContext via UserPrefs::Get().
  PrefService* pref_service = GetPrefs();
  DCHECK(pref_service);
  user_prefs::UserPrefs::Set(this, pref_service);

  if (extensions_enabled)
    extension_system_->Init();

  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      this, resource_context_.get());

  if (!net_service::IsEnabled()) {
    // Create the CefURLRequestContextGetter via an indirect call to
    // CreateRequestContext. Triggers a call to CefURLRequestContextGetter::
    // GetURLRequestContext() on the IO thread which creates the
    // CefURLRequestContext.
    GetRequestContext();
    DCHECK(url_request_getter_.get());
  }
}

void CefBrowserContext::AddCefRequestContext(CefRequestContextImpl* context) {
  CEF_REQUIRE_UIT();
  request_context_set_.insert(context);
}

void CefBrowserContext::RemoveCefRequestContext(
    CefRequestContextImpl* context) {
  CEF_REQUIRE_UIT();

  if (extensions::ExtensionsEnabled()) {
    extension_system()->OnRequestContextDeleted(context);
  }

  request_context_set_.erase(context);

  // Delete ourselves when the reference count reaches zero.
  if (request_context_set_.empty())
    delete this;
}

// static
CefBrowserContext* CefBrowserContext::GetForCachePath(
    const base::FilePath& cache_path) {
  return g_manager.Get().GetImplForPath(cache_path);
}

// static
CefBrowserContext* CefBrowserContext::GetForContext(
    content::BrowserContext* context) {
  return g_manager.Get().GetImplForContext(context);
}

// static
std::vector<CefBrowserContext*> CefBrowserContext::GetAll() {
  return g_manager.Get().GetAllImpl();
}

content::ResourceContext* CefBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

content::ClientHintsControllerDelegate*
CefBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

net::URLRequestContextGetter* CefBrowserContext::GetRequestContext() {
  CEF_REQUIRE_UIT();
  DCHECK(!net_service::IsEnabled());
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter* CefBrowserContext::CreateMediaRequestContext() {
  return GetRequestContext();
}

net::URLRequestContextGetter*
CefBrowserContext::CreateMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return nullptr;
}

void CefBrowserContext::SetCorsOriginAccessListForOrigin(
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  // This method is called for Extension support.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(closure));
}

ChromeZoomLevelPrefs* CefBrowserContext::GetZoomLevelPrefs() {
  return static_cast<ChromeZoomLevelPrefs*>(
      GetStoragePartition(this, NULL)->GetZoomLevelDelegate());
}

scoped_refptr<network::SharedURLLoaderFactory>
CefBrowserContext::GetURLLoaderFactory() {
  return GetDefaultStoragePartition(this)
      ->GetURLLoaderFactoryForBrowserProcess();
}

base::FilePath CefBrowserContext::GetPath() const {
  return cache_path_;
}

std::unique_ptr<content::ZoomLevelDelegate>
CefBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  if (cache_path_.empty())
    return std::unique_ptr<content::ZoomLevelDelegate>();

  return base::WrapUnique(new ChromeZoomLevelPrefs(
      GetPrefs(), cache_path_, partition_path,
      zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr()));
}

bool CefBrowserContext::IsOffTheRecord() const {
  // CEF contexts are never flagged as off-the-record. It causes problems
  // for the extension system.
  return false;
}

content::DownloadManagerDelegate*
CefBrowserContext::GetDownloadManagerDelegate() {
  if (!download_manager_delegate_) {
    content::DownloadManager* manager =
        BrowserContext::GetDownloadManager(this);
    download_manager_delegate_.reset(new CefDownloadManagerDelegate(manager));
  }
  return download_manager_delegate_.get();
}

content::BrowserPluginGuestManager* CefBrowserContext::GetGuestManager() {
  DCHECK(extensions::ExtensionsEnabled());
  return guest_view::GuestViewManager::FromBrowserContext(this);
}

storage::SpecialStoragePolicy* CefBrowserContext::GetSpecialStoragePolicy() {
  return NULL;
}

content::PushMessagingService* CefBrowserContext::GetPushMessagingService() {
  return NULL;
}

content::SSLHostStateDelegate* CefBrowserContext::GetSSLHostStateDelegate() {
  if (!ssl_host_state_delegate_.get())
    ssl_host_state_delegate_.reset(new CefSSLHostStateDelegate());
  return ssl_host_state_delegate_.get();
}

content::PermissionControllerDelegate*
CefBrowserContext::GetPermissionControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
CefBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
CefBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
CefBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

net::URLRequestContextGetter* CefBrowserContext::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  CEF_REQUIRE_UIT();
  DCHECK(!net_service::IsEnabled());
  DCHECK(!url_request_getter_.get());

  auto io_thread_runner =
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});

  // Initialize the proxy configuration service.
  // TODO(cef): Determine if we can use the Chrome/Mojo implementation from
  // https://crrev.com/d0d0d050
  std::unique_ptr<net::ProxyConfigService> base_service(
      net::ProxyResolutionService::CreateSystemProxyConfigService(
          io_thread_runner));
  std::unique_ptr<net::ProxyConfigService> proxy_config_service(
      pref_proxy_config_tracker_->CreateTrackingProxyConfigService(
          std::move(base_service)));

  if (extensions::ExtensionsEnabled()) {
    // Handle only chrome-extension:// requests. CEF does not support
    // chrome-extension-resource:// requests (it does not store shared extension
    // data in its installation directory).
    extensions::InfoMap* extension_info_map = extension_system()->info_map();
    (*protocol_handlers)[extensions::kExtensionScheme] =
        extensions::CreateExtensionProtocolHandler(IsOffTheRecord(),
                                                   extension_info_map);
  }

  url_request_getter_ = new CefURLRequestContextGetter(
      settings_, GetPrefs(), io_thread_runner, protocol_handlers,
      std::move(proxy_config_service), std::move(request_interceptors));
  return url_request_getter_.get();
}

net::URLRequestContextGetter*
CefBrowserContext::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return nullptr;
}

PrefService* CefBrowserContext::GetPrefs() {
  return pref_service_.get();
}

const PrefService* CefBrowserContext::GetPrefs() const {
  return pref_service_.get();
}

SimpleFactoryKey* CefBrowserContext::GetSimpleFactoryKey() const {
  return nullptr;
}

const CefRequestContextSettings& CefBrowserContext::GetSettings() const {
  return settings_;
}

HostContentSettingsMap* CefBrowserContext::GetHostContentSettingsMap() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!host_content_settings_map_.get()) {
    // The |is_incognito_profile| and |is_guest_profile| arguments are
    // intentionally set to false as they otherwise limit the types of values
    // that can be stored in the settings map (for example, default values set
    // via DefaultProvider::SetWebsiteSetting).
    host_content_settings_map_ =
        new HostContentSettingsMap(GetPrefs(), false, false, false, false);

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

void CefBrowserContext::AddVisitedURLs(const std::vector<GURL>& urls) {
  visitedlink_master_->AddURLs(urls);
}

void CefBrowserContext::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  // Called when visited links will not or cannot be loaded from disk.
  enumerator->OnComplete(true);
}

void CefBrowserContext::OnRenderFrameCreated(
    CefRequestContextImpl* request_context,
    int render_process_id,
    int render_frame_id,
    int frame_tree_node_id,
    bool is_main_frame,
    bool is_guest_view) {
  CEF_REQUIRE_UIT();
  DCHECK_GE(render_process_id, 0);
  DCHECK_GE(render_frame_id, 0);
  DCHECK_GE(frame_tree_node_id, 0);

  CefRefPtr<CefRequestContextHandler> handler = request_context->GetHandler();
  if (handler && resource_context_) {
    DCHECK_GE(render_process_id, 0);
    // Using base::Unretained() is safe because both this callback and possible
    // deletion of |resource_context_| will execute on the IO thread, and this
    // callback will be executed first.
    CEF_POST_TASK(CEF_IOT, base::Bind(&CefResourceContext::AddHandler,
                                      base::Unretained(resource_context_.get()),
                                      render_process_id, render_frame_id,
                                      frame_tree_node_id, handler));
  }
}

void CefBrowserContext::OnRenderFrameDeleted(
    CefRequestContextImpl* request_context,
    int render_process_id,
    int render_frame_id,
    int frame_tree_node_id,
    bool is_main_frame,
    bool is_guest_view) {
  CEF_REQUIRE_UIT();
  DCHECK_GE(render_process_id, 0);
  DCHECK_GE(render_frame_id, 0);
  DCHECK_GE(frame_tree_node_id, 0);

  CefRefPtr<CefRequestContextHandler> handler = request_context->GetHandler();
  if (handler && resource_context_) {
    DCHECK_GE(render_process_id, 0);
    // Using base::Unretained() is safe because both this callback and possible
    // deletion of |resource_context_| will execute on the IO thread, and this
    // callback will be executed first.
    CEF_POST_TASK(CEF_IOT, base::Bind(&CefResourceContext::RemoveHandler,
                                      base::Unretained(resource_context_.get()),
                                      render_process_id, render_frame_id,
                                      frame_tree_node_id));
  }

  if (resource_context_ && is_main_frame) {
    DCHECK_GE(render_process_id, 0);
    // Using base::Unretained() is safe because both this callback and possible
    // deletion of |resource_context_| will execute on the IO thread, and this
    // callback will be executed first.
    CEF_POST_TASK(CEF_IOT,
                  base::Bind(&CefResourceContext::ClearPluginLoadDecision,
                             base::Unretained(resource_context_.get()),
                             render_process_id));
  }
}

void CefBrowserContext::OnPurgePluginListCache() {
  CEF_REQUIRE_UIT();
  if (resource_context_) {
    // Using base::Unretained() is safe because both this callback and possible
    // deletion of |resource_context_| will execute on the IO thread, and this
    // callback will be executed first.
    CEF_POST_TASK(CEF_IOT,
                  base::Bind(&CefResourceContext::ClearPluginLoadDecision,
                             base::Unretained(resource_context_.get()), -1));
  }
}

void CefBrowserContext::RegisterSchemeHandlerFactory(
    const std::string& scheme_name,
    const std::string& domain_name,
    CefRefPtr<CefSchemeHandlerFactory> factory) {
  if (resource_context_) {
    // Using base::Unretained() is safe because both this callback and possible
    // deletion of |resource_context_| will execute on the IO thread, and this
    // callback will be executed first.
    CEF_POST_TASK(CEF_IOT,
                  base::Bind(&CefResourceContext::RegisterSchemeHandlerFactory,
                             base::Unretained(resource_context_.get()),
                             scheme_name, domain_name, factory));
  }
}

void CefBrowserContext::ClearSchemeHandlerFactories() {
  if (resource_context_) {
    // Using base::Unretained() is safe because both this callback and possible
    // deletion of |resource_context_| will execute on the IO thread, and this
    // callback will be executed first.
    CEF_POST_TASK(CEF_IOT,
                  base::Bind(&CefResourceContext::ClearSchemeHandlerFactories,
                             base::Unretained(resource_context_.get())));
  }
}

network::mojom::NetworkContext* CefBrowserContext::GetNetworkContext() {
  CEF_REQUIRE_UIT();
  DCHECK(net_service::IsEnabled());
  return GetDefaultStoragePartition(this)->GetNetworkContext();
}
