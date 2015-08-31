// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/content_browser_client.h"

#include <algorithm>

#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_main.h"
#include "libcef/browser/browser_message_filter.h"
#include "libcef/browser/browser_settings.h"
#include "libcef/browser/chrome_scheme_handler.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools_delegate.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/extensions/plugin_info_message_filter.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/pepper/browser_pepper_host_factory.h"
#include "libcef/browser/printing/printing_message_filter.h"
#include "libcef/browser/resource_dispatcher_host_delegate.h"
#include "libcef/browser/speech_recognition_manager_delegate.h"
#include "libcef/browser/ssl_info_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/web_plugin_impl.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/command_line_impl.h"
#include "libcef/common/content_client.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/scheme_registration.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/spellchecker/spellcheck_message_filter.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/plugin_service_impl.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/quota_permission_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/storage_quota_params.h"
#include "content/public/common/web_preferences.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/extensions_guest_view_message_filter.h"
#include "extensions/browser/io_thread_extension_message_filter.h"
#include "extensions/common/constants.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "chrome/browser/spellchecker/spellcheck_message_filter_platform.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/debug/leak_annotations.h"
#include "components/crash/app/breakpad_linux.h"
#include "components/crash/browser/crash_handler_host_linux.h"
#include "content/public/common/content_descriptors.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_policy.h"
#endif

namespace {

// In-memory store for access tokens used by geolocation.
class CefAccessTokenStore : public content::AccessTokenStore {
 public:
  // |system_context| is used by NetworkLocationProvider to communicate with a
  // remote geolocation service.
  explicit CefAccessTokenStore(net::URLRequestContextGetter* system_context)
      : system_context_(system_context) {}

  void LoadAccessTokens(
      const LoadAccessTokensCallbackType& callback) override {
    callback.Run(access_token_set_, system_context_);
  }

  void SaveAccessToken(
      const GURL& server_url, const base::string16& access_token) override {
    access_token_set_[server_url] = access_token;
  }

 private:
  net::URLRequestContextGetter* system_context_;
  AccessTokenSet access_token_set_;

  DISALLOW_COPY_AND_ASSIGN(CefAccessTokenStore);
};

class CefQuotaCallbackImpl : public CefRequestCallback {
 public:
  explicit CefQuotaCallbackImpl(
      const content::QuotaPermissionContext::PermissionCallback& callback)
      : callback_(callback) {
  }

  ~CefQuotaCallbackImpl() {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_IOT()) {
        RunNow(callback_, false);
      } else {
        CEF_POST_TASK(CEF_IOT,
            base::Bind(&CefQuotaCallbackImpl::RunNow, callback_, false));
      }
    }
  }

  void Continue(bool allow) override {
    if (CEF_CURRENTLY_ON_IOT()) {
      if (!callback_.is_null()) {
        RunNow(callback_, allow);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_IOT,
          base::Bind(&CefQuotaCallbackImpl::Continue, this, allow));
    }
  }

  void Cancel() override {
    Continue(false);
  }

  void Disconnect() {
    callback_.Reset();
  }

 private:
  static void RunNow(
      const content::QuotaPermissionContext::PermissionCallback& callback,
      bool allow) {
    CEF_REQUIRE_IOT();
    callback.Run(allow ?
          content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW :
          content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_DISALLOW);
  }

  content::QuotaPermissionContext::PermissionCallback callback_;

  IMPLEMENT_REFCOUNTING(CefQuotaCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefQuotaCallbackImpl);
};

class CefAllowCertificateErrorCallbackImpl : public CefRequestCallback {
 public:
  typedef base::Callback<void(bool)>  // NOLINT(readability/function)
      CallbackType;

  explicit CefAllowCertificateErrorCallbackImpl(const CallbackType& callback)
      : callback_(callback) {
  }

  ~CefAllowCertificateErrorCallbackImpl() {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        RunNow(callback_, false);
      } else {
        CEF_POST_TASK(CEF_UIT,
            base::Bind(&CefAllowCertificateErrorCallbackImpl::RunNow,
                       callback_, false));
      }
    }
  }

  void Continue(bool allow) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        RunNow(callback_, allow);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefAllowCertificateErrorCallbackImpl::Continue,
                     this, allow));
    }
  }

  void Cancel() override {
    Continue(false);
  }

  void Disconnect() {
    callback_.Reset();
  }

 private:
  static void RunNow(const CallbackType& callback, bool allow) {
    CEF_REQUIRE_UIT();
    callback.Run(allow);
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefAllowCertificateErrorCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefAllowCertificateErrorCallbackImpl);
};

