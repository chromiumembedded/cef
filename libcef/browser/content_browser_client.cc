// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/content_browser_client.h"

#include <algorithm>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_main.h"
#include "libcef/browser/browser_message_filter.h"
#include "libcef/browser/browser_settings.h"
#include "libcef/browser/chrome_scheme_handler.h"
#include "libcef/browser/context.h"
#include "libcef/browser/resource_dispatcher_host_delegate.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/web_plugin_impl.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/command_line_impl.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "content/browser/plugin_service_impl.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/quota_permission_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/common/content_switches.h"
#include "googleurl/src/gurl.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "libcef/browser/web_contents_view_osr.h"
#endif

namespace {

// In-memory store for access tokens used by geolocation.
class CefAccessTokenStore : public content::AccessTokenStore {
 public:
  CefAccessTokenStore() {}

  virtual void LoadAccessTokens(
      const LoadAccessTokensCallbackType& callback) OVERRIDE {
    callback.Run(access_token_set_,
        _Context->browser_context()->GetRequestContext());
  }

  virtual void SaveAccessToken(
      const GURL& server_url, const string16& access_token) OVERRIDE {
    access_token_set_[server_url] = access_token;
  }

 private:
  AccessTokenSet access_token_set_;
};

class CefQuotaCallbackImpl : public CefQuotaCallback {
 public:
  explicit CefQuotaCallbackImpl(
      const content::QuotaPermissionContext::PermissionCallback& callback)
      : callback_(callback) {
  }
  ~CefQuotaCallbackImpl() {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_IOT()) {
        CancelNow(callback_);
      } else {
        CEF_POST_TASK(CEF_IOT,
            base::Bind(&CefQuotaCallbackImpl::CancelNow, callback_));
      }
    }
  }

  virtual void Continue(bool allow) OVERRIDE {
    if (CEF_CURRENTLY_ON_IOT()) {
      if (!callback_.is_null()) {
        callback_.Run(allow ?
          content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW :
          content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_DISALLOW);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_IOT,
          base::Bind(&CefQuotaCallbackImpl::Continue, this, allow));
    }
  }

  virtual void Cancel() OVERRIDE {
    if (CEF_CURRENTLY_ON_IOT()) {
      if (!callback_.is_null()) {
        CancelNow(callback_);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_IOT, base::Bind(&CefQuotaCallbackImpl::Cancel, this));
    }
  }

  void Disconnect() {
    callback_.Reset();
  }

 private:
  static void CancelNow(
      const content::QuotaPermissionContext::PermissionCallback& callback) {
    CEF_REQUIRE_IOT();
    callback.Run(
        content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_CANCELLED);
  }

  content::QuotaPermissionContext::PermissionCallback callback_;

  IMPLEMENT_REFCOUNTING(CefQuotaCallbackImpl);
};

class CefQuotaPermissionContext : public content::QuotaPermissionContext {
 public:
  CefQuotaPermissionContext() {
  }

  // The callback will be dispatched on the IO thread.
  virtual void RequestQuotaPermission(
      const GURL& origin_url,
      quota::StorageType type,
      int64 new_quota,
      int render_process_id,
      int render_view_id,
      const PermissionCallback& callback) OVERRIDE {
    if (type != quota::kStorageTypePersistent) {
      // To match Chrome behavior we only support requesting quota with this
      // interface for Persistent storage type.
      callback.Run(QUOTA_PERMISSION_RESPONSE_DISALLOW);
      return;
    }

    bool handled = false;

    CefRefPtr<CefBrowserHostImpl> browser =
        CefBrowserHostImpl::GetBrowserByRoutingID(render_process_id,
                                                  render_view_id);
    if (browser.get()) {
      CefRefPtr<CefClient> client = browser->GetClient();
      if (client.get()) {
        CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
        if (handler.get()) {
          CefRefPtr<CefQuotaCallbackImpl> callbackImpl(
              new CefQuotaCallbackImpl(callback));
          handled = handler->OnQuotaRequest(browser.get(), origin_url.spec(),
                                            new_quota, callbackImpl.get());
          if (!handled)
            callbackImpl->Disconnect();
        }
      }
    }

    if (!handled) {
      // Disallow the request by default.
      callback.Run(QUOTA_PERMISSION_RESPONSE_DISALLOW);
    }
  }

 private:
  virtual ~CefQuotaPermissionContext() {
  }
};

class CefPluginServiceFilter : public content::PluginServiceFilter {
 public:
  CefPluginServiceFilter() {}
  virtual ~CefPluginServiceFilter() {}

