// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/storage_partition_proxy.h"

#include "libcef/common/net_service/util.h"

#include "base/logging.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

CefStoragePartitionProxy::CefStoragePartitionProxy(
    content::StoragePartition* parent,
    CefURLRequestContextGetterProxy* url_request_context)
    : parent_(parent), url_request_context_(url_request_context) {
  DCHECK(net_service::IsEnabled() || url_request_context_);
}

CefStoragePartitionProxy::~CefStoragePartitionProxy() {
  if (url_request_context_)
    url_request_context_->ShutdownOnUIThread();
}

base::FilePath CefStoragePartitionProxy::GetPath() {
  return parent_->GetPath();
}

net::URLRequestContextGetter* CefStoragePartitionProxy::GetURLRequestContext() {
  return url_request_context_.get();
}

net::URLRequestContextGetter*
CefStoragePartitionProxy::GetMediaURLRequestContext() {
  return GetURLRequestContext();
}

network::mojom::NetworkContext* CefStoragePartitionProxy::GetNetworkContext() {
  return parent_->GetNetworkContext();
}

scoped_refptr<network::SharedURLLoaderFactory>
CefStoragePartitionProxy::GetURLLoaderFactoryForBrowserProcess() {
  return parent_->GetURLLoaderFactoryForBrowserProcess();
}

std::unique_ptr<network::SharedURLLoaderFactoryInfo>
CefStoragePartitionProxy::GetURLLoaderFactoryForBrowserProcessIOThread() {
  return parent_->GetURLLoaderFactoryForBrowserProcessIOThread();
}

network::mojom::CookieManager*
CefStoragePartitionProxy::GetCookieManagerForBrowserProcess() {
  return parent_->GetCookieManagerForBrowserProcess();
}

storage::QuotaManager* CefStoragePartitionProxy::GetQuotaManager() {
  return parent_->GetQuotaManager();
}

content::AppCacheService* CefStoragePartitionProxy::GetAppCacheService() {
  return parent_->GetAppCacheService();
}

storage::FileSystemContext* CefStoragePartitionProxy::GetFileSystemContext() {
  return parent_->GetFileSystemContext();
}

storage::DatabaseTracker* CefStoragePartitionProxy::GetDatabaseTracker() {
  return parent_->GetDatabaseTracker();
}

content::DOMStorageContext* CefStoragePartitionProxy::GetDOMStorageContext() {
  return parent_->GetDOMStorageContext();
}

content::IdleManager* CefStoragePartitionProxy::GetIdleManager() {
  return parent_->GetIdleManager();
}

content::LockManager* CefStoragePartitionProxy::GetLockManager() {
  return parent_->GetLockManager();
}

content::IndexedDBContext* CefStoragePartitionProxy::GetIndexedDBContext() {
  return parent_->GetIndexedDBContext();
}

content::ServiceWorkerContext*
CefStoragePartitionProxy::GetServiceWorkerContext() {
  return parent_->GetServiceWorkerContext();
}

content::SharedWorkerService*
CefStoragePartitionProxy::GetSharedWorkerService() {
  return parent_->GetSharedWorkerService();
}

content::CacheStorageContext*
CefStoragePartitionProxy::GetCacheStorageContext() {
  return parent_->GetCacheStorageContext();
}

content::GeneratedCodeCacheContext*
CefStoragePartitionProxy::GetGeneratedCodeCacheContext() {
  return parent_->GetGeneratedCodeCacheContext();
}

content::HostZoomMap* CefStoragePartitionProxy::GetHostZoomMap() {
  return parent_->GetHostZoomMap();
}

content::HostZoomLevelContext*
CefStoragePartitionProxy::GetHostZoomLevelContext() {
  return parent_->GetHostZoomLevelContext();
}

content::ZoomLevelDelegate* CefStoragePartitionProxy::GetZoomLevelDelegate() {
  return parent_->GetZoomLevelDelegate();
}

content::PlatformNotificationContext*
CefStoragePartitionProxy::GetPlatformNotificationContext() {
  return parent_->GetPlatformNotificationContext();
}

void CefStoragePartitionProxy::ClearDataForOrigin(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const GURL& storage_origin) {
  parent_->ClearDataForOrigin(remove_mask, quota_storage_remove_mask,
                              storage_origin);
}

