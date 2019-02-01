// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include <string>
#include <utility>

#include "include/cef_request_context_handler.h"
#include "libcef/browser/net/url_request_context_getter_impl.h"
#include "libcef/browser/request_context_impl.h"

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "third_party/skia/include/core/SkColor.h"

class CefBrowserMainParts;
class CefDevToolsDelegate;
class CefResourceDispatcherHostDelegate;

namespace content {
class PluginServiceFilter;
class SiteInstance;
}  // namespace content

namespace extensions {
class Extension;
}

class CefContentBrowserClient : public content::ContentBrowserClient {
 public:
  CefContentBrowserClient();
  ~CefContentBrowserClient() override;

  // Returns the singleton CefContentBrowserClient instance.
  static CefContentBrowserClient* Get();

  // ContentBrowserClient implementation.
  content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  void RenderProcessWillLaunch(
      content::RenderProcessHost* host,
      service_manager::mojom::ServiceRequest* service_request) override;
  bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                               const GURL& effective_url) override;
  bool IsHandledURL(const GURL& url) override;
  void SiteInstanceGotProcess(content::SiteInstance* site_instance) override;
  void SiteInstanceDeleting(content::SiteInstance* site_instance) override;
  void RegisterIOThreadServiceHandlers(
      content::ServiceManagerConnection* connection) override;
  void RegisterOutOfProcessServices(OutOfProcessServiceMap* services) override;
  base::Optional<service_manager::Manifest> GetServiceManifestOverlay(
      base::StringPiece name) override;
  std::vector<service_manager::Manifest> GetExtraServiceManifests() override;
  bool IsSameBrowserContext(content::BrowserContext* context1,
                            content::BrowserContext* context2) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  void AdjustUtilityServiceProcessCommandLine(
      const service_manager::Identity& identity,
      base::CommandLine* command_line) override;
  bool ShouldEnableStrictSiteIsolation() override;
  std::string GetApplicationLocale() override;
  content::QuotaPermissionContext* CreateQuotaPermissionContext() override;
  void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      storage::OptionalQuotaSettingsCallback callback) override;
  content::MediaObserver* GetMediaObserver() override;
  content::SpeechRecognitionManagerDelegate*
  CreateSpeechRecognitionManagerDelegate() override;
  content::GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  void AllowCertificateError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      content::ResourceType resource_type,
      bool strict_enforcement,
      bool expired_previous_decision,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback) override;
  void SelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const url::Origin& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access) override;
  void ResourceDispatcherHostCreated() override;
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           content::WebPreferences* prefs) override;
  void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) override;
  std::string GetDefaultDownloadName() override;
  void DidCreatePpapiPlugin(content::BrowserPpapiHost* browser_host) override;
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(
      content::NavigationHandle* navigation_handle) override;

#if defined(OS_LINUX)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif

#if defined(OS_WIN)
  const wchar_t* GetResourceDllName();
  bool PreSpawnRenderer(sandbox::TargetPolicy* policy) override;
#endif

  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      content::ResourceContext* resource_context) override;

  void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      NonNetworkURLLoaderFactoryMap* factories) override;
  bool WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      bool is_navigation,
      const url::Origin& request_initiator,
      network::mojom::URLLoaderFactoryRequest* factory_request,
      network::mojom::TrustedURLLoaderHeaderClientPtrInfo* header_client,
      bool* bypass_redirect_checks) override;

  bool HandleExternalProtocol(
      const GURL& url,
      content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
      int child_id,
      content::NavigationUIData* navigation_data,
      bool is_main_frame,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::string& method,
      const net::HttpRequestHeaders& headers) override;
  std::string GetProduct() const override;
  std::string GetChromeProduct() const override;
  std::string GetUserAgent() const override;

  // Perform browser process registration for the custom scheme.
  void RegisterCustomScheme(const std::string& scheme);

  CefRefPtr<CefRequestContextImpl> request_context() const;
  CefDevToolsDelegate* devtools_delegate() const;

  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner() const;
  scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner() const;
  scoped_refptr<base::SingleThreadTaskRunner> user_blocking_task_runner() const;

 private:
  // Returns the extension or app associated with |site_instance| or NULL.
  const extensions::Extension* GetExtension(
      content::SiteInstance* site_instance);

  static void HandleExternalProtocolOnUIThread(
      const GURL& url,
      const content::ResourceRequestInfo::WebContentsGetter&
          web_contents_getter);

  CefBrowserMainParts* browser_main_parts_;

  std::unique_ptr<content::PluginServiceFilter> plugin_service_filter_;
  std::unique_ptr<CefResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;
};

#endif  // CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