  virtual bool ShouldUsePlugin(int render_process_id,
                               int render_view_id,
                               const void* context,
                               const GURL& url,
                               const GURL& policy_url,
                               webkit::WebPluginInfo* plugin) OVERRIDE {
    bool allowed = true;

    CefRefPtr<CefBrowserHostImpl> browser =
        CefBrowserHostImpl::GetBrowserByRoutingID(render_process_id,
                                                  render_view_id);
    if (browser.get()) {
      CefRefPtr<CefClient> client = browser->GetClient();
      if (client.get()) {
        CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
        if (handler.get()) {
          CefRefPtr<CefWebPluginInfoImpl> pluginInfo(
              new CefWebPluginInfoImpl(*plugin));
          allowed =
              !handler->OnBeforePluginLoad(browser.get(),
                                           url.possibly_invalid_spec(),
                                           policy_url.possibly_invalid_spec(),
                                           pluginInfo.get());
        }
      }
    }

    return allowed;
  }
};

}  // namespace


class CefMediaObserver : public content::MediaObserver {
 public:
  CefMediaObserver() {}
  virtual ~CefMediaObserver() {}

  virtual void OnDeleteAudioStream(void* host, int stream_id) OVERRIDE {}

  virtual void OnSetAudioStreamPlaying(void* host, int stream_id,
                                       bool playing) OVERRIDE {}
  virtual void OnSetAudioStreamStatus(void* host, int stream_id,
                                      const std::string& status) OVERRIDE {}
  virtual void OnSetAudioStreamVolume(void* host, int stream_id,
                                      double volume) OVERRIDE {}
  virtual void OnMediaEvent(int render_process_id,
                            const media::MediaLogEvent& event) OVERRIDE {}
  virtual void OnCaptureDevicesOpened(
      int render_process_id,
      int render_view_id,
      const content::MediaStreamDevices& devices) OVERRIDE {}
  virtual void OnCaptureDevicesClosed(
      int render_process_id,
      int render_view_id,
      const content::MediaStreamDevices& devices) OVERRIDE {}
  virtual void OnAudioCaptureDevicesChanged(
      const content::MediaStreamDevices& devices) OVERRIDE {}
  virtual void OnVideoCaptureDevicesChanged(
      const content::MediaStreamDevices& devices) OVERRIDE {}
  virtual void OnMediaRequestStateChanged(
      int render_process_id,
      int render_view_id,
      const content::MediaStreamDevice& device,
      content::MediaRequestState state) OVERRIDE {}
};


CefContentBrowserClient::CefContentBrowserClient()
    : browser_main_parts_(NULL) {
  plugin_service_filter_.reset(new CefPluginServiceFilter);
  content::PluginServiceImpl::GetInstance()->SetFilter(
      plugin_service_filter_.get());
}

CefContentBrowserClient::~CefContentBrowserClient() {
}

// static
CefContentBrowserClient* CefContentBrowserClient::Get() {
  return static_cast<CefContentBrowserClient*>(
      content::GetContentClient()->browser());
}

void CefContentBrowserClient::GetNewPopupBrowserInfo(
    int render_process_id, int render_view_id, NewPopupBrowserInfo* info) {
  base::AutoLock lock_scope(new_popup_browser_lock_);

  NewPopupBrowserInfoMap::const_iterator it =
      new_popup_browser_info_map_.find(
          std::make_pair(render_process_id, render_view_id));
  if (it != new_popup_browser_info_map_.end()) {
    *info = it->second;
    return;
  }

  // Create the info now.
  NewPopupBrowserInfo new_info;
  new_info.browser_id = _Context->GetNextBrowserID();
  new_popup_browser_info_map_.insert(
      std::make_pair(
          std::make_pair(render_process_id, render_view_id), new_info));
  *info = new_info;
}

void CefContentBrowserClient::ClearNewPopupBrowserInfo(int render_process_id,
                                                       int render_view_id) {
  base::AutoLock lock_scope(new_popup_browser_lock_);

  NewPopupBrowserInfoMap::iterator it =
      new_popup_browser_info_map_.find(
          std::make_pair(render_process_id, render_view_id));
  if (it != new_popup_browser_info_map_.end())
    new_popup_browser_info_map_.erase(it);
}

content::BrowserMainParts* CefContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  browser_main_parts_ = new CefBrowserMainParts(parameters);
  return browser_main_parts_;
}

