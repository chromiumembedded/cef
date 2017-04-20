// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_context_proxy.h"

#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/net/url_request_context_getter_proxy.h"
#include "libcef/browser/storage_partition_proxy.h"
#include "libcef/browser/thread_util.h"

#include "base/logging.h"
#include "chrome/browser/font_family_cache.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/public/browser/storage_partition.h"

namespace {

bool ShouldProxyUserData(const void* key) {
  // If this value is not proxied then multiple StoragePartitionImpl objects
  // will be created and filesystem API access will fail, among other things.
  if (key == content::BrowserContext::GetStoragePartitionMapUserDataKey())
    return true;

  // If these values are not proxied then blob data fails to load for the PDF
  // extension.
  // See also the call to InitializeResourceContext().
  if (key == content::ChromeBlobStorageContext::GetUserDataKey() ||
      key == content::StreamContext::GetUserDataKey()) {
    return true;
  }

  // If this value is not proxied then CefBrowserContextImpl::GetGuestManager()
  // returns NULL.
  // See also CefExtensionsAPIClient::CreateGuestViewManagerDelegate.
  if (key == guest_view::kGuestViewManagerKeyName)
    return true;

  // If this value is not proxied then there will be a use-after-free while
  // destroying the FontFamilyCache because it will try to access the
  // ProxyService owned by CefBrowserContextImpl (which has already been freed).
  if (key == kFontFamilyCacheKey)
    return true;

  // If this value is not proxied WebUI will fail to load.
  if (key == content::URLDataManager::GetUserDataKey())
    return true;

  return false;
}

}  // namespace

CefBrowserContextProxy::CefBrowserContextProxy(
    CefRefPtr<CefRequestContextHandler> handler,
    CefBrowserContextImpl* parent)
    : CefBrowserContext(true),
      handler_(handler),
      parent_(parent) {
  DCHECK(handler_.get());
  DCHECK(parent_);
  parent_->AddProxy(this);
}

CefBrowserContextProxy::~CefBrowserContextProxy() {
  CEF_REQUIRE_UIT();

  Shutdown();

  parent_->RemoveProxy(this);
}

void CefBrowserContextProxy::Initialize() {
  CefBrowserContext::Initialize();

  // This object's CefResourceContext needs to proxy some UserData requests to
  // the parent object's CefResourceContext.
  resource_context()->set_parent(parent_->resource_context());

  CefBrowserContext::PostInitialize();
}

base::SupportsUserData::Data*
    CefBrowserContextProxy::GetUserData(const void* key) const {
  if (ShouldProxyUserData(key))
    return parent_->GetUserData(key);
  return BrowserContext::GetUserData(key);
}

void CefBrowserContextProxy::SetUserData(const void* key, Data* data) {
  if (ShouldProxyUserData(key))
    parent_->SetUserData(key, data);
  else
    BrowserContext::SetUserData(key, data);
}

void CefBrowserContextProxy::SetUserData(const void* key,
                                         std::unique_ptr<Data> data) {
  if (ShouldProxyUserData(key))
    parent_->SetUserData(key, std::move(data));
  else
    BrowserContext::SetUserData(key, std::move(data));
}

void CefBrowserContextProxy::RemoveUserData(const void* key) {
  if (ShouldProxyUserData(key))
    parent_->RemoveUserData(key);
  else
    BrowserContext::RemoveUserData(key);
}

base::FilePath CefBrowserContextProxy::GetPath() const {
  return parent_->GetPath();
}

std::unique_ptr<content::ZoomLevelDelegate>
    CefBrowserContextProxy::CreateZoomLevelDelegate(
        const base::FilePath& partition_path) {
  return parent_->CreateZoomLevelDelegate(partition_path);
}

bool CefBrowserContextProxy::IsOffTheRecord() const {
  return parent_->IsOffTheRecord();
}

content::DownloadManagerDelegate*
    CefBrowserContextProxy::GetDownloadManagerDelegate() {
  DCHECK(!download_manager_delegate_.get());

  content::DownloadManager* manager = BrowserContext::GetDownloadManager(this);
  download_manager_delegate_.reset(new CefDownloadManagerDelegate(manager));
  return download_manager_delegate_.get();
}

content::BrowserPluginGuestManager* CefBrowserContextProxy::GetGuestManager() {
  return parent_->GetGuestManager();
}

storage::SpecialStoragePolicy*
    CefBrowserContextProxy::GetSpecialStoragePolicy() {
  return parent_->GetSpecialStoragePolicy();
}

content::PushMessagingService*
    CefBrowserContextProxy::GetPushMessagingService() {
  return parent_->GetPushMessagingService();
}

content::SSLHostStateDelegate*
    CefBrowserContextProxy::GetSSLHostStateDelegate() {
  return parent_->GetSSLHostStateDelegate();
}

content::PermissionManager* CefBrowserContextProxy::GetPermissionManager() {
  return parent_->GetPermissionManager();
}

content::BackgroundSyncController*
    CefBrowserContextProxy::GetBackgroundSyncController() {
  return parent_->GetBackgroundSyncController();
}

net::URLRequestContextGetter* CefBrowserContextProxy::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  // CefBrowserContextImpl::GetOrCreateStoragePartitionProxy is called instead
  // of this method.
  NOTREACHED();
  return nullptr;
}

net::URLRequestContextGetter*
    CefBrowserContextProxy::CreateRequestContextForStoragePartition(
        const base::FilePath& partition_path,
        bool in_memory,
        content::ProtocolHandlerMap* protocol_handlers,
        content::URLRequestInterceptorScopedVector request_interceptors) {
  return nullptr;
}

void CefBrowserContextProxy::RegisterInProcessServices(
    StaticServiceMap* services) {
  parent_->RegisterInProcessServices(services);
}

PrefService* CefBrowserContextProxy::GetPrefs() {
  return parent_->GetPrefs();
}

const PrefService* CefBrowserContextProxy::GetPrefs() const {
  return parent_->GetPrefs();
}

const CefRequestContextSettings& CefBrowserContextProxy::GetSettings() const {
  return parent_->GetSettings();
}

CefRefPtr<CefRequestContextHandler> CefBrowserContextProxy::GetHandler() const {
  return handler_;
}

HostContentSettingsMap* CefBrowserContextProxy::GetHostContentSettingsMap() {
  return parent_->GetHostContentSettingsMap();
}

void CefBrowserContextProxy::AddVisitedURLs(const std::vector<GURL>& urls) {
  parent_->AddVisitedURLs(urls);
}

content::StoragePartition*
CefBrowserContextProxy::GetOrCreateStoragePartitionProxy(
    content::StoragePartition* partition_impl) {
  CEF_REQUIRE_UIT();

  if (!storage_partition_proxy_) {
    scoped_refptr<CefURLRequestContextGetterProxy> url_request_getter =
        new CefURLRequestContextGetterProxy(handler_,
                                            parent_->request_context());
    resource_context()->set_url_request_context_getter(
        url_request_getter.get());
    storage_partition_proxy_.reset(
        new CefStoragePartitionProxy(partition_impl,
                                     url_request_getter.get()));

    // Associates UserData keys with the ResourceContext.
    // Called from StoragePartitionImplMap::Get() for CefBrowserContextImpl.
    content::InitializeResourceContext(this);
  }

  // There should only be one CefStoragePartitionProxy for this
  // CefBrowserContextProxy.
  DCHECK_EQ(storage_partition_proxy_->parent(), partition_impl);
  return storage_partition_proxy_.get();
}
