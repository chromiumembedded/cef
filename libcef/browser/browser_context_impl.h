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
  virtual ~CefBrowserContextImpl();

  // BrowserContext methods.
  virtual base::FilePath GetPath() const OVERRIDE;
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual content::DownloadManagerDelegate* GetDownloadManagerDelegate() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path,
          bool in_memory) OVERRIDE;
  virtual content::ResourceContext* GetResourceContext() OVERRIDE;
  virtual content::BrowserPluginGuestManager* GetGuestManager() OVERRIDE;
  virtual storage::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;
  virtual content::PushMessagingService* GetPushMessagingService() OVERRIDE;
  virtual content::SSLHostStateDelegate* GetSSLHostStateDelegate() OVERRIDE;

  // CefBrowserContext methods.
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors)
      OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors)
      OVERRIDE;

 private:
  class CefResourceContext;

  scoped_ptr<CefResourceContext> resource_context_;
  scoped_ptr<CefDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<CefURLRequestContextGetter> url_request_getter_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserContextImpl);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_IMPL_H_