content::WebContentsView*
CefContentBrowserClient::OverrideCreateWebContentsView(
    content::WebContents* web_contents,
    content::RenderViewHostDelegateView** render_view_host_delegate_view) {
  content::WebContentsView* view = NULL;
  *render_view_host_delegate_view = NULL;
  // TODO(port): Implement this method to work on other platforms as part of
  // off-screen rendering support.
#if defined(OS_WIN)
  CefBrowserContext* browserContext =
      static_cast<CefBrowserContext*>(web_contents->GetBrowserContext());

  if (browserContext && browserContext->use_osr_next_contents_view()) {
    CefWebContentsViewOSR* view_or = new CefWebContentsViewOSR(web_contents,
        GetWebContentsViewDelegate(web_contents));
    *render_view_host_delegate_view = view_or;
    view = view_or;
  }
#endif  // OS_WIN

  return view;
}

void CefContentBrowserClient::RenderProcessHostCreated(
    content::RenderProcessHost* host) {
  host->GetChannel()->AddFilter(new CefBrowserMessageFilter(host));
}

void CefContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line, int child_process_id) {
  const CommandLine& browser_cmd = *CommandLine::ForCurrentProcess();

  {
    // Propagate the following switches to all command lines (along with any
    // associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
      switches::kLang,
      switches::kLocalesDirPath,
      switches::kLogFile,
      switches::kLogSeverity,
      switches::kReleaseDcheckEnabled,
      switches::kPackLoadingDisabled,
      switches::kResourcesDirPath,
    };
    command_line->CopySwitchesFrom(browser_cmd, kSwitchNames,
                                   arraysize(kSwitchNames));
  }

  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type == switches::kRendererProcess) {
    // Propagate the following switches to the renderer command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
      switches::kContextSafetyImplementation,
      switches::kProductVersion,
      switches::kUncaughtExceptionStackSize,
    };
    command_line->CopySwitchesFrom(browser_cmd, kSwitchNames,
                                   arraysize(kSwitchNames));
  }

  CefRefPtr<CefApp> app = _Context->application();
  if (app.get()) {
    CefRefPtr<CefBrowserProcessHandler> handler =
        app->GetBrowserProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefCommandLineImpl> commandLinePtr(
          new CefCommandLineImpl(command_line, false, false));
      handler->OnBeforeChildProcessLaunch(commandLinePtr.get());
      commandLinePtr->Detach(NULL);
    }
  }
}

content::QuotaPermissionContext*
    CefContentBrowserClient::CreateQuotaPermissionContext() {
  return new CefQuotaPermissionContext();
}

content::MediaObserver* CefContentBrowserClient::GetMediaObserver() {
  // TODO(cef): Return NULL once it's supported. See crbug.com/116113.
  if (!media_observer_.get())
     media_observer_.reset(new CefMediaObserver());
  return media_observer_.get();
}

content::AccessTokenStore* CefContentBrowserClient::CreateAccessTokenStore() {
  return new CefAccessTokenStore;
}

void CefContentBrowserClient::ResourceDispatcherHostCreated() {
  resource_dispatcher_host_delegate_.reset(
      new CefResourceDispatcherHostDelegate());
  content::ResourceDispatcherHost::Get()->SetDelegate(
      resource_dispatcher_host_delegate_.get());
}

void CefContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    const GURL& url,
    webkit_glue::WebPreferences* prefs) {
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForHost(rvh);
  DCHECK(browser.get());

  // Populate WebPreferences based on CefBrowserSettings.
  BrowserToWebSettings(browser->settings(), *prefs);
}

void CefContentBrowserClient::BrowserURLHandlerCreated(
    content::BrowserURLHandler* handler) {
  // Used to redirect about: URLs to chrome: URLs.
  handler->AddHandlerPair(&scheme::WillHandleBrowserAboutURL,
                          content::BrowserURLHandler::null_handler());
}

std::string CefContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

#if defined(OS_WIN)
const wchar_t* CefContentBrowserClient::GetResourceDllName() {
  static wchar_t file_path[MAX_PATH+1] = {0};

  if (file_path[0] == 0) {
    // Retrieve the module path (usually libcef.dll).
    FilePath module;
    PathService::Get(base::FILE_MODULE, &module);
    const std::wstring wstr = module.value();
    size_t count = std::min(static_cast<size_t>(MAX_PATH), wstr.size());
    wcsncpy(file_path, wstr.c_str(), count);
    file_path[count] = 0;
  }

  return file_path;
}
#endif  // defined(OS_WIN)
