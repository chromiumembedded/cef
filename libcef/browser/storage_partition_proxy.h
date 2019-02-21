// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_STORAGE_PARTITION_PROXY_H_
#define CEF_LIBCEF_BROWSER_STORAGE_PARTITION_PROXY_H_
#pragma once

#include "libcef/browser/net/url_request_context_getter_proxy.h"

#include "content/public/browser/storage_partition.h"

// StoragePartition implementation for a particular CefBrowserContextProxy. Life
// span is controlled by CefBrowserContextProxy. Only accessed on the UI thread.
// See browser_context.h for an object relationship diagram.
class CefStoragePartitionProxy : public content::StoragePartition {
 public:
  CefStoragePartitionProxy(
      content::StoragePartition* parent,
      CefURLRequestContextGetterProxy* url_request_context);
  ~CefStoragePartitionProxy() override;

  // StoragePartition methods:
  base::FilePath GetPath() override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  net::URLRequestContextGetter* GetMediaURLRequestContext() override;
  network::mojom::NetworkContext* GetNetworkContext() override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcess() override;
  std::unique_ptr<network::SharedURLLoaderFactoryInfo>
  GetURLLoaderFactoryForBrowserProcessIOThread() override;
  network::mojom::CookieManager* GetCookieManagerForBrowserProcess() override;
  storage::QuotaManager* GetQuotaManager() override;
  content::AppCacheService* GetAppCacheService() override;
  storage::FileSystemContext* GetFileSystemContext() override;
  storage::DatabaseTracker* GetDatabaseTracker() override;
  content::DOMStorageContext* GetDOMStorageContext() override;
  content::IdleManager* GetIdleManager() override;
  content::LockManager* GetLockManager() override;
  content::IndexedDBContext* GetIndexedDBContext() override;
  content::ServiceWorkerContext* GetServiceWorkerContext() override;
  content::SharedWorkerService* GetSharedWorkerService() override;
  content::CacheStorageContext* GetCacheStorageContext() override;
  content::GeneratedCodeCacheContext* GetGeneratedCodeCacheContext() override;
  content::HostZoomMap* GetHostZoomMap() override;
  content::HostZoomLevelContext* GetHostZoomLevelContext() override;
  content::ZoomLevelDelegate* GetZoomLevelDelegate() override;
  content::PlatformNotificationContext* GetPlatformNotificationContext()
      override;
  void ClearDataForOrigin(uint32_t remove_mask,
                          uint32_t quota_storage_remove_mask,
                          const GURL& storage_origin) override;
  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const GURL& storage_origin,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override;
  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const OriginMatcherFunction& origin_matcher,
                 network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
                 bool perform_cleanup,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override;
  void ClearHttpAndMediaCaches(
      const base::Time begin,
      const base::Time end,
      const base::Callback<bool(const GURL&)>& url_matcher,
      base::OnceClosure callback) override;
  void ClearCodeCaches(
      base::Time begin,
      base::Time end,
      const base::RepeatingCallback<bool(const GURL&)>& url_matcher,
      base::OnceClosure callback) override;
  void Flush() override;
  void ResetURLLoaderFactories() override;
  void ClearBluetoothAllowedDevicesMapForTesting() override;
  void FlushNetworkInterfaceForTesting() override;
  void WaitForDeletionTasksForTesting() override;
  content::BackgroundFetchContext* GetBackgroundFetchContext() override;
  content::BackgroundSyncContext* GetBackgroundSyncContext() override;
  content::PaymentAppContextImpl* GetPaymentAppContext() override;
  content::BroadcastChannelProvider* GetBroadcastChannelProvider() override;
  content::BluetoothAllowedDevicesMap* GetBluetoothAllowedDevicesMap() override;
  content::BlobRegistryWrapper* GetBlobRegistry() override;
  content::PrefetchURLLoaderService* GetPrefetchURLLoaderService() override;
  content::CookieStoreContext* GetCookieStoreContext() override;
  content::DevToolsBackgroundServicesContext*
  GetDevToolsBackgroundServicesContext() override;
  content::URLLoaderFactoryGetter* url_loader_factory_getter() override;
  content::BrowserContext* browser_context() const override;
  mojo::BindingId Bind(
      int process_id,
      mojo::InterfaceRequest<blink::mojom::StoragePartitionService> request)
      override;
  void Unbind(mojo::BindingId binding_id) override;
  void set_site_for_service_worker(
      const GURL& site_for_service_worker) override;
  const GURL& site_for_service_worker() const override;

  content::StoragePartition* parent() const { return parent_; }

 private:
  content::StoragePartition* parent_;  // Not owned.
  scoped_refptr<CefURLRequestContextGetterProxy> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(CefStoragePartitionProxy);
};

#endif  // CEF_LIBCEF_BROWSER_STORAGE_PARTITION_PROXY_H_
