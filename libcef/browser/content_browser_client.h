// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/content_browser_client.h"

class CefBrowserMainParts;
class CefMediaObserver;

namespace content {
class SiteInstance;
}

class CefContentBrowserClient : public content::ContentBrowserClient {
 public:
  CefContentBrowserClient();
  virtual ~CefContentBrowserClient();

  void set_browser_main_parts(CefBrowserMainParts* parts) {
    browser_main_parts_ = parts;
  }
  CefBrowserMainParts* browser_main_parts() const {
    return browser_main_parts_;
  }

  virtual content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) OVERRIDE;
  virtual content::WebContentsView* OverrideCreateWebContentsView(
      content::WebContents* web_contents) OVERRIDE;
  virtual content::WebContentsViewDelegate* GetWebContentsViewDelegate(
      content::WebContents* web_contents) OVERRIDE;
  virtual void RenderViewHostCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderProcessHostCreated(
      content::RenderProcessHost* host) OVERRIDE;
  virtual content::WebUIControllerFactory* GetWebUIControllerFactory() OVERRIDE;
  virtual GURL GetEffectiveURL(content::BrowserContext* browser_context,
                               const GURL& url) OVERRIDE;
  virtual bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                                       const GURL& effective_url) OVERRIDE;
  virtual bool IsHandledURL(const GURL& url) OVERRIDE;
  virtual bool IsSuitableHost(content::RenderProcessHost* process_host,
                              const GURL& site_url) OVERRIDE;
  virtual bool ShouldTryToUseExistingProcessHost(
      content::BrowserContext* browser_context, const GURL& url) OVERRIDE;
  virtual void SiteInstanceGotProcess(
      content::SiteInstance* site_instance) OVERRIDE;
  virtual void SiteInstanceDeleting(
      content::SiteInstance* site_instance) OVERRIDE;
  virtual bool ShouldSwapProcessesForNavigation(const GURL& current_url,
                                                const GURL& new_url) OVERRIDE;

  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name) OVERRIDE;
  virtual void AppendExtraCommandLineSwitches(CommandLine* command_line,
                                              int child_process_id) OVERRIDE;
  virtual std::string GetApplicationLocale() OVERRIDE;
  virtual std::string GetAcceptLangs(
      content::BrowserContext* context) OVERRIDE;
  virtual SkBitmap* GetDefaultFavicon() OVERRIDE;
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const GURL& first_party,
                             content::ResourceContext* context) OVERRIDE;
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              content::ResourceContext* context,
                              int render_process_id,
                              int render_view_id) OVERRIDE;
  virtual bool AllowSetCookie(const GURL& url,
                              const GURL& first_party,
                              const std::string& cookie_line,
                              content::ResourceContext* context,
                              int render_process_id,
                              int render_view_id,
                              net::CookieOptions* options) OVERRIDE;
  virtual bool AllowSaveLocalState(
      content::ResourceContext* context) OVERRIDE;
  virtual bool AllowWorkerDatabase(
      const GURL& url,
      const string16& name,
      const string16& display_name,
      unsigned long estimated_size,  // NOLINT(runtime/int)
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_views) OVERRIDE;
  virtual bool AllowWorkerFileSystem(
      const GURL& url,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_views) OVERRIDE;
  virtual bool AllowWorkerIndexedDB(
      const GURL& url,
      const string16& name,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_views) OVERRIDE;
  virtual net::URLRequestContext* OverrideRequestContextForURL(
      const GURL& url, content::ResourceContext* context) OVERRIDE;
  virtual content::QuotaPermissionContext* CreateQuotaPermissionContext()
      OVERRIDE;
  virtual void OpenItem(const FilePath& path) OVERRIDE;
  virtual void ShowItemInFolder(const FilePath& path) OVERRIDE;
  virtual void AllowCertificateError(
      int render_process_id,
      int render_view_id,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool overridable,
      const base::Callback<void(bool)>& callback,
      bool* cancel_request) OVERRIDE;
  virtual void SelectClientCertificate(
      int render_process_id,
      int render_view_id,
      const net::HttpNetworkSession* network_session,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback) OVERRIDE;
  virtual void AddNewCertificate(
      net::URLRequest* request,
      net::X509Certificate* cert,
      int render_process_id,
      int render_view_id) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      const content::MediaStreamRequest* request,
      const content::MediaResponseCallback& callback) OVERRIDE;
  virtual content::MediaObserver* GetMediaObserver() OVERRIDE;
  virtual void RequestDesktopNotificationPermission(
      const GURL& source_origin,
      int callback_context,
      int render_process_id,
      int render_view_id) OVERRIDE;
  virtual WebKit::WebNotificationPresenter::Permission
      CheckDesktopNotificationPermission(
          const GURL& origin,
          content::ResourceContext* context,
          int render_process_id) OVERRIDE;
  virtual void ShowDesktopNotification(
      const content::ShowDesktopNotificationHostMsgParams& params,
      int render_process_id,
      int render_view_id,
      bool worker) OVERRIDE;
  virtual void CancelDesktopNotification(
      int render_process_id,
      int render_view_id,
      int notification_id) OVERRIDE;
  virtual bool CanCreateWindow(
      const GURL& opener_url,
      const GURL& origin,
      WindowContainerType container_type,
      content::ResourceContext* context,
      int render_process_id) OVERRIDE;
  virtual std::string GetWorkerProcessTitle(
      const GURL& url, content::ResourceContext* context) OVERRIDE;
  virtual content::SpeechRecognitionManagerDelegate*
      GetSpeechRecognitionManagerDelegate() OVERRIDE;
  virtual void ResourceDispatcherHostCreated() OVERRIDE;
  virtual ui::Clipboard* GetClipboard() OVERRIDE;
  virtual net::NetLog* GetNetLog() OVERRIDE;
  virtual content::AccessTokenStore* CreateAccessTokenStore() OVERRIDE;
  virtual bool IsFastShutdownPossible() OVERRIDE;
  virtual void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                                   const GURL& url,
                                   WebPreferences* prefs) OVERRIDE;
  virtual void UpdateInspectorSetting(content::RenderViewHost* rvh,
                                      const std::string& key,
                                      const std::string& value) OVERRIDE;
  virtual void ClearInspectorSettings(content::RenderViewHost* rvh) OVERRIDE;
  virtual void BrowserURLHandlerCreated(content::BrowserURLHandler* handler)
      OVERRIDE;
  virtual void ClearCache(content::RenderViewHost* rvh)  OVERRIDE;
  virtual void ClearCookies(content::RenderViewHost* rvh)  OVERRIDE;
  virtual FilePath GetDefaultDownloadDirectory() OVERRIDE;
  virtual std::string GetDefaultDownloadName() OVERRIDE;
  virtual bool AllowSocketAPI(content::BrowserContext* browser_context,
                              const GURL& url) OVERRIDE;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  virtual int GetCrashSignalFD(const CommandLine& command_line) OVERRIDE;
#endif

#if defined(OS_WIN)
  virtual const wchar_t* GetResourceDllName() OVERRIDE;
#endif

#if defined(USE_NSS)
  virtual
      crypto::CryptoModuleBlockingPasswordDelegate* GetCryptoPasswordDelegate(
          const GURL& url) OVERRIDE;
#endif

 private:
  CefBrowserMainParts* browser_main_parts_;

  scoped_ptr<CefMediaObserver> media_observer_;
};

#endif  // CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
