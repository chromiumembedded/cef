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
  virtual ~CefBrowserContextProxy();

  // Reference counting and object life span is managed by
  // CefContentBrowserClient.
  void AddRef() { refct_++; }
  bool Release() { return (--refct_ == 0); }

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
  virtual void RequestMIDISysExPermission(
      int render_process_id,
      int render_view_id,
      const GURL& requesting_frame,
      const MIDISysExPermissionCallback& callback) OVERRIDE;
  virtual content::ResourceContext* GetResourceContext() OVERRIDE;
  virtual content::GeolocationPermissionContext*
      GetGeolocationPermissionContext() OVERRIDE;
  virtual quota::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;

  // CefBrowserContext methods.
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers) OVERRIDE;

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
