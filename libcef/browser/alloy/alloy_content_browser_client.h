// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_ALLOY_CONTENT_BROWSER_CLIENT_H_
#define CEF_LIBCEF_BROWSER_ALLOY_ALLOY_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include <string>
#include <utility>

#include "include/cef_request_context_handler.h"
#include "libcef/browser/request_context_impl.h"

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "third_party/skia/include/core/SkColor.h"

class AlloyBrowserMainParts;
class CefDevToolsDelegate;

namespace content {
class SiteInstance;
}  // namespace content

namespace extensions {
class Extension;
}

class AlloyContentBrowserClient : public content::ContentBrowserClient {
 public:
  AlloyContentBrowserClient();
  ~AlloyContentBrowserClient() override;

  // ContentBrowserClient implementation.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
  bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                               const GURL& site_url) override;
  bool ShouldUseSpareRenderProcessHost(content::BrowserContext* browser_context,
                                       const GURL& site_url) override;
  bool DoesSiteRequireDedicatedProcess(content::BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;
  bool ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
      base::StringPiece scheme,
      bool is_embedded_origin_secure) override;
  bool ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
      base::StringPiece scheme,
      bool is_embedded_origin_secure) override;
  void OverrideURLLoaderFactoryParams(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      bool is_for_isolated_world,
      network::mojom::URLLoaderFactoryParams* factory_params) override;
  void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) override;
  void GetAdditionalViewSourceSchemes(
      std::vector<std::string>* additional_schemes) override;
  std::unique_ptr<ui::SelectFilePolicy> CreateSelectFilePolicy(
      content::WebContents* web_contents) override;
  void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_allowed_schemes) override;
  bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin) override;
  bool IsHandledURL(const GURL& url) override;
  void SiteInstanceGotProcessAndSite(
      content::SiteInstance* site_instance) override;
  void BindHostReceiverForRenderer(
      content::RenderProcessHost* render_process_host,
      mojo::GenericPendingReceiver receiver) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::string GetApplicationLocale() override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetSystemSharedURLLoaderFactory() override;
  network::mojom::NetworkContext* GetSystemNetworkContext() override;
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
      bool is_main_frame_request,
      bool strict_enforcement,
      base::OnceCallback<void(content::CertificateRequestResultType)> callback)
      override;
  base::OnceClosure SelectClientCertificate(
      content::BrowserContext* browser_context,
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
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  bool OverrideWebPreferencesAfterNavigation(
      content::WebContents* web_contents,
      blink::web_pref::WebPreferences* prefs) override;
  void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) override;
  std::string GetDefaultDownloadName() override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;
  void RegisterAssociatedInterfaceBindersForServiceWorker(
      const content::ServiceWorkerVersionBaseInfo& service_worker_version_info,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(
      content::NavigationHandle* navigation_handle) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id,
      absl::optional<int64_t> navigation_id) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottlesForKeepAlive(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      int frame_tree_node_id) override;
  std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
  WillCreateURLLoaderRequestInterceptors(
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id,
      int64_t navigation_id,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner)
      override;

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      content::BrowserContext* browser_context) override;
  std::unique_ptr<content::LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const content::GlobalRequestID& request_id,
      bool is_request_for_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;
  void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      const std::optional<url::Origin>& request_initiator_origin,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      std::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      network::URLLoaderFactoryBuilder& factory_builder,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks,
      bool* disable_secure_dns,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner)
      override;
  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;
  bool ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override;
  std::vector<base::FilePath> GetNetworkContextsParentDirectory() override;
  bool HandleExternalProtocol(
      const GURL& url,
      content::WebContents::Getter web_contents_getter,
      int frame_tree_node_id,
      content::NavigationUIData* navigation_data,
      bool is_primary_main_frame,
      bool is_in_fenced_frame_tree,
      network::mojom::WebSandboxFlags sandbox_flags,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::optional<url::Origin>& initiating_origin,
      content::RenderFrameHost* initiator_document,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory)
      override;
  bool HandleExternalProtocol(
      content::WebContents::Getter web_contents_getter,
      int frame_tree_node_id,
      content::NavigationUIData* navigation_data,
      bool is_primary_main_frame,
      bool is_in_fenced_frame_tree,
      network::mojom::WebSandboxFlags sandbox_flags,
      const network::ResourceRequest& request,
      const std::optional<url::Origin>& initiating_origin,
      content::RenderFrameHost* initiator_document,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory)
      override;
  std::unique_ptr<content::VideoOverlayWindow>
  CreateWindowForVideoPictureInPicture(
      content::VideoPictureInPictureWindowController* controller) override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
  void RegisterBrowserInterfaceBindersForServiceWorker(
      content::BrowserContext* browser_context,
      const content::ServiceWorkerVersionBaseInfo& service_worker_version_info,
      mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>*
          map) override;
  base::FilePath GetSandboxedStorageServiceDataDirectory() override;
  base::FilePath GetShaderDiskCacheDirectory() override;
  base::FilePath GetGrShaderDiskCacheDirectory() override;
  base::FilePath GetGraphiteDawnDiskCacheDirectory() override;
  base::FilePath GetNetLogDefaultDirectory() override;
  base::FilePath GetFirstPartySetsDirectory() override;
  std::optional<base::FilePath> GetLocalTracesDirectory() override;
  std::string GetProduct() override;
  std::string GetChromeProduct() override;
  std::string GetUserAgent() override;
  std::unique_ptr<content::WebContentsViewDelegate> GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  base::flat_set<std::string> GetPluginMimeTypesWithExternalHandlers(
      content::BrowserContext* browser_context) override;
  void GetMediaDeviceIDSalt(
      content::RenderFrameHost* rfh,
      const net::SiteForCookies& site_for_cookies,
      const blink::StorageKey& storage_key,
      base::OnceCallback<void(bool, const std::string&)> callback) override;
  void OnWebContentsCreated(content::WebContents* web_contents) override;
  bool IsFindInPageDisabledForOrigin(const url::Origin& origin) override;

  CefRefPtr<CefRequestContextImpl> request_context() const;
  CefDevToolsDelegate* devtools_delegate() const;

  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner() const;
  scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner() const;
  scoped_refptr<base::SingleThreadTaskRunner> user_blocking_task_runner() const;

 private:
  // Returns the extension or app associated with |site_instance| or NULL.
  const extensions::Extension* GetExtension(
      content::SiteInstance* site_instance);

  AlloyBrowserMainParts* browser_main_parts_ = nullptr;
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_ALLOY_CONTENT_BROWSER_CLIENT_H_
