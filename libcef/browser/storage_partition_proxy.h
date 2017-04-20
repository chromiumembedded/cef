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
  storage::QuotaManager* GetQuotaManager() override;
  content::AppCacheService* GetAppCacheService() override;
  storage::FileSystemContext* GetFileSystemContext() override;
  storage::DatabaseTracker* GetDatabaseTracker() override;
  content::DOMStorageContext* GetDOMStorageContext() override;
  content::IndexedDBContext* GetIndexedDBContext() override;
  content::ServiceWorkerContext* GetServiceWorkerContext() override;
  content::CacheStorageContext* GetCacheStorageContext() override;
  content::HostZoomMap* GetHostZoomMap() override;
  content::HostZoomLevelContext* GetHostZoomLevelContext() override;
  content::ZoomLevelDelegate* GetZoomLevelDelegate() override;
  content::PlatformNotificationContext* GetPlatformNotificationContext()
      override;
  content::BackgroundFetchContext* GetBackgroundFetchContext() override;
  content::BackgroundSyncContext* GetBackgroundSyncContext() override;
  content::PaymentAppContextImpl* GetPaymentAppContext() override;
  content::BroadcastChannelProvider* GetBroadcastChannelProvider() override;
  content::BluetoothAllowedDevicesMap* GetBluetoothAllowedDevicesMap() override;
  void ClearDataForOrigin(uint32_t remove_mask,
                          uint32_t quota_storage_remove_mask,
                          const GURL& storage_origin,
                          net::URLRequestContextGetter* rq_context,
                          const base::Closure& callback);
  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const GURL& storage_origin,
                 const OriginMatcherFunction& origin_matcher,
                 const base::Time begin,
                 const base::Time end,
                 const base::Closure& callback);
  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const OriginMatcherFunction& origin_matcher,
                 const CookieMatcherFunction& cookie_matcher,
                 const base::Time begin,
                 const base::Time end,
                 const base::Closure& callback) override;
  void ClearHttpAndMediaCaches(
      const base::Time begin,
      const base::Time end,
      const base::Callback<bool(const GURL&)>& url_matcher,
      const base::Closure& callback) override;
  void Flush() override;
  void ClearBluetoothAllowedDevicesMapForTesting() override;
  void Bind(
      mojo::InterfaceRequest<content::mojom::StoragePartitionService> request)
      override;

  content::StoragePartition* parent() const { return parent_; }

 private:
  content::StoragePartition* parent_;  // Not owned.
  scoped_refptr<CefURLRequestContextGetterProxy> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(CefStoragePartitionProxy);
};

#endif  // CEF_LIBCEF_BROWSER_STORAGE_PARTITION_PROXY_H_
