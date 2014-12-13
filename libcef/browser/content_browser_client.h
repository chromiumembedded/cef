// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "include/cef_request_context_handler.h"

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/content_browser_client.h"
#include "net/proxy/proxy_config_service.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

class CefBrowserContext;
class CefBrowserInfo;
class CefBrowserMainParts;
class CefDevToolsDelegate;
class CefResourceDispatcherHostDelegate;
class PrefService;

namespace content {
class PluginServiceFilter;
class SiteInstance;
}

class CefContentBrowserClient : public content::ContentBrowserClient {
 public:
  CefContentBrowserClient();
  ~CefContentBrowserClient() override;

  // Returns the singleton CefContentBrowserClient instance.
  static CefContentBrowserClient* Get();

  // Methods for managing CefBrowserInfo life span. Do not add new callers of
  // these methods.
  // During popup window creation there is a race between the call to
  // CefBrowserMessageFilter::OnGetNewBrowserInfo on the IO thread and the call
  // to CefBrowserHostImpl::ShouldCreateWebContents on the UI thread. To resolve
  // this race CefBrowserInfo may be created when requested for the first time
  // and before the associated CefBrowserHostImpl is created.
  scoped_refptr<CefBrowserInfo> CreateBrowserInfo(bool is_popup);
  scoped_refptr<CefBrowserInfo> GetOrCreateBrowserInfo(
      int render_view_process_id,
      int render_view_routing_id,
      int render_frame_process_id,
      int render_frame_routing_id);
  void RemoveBrowserInfo(scoped_refptr<CefBrowserInfo> browser_info);
  void DestroyAllBrowsers();

  // Retrieves the CefBrowserInfo matching the specified IDs or an empty
  // pointer if no match is found. It is allowed to add new callers of this
  // method but consider using CefBrowserHostImpl::GetBrowserFor[View|Frame]()
  // instead.
  scoped_refptr<CefBrowserInfo> GetBrowserInfoForView(int render_process_id,
                                                      int render_routing_id);
  scoped_refptr<CefBrowserInfo> GetBrowserInfoForFrame(int render_process_id,
                                                       int render_routing_id);

  // Create and return a new CefBrowserContextProxy object.
  CefBrowserContext* CreateBrowserContextProxy(
      CefRefPtr<CefRequestContextHandler> handler);

  // BrowserContexts are nominally owned by RenderViewHosts and
  // CefRequestContextImpls. Keep track of how many objects reference a given
  // context and delete the context when the reference count reaches zero.
  void AddBrowserContextReference(CefBrowserContext* context);
  void RemoveBrowserContextReference(CefBrowserContext* context);

  // ContentBrowserClient implementation.
  content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  void RenderProcessWillLaunch(
      content::RenderProcessHost* host) override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors)
      override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      content::BrowserContext* browser_context,
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors)
      override;
  bool IsHandledURL(const GURL& url) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                              int child_process_id) override;
  content::QuotaPermissionContext*
      CreateQuotaPermissionContext() override;
  content::MediaObserver* GetMediaObserver() override;
  content::SpeechRecognitionManagerDelegate*
      CreateSpeechRecognitionManagerDelegate() override;
  void AllowCertificateError(
      int render_process_id,
      int render_frame_id,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      content::ResourceType resource_type,
      bool overridable,
      bool strict_enforcement,
      bool expired_previous_decision,
      const base::Callback<void(bool)>& callback,
      content::CertificateRequestResultType* result) override;
  content::AccessTokenStore* CreateAccessTokenStore() override;
  void RequestPermission(
      content::PermissionType permission,
      content::WebContents* web_contents,
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      const base::Callback<void(bool)>& result_callback) override;
  void CancelPermissionRequest(content::PermissionType permission,
                                       content::WebContents* web_contents,
                                       int bridge_id,
                                       const GURL& requesting_frame) override;
  bool CanCreateWindow(const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const GURL& source_origin,
                       WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       WindowOpenDisposition disposition,
                       const blink::WebWindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       content::ResourceContext* context,
                       int render_process_id,
                       int opener_id,
                       bool* no_javascript_access) override;
  void ResourceDispatcherHostCreated() override;
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           const GURL& url,
                           content::WebPreferences* prefs) override;
  SkColor GetBaseBackgroundColor(content::RenderViewHost* rvh) override;
  void BrowserURLHandlerCreated(
      content::BrowserURLHandler* handler) override;
  std::string GetDefaultDownloadName() override;
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate()
      override;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::FileDescriptorInfo* mappings) override;
#endif

#if defined(OS_WIN)
  const wchar_t* GetResourceDllName() override;
#endif

  // Perform browser process registration for the custom scheme.
  void RegisterCustomScheme(const std::string& scheme);

  // Store additional state from the ViewHostMsg_CreateWindow message that will
  // be used when CanCreateWindow() is called.
  struct LastCreateWindowParams {
    int opener_process_id;
    int opener_view_id;
    int64 opener_frame_id;
    GURL target_url;
    base::string16 target_frame_name;
  };
  void set_last_create_window_params(const LastCreateWindowParams& params);

  CefBrowserContext* browser_context() const;
  scoped_refptr<net::URLRequestContextGetter> request_context() const;
  CefDevToolsDelegate* devtools_delegate() const;
  PrefService* pref_service() const;

  // Passes ownership.
  scoped_ptr<net::ProxyConfigService> proxy_config_service() const;

 private:
  CefBrowserMainParts* browser_main_parts_;

  scoped_ptr<content::PluginServiceFilter> plugin_service_filter_;
  scoped_ptr<CefResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;

  base::Lock browser_info_lock_;

  // Access must be protected by |browser_info_lock_|.
  typedef std::list<scoped_refptr<CefBrowserInfo> > BrowserInfoList;
  BrowserInfoList browser_info_list_;
  int next_browser_id_;

  // Only accessed on the IO thread.
  LastCreateWindowParams last_create_window_params_;
};

#endif  // CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