void CefStoragePartitionProxy::ClearData(uint32_t remove_mask,
                                         uint32_t quota_storage_remove_mask,
                                         const GURL& storage_origin,
                                         const base::Time begin,
                                         const base::Time end,
                                         base::OnceClosure callback) {
  parent_->ClearData(remove_mask, quota_storage_remove_mask, storage_origin,
                     begin, end, std::move(callback));
}

void CefStoragePartitionProxy::ClearData(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const OriginMatcherFunction& origin_matcher,
    network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
    bool perform_cleanup,
    const base::Time begin,
    const base::Time end,
    base::OnceClosure callback) {
  parent_->ClearData(remove_mask, quota_storage_remove_mask, origin_matcher,
                     std::move(cookie_deletion_filter), perform_cleanup, begin,
                     end, std::move(callback));
}

void CefStoragePartitionProxy::ClearHttpAndMediaCaches(
    const base::Time begin,
    const base::Time end,
    const base::Callback<bool(const GURL&)>& url_matcher,
    base::OnceClosure callback) {
  parent_->ClearHttpAndMediaCaches(begin, end, url_matcher,
                                   std::move(callback));
}

void CefStoragePartitionProxy::ClearCodeCaches(
    base::Time begin,
    base::Time end,
    const base::RepeatingCallback<bool(const GURL&)>& url_matcher,
    base::OnceClosure callback) {
  parent_->ClearCodeCaches(begin, end, url_matcher, std::move(callback));
}

void CefStoragePartitionProxy::Flush() {
  parent_->Flush();
}

void CefStoragePartitionProxy::ResetURLLoaderFactories() {
  parent_->ResetURLLoaderFactories();
}

void CefStoragePartitionProxy::ClearBluetoothAllowedDevicesMapForTesting() {
  parent_->ClearBluetoothAllowedDevicesMapForTesting();
}

void CefStoragePartitionProxy::FlushNetworkInterfaceForTesting() {
  parent_->FlushNetworkInterfaceForTesting();
}

void CefStoragePartitionProxy::WaitForDeletionTasksForTesting() {
  parent_->WaitForDeletionTasksForTesting();
}

content::BackgroundFetchContext*
CefStoragePartitionProxy::GetBackgroundFetchContext() {
  return parent_->GetBackgroundFetchContext();
}

content::BackgroundSyncContext*
CefStoragePartitionProxy::GetBackgroundSyncContext() {
  return parent_->GetBackgroundSyncContext();
}

content::PaymentAppContextImpl*
CefStoragePartitionProxy::GetPaymentAppContext() {
  return parent_->GetPaymentAppContext();
}

content::BroadcastChannelProvider*
CefStoragePartitionProxy::GetBroadcastChannelProvider() {
  return parent_->GetBroadcastChannelProvider();
}

content::BluetoothAllowedDevicesMap*
CefStoragePartitionProxy::GetBluetoothAllowedDevicesMap() {
  return parent_->GetBluetoothAllowedDevicesMap();
}

content::BlobRegistryWrapper* CefStoragePartitionProxy::GetBlobRegistry() {
  return parent_->GetBlobRegistry();
}

content::PrefetchURLLoaderService*
CefStoragePartitionProxy::GetPrefetchURLLoaderService() {
  return parent_->GetPrefetchURLLoaderService();
}

content::CookieStoreContext* CefStoragePartitionProxy::GetCookieStoreContext() {
  return parent_->GetCookieStoreContext();
}

content::DevToolsBackgroundServicesContext*
CefStoragePartitionProxy::GetDevToolsBackgroundServicesContext() {
  return parent_->GetDevToolsBackgroundServicesContext();
}

content::URLLoaderFactoryGetter*
CefStoragePartitionProxy::url_loader_factory_getter() {
  return parent_->url_loader_factory_getter();
}

content::BrowserContext* CefStoragePartitionProxy::browser_context() const {
  return parent_->browser_context();
}

mojo::BindingId CefStoragePartitionProxy::Bind(
    int process_id,
    mojo::InterfaceRequest<blink::mojom::StoragePartitionService> request) {
  return parent_->Bind(process_id, std::move(request));
}

void CefStoragePartitionProxy::Unbind(mojo::BindingId binding_id) {
  parent_->Unbind(binding_id);
}

void CefStoragePartitionProxy::set_site_for_service_worker(
    const GURL& site_for_service_worker) {
  parent_->set_site_for_service_worker(site_for_service_worker);
}

const GURL& CefStoragePartitionProxy::site_for_service_worker() const {
  return parent_->site_for_service_worker();
}
