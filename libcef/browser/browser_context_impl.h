// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_IMPL_H_
#define CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_IMPL_H_
#pragma once

#include "libcef/browser/browser_context.h"

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace content {
class DownloadManagerDelegate;
class SpeechRecognitionPreferences;
}

class CefDownloadManagerDelegate;
class CefURLRequestContextGetter;

class CefBrowserContextImpl : public CefBrowserContext {
 public:
  CefBrowserContextImpl();
  ~CefBrowserContextImpl() override;

  // BrowserContext methods.
  base::FilePath GetPath() const override;
  scoped_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  bool IsOffTheRecord() const override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  net::URLRequestContextGetter* GetRequestContext() override;
  net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) override;
  net::URLRequestContextGetter* GetMediaRequestContext() override;
  net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) override;
  net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path,
          bool in_memory) override;
  content::ResourceContext* GetResourceContext() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;

  // CefBrowserContext methods.
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors)
      override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors)
      override;

 private:
  class CefResourceContext;

  scoped_ptr<CefResourceContext> resource_context_;
  scoped_ptr<CefDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<CefURLRequestContextGetter> url_request_getter_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserContextImpl);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_IMPL_H_
