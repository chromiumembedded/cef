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
  virtual ~CefContentBrowserClient();

  // Returns the singleton CefContentBrowserClient instance.
  static CefContentBrowserClient* Get();

  // Methods for managing CefBrowserInfo life span. Do not add new callers of
  // these methods.
  // During popup window creation there is a race between the call to
  // CefBrowserMessageFilter::OnGetNewBrowserInfo on the IO thread and the call
  // to CefBrowserHostImpl::ShouldCreateWebContents on the UI thread. To resolve
  // this race CefBrowserInfo may be created when requested for the first time
  // and before the associated CefBrowserHostImpl is created.
  scoped_refptr<CefBrowserInfo> CreateBrowserInfo();
  scoped_refptr<CefBrowserInfo> GetOrCreateBrowserInfo(int render_process_id,
                                                       int render_view_id);
  void RemoveBrowserInfo(scoped_refptr<CefBrowserInfo> browser_info);
  void DestroyAllBrowsers();

  // Retrieves the CefBrowserInfo matching the specified IDs or an empty
  // pointer if no match is found. It is allowed to add new callers of this
  // method but consider using CefBrowserHostImpl::GetBrowserByRoutingID()
  // instead.
  scoped_refptr<CefBrowserInfo> GetBrowserInfo(int render_process_id,
                                               int render_view_id);

  // Create and return a new CefBrowserContextProxy object.
  CefBrowserContext* CreateBrowserContextProxy(
      CefRefPtr<CefRequestContextHandler> handler);

  // BrowserContexts are nominally owned by RenderViewHosts and
  // CefRequestContextImpls. Keep track of how many objects reference a given
  // context and delete the context when the reference count reaches zero.
  void AddBrowserContextReference(CefBrowserContext* context);
  void RemoveBrowserContextReference(CefBrowserContext* context);

  // ContentBrowserClient implementation.
  virtual content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) OVERRIDE;
  virtual content::WebContentsViewPort* OverrideCreateWebContentsView(
      content::WebContents* web_contents,
      content::RenderViewHostDelegateView** rvhdv) OVERRIDE;
  virtual void RenderProcessHostCreated(
      content::RenderProcessHost* host) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      content::BrowserContext* browser_context,
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers) OVERRIDE;
  virtual bool IsHandledURL(const GURL& url) OVERRIDE;
  virtual void AppendExtraCommandLineSwitches(CommandLine* command_line,
                                              int child_process_id) OVERRIDE;
  virtual content::QuotaPermissionContext*
      CreateQuotaPermissionContext() OVERRIDE;
  virtual content::MediaObserver* GetMediaObserver() OVERRIDE;
  virtual content::SpeechRecognitionManagerDelegate*
      GetSpeechRecognitionManagerDelegate() OVERRIDE;
  virtual void AllowCertificateError(
      int render_process_id,
      int render_view_id,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      ResourceType::Type resource_type,
      bool overridable,
      bool strict_enforcement,
      const base::Callback<void(bool)>& callback,
      content::CertificateRequestResultType* result) OVERRIDE;
  virtual content::AccessTokenStore* CreateAccessTokenStore() OVERRIDE;
  virtual bool CanCreateWindow(const GURL& opener_url,
                               const GURL& opener_top_level_frame_url,
                               const GURL& source_origin,
                               WindowContainerType container_type,
                               const GURL& target_url,
                               const content::Referrer& referrer,
                               WindowOpenDisposition disposition,
                               const WebKit::WebWindowFeatures& features,
                               bool user_gesture,
                               bool opener_suppressed,
                               content::ResourceContext* context,
                               int render_process_id,
                               bool is_guest,
                               int opener_id,
                               bool* no_javascript_access) OVERRIDE;
  virtual void ResourceDispatcherHostCreated() OVERRIDE;
  virtual void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                                   const GURL& url,
                                   WebPreferences* prefs) OVERRIDE;
  virtual void BrowserURLHandlerCreated(
      content::BrowserURLHandler* handler) OVERRIDE;
  virtual std::string GetDefaultDownloadName() OVERRIDE;

#if defined(OS_WIN)
  const wchar_t* GetResourceDllName() OVERRIDE;
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
    string16 target_frame_name;
  };
  void set_last_create_window_params(const LastCreateWindowParams& params);

  // To disable window rendering call this function with |override|=true
  // just before calling WebContents::Create. This will cause
  // OverrideCreateWebContentsView to create a windowless WebContentsView.
  void set_use_osr_next_contents_view(bool value) {
    use_osr_next_contents_view_ = value;
  }
  bool use_osr_next_contents_view() const {
    return use_osr_next_contents_view_;
  }

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

  bool use_osr_next_contents_view_;
};

#endif  // CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
