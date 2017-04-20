// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/storage_partition_proxy.h"

CefStoragePartitionProxy::CefStoragePartitionProxy(
    content::StoragePartition* parent,
    CefURLRequestContextGetterProxy* url_request_context)
  : parent_(parent),
    url_request_context_(url_request_context) {
}

CefStoragePartitionProxy::~CefStoragePartitionProxy() {
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

content::IndexedDBContext* CefStoragePartitionProxy::GetIndexedDBContext() {
  return parent_->GetIndexedDBContext();
}

content::ServiceWorkerContext*
CefStoragePartitionProxy::GetServiceWorkerContext() {
  return parent_->GetServiceWorkerContext();
}

content::CacheStorageContext*
CefStoragePartitionProxy::GetCacheStorageContext() {
  return parent_->GetCacheStorageContext();
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

void CefStoragePartitionProxy::ClearDataForOrigin(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const GURL& storage_origin,
    net::URLRequestContextGetter* rq_context,
    const base::Closure& callback) {
  parent_->ClearDataForOrigin(remove_mask, quota_storage_remove_mask,
                              storage_origin, rq_context, callback);
}

void CefStoragePartitionProxy::ClearData(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const GURL& storage_origin,
    const OriginMatcherFunction& origin_matcher,
    const base::Time begin,
    const base::Time end,
    const base::Closure& callback) {
  parent_->ClearData(remove_mask, quota_storage_remove_mask, storage_origin,
                     origin_matcher, begin, end, callback);
}

void CefStoragePartitionProxy::ClearData(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const OriginMatcherFunction& origin_matcher,
    const CookieMatcherFunction& cookie_matcher,
    const base::Time begin,
    const base::Time end,
    const base::Closure& callback) {
  parent_->ClearData(remove_mask, quota_storage_remove_mask, origin_matcher,
                     cookie_matcher, begin, end, callback);
}

void CefStoragePartitionProxy::ClearHttpAndMediaCaches(
    const base::Time begin,
    const base::Time end,
    const base::Callback<bool(const GURL&)>& url_matcher,
    const base::Closure& callback) {
  parent_->ClearHttpAndMediaCaches(begin, end, url_matcher, callback);
}

void CefStoragePartitionProxy::Flush() {
  parent_->Flush();
}

void CefStoragePartitionProxy::ClearBluetoothAllowedDevicesMapForTesting() {
  parent_->ClearBluetoothAllowedDevicesMapForTesting();
}

void CefStoragePartitionProxy::Bind(
    mojo::InterfaceRequest<content::mojom::StoragePartitionService> request) {
  parent_->Bind(std::move(request));
}

