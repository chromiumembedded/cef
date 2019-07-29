// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_IMPL_H_
#define CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_IMPL_H_
#pragma once

#include "include/cef_request_context_handler.h"
#include "libcef/browser/chrome_profile_stub.h"
#include "libcef/browser/resource_context.h"

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/download/download_prefs.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"

/*
// Classes used in request processing (network, storage, service, etc.):
//
// WC = WebContents
//  Content API representation of a browser. Created by BHI or the system (for
//  popups) and owned by BHI. Keeps a pointer to BC.
//
// BHI = CefBrowserHostImpl
//  Implements the CefBrowser and CefBrowserHost interfaces which are exposed
//  to clients. References an RCI instance. Owns a WC. Life span is controlled
//  by client references and CefContentBrowserClient.
//
// RCI = CefRequestContextImpl
//  Implements the CefRequestContext interface which is exposed to clients.
//  References the isolated BC.
//
// BC = CefBrowserContext
//   Entry point from WC when using an isolated RCI. Owns the RC and creates the
//   SPI indirectly. Owned by CefBrowserMainParts for the global context or RCI
//   for non-global contexts.
//
// SPI = content::StoragePartitionImpl
//   Owns storage-related objects like Quota, IndexedDB, Cache, etc. Created by
//   StoragePartitionImplMap::Get(). Provides access to the URCG. Life span is
//   controlled indirectly by BC.
//
// RC = CefResourceContext
//   Acts as a bridge for resource loading. Network request life span is tied to
//   this object. Must be destroyed before the associated URCG. Life span is
//   controlled by BC.
//
//
// Relationship diagram:
//   ref = reference (CefRefPtr/scoped_refptr)
//   own = ownership (std::unique_ptr)
//   ptr = raw pointer
//
//               CefBrowserMainParts
//                       |
//                      own
//                       v
// BHI -own-> WC -ptr-> BC -own-> SPI
//
// BHI -ref-> RCI -own-> BC -own-> RC
//
//
// How shutdown works:
// 1. CefBrowserHostImpl is destroyed on any thread due to browser close,
//    ref release, etc.
// 2. CefRequestContextImpl is destroyed (possibly asynchronously) on the UI
//    thread due to CefBrowserHostImpl destruction, ref release, etc.
// 3. CefBrowserContext is destroyed on the UI thread due to
//    CefRequestContextImpl destruction or deletion in
//    CefBrowserMainParts::PostMainMessageLoopRun().
// 4. CefResourceContext is destroyed asynchronously on the IO thread due to
//    CefBrowserContext destruction. This cancels/destroys any pending
//    network requests.
*/

class CefDownloadManagerDelegate;
class CefRequestContextImpl;
class CefSSLHostStateDelegate;
class CefVisitedLinkListener;
class HostContentSettingsMap;
class PrefService;

namespace extensions {
class CefExtensionSystem;
}

namespace visitedlink {
class VisitedLinkMaster;
}

