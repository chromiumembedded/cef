// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_context_impl.h"

#include <map>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/url_request_context_getter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"

using content::BrowserThread;

class CefBrowserContextImpl::CefResourceContext : public content::ResourceContext {
 public:
  CefResourceContext() : getter_(NULL) {}

  // ResourceContext implementation:
  net::HostResolver* GetHostResolver() override {
    CHECK(getter_);
    return getter_->host_resolver();
  }
  net::URLRequestContext* GetRequestContext() override {
    CHECK(getter_);
    return getter_->GetURLRequestContext();
  }

  void set_url_request_context_getter(CefURLRequestContextGetter* getter) {
    getter_ = getter;
  }

 private:
  CefURLRequestContextGetter* getter_;

  DISALLOW_COPY_AND_ASSIGN(CefResourceContext);
};

CefBrowserContextImpl::CefBrowserContextImpl()
    : resource_context_(new CefResourceContext) {
  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);
}

CefBrowserContextImpl::~CefBrowserContextImpl() {
  // Delete the download manager delegate here because otherwise we'll crash
  // when it's accessed from the content::BrowserContext destructor.
  if (download_manager_delegate_.get())
    download_manager_delegate_.reset(NULL);

  if (resource_context_.get()) {
    BrowserThread::DeleteSoon(
        BrowserThread::IO, FROM_HERE, resource_context_.release());
  }

  // Remove any BrowserContextKeyedServiceFactory associations.
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);
}

base::FilePath CefBrowserContextImpl::GetPath() const {
  return CefContext::Get()->cache_path();
}

scoped_ptr<content::ZoomLevelDelegate>
    CefBrowserContextImpl::CreateZoomLevelDelegate(const base::FilePath&) {
  return scoped_ptr<content::ZoomLevelDelegate>();
}

bool CefBrowserContextImpl::IsOffTheRecord() const {
  return false;
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

content::ResourceContext* CefBrowserContextImpl::GetResourceContext() {
  return resource_context_.get();
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
  return NULL;
}

net::URLRequestContextGetter* CefBrowserContextImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK(!url_request_getter_.get());
  url_request_getter_ = new CefURLRequestContextGetter(
      BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::IO),
      BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::FILE),
      protocol_handlers,
      request_interceptors.Pass());
  resource_context_->set_url_request_context_getter(url_request_getter_.get());
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