class CefQuotaPermissionContext : public content::QuotaPermissionContext {
 public:
  CefQuotaPermissionContext() {
  }

  // The callback will be dispatched on the IO thread.
  void RequestQuotaPermission(
      const content::StorageQuotaParams& params,
      int render_process_id,
      const PermissionCallback& callback) override {
    if (params.storage_type != storage::kStorageTypePersistent) {
      // To match Chrome behavior we only support requesting quota with this
      // interface for Persistent storage type.
      callback.Run(QUOTA_PERMISSION_RESPONSE_DISALLOW);
      return;
    }

    bool handled = false;

    CefRefPtr<CefBrowserHostImpl> browser =
        CefBrowserHostImpl::GetBrowserForView(render_process_id,
                                              params.render_view_id);
    if (browser.get()) {
      CefRefPtr<CefClient> client = browser->GetClient();
      if (client.get()) {
        CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
        if (handler.get()) {
          CefRefPtr<CefQuotaCallbackImpl> callbackImpl(
              new CefQuotaCallbackImpl(callback));
          handled = handler->OnQuotaRequest(browser.get(),
                                            params.origin_url.spec(),
                                            params.requested_size,
                                            callbackImpl.get());
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
  ~CefQuotaPermissionContext() override {}

  DISALLOW_COPY_AND_ASSIGN(CefQuotaPermissionContext);
};

class CefPluginServiceFilter : public content::PluginServiceFilter {
 public:
  CefPluginServiceFilter() {}

  bool IsPluginAvailable(int render_process_id,
                         int render_frame_id,
                         const void* context,
                         const GURL& url,
                         const GURL& policy_url,
                         content::WebPluginInfo* plugin) override {
    bool allowed = true;

    CefRefPtr<CefBrowserHostImpl> browser =
        CefBrowserHostImpl::GetBrowserForFrame(render_process_id,
                                               render_frame_id);
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

  bool CanLoadPlugin(int render_process_id,
                     const base::FilePath& path) override {
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(CefPluginServiceFilter);
};

void TranslatePopupFeatures(const blink::WebWindowFeatures& webKitFeatures,
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
    str = base::string16(webKitFeatures.additionalFeatures[i]);
    cef_string_list_append(features.additionalFeatures, str.GetStruct());
  }
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
    const std::string& process_type) {
  base::FilePath dumps_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kCrashDumpsDir);
  {
    ANNOTATE_SCOPED_MEMORY_LEAK;
    breakpad::CrashHandlerHostLinux* crash_handler =
        new breakpad::CrashHandlerHostLinux(
            process_type, dumps_path, false);
    crash_handler->StartUploaderThread();
    return crash_handler;
  }
}

int GetCrashSignalFD(const base::CommandLine& command_line) {
  if (!breakpad::IsCrashReporterEnabled())
    return -1;

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kPpapiPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kGpuProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  return -1;
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

}  // namespace


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
      CefContentClient::Get()->browser());
}

scoped_refptr<CefBrowserInfo> CefContentBrowserClient::CreateBrowserInfo(
    bool is_popup) {
  base::AutoLock lock_scope(browser_info_lock_);

  scoped_refptr<CefBrowserInfo> browser_info =
      new CefBrowserInfo(++next_browser_id_, is_popup);
  browser_info_list_.push_back(browser_info);
  return browser_info;
}

scoped_refptr<CefBrowserInfo>
    CefContentBrowserClient::GetOrCreateBrowserInfo(
        int render_view_process_id,
        int render_view_routing_id,
        int render_frame_process_id,
        int render_frame_routing_id) {
  base::AutoLock lock_scope(browser_info_lock_);

  BrowserInfoList::const_iterator it = browser_info_list_.begin();
  for (; it != browser_info_list_.end(); ++it) {
    const scoped_refptr<CefBrowserInfo>& browser_info = *it;
    if (browser_info->is_render_view_id_match(render_view_process_id,
                                              render_view_routing_id)) {
      // Make sure the frame id is also registered.
      browser_info->add_render_frame_id(render_frame_process_id,
                                        render_frame_routing_id);
      return browser_info;
    } else if (browser_info->is_render_frame_id_match(
                  render_frame_process_id,
                  render_frame_routing_id)) {
      // Make sure the view id is also registered.
      browser_info->add_render_view_id(render_view_process_id,
                                       render_view_routing_id);
      return browser_info;
    }
  }

  // Must be a popup if it hasn't already been created.
  scoped_refptr<CefBrowserInfo> browser_info =
      new CefBrowserInfo(++next_browser_id_, true);
  browser_info->add_render_view_id(render_view_process_id,
                                   render_view_routing_id);
  browser_info->add_render_frame_id(render_frame_process_id,
                                    render_frame_routing_id);
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

scoped_refptr<CefBrowserInfo> CefContentBrowserClient::GetBrowserInfoForView(
    int render_process_id, int render_routing_id) {
  base::AutoLock lock_scope(browser_info_lock_);

  BrowserInfoList::const_iterator it = browser_info_list_.begin();
  for (; it != browser_info_list_.end(); ++it) {
    const scoped_refptr<CefBrowserInfo>& browser_info = *it;
    if (browser_info->is_render_view_id_match(
          render_process_id, render_routing_id)) {
      return browser_info;
    }
  }

  LOG(WARNING) << "No browser info matching view process id " <<
                  render_process_id << " and routing id " << render_routing_id;

  return scoped_refptr<CefBrowserInfo>();
}

scoped_refptr<CefBrowserInfo> CefContentBrowserClient::GetBrowserInfoForFrame(
    int render_process_id, int render_routing_id) {
  base::AutoLock lock_scope(browser_info_lock_);

  BrowserInfoList::const_iterator it = browser_info_list_.begin();
  for (; it != browser_info_list_.end(); ++it) {
    const scoped_refptr<CefBrowserInfo>& browser_info = *it;
    if (browser_info->is_render_frame_id_match(
          render_process_id, render_routing_id)) {
      return browser_info;
    }
  }

  LOG(WARNING) << "No browser info matching frame process id " <<
                  render_process_id << " and routing id " << render_routing_id;

  return scoped_refptr<CefBrowserInfo>();
}

content::BrowserMainParts* CefContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  browser_main_parts_ = new CefBrowserMainParts(parameters);
  return browser_main_parts_;
}

void CefContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const int id = host->GetID();

  host->GetChannel()->AddFilter(new CefBrowserMessageFilter(host));
  host->AddFilter(new printing::PrintingMessageFilter(id));

  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) {
    host->AddFilter(new SpellCheckMessageFilter(id));
#if defined(OS_MACOSX)
    host->AddFilter(new SpellCheckMessageFilterPlatform(id));
#endif
  }