// Main entry point for configuring behavior on a per-browser basis. An instance
// of this class is passed to WebContents::Create in CefBrowserHostImpl::
// CreateInternal. Only accessed on the UI thread unless otherwise indicated.
class CefBrowserContext : public ChromeProfileStub,
                          public visitedlink::VisitedLinkDelegate {
 public:
  explicit CefBrowserContext(const CefRequestContextSettings& settings);

  // Returns the existing instance, if any, associated with the specified
  // |cache_path|.
  static CefBrowserContext* GetForCachePath(const base::FilePath& cache_path);

  // Returns the underlying CefBrowserContext if any.
  static CefBrowserContext* GetForContext(content::BrowserContext* context);

  // Returns all existing CefBrowserContext.
  static std::vector<CefBrowserContext*> GetAll();

  // Must be called immediately after this object is created.
  void Initialize();

  // Track associated CefRequestContextImpl objects. This object will delete
  // itself when the count reaches zero.
  void AddCefRequestContext(CefRequestContextImpl* context);
  void RemoveCefRequestContext(CefRequestContextImpl* context);

  // BrowserContext methods.
  content::ResourceContext* GetResourceContext() override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  net::URLRequestContextGetter* GetRequestContext() override;
  net::URLRequestContextGetter* CreateMediaRequestContext() override;
  net::URLRequestContextGetter* CreateMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;
  void SetCorsOriginAccessListForOrigin(
      const url::Origin& source_origin,
      std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure) override;
  base::FilePath GetPath() const override;
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  bool IsOffTheRecord() const override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;

  // Profile methods.
  ChromeZoomLevelPrefs* GetZoomLevelPrefs() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  PrefService* GetPrefs() override;
  bool AllowsBrowserWindows() const override { return false; }
  const PrefService* GetPrefs() const override;
  ProfileKey* GetProfileKey() const override;
  policy::SchemaRegistryService* GetPolicySchemaRegistryService() override;
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override;
  policy::ProfilePolicyConnector* GetProfilePolicyConnector() override;
  const policy::ProfilePolicyConnector* GetProfilePolicyConnector()
      const override;

  // Values checked in ProfileNetworkContextService::CreateNetworkContextParams
  // when creating the NetworkContext.
  bool ShouldRestoreOldSessionCookies() override {
    return should_persist_session_cookies_;
  }
  bool ShouldPersistSessionCookies() override {
    return should_persist_session_cookies_;
  }
  base::Optional<std::vector<std::string>> GetCookieableSchemes() override {
    return cookieable_schemes_;
  }

  // visitedlink::VisitedLinkDelegate methods.
  void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) override;

  // Returns the settings associated with this object. Safe to call from any
  // thread.
  const CefRequestContextSettings& GetSettings() const;

  // Settings for plugins and extensions.
  HostContentSettingsMap* GetHostContentSettingsMap();

  // Called from CefBrowserHostImpl::DidNavigateAnyFrame to update the table of
  // visited links.
  void AddVisitedURLs(const std::vector<GURL>& urls);

  // Called from CefRequestContextImpl::OnRenderFrameCreated.
  void OnRenderFrameCreated(CefRequestContextImpl* request_context,
                            int render_process_id,
                            int render_frame_id,
                            int frame_tree_node_id,
                            bool is_main_frame,
                            bool is_guest_view);

  // Called from CefRequestContextImpl::OnRenderFrameDeleted.
  void OnRenderFrameDeleted(CefRequestContextImpl* request_context,
                            int render_process_id,
                            int render_frame_id,
                            int frame_tree_node_id,
                            bool is_main_frame,
                            bool is_guest_view);

  // Called from CefRequestContextImpl::PurgePluginListCacheInternal when the
  // plugin list cache should be purged.
  void OnPurgePluginListCache();

  // Called from CefRequestContextImpl methods of the same name.
  void RegisterSchemeHandlerFactory(const std::string& scheme_name,
                                    const std::string& domain_name,
                                    CefRefPtr<CefSchemeHandlerFactory> factory);
  void ClearSchemeHandlerFactories();

  network::mojom::NetworkContext* GetNetworkContext();

  void set_should_persist_session_cookies(bool value) {
    should_persist_session_cookies_ = value;
  }
  void set_cookieable_schemes(
      base::Optional<std::vector<std::string>> schemes) {
    cookieable_schemes_ = schemes;
  }

  CefResourceContext* resource_context() const {
    return resource_context_.get();
  }
  extensions::CefExtensionSystem* extension_system() const {
    return extension_system_;
  }

  // Called from DownloadPrefs::FromBrowserContext.
  DownloadPrefs* GetDownloadPrefs();

  // Returns true if this context supports print preview.
  bool IsPrintPreviewSupported() const;

 private:
  // Allow deletion via std::unique_ptr().
  friend std::default_delete<CefBrowserContext>;

  ~CefBrowserContext() override;

  // Members initialized during construction are safe to access from any thread.
  CefRequestContextSettings settings_;
  base::FilePath cache_path_;

  // CefRequestContextImpl objects referencing this object.
  std::set<CefRequestContextImpl*> request_context_set_;

  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  std::unique_ptr<CefDownloadManagerDelegate> download_manager_delegate_;
  std::unique_ptr<CefSSLHostStateDelegate> ssl_host_state_delegate_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  std::unique_ptr<visitedlink::VisitedLinkMaster> visitedlink_master_;
  // |visitedlink_listener_| is owned by visitedlink_master_.
  CefVisitedLinkListener* visitedlink_listener_;
  bool should_persist_session_cookies_ = false;
  base::Optional<std::vector<std::string>> cookieable_schemes_;

  std::unique_ptr<CefResourceContext> resource_context_;

  // Owned by the KeyedService system.
  extensions::CefExtensionSystem* extension_system_ = nullptr;

  // The key to index KeyedService instances created by
  // SimpleKeyedServiceFactory.
  std::unique_ptr<ProfileKey> key_;

  std::unique_ptr<DownloadPrefs> download_prefs_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserContext);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_IMPL_H_
