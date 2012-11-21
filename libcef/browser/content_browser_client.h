// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/content_browser_client.h"

class CefBrowserMainParts;
class CefMediaObserver;
class CefResourceDispatcherHostDelegate;

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

  CefBrowserMainParts* browser_main_parts() const {
    return browser_main_parts_;
  }

  // During popup window creation there is a race between the call to
  // CefBrowserMessageFilter::OnGetNewBrowserInfo on the IO thread and the call
  // to CefBrowserHostImpl::WebContentsCreated on the UI thread. To resolve this
  // race we create the info when requested for the first time.
  class NewPopupBrowserInfo {
   public:
    NewPopupBrowserInfo()
        : browser_id(0) {}
    int browser_id;
  };
  void GetNewPopupBrowserInfo(int render_process_id,
                              int render_view_id,
                              NewPopupBrowserInfo* info);
  void ClearNewPopupBrowserInfo(int render_process_id,
                                int render_view_id);

  virtual content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) OVERRIDE;
  virtual void RenderProcessHostCreated(
      content::RenderProcessHost* host) OVERRIDE;
  virtual content::WebContentsView* OverrideCreateWebContentsView(
      content::WebContents* web_contents,
      content::RenderViewHostDelegateView** rvhdv) OVERRIDE;
  virtual void AppendExtraCommandLineSwitches(CommandLine* command_line,
                                              int child_process_id) OVERRIDE;
  virtual content::QuotaPermissionContext*
      CreateQuotaPermissionContext() OVERRIDE;
  virtual content::MediaObserver* GetMediaObserver() OVERRIDE;
  virtual content::AccessTokenStore* CreateAccessTokenStore() OVERRIDE;
  virtual void ResourceDispatcherHostCreated() OVERRIDE;
  virtual void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                                   const GURL& url,
                                   webkit_glue::WebPreferences* prefs) OVERRIDE;
  virtual void BrowserURLHandlerCreated(
      content::BrowserURLHandler* handler) OVERRIDE;
  virtual std::string GetDefaultDownloadName() OVERRIDE;

#if defined(OS_WIN)
  const wchar_t* GetResourceDllName() OVERRIDE;
#endif

 private:
  CefBrowserMainParts* browser_main_parts_;

  scoped_ptr<CefMediaObserver> media_observer_;
  scoped_ptr<content::PluginServiceFilter> plugin_service_filter_;
  scoped_ptr<CefResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;

  base::Lock new_popup_browser_lock_;

  // Map of (render_process_id, render_view_id) to info. Access must be
  // protected by |new_popup_browser_lock_|.
  typedef std::map<std::pair<int, int>, NewPopupBrowserInfo>
      NewPopupBrowserInfoMap;
  NewPopupBrowserInfoMap new_popup_browser_info_map_;
};

#endif  // CEF_LIBCEF_BROWSER_CONTENT_BROWSER_CLIENT_H_