  content::BrowserContext* browser_context = host->GetBrowserContext();

  if (extensions::ExtensionsEnabled()) {
    host->AddFilter(
        new extensions::CefPluginInfoMessageFilter(id, browser_context));
    host->AddFilter(
        new extensions::ExtensionMessageFilter(id, browser_context));
    host->AddFilter(
        new extensions::IOThreadExtensionMessageFilter(id, browser_context));
    host->AddFilter(
        new extensions::ExtensionsGuestViewMessageFilter(id, browser_context));
  }

  host->Send(new CefProcessMsg_SetIsIncognitoProcess(
      browser_context->IsOffTheRecord()));
}

bool CefContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context,
    const GURL& effective_url) {
  if (!extensions::ExtensionsEnabled())
    return false;

  if (!effective_url.SchemeIs(extensions::kExtensionScheme))
    return false;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context);
  if (!registry)
    return false;

  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(effective_url.host());
  if (!extension)
    return false;

  // TODO(extensions): Extra checks required if type is TYPE_HOSTED_APP.

  // Hosted apps that have script access to their background page must use
  // process per site, since all instances can make synchronous calls to the
  // background window.  Other extensions should use process per site as well.
  return true;
}

net::URLRequestContextGetter* CefContentBrowserClient::CreateRequestContext(
    content::BrowserContext* content_browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  scoped_refptr<CefBrowserContext> context =
      static_cast<CefBrowserContext*>(content_browser_context);

  if (extensions::ExtensionsEnabled()) {
    // Handle only chrome-extension:// requests. CEF does not support
    // chrome-extension-resource:// requests (it does not store shared extension
    // data in its installation directory).
    extensions::InfoMap* extension_info_map =
        context->extension_system()->info_map();
    (*protocol_handlers)[extensions::kExtensionScheme] =
        linked_ptr<net::URLRequestJobFactory::ProtocolHandler>(
            extensions::CreateExtensionProtocolHandler(
                context->IsOffTheRecord(), extension_info_map).release());
  }

  return context->CreateRequestContext(
      protocol_handlers,
      request_interceptors.Pass());
}

