// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/content_browser_client.h"

#include <algorithm>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_info.h"
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
#include "chrome/common/chrome_switches.h"
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
    : browser_main_parts_(NULL),
      next_browser_id_(0) {
  plugin_service_filter_.reset(new CefPluginServiceFilter);
  content::PluginServiceImpl::GetInstance()->SetFilter(
      plugin_service_filter_.get());

  last_create_window_params_.opener_process_id = MSG_ROUTING_NONE;
}

CefContentBrowserClient::~CefContentBrowserClient() {
}

// static
CefContentBrowserClient* CefContentBrowserClient::Get() {
  return static_cast<CefContentBrowserClient*>(
      content::GetContentClient()->browser());
}

scoped_refptr<CefBrowserInfo> CefContentBrowserClient::CreateBrowserInfo() {
  base::AutoLock lock_scope(browser_info_lock_);

  scoped_refptr<CefBrowserInfo> browser_info =
      new CefBrowserInfo(++next_browser_id_, false);
  browser_info_list_.push_back(browser_info);
  return browser_info;
}

scoped_refptr<CefBrowserInfo>
    CefContentBrowserClient::GetOrCreateBrowserInfo(int render_process_id,
                                                    int render_view_id) {
  base::AutoLock lock_scope(browser_info_lock_);

  BrowserInfoList::const_iterator it = browser_info_list_.begin();
  for (; it != browser_info_list_.end(); ++it) {
    const scoped_refptr<CefBrowserInfo>& browser_info = *it;
    if (browser_info->is_render_id_match(render_process_id, render_view_id))
      return browser_info;
  }

  // Must be a popup if it hasn't already been created.
  scoped_refptr<CefBrowserInfo> browser_info =
      new CefBrowserInfo(++next_browser_id_, true);
  browser_info->set_render_ids(render_process_id, render_view_id);
  browser_info_list_.push_back(browser_info);
  return browser_info;
}

void CefContentBrowserClient::RemoveBrowserInfo(
    scoped_refptr<CefBrowserInfo> browser_info) {
  base::AutoLock lock_scope(browser_info_lock_);

  BrowserInfoList::iterator it = browser_info_list_.begin();
  for (; it != browser_info_list_.end(); ++it) {
    if (*it == browser_info) {
      browser_info_list_.erase(it);
      return;
    }
  }

  NOTREACHED();
}

void CefContentBrowserClient::DestroyAllBrowsers() {
  BrowserInfoList list;

  {
    base::AutoLock lock_scope(browser_info_lock_);
    list = browser_info_list_;
  }

  // Destroy any remaining browser windows.
  if (!list.empty()) {
    BrowserInfoList::iterator it = list.begin();
    for (; it != list.end(); ++it) {
      CefRefPtr<CefBrowserHostImpl> browser = (*it)->browser();
      if (browser.get()) {
        // DestroyBrowser will call RemoveBrowserInfo.
        browser->DestroyBrowser();
      } else {
        // Canceled popup windows may have browser info but no browser because
        // CefBrowserMessageFilter::OnGetNewBrowserInfo is still called.
        DCHECK((*it)->is_popup());
        RemoveBrowserInfo(*it);
      }
    }
  }

#ifndef NDEBUG
  {
    // Verify that all browser windows have been destroyed.
    base::AutoLock lock_scope(browser_info_lock_);
    DCHECK(browser_info_list_.empty());
  }
#endif
}

scoped_refptr<CefBrowserInfo> CefContentBrowserClient::GetBrowserInfo(
    int render_process_id, int render_view_id) {
  base::AutoLock lock_scope(browser_info_lock_);

  BrowserInfoList::const_iterator it = browser_info_list_.begin();
  for (; it != browser_info_list_.end(); ++it) {
    const scoped_refptr<CefBrowserInfo>& browser_info = *it;
    if (browser_info->is_render_id_match(render_process_id, render_view_id))
      return browser_info;
  }

  LOG(WARNING) << "No browser info matching process id " <<
                  render_process_id << " and view id " << render_view_id;

  return scoped_refptr<CefBrowserInfo>();
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
      switches::kEnableReleaseDcheck,
      switches::kDisablePackLoading,
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

bool CefContentBrowserClient::CanCreateWindow(
    const GURL& opener_url,
    const GURL& origin,
    WindowContainerType container_type,
    content::ResourceContext* context,
    int render_process_id,
    bool* no_javascript_access) {
  CEF_REQUIRE_IOT();
  *no_javascript_access = false;

  DCHECK_NE(last_create_window_params_.opener_process_id, MSG_ROUTING_NONE);
  if (last_create_window_params_.opener_process_id == MSG_ROUTING_NONE)
    return false;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserByRoutingID(
          last_create_window_params_.opener_process_id,
          last_create_window_params_.opener_view_id);
  DCHECK(browser.get());
  if (!browser.get()) {
    LOG(WARNING) << "CanCreateWindow called before browser was created";
    return false;
  }

  CefRefPtr<CefClient> client = browser->GetClient();
  bool allow = true;

  scoped_ptr<CefBrowserHostImpl::PendingPopupInfo> pending_info;
  pending_info.reset(new CefBrowserHostImpl::PendingPopupInfo);

#if defined(OS_WIN)
  pending_info->window_info.SetAsPopup(NULL, CefString());
#endif

  // Start with the current browser's settings.
  pending_info->client = client;
  pending_info->settings = browser->settings();

  if (client.get()) {
    CefRefPtr<CefLifeSpanHandler> handler = client->GetLifeSpanHandler();
    if (handler.get()) {
      CefRefPtr<CefFrame> frame =
          browser->GetFrame(last_create_window_params_.opener_frame_id);

      // TODO(cef): Figure out how to populate CefPopupFeatures.
      // See: http://crbug.com/110510
      CefPopupFeatures features;

#if (defined(OS_WIN) || defined(OS_MACOSX))
      // Default to the size from the popup features.
      if (features.xSet)
        pending_info->window_info.x = features.x;
      if (features.ySet)
        pending_info->window_info.y = features.y;
      if (features.widthSet)
        pending_info->window_info.width = features.width;
      if (features.heightSet)
        pending_info->window_info.height = features.height;
#endif

      allow = !handler->OnBeforePopup(browser.get(),
          frame,
          last_create_window_params_.target_url.spec(),
          last_create_window_params_.target_frame_name,
          features,
          pending_info->window_info,
          pending_info->client,
          pending_info->settings,
          no_javascript_access);
      if (allow) {
        if (CefBrowserHostImpl::IsWindowRenderingDisabled(
                pending_info->window_info)) {
          if (!pending_info->client->GetRenderHandler().get()) {
            NOTREACHED() << "CefRenderHandler implementation is required";
            allow = false;
          }
          if (pending_info->settings.accelerated_compositing != STATE_DISABLED) {
            // Accelerated compositing is not supported when window rendering is
            // disabled.
            pending_info->settings.accelerated_compositing = STATE_DISABLED;
          }
        }
      }
    }
  }

  if (allow) {
    CefRefPtr<CefClient> pending_client = pending_info->client;
    allow = browser->SetPendingPopupInfo(pending_info.Pass());
    if (!allow) {
      LOG(WARNING) << "Creation of popup window denied because one is already "
                      "pending.";
    }
  }

  last_create_window_params_.opener_process_id = MSG_ROUTING_NONE;

  return allow;
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

void CefContentBrowserClient::set_last_create_window_params(
    const LastCreateWindowParams& params) {
  CEF_REQUIRE_IOT();
  last_create_window_params_ = params;
}
