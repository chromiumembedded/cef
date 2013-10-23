// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/content_browser_client.h"

#include <algorithm>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_context_proxy.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_main.h"
#include "libcef/browser/browser_message_filter.h"
#include "libcef/browser/browser_settings.h"
#include "libcef/browser/chrome_scheme_handler.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/printing/printing_message_filter.h"
#include "libcef/browser/resource_dispatcher_host_delegate.h"
#include "libcef/browser/speech_recognition_manager_delegate.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/web_contents_view_osr.h"
#include "libcef/browser/web_plugin_impl.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/command_line_impl.h"
#include "libcef/common/content_client.h"
#include "libcef/common/scheme_registration.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/plugin_service_impl.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/quota_permission_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"

namespace {

// In-memory store for access tokens used by geolocation.
class CefAccessTokenStore : public content::AccessTokenStore {
 public:
  CefAccessTokenStore() {}

  virtual void LoadAccessTokens(
      const LoadAccessTokensCallbackType& callback) OVERRIDE {
    callback.Run(access_token_set_,
        CefContentBrowserClient::Get()->request_context());
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

class CefAllowCertificateErrorCallbackImpl
    : public CefAllowCertificateErrorCallback {
 public:
  explicit CefAllowCertificateErrorCallbackImpl(
      const base::Callback<void(bool)>& callback) : callback_(callback) {
  }

  virtual void Continue(bool allow) OVERRIDE {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        callback_.Run(allow);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefAllowCertificateErrorCallbackImpl::Continue,
              this, allow));
    }
  }

  void Disconnect() {
    callback_.Reset();
  }

 private:
  base::Callback<void(bool)> callback_;

  IMPLEMENT_REFCOUNTING(CefAllowCertificateErrorCallbackImpl);
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

  virtual bool IsPluginAvailable(int render_process_id,
                                 int render_view_id,
                                 const void* context,
                                 const GURL& url,
                                 const GURL& policy_url,
                                 content::WebPluginInfo* plugin) OVERRIDE {
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

  virtual bool CanLoadPlugin(int render_process_id,
                             const base::FilePath& path) OVERRIDE {
    return true;
  }
};

void TranslatePopupFeatures(const WebKit::WebWindowFeatures& webKitFeatures,
                            CefPopupFeatures& features) {
  features.x = static_cast<int>(webKitFeatures.x);
  features.xSet = webKitFeatures.xSet;
  features.y = static_cast<int>(webKitFeatures.y);
  features.ySet = webKitFeatures.ySet;
  features.width = static_cast<int>(webKitFeatures.width);
  features.widthSet = webKitFeatures.widthSet;
  features.height = static_cast<int>(webKitFeatures.height);
  features.heightSet =  webKitFeatures.heightSet;

  features.menuBarVisible =  webKitFeatures.menuBarVisible;
  features.statusBarVisible =  webKitFeatures.statusBarVisible;
  features.toolBarVisible =  webKitFeatures.toolBarVisible;
  features.locationBarVisible =  webKitFeatures.locationBarVisible;
  features.scrollbarsVisible =  webKitFeatures.scrollbarsVisible;
  features.resizable =  webKitFeatures.resizable;

  features.fullscreen =  webKitFeatures.fullscreen;
  features.dialog =  webKitFeatures.dialog;
  features.additionalFeatures = NULL;
  if (webKitFeatures.additionalFeatures.size() > 0)
     features.additionalFeatures = cef_string_list_alloc();

  CefString str;
  for (unsigned int i = 0; i < webKitFeatures.additionalFeatures.size(); ++i) {
    str = string16(webKitFeatures.additionalFeatures[i]);
    cef_string_list_append(features.additionalFeatures, str.GetStruct());
  }
}

}  // namespace


CefContentBrowserClient::CefContentBrowserClient()
    : browser_main_parts_(NULL),
      next_browser_id_(0),
      use_osr_next_contents_view_(false) {
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
      CefContentClient::Get()->browser());
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
  browser_info->add_render_id(render_process_id, render_view_id);
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

