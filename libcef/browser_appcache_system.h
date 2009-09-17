// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef _BROWSER_APPCACHE_SYSTEM_H
#define _BROWSER_APPCACHE_SYSTEM_H

#include "base/file_path.h"
#include "base/message_loop.h"
#include "webkit/appcache/appcache_backend_impl.h"
#include "webkit/appcache/appcache_frontend_impl.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/glue/resource_type.h"

namespace WebKit {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
}
class BrowserBackendProxy;
class BrowserFrontendProxy;
class URLRequest;
class URLRequestContext;

// A class that composes the constituent parts of an appcache system
// together for use in a single process with two relavant threads,
// a UI thread on which webkit runs and an IO thread on which URLRequests
// are handled. This class conspires with BrowserResourceLoaderBridge to
// retrieve resources from the appcache.
class BrowserAppCacheSystem : public MessageLoop::DestructionObserver {
 public:
  // Should be instanced somewhere in main(). If not instanced, the public
  // static methods are all safe no-ops.
  BrowserAppCacheSystem();
  virtual ~BrowserAppCacheSystem();

  // One-time main UI thread initialization.
  static void InitializeOnUIThread(const FilePath& cache_directory) {
    if (instance_)
      instance_->InitOnUIThread(cache_directory);
  }

  // Called by BrowserResourceLoaderBridge's IOThread class.
  // Per IO thread initialization. Only one IO thread can exist
  // at a time, but after IO thread termination a new one can be
  // started on which this method should be called. The instance
  // is assumed to outlive the IO thread.
  static void InitializeOnIOThread(URLRequestContext* request_context) {
    if (instance_)
      instance_->InitOnIOThread(request_context);
  }

  // Called by TestShellWebKitInit to manufacture a 'host' for webcore.
  static WebKit::WebApplicationCacheHost* CreateApplicationCacheHost(
      WebKit::WebApplicationCacheHostClient* client) {
    return instance_ ? instance_->CreateCacheHostForWebKit(client) : NULL;
  }

  // Called by BrowserResourceLoaderBridge to hook into resource loads.
  static void SetExtraRequestInfo(URLRequest* request,
                                  int host_id,
                                  ResourceType::Type resource_type) {
    if (instance_)
      instance_->SetExtraRequestBits(request, host_id, resource_type);
  }

  // Called by BrowserResourceLoaderBridge extract extra response bits.
  static void GetExtraResponseInfo(URLRequest* request,
                            int64* cache_id,
                            GURL* manifest_url) {
    if (instance_)
      instance_->GetExtraResponseBits(request, cache_id, manifest_url);
  }

 private:
  friend class BrowserBackendProxy;
  friend class BrowserFrontendProxy;

  // A low-tech singleton.
  static BrowserAppCacheSystem* instance_;

  // Instance methods called by our static public methods
  void InitOnUIThread(const FilePath& cache_directory);
  void InitOnIOThread(URLRequestContext* request_context);
  WebKit::WebApplicationCacheHost* CreateCacheHostForWebKit(
      WebKit::WebApplicationCacheHostClient* client);
  void SetExtraRequestBits(URLRequest* request,
                           int host_id,
                           ResourceType::Type resource_type);
  void GetExtraResponseBits(URLRequest* request,
                            int64* cache_id,
                            GURL* manifest_url);

  // Helpers
  MessageLoop* io_message_loop() { return io_message_loop_; }
  MessageLoop* ui_message_loop() { return ui_message_loop_; }
  bool is_io_thread() { return MessageLoop::current() == io_message_loop_; }
  bool is_ui_thread() { return MessageLoop::current() == ui_message_loop_; }
  bool is_initialized() {
    return io_message_loop_ && is_initailized_on_ui_thread();
  }
  bool is_initailized_on_ui_thread() {
    return ui_message_loop_ && !cache_directory_.empty();
  }

  // IOThread DestructionObserver
  virtual void WillDestroyCurrentMessageLoop();

  FilePath cache_directory_;
  MessageLoop* io_message_loop_;
  MessageLoop* ui_message_loop_;
  scoped_refptr<BrowserBackendProxy> backend_proxy_;
  scoped_refptr<BrowserFrontendProxy> frontend_proxy_;
  appcache::AppCacheFrontendImpl frontend_impl_;

  // Created and used only on the IO thread, these do
  // not survive IO thread termination. If a new IO thread
  // is started new instances will be created.
  appcache::AppCacheBackendImpl* backend_impl_;
  appcache::AppCacheService* service_;
};

#endif  // _BROWSER_APPCACHE_SYSTEM_H
