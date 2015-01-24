// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_PROXY_H_
#define CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_PROXY_H_
#pragma once

#include "include/cef_request_context_handler.h"
#include "libcef/browser/browser_context.h"

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace content {
class DownloadManagerDelegate;
class SpeechRecognitionPreferences;
}

class CefDownloadManagerDelegate;
class CefURLRequestContextGetterProxy;

// This class is only accessed on the UI thread.
class CefBrowserContextProxy : public CefBrowserContext {
 public:
  CefBrowserContextProxy(CefRefPtr<CefRequestContextHandler> handler,
                         CefBrowserContext* parent);
  ~CefBrowserContextProxy() override;

  // Reference counting and object life span is managed by
  // CefContentBrowserClient.
  void AddRef() { refct_++; }
  bool Release() { return (--refct_ == 0); }

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

  CefRefPtr<CefRequestContextHandler> handler() const { return handler_; }

 private:
  class CefResourceContext;

  int refct_;
  CefRefPtr<CefRequestContextHandler> handler_;
  CefBrowserContext* parent_;
  scoped_ptr<CefResourceContext> resource_context_;
  scoped_ptr<CefDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<CefURLRequestContextGetterProxy> url_request_getter_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserContextProxy);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_PROXY_H_