CefBrowserContext* CefContentBrowserClient::CreateBrowserContextProxy(
    CefRefPtr<CefRequestContextHandler> handler) {
  CEF_REQUIRE_UIT();
  CefBrowserContextProxy* context =
      new CefBrowserContextProxy(handler, browser_context());
  browser_main_parts_->AddBrowserContext(context);
  context->AddRef();
  return context;
}

void CefContentBrowserClient::AddBrowserContextReference(
    CefBrowserContext* context) {
  CEF_REQUIRE_UIT();
  // Skip the global browser context.
  if (context == browser_context())
    return;

  CefBrowserContextProxy* proxy = static_cast<CefBrowserContextProxy*>(context);
  proxy->AddRef();
}

void CefContentBrowserClient::RemoveBrowserContextReference(
    CefBrowserContext* context) {
  CEF_REQUIRE_UIT();
  // Skip the global browser context.
  if (context == browser_context())
    return;

  CefBrowserContextProxy* proxy = static_cast<CefBrowserContextProxy*>(context);
  if (proxy->Release())
    browser_main_parts_->RemoveBrowserContext(proxy);
}

content::BrowserMainParts* CefContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  browser_main_parts_ = new CefBrowserMainParts(parameters);
  return browser_main_parts_;
}

content::WebContentsViewPort*
CefContentBrowserClient::OverrideCreateWebContentsView(
    content::WebContents* web_contents,
    content::RenderViewHostDelegateView** render_view_host_delegate_view) {
  content::WebContentsViewPort* view = NULL;
  *render_view_host_delegate_view = NULL;

  if (use_osr_next_contents_view()) {
    CefWebContentsViewOSR* view_or = new CefWebContentsViewOSR(web_contents,
        GetWebContentsViewDelegate(web_contents));
    *render_view_host_delegate_view = view_or;
    view = view_or;
  }

  return view;
}

void CefContentBrowserClient::RenderProcessHostCreated(
    content::RenderProcessHost* host) {
  host->GetChannel()->AddFilter(new CefBrowserMessageFilter(host));
  host->AddFilter(new PrintingMessageFilter(host->GetID()));

  AddBrowserContextReference(
      static_cast<CefBrowserContext*>(host->GetBrowserContext()));
}

net::URLRequestContextGetter* CefContentBrowserClient::CreateRequestContext(
    content::BrowserContext* content_browser_context,
    content::ProtocolHandlerMap* protocol_handlers) {
  CefBrowserContext* cef_browser_context =
      static_cast<CefBrowserContext*>(content_browser_context);
  return cef_browser_context->CreateRequestContext(protocol_handlers);
}

net::URLRequestContextGetter*
CefContentBrowserClient::CreateRequestContextForStoragePartition(
    content::BrowserContext* content_browser_context,
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers) {
  CefBrowserContext* cef_browser_context =
      static_cast<CefBrowserContext*>(content_browser_context);
  return cef_browser_context->CreateRequestContextForStoragePartition(
      partition_path, in_memory, protocol_handlers);
}

bool CefContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  const std::string& scheme = url.scheme();
  DCHECK_EQ(scheme, StringToLowerASCII(scheme));

  if (scheme::IsInternalHandledScheme(scheme))
    return true;

  return CefContentClient::Get()->HasCustomScheme(scheme);
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

  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type == switches::kRendererProcess) {
    // Propagate the following switches to the renderer command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
      switches::kContextSafetyImplementation,
      switches::kEnableMediaStream,
      switches::kEnableSpeechInput,
      switches::kProductVersion,
      switches::kUncaughtExceptionStackSize,
    };
    command_line->CopySwitchesFrom(browser_cmd, kSwitchNames,
                                   arraysize(kSwitchNames));
  }

#if defined(OS_LINUX)
  if (process_type == switches::kZygoteProcess &&
      browser_cmd.HasSwitch(switches::kBrowserSubprocessPath)) {
    // Force use of the sub-process executable path for the zygote process.
    const base::FilePath& subprocess_path =
        browser_cmd.GetSwitchValuePath(switches::kBrowserSubprocessPath);
    if (!subprocess_path.empty())
      command_line->SetProgram(subprocess_path);
  }
#endif  // defined(OS_LINUX)

  CefRefPtr<CefApp> app = CefContentClient::Get()->application();
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
  return CefMediaCaptureDevicesDispatcher::GetInstance();
}

