// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_context_impl.h"

#include <map>

#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/permission_manager.h"
#include "libcef/browser/ssl_host_state_delegate.h"
#include "libcef/browser/thread_util.h"

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/proxy/proxy_config_service.h"

using content::BrowserThread;

namespace {

// Manages the global mapping of cache path to Impl instance.
class ImplManager {
 public:
  ImplManager() {}
  ~ImplManager() {
    DCHECK(map_.empty());
  }

  CefBrowserContextImpl* GetImpl(const base::FilePath& path) {
    CEF_REQUIRE_UIT();
    DCHECK(!path.empty());
    PathMap::const_iterator it = map_.find(path);
    if (it != map_.end())
      return it->second;
    return NULL;
  }

  void AddImpl(const base::FilePath& path, CefBrowserContextImpl* impl) {
    CEF_REQUIRE_UIT();
    DCHECK(!path.empty());
    DCHECK(GetImpl(path) == NULL);
    map_.insert(std::make_pair(path, impl));
  }

  void RemoveImpl(const base::FilePath& path) {
    CEF_REQUIRE_UIT();
    DCHECK(!path.empty());
    PathMap::iterator it = map_.find(path);
    DCHECK(it != map_.end());
    if (it != map_.end())
      map_.erase(it);
  }

 private:
  typedef std::map<base::FilePath, CefBrowserContextImpl*> PathMap;
  PathMap map_;

  DISALLOW_COPY_AND_ASSIGN(ImplManager);
};

base::LazyInstance<ImplManager> g_manager = LAZY_INSTANCE_INITIALIZER;

}  // namespace

CefBrowserContextImpl::CefBrowserContextImpl(
    const CefRequestContextSettings& settings)
    : settings_(settings) {
}

CefBrowserContextImpl::~CefBrowserContextImpl() {
  pref_proxy_config_tracker_->DetachFromPrefService();

  // Delete the download manager delegate here because otherwise we'll crash
  // when it's accessed from the content::BrowserContext destructor.
  if (download_manager_delegate_.get())
    download_manager_delegate_.reset(NULL);

  if (!cache_path_.empty())
    g_manager.Get().RemoveImpl(cache_path_);
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
    g_manager.Get().AddImpl(cache_path_, this);

  if (settings_.accept_language_list.length == 0) {
    // Use the global language list setting.
    CefString(&settings_.accept_language_list) =
        CefString(&CefContext::Get()->settings().accept_language_list);
  }

  // Initialize proxy configuration tracker.
  pref_proxy_config_tracker_.reset(
      ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
          CefContentBrowserClient::Get()->pref_service()));

  // Create the CefURLRequestContextGetterImpl via an indirect call to
  // CreateRequestContext. Triggers a call to CefURLRequestContextGetterImpl::
  // GetURLRequestContext() on the IO thread which creates the
  // CefURLRequestContextImpl.
  GetRequestContext();
  DCHECK(url_request_getter_.get());
}

// static
scoped_refptr<CefBrowserContextImpl> CefBrowserContextImpl::GetForCachePath(
    const base::FilePath& cache_path) {
  return g_manager.Get().GetImpl(cache_path);
}

base::FilePath CefBrowserContextImpl::GetPath() const {
  return cache_path_;
}

scoped_ptr<content::ZoomLevelDelegate>
    CefBrowserContextImpl::CreateZoomLevelDelegate(const base::FilePath&) {
  return scoped_ptr<content::ZoomLevelDelegate>();
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

net::URLRequestContextGetter* CefBrowserContextImpl::GetRequestContext() {
  CEF_REQUIRE_UIT();
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContextImpl::GetRequestContextForRenderProcess(
        int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContextImpl::GetMediaRequestContext() {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContextImpl::GetMediaRequestContextForRenderProcess(
        int renderer_child_id)  {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContextImpl::GetMediaRequestContextForStoragePartition(
        const base::FilePath& partition_path,
        bool in_memory) {
  return GetRequestContext();
}

content::BrowserPluginGuestManager* CefBrowserContextImpl::GetGuestManager() {
  return NULL;
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
    permission_manager_.reset(new CefPermissionManager());
  return permission_manager_.get();
}

bool CefBrowserContextImpl::IsProxy() const {
  return false;
}

const CefRequestContextSettings& CefBrowserContextImpl::GetSettings() const {
  return settings_;
}

CefRefPtr<CefRequestContextHandler> CefBrowserContextImpl::GetHandler() const {
  return NULL;
}

net::URLRequestContextGetter* CefBrowserContextImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  CEF_REQUIRE_UIT();
  DCHECK(!url_request_getter_.get());

  // Initialize the proxy configuration service.
  scoped_ptr<net::ProxyConfigService> proxy_config_service;
  proxy_config_service.reset(
      ProxyServiceFactory::CreateProxyConfigService(
          pref_proxy_config_tracker_.get()));

  url_request_getter_ = new CefURLRequestContextGetterImpl(
      settings_,
      BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::IO),
      BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::FILE),
      protocol_handlers,
      proxy_config_service.Pass(),
      request_interceptors.Pass());
  resource_context()->set_url_request_context_getter(url_request_getter_.get());
  return url_request_getter_.get();
}

net::URLRequestContextGetter*
    CefBrowserContextImpl::CreateRequestContextForStoragePartition(
        const base::FilePath& partition_path,
        bool in_memory,
        content::ProtocolHandlerMap* protocol_handlers,
        content::URLRequestInterceptorScopedVector request_interceptors) {
  return NULL;
}