net::URLRequestContextGetter*
CefContentBrowserClient::CreateRequestContextForStoragePartition(
    content::BrowserContext* content_browser_context,
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  scoped_refptr<CefBrowserContext> context =
      static_cast<CefBrowserContext*>(content_browser_context);
  return context->CreateRequestContextForStoragePartition(
      partition_path,
      in_memory,
      protocol_handlers,
      request_interceptors.Pass());
}

bool CefContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  const std::string& scheme = url.scheme();
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));

  if (scheme::IsInternalHandledScheme(scheme))
    return true;

  return CefContentClient::Get()->HasCustomScheme(scheme);
}

bool CefContentBrowserClient::IsNPAPIEnabled() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableNPAPI);
#else
  return false;
#endif
}

void CefContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line, int child_process_id) {
  const base::CommandLine* browser_cmd =
      base::CommandLine::ForCurrentProcess();

  {
    // Propagate the following switches to all command lines (along with any
    // associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
#if !defined(OS_WIN)
      switches::kCrashDumpsDir,
#endif
      switches::kDisablePackLoading,
      switches::kEnableCrashReporter,
      switches::kLang,
      switches::kLocalesDirPath,
      switches::kLogFile,
      switches::kLogSeverity,
      switches::kProductVersion,
      switches::kResourcesDirPath,
      switches::kUserAgent,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames,
                                   arraysize(kSwitchNames));
  }

  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type == switches::kRendererProcess) {
    // Propagate the following switches to the renderer command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
      switches::kContextSafetyImplementation,
      switches::kDisableExtensions,
      switches::kDisablePdfExtension,
      switches::kDisableScrollBounce,
      switches::kDisableSpellChecking,
      switches::kEnableSpeechInput,
      switches::kEnableSpellingAutoCorrect,
      switches::kEnableSystemFlash,
      switches::kPpapiFlashArgs,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
      switches::kUncaughtExceptionStackSize,
      switches::kWidevineCdmPath,
      switches::kWidevineCdmVersion,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames,
                                   arraysize(kSwitchNames));
  }

#if defined(OS_LINUX)
  if (process_type == switches::kZygoteProcess) {
    // Propagate the following switches to the zygone command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
      switches::kWidevineCdmPath,
      switches::kWidevineCdmVersion,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames,
                                   arraysize(kSwitchNames));

    if (browser_cmd->HasSwitch(switches::kBrowserSubprocessPath)) {
      // Force use of the sub-process executable path for the zygote process.
      const base::FilePath& subprocess_path =
          browser_cmd->GetSwitchValuePath(switches::kBrowserSubprocessPath);
      if (!subprocess_path.empty())
        command_line->SetProgram(subprocess_path);
    }
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
    CefContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSpeechInput))
    return new CefSpeechRecognitionManagerDelegate();

  return NULL;
}

void CefContentBrowserClient::AllowCertificateError(
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
    content::CertificateRequestResultType* result) {
  CEF_REQUIRE_UIT();

  if (resource_type != content::ResourceType::RESOURCE_TYPE_MAIN_FRAME) {
    // A sub-resource has a certificate error. The user doesn't really
    // have a context for making the right decision, so block the request
    // hard.
    *result = content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
    return;
  }

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForFrame(render_process_id,
                                             render_frame_id);
  if (!browser.get())
    return;
  CefRefPtr<CefClient> client = browser->GetClient();
  if (!client.get())
    return;
  CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
  if (!handler.get())
    return;

  CefRefPtr<CefSSLInfo> cef_ssl_info = new CefSSLInfoImpl(ssl_info);

  CefRefPtr<CefAllowCertificateErrorCallbackImpl> callbackImpl;
  if (overridable && !strict_enforcement)
    callbackImpl = new CefAllowCertificateErrorCallbackImpl(callback);

  bool proceed = handler->OnCertificateError(
      browser.get(), static_cast<cef_errorcode_t>(cert_error),
      request_url.spec(), cef_ssl_info, callbackImpl.get());
  if (!proceed && callbackImpl.get())
    callbackImpl->Disconnect();

  *result = proceed ? content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE :
                      content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
}

void CefContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    scoped_ptr<content::ClientCertificateDelegate> delegate) {
  if (!cert_request_info->client_certs.empty()) {
    // Use the first certificate.
    delegate->ContinueWithCertificate(cert_request_info->client_certs[0].get());
  }
}

content::AccessTokenStore* CefContentBrowserClient::CreateAccessTokenStore() {
  return new CefAccessTokenStore(
      browser_main_parts_->browser_context()->request_context().get());
}

bool CefContentBrowserClient::CanCreateWindow(
    const GURL& opener_url,
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
    int opener_render_view_id,
    int opener_render_frame_id,
    bool* no_javascript_access) {
  CEF_REQUIRE_IOT();
  *no_javascript_access = false;

  DCHECK_NE(last_create_window_params_.opener_process_id, MSG_ROUTING_NONE);
  if (last_create_window_params_.opener_process_id == MSG_ROUTING_NONE)
    return false;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForView(
          last_create_window_params_.opener_process_id,
          last_create_window_params_.opener_view_id);
  if (!browser.get())
    return false;

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
          static_cast<cef_window_open_disposition_t>(disposition),
          user_gesture,
          cef_features,
          pending_info->window_info,
          pending_info->client,
          pending_info->settings,
          no_javascript_access);
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
    content::WebPreferences* prefs) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForHost(rvh);
  if (!browser.get() && extensions::ExtensionsEnabled()) {
    // Retrieve the owner browser, if any.
    content::WebContents* owner = extensions::GetOwnerForGuestContents(
        content::WebContents::FromRenderViewHost(rvh));
    if (owner)
      browser = CefBrowserHostImpl::GetBrowserForContents(owner);
  }

  if (browser.get()) {
    // Populate WebPreferences based on CefBrowserSettings.
    BrowserToWebSettings(browser->settings(), *prefs);
  }

  prefs->base_background_color = GetBaseBackgroundColor(browser);
  if (rvh->GetView())
    rvh->GetView()->SetBackgroundColor(prefs->base_background_color);

  prefs->asynchronous_spell_checking_enabled = true;
  // Auto-correct does not work in combination with the unified text checker.
  prefs->unified_textchecker_enabled =
      !command_line->HasSwitch(switches::kEnableSpellingAutoCorrect);
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

void CefContentBrowserClient::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* browser_host) {
  browser_host->GetPpapiHost()->AddHostFactoryFilter(
      scoped_ptr<ppapi::host::HostFactory>(
          new CefBrowserPepperHostFactory(browser_host)));
}

content::DevToolsManagerDelegate*
    CefContentBrowserClient::GetDevToolsManagerDelegate() {
  return new CefDevToolsManagerDelegate();
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
void CefContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::FileDescriptorInfo* mappings) {
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)


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

void CefContentBrowserClient::PreSpawnRenderer(
    sandbox::TargetPolicy* policy,
    bool* success) {
  // Flash requires this permission to play video files.
  sandbox::ResultCode result = policy->AddRule(
      sandbox::TargetPolicy::SUBSYS_HANDLES,
      sandbox::TargetPolicy::HANDLES_DUP_ANY,
      L"File");
  if (result != sandbox::SBOX_ALL_OK) {
    *success = false;
    return;
  }
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

scoped_refptr<CefBrowserContextImpl>
CefContentBrowserClient::browser_context() const {
  return browser_main_parts_->browser_context();
}

CefDevToolsDelegate* CefContentBrowserClient::devtools_delegate() const {
  return browser_main_parts_->devtools_delegate();
}

PrefService* CefContentBrowserClient::pref_service() const {
  return browser_main_parts_->pref_service();
}

// static
SkColor CefContentBrowserClient::GetBaseBackgroundColor(
    CefRefPtr<CefBrowserHostImpl> browser) {
  if (browser.get()) {
    const CefBrowserSettings& browser_settings = browser->settings();
    if (CefColorGetA(browser_settings.background_color) > 0) {
      return SkColorSetRGB(
          CefColorGetR(browser_settings.background_color),
          CefColorGetG(browser_settings.background_color),
          CefColorGetB(browser_settings.background_color));
    }
  }

  const CefSettings& settings = CefContext::Get()->settings();
  if (CefColorGetA(settings.background_color) > 0) {
    return SkColorSetRGB(
        CefColorGetR(settings.background_color),
        CefColorGetG(settings.background_color),
        CefColorGetB(settings.background_color));
  }

  return SK_ColorWHITE;
}
