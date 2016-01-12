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
#include "base/memory/scoped_ptr.h"
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
// CefRequestContextImpl and (for the main context) CefBrowserMainParts. Only
// accessed on the UI thread unless otherwise indicated. See browser_context.h
// for an object relationship diagram.
class CefBrowserContextImpl : public CefBrowserContext,
                              public visitedlink::VisitedLinkDelegate {
 public:
  explicit CefBrowserContextImpl(const CefRequestContextSettings& settings);

  // Returns the existing instance, if any, associated with the specified
  // |cache_path|.
  static scoped_refptr<CefBrowserContextImpl> GetForCachePath(
      const base::FilePath& cache_path);

  // Returns the underlying CefBrowserContextImpl if any.
  static CefRefPtr<CefBrowserContextImpl> GetForContext(
      content::BrowserContext* context);

  // Returns all existing CefBrowserContextImpl.
  static std::vector<CefBrowserContextImpl*> GetAll();

  // Must be called immediately after this object is created.
  void Initialize() override;

  // Track associated proxy objects.
  void AddProxy(const CefBrowserContextProxy* proxy);
  void RemoveProxy(const CefBrowserContextProxy* proxy);
  bool HasProxy(const content::BrowserContext* context) const;

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
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionManager* GetPermissionManager() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;

  // Profile methods.
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;

  // CefBrowserContext methods.
  const CefRequestContextSettings& GetSettings() const override;
  CefRefPtr<CefRequestContextHandler> GetHandler() const override;
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
  HostContentSettingsMap* GetHostContentSettingsMap() override;
  void AddVisitedURLs(const std::vector<GURL>& urls) override;

  // visitedlink::VisitedLinkDelegate methods.
  void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) override;

  // Guaranteed to exist once this object has been initialized.
  scoped_refptr<CefURLRequestContextGetterImpl> request_context() const {
    return url_request_getter_;
  }

 private:
  // Only allow deletion via scoped_refptr().
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<CefBrowserContextImpl>;

  ~CefBrowserContextImpl() override;

  // Members initialized during construction are safe to access from any thread.
  CefRequestContextSettings settings_;
  base::FilePath cache_path_;

  // Not owned by this class.
  typedef std::vector<const CefBrowserContextProxy*> ProxyList;
  ProxyList proxy_list_;

  scoped_ptr<PrefService> pref_service_;
  scoped_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  scoped_ptr<CefDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<CefURLRequestContextGetterImpl> url_request_getter_;
  scoped_ptr<content::PermissionManager> permission_manager_;
  scoped_ptr<CefSSLHostStateDelegate> ssl_host_state_delegate_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_ptr<visitedlink::VisitedLinkMaster> visitedlink_master_;
  // |visitedlink_listener_| is owned by visitedlink_master_.
  CefVisitedLinkListener* visitedlink_listener_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserContextImpl);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_IMPL_H_
