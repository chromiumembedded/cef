// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_IMPL_H_
#define CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_IMPL_H_
#pragma once

#include "libcef/browser/browser_context.h"

#include "libcef/browser/net/url_request_context_getter_impl.h"

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"

class CefBrowserContextProxy;
class CefDownloadManagerDelegate;
class CefSSLHostStateDelegate;
class CefVisitedLinkListener;

namespace visitedlink {
class VisitedLinkMaster;
}

// Isolated BrowserContext implementation. Life span is controlled by
// CefBrowserMainParts for the global context and CefRequestContextImpl
// for non-global contexts. Only accessed on the UI thread unless otherwise
// indicated. See browser_context.h for an object relationship diagram.
class CefBrowserContextImpl : public CefBrowserContext,
                              public visitedlink::VisitedLinkDelegate {
 public:
  explicit CefBrowserContextImpl(const CefRequestContextSettings& settings);

  // Returns the existing instance, if any, associated with the specified
  // |cache_path|.
  static CefBrowserContextImpl* GetForCachePath(
      const base::FilePath& cache_path);

  // Returns the underlying CefBrowserContextImpl if any.
  static CefBrowserContextImpl* GetForContext(
      content::BrowserContext* context);

  // Returns all existing CefBrowserContextImpl.
  static std::vector<CefBrowserContextImpl*> GetAll();

  // Must be called immediately after this object is created.
  void Initialize() override;

  // Track associated CefBrowserContextProxy objects.
  void AddProxy(const CefBrowserContextProxy* proxy);
  void RemoveProxy(const CefBrowserContextProxy* proxy);

  // Track associated CefRequestContextImpl objects. If this object is a non-
  // global context then it will delete itself when the count reaches zero.
  void AddRequestContext();
  void RemoveRequestContext();

  // BrowserContext methods.
  base::FilePath GetPath() const override;
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  bool IsOffTheRecord() const override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionManager* GetPermissionManager() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
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
  content::StoragePartition* GetStoragePartitionProxy(
      content::BrowserContext* browser_context,
      content::StoragePartition* partition_impl) override;

  // Profile methods.
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;

  // CefBrowserContext methods.
  const CefRequestContextSettings& GetSettings() const override;
  CefRefPtr<CefRequestContextHandler> GetHandler() const override;
  HostContentSettingsMap* GetHostContentSettingsMap() override;
  void AddVisitedURLs(const std::vector<GURL>& urls) override;

  // visitedlink::VisitedLinkDelegate methods.
  void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) override;

  // Guaranteed to exist once this object has been initialized.
  scoped_refptr<CefURLRequestContextGetterImpl> request_context() const {
    return url_request_getter_;
  }

 private:
  // Allow deletion via std::unique_ptr().
  friend std::default_delete<CefBrowserContextImpl>;

  ~CefBrowserContextImpl() override;

  // Members initialized during construction are safe to access from any thread.
  CefRequestContextSettings settings_;
  base::FilePath cache_path_;

  // Number of CefRequestContextImpl objects referencing this object.
  int request_context_count_ = 0;

  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  std::unique_ptr<CefDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<CefURLRequestContextGetterImpl> url_request_getter_;
  std::unique_ptr<content::PermissionManager> permission_manager_;
  std::unique_ptr<CefSSLHostStateDelegate> ssl_host_state_delegate_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  std::unique_ptr<visitedlink::VisitedLinkMaster> visitedlink_master_;
  // |visitedlink_listener_| is owned by visitedlink_master_.
  CefVisitedLinkListener* visitedlink_listener_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserContextImpl);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_IMPL_H_
