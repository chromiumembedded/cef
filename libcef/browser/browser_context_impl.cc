// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_context_impl.h"

#include <map>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/thread_util.h"

#include "base/logging.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"

using content::BrowserThread;

CefBrowserContextImpl::CefBrowserContextImpl() {
}

CefBrowserContextImpl::~CefBrowserContextImpl() {
  // Delete the download manager delegate here because otherwise we'll crash
  // when it's accessed from the content::BrowserContext destructor.
  if (download_manager_delegate_.get())
    download_manager_delegate_.reset(NULL);
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
  url_request_getter_ = new CefURLRequestContextGetterImpl(
      BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::IO),
      BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::FILE),
      protocol_handlers,
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
