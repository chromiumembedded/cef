// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_context_proxy.h"

#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/net/url_request_context_getter_proxy.h"
#include "libcef/browser/thread_util.h"

#include "base/logging.h"
#include "chrome/browser/font_family_cache.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "content/browser/streams/stream_context.h"
#include "content/public/browser/storage_partition.h"

namespace {

bool ShouldProxyUserData(const void* key) {
  // If this value is not proxied the blob data fails to load for the PDF
  // extension.
  if (key == content::StreamContext::GetUserDataKey())
    return true;

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

  return false;
}

}  // namespace

CefBrowserContextProxy::CefBrowserContextProxy(
    CefRefPtr<CefRequestContextHandler> handler,
    scoped_refptr<CefBrowserContextImpl> parent)
    : handler_(handler),
      parent_(parent) {
  DCHECK(handler_.get());
  DCHECK(parent_.get());
  parent_->AddProxy(this);
}

CefBrowserContextProxy::~CefBrowserContextProxy() {
  Shutdown();

  parent_->RemoveProxy(this);
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

void CefBrowserContextProxy::RemoveUserData(const void* key) {
  if (ShouldProxyUserData(key))
    parent_->RemoveUserData(key);
  else
    BrowserContext::RemoveUserData(key);
}

base::FilePath CefBrowserContextProxy::GetPath() const {
  return parent_->GetPath();
}

scoped_ptr<content::ZoomLevelDelegate>
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

net::URLRequestContextGetter* CefBrowserContextProxy::GetRequestContext() {
  CEF_REQUIRE_UIT();
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContextProxy::GetRequestContextForRenderProcess(
        int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContextProxy::GetMediaRequestContext() {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContextProxy::GetMediaRequestContextForRenderProcess(
        int renderer_child_id)  {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContextProxy::GetMediaRequestContextForStoragePartition(
        const base::FilePath& partition_path,
        bool in_memory) {
  return GetRequestContext();
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

net::URLRequestContextGetter* CefBrowserContextProxy::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  CEF_REQUIRE_UIT();
  DCHECK(!url_request_getter_.get());
  url_request_getter_ =
      new CefURLRequestContextGetterProxy(handler_, parent_->request_context());
  resource_context()->set_url_request_context_getter(url_request_getter_.get());
  return url_request_getter_.get();
}

net::URLRequestContextGetter*
    CefBrowserContextProxy::CreateRequestContextForStoragePartition(
        const base::FilePath& partition_path,
        bool in_memory,
        content::ProtocolHandlerMap* protocol_handlers,
        content::URLRequestInterceptorScopedVector request_interceptors) {
  return NULL;
}

HostContentSettingsMap* CefBrowserContextProxy::GetHostContentSettingsMap() {
  return parent_->GetHostContentSettingsMap();
}

void CefBrowserContextProxy::AddVisitedURLs(const std::vector<GURL>& urls) {
  parent_->AddVisitedURLs(urls);
}