content::SpeechRecognitionManagerDelegate*
    CefContentBrowserClient::GetSpeechRecognitionManagerDelegate() {
#if defined(ENABLE_INPUT_SPEECH)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableSpeechInput))
    return new CefSpeechRecognitionManagerDelegate();
#endif

  return NULL;
}

void CefContentBrowserClient::AllowCertificateError(
    int render_process_id,
    int render_view_id,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    ResourceType::Type resource_type,
    bool overridable,
    bool strict_enforcement,
    const base::Callback<void(bool)>& callback,
    content::CertificateRequestResultType* result) {
  CEF_REQUIRE_UIT();

  if (resource_type != ResourceType::MAIN_FRAME) {
    // A sub-resource has a certificate error. The user doesn't really
    // have a context for making the right decision, so block the request
    // hard.
    *result = content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
    return;
  }

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserByRoutingID(render_process_id,
                                                render_view_id);
  if (!browser.get())
    return;
  CefRefPtr<CefClient> client = browser->GetClient();
  if (!client.get())
    return;
  CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
  if (!handler.get())
    return;

  CefRefPtr<CefAllowCertificateErrorCallbackImpl> callbackImpl;
  if (overridable && !strict_enforcement)
    callbackImpl = new CefAllowCertificateErrorCallbackImpl(callback);

  bool proceed = handler->OnCertificateError(
      static_cast<cef_errorcode_t>(cert_error), request_url.spec(),
      callbackImpl.get());
  if (!proceed && callbackImpl.get())
    callbackImpl->Disconnect();

  *result = proceed ? content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE :
                      content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
}

content::AccessTokenStore* CefContentBrowserClient::CreateAccessTokenStore() {
  return new CefAccessTokenStore;
}

bool CefContentBrowserClient::CanCreateWindow(
    const GURL& opener_url,
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

      CefPopupFeatures cef_features;
      TranslatePopupFeatures(features, cef_features);

#if (defined(OS_WIN) || defined(OS_MACOSX))
      // Default to the size from the popup features.
      if (cef_features.xSet)
        pending_info->window_info.x = cef_features.x;
      if (cef_features.ySet)
        pending_info->window_info.y = cef_features.y;
      if (cef_features.widthSet)
        pending_info->window_info.width = cef_features.width;
      if (cef_features.heightSet)
        pending_info->window_info.height = cef_features.height;
#endif

      allow = !handler->OnBeforePopup(browser.get(),
          frame,
          last_create_window_params_.target_url.spec(),
          last_create_window_params_.target_frame_name,
          cef_features,
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
    WebPreferences* prefs) {
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
    base::FilePath module;
    PathService::Get(base::FILE_MODULE, &module);
    const std::wstring wstr = module.value();
    size_t count = std::min(static_cast<size_t>(MAX_PATH), wstr.size());
    wcsncpy(file_path, wstr.c_str(), count);
    file_path[count] = 0;
  }

  return file_path;
}
#endif  // defined(OS_WIN)

void CefContentBrowserClient::RegisterCustomScheme(const std::string& scheme) {
  // Register as a Web-safe scheme so that requests for the scheme from a
  // render process will be allowed in resource_dispatcher_host_impl.cc
  // ShouldServiceRequest.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  if (!policy->IsWebSafeScheme(scheme))
    policy->RegisterWebSafeScheme(scheme);
}

void CefContentBrowserClient::set_last_create_window_params(
    const LastCreateWindowParams& params) {
  CEF_REQUIRE_IOT();
  last_create_window_params_ = params;
}

CefBrowserContext* CefContentBrowserClient::browser_context() const {
  return browser_main_parts_->browser_context();
}

scoped_refptr<net::URLRequestContextGetter>
CefContentBrowserClient::request_context() const {
  return browser_main_parts_->request_context();
}

CefDevToolsDelegate* CefContentBrowserClient::devtools_delegate() const {
  return browser_main_parts_->devtools_delegate();
}

PrefService* CefContentBrowserClient::pref_service() const {
  return browser_main_parts_->pref_service();
}

scoped_ptr<net::ProxyConfigService>
CefContentBrowserClient::proxy_config_service() const {
  return browser_main_parts_->proxy_config_service();
}
