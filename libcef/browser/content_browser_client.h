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

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "third_party/skia/include/core/SkColor.h"

class CefBrowserMainParts;
class CefBrowserContextImpl;
class CefDevToolsDelegate;
class CefResourceDispatcherHostDelegate;

namespace content {
class PluginServiceFilter;
class SiteInstance;
}

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
      content::RenderProcessHost* host) override;
  bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                               const GURL& effective_url) override;
  bool IsHandledURL(const GURL& url) override;
  void SiteInstanceGotProcess(content::SiteInstance* site_instance) override;
  void SiteInstanceDeleting(content::SiteInstance* site_instance) override;
  std::unique_ptr<base::Value> GetServiceManifestOverlay(
      base::StringPiece name) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                              int child_process_id) override;
  content::QuotaPermissionContext*
      CreateQuotaPermissionContext() override;
  void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      const storage::OptionalQuotaSettingsCallback& callback) override;
  content::MediaObserver* GetMediaObserver() override;
  content::SpeechRecognitionManagerDelegate*
      CreateSpeechRecognitionManagerDelegate() override;
  void AllowCertificateError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      content::ResourceType resource_type,
      bool overridable,
      bool strict_enforcement,
      bool expired_previous_decision,
      const base::Callback<
          void(content::CertificateRequestResultType)>& callback) override;
  void SelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
  bool CanCreateWindow(int opener_render_process_id,
                       int opener_render_frame_id,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const GURL& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       content::ResourceContext* context,
                       bool* no_javascript_access) override;
  void ResourceDispatcherHostCreated() override;
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           content::WebPreferences* prefs) override;
  void BrowserURLHandlerCreated(
      content::BrowserURLHandler* handler) override;
  std::string GetDefaultDownloadName() override;
  void DidCreatePpapiPlugin(content::BrowserPpapiHost* browser_host) override;
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate()
      override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(
      content::NavigationHandle* navigation_handle) override;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::FileDescriptorInfo* mappings) override;
#endif

#if defined(OS_WIN)
  const wchar_t* GetResourceDllName();
  bool PreSpawnRenderer(sandbox::TargetPolicy* policy) override;
#endif

  // Perform browser process registration for the custom scheme.
  void RegisterCustomScheme(const std::string& scheme);

  CefBrowserContextImpl* browser_context() const;
  CefDevToolsDelegate* devtools_delegate() const;

 private:
  // Returns the extension or app associated with |site_instance| or NULL.
  const extensions::Extension* GetExtension(
      content::SiteInstance* site_instance);

  CefBrowserMainParts* browser_main_parts_;

  std::unique_ptr<content::PluginServiceFilter> plugin_service_filter_;
  std::unique_ptr<CefResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;
};

#endif  // CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
