// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/content_browser_client.h"

#include <algorithm>
#include <utility>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_main.h"
#include "libcef/browser/browser_message_filter.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools_manager_delegate.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/net/chrome_scheme_handler.h"
#include "libcef/browser/pepper/browser_pepper_host_factory.h"
#include "libcef/browser/plugins/plugin_info_message_filter.h"
#include "libcef/browser/plugins/plugin_service_filter.h"
#include "libcef/browser/prefs/renderer_prefs.h"
#include "libcef/browser/printing/printing_message_filter.h"
#include "libcef/browser/resource_dispatcher_host_delegate.h"
#include "libcef/browser/speech_recognition_manager_delegate.h"
#include "libcef/browser/ssl_info_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/x509_certificate_impl.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/command_line_impl.h"
#include "libcef/common/content_client.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/net/scheme_registration.h"
#include "libcef/common/request_impl.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "cef/grit/cef_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_message_filter.h"
#include "chrome/common/chrome_switches.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/plugin_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/quota_permission_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/storage_quota_params.h"
#include "content/public/common/web_preferences.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/extensions_guest_view_message_filter.h"
#include "extensions/browser/io_thread_extension_message_filter.h"
#include "extensions/common/constants.h"
#include "extensions/common/switches.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ppapi/host/ppapi_host.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"

#if defined(OS_LINUX)
#include "libcef/common/widevine_loader.h"
#endif

#if defined(OS_MACOSX)
#include "components/spellcheck/browser/spellcheck_message_filter_platform.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/debug/leak_annotations.h"
#include "chrome/common/chrome_paths.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "content/public/common/content_descriptors.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_policy.h"
#endif

namespace {

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
  typedef base::Callback<void(content::CertificateRequestResultType)>
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
    callback.Run(allow ? content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE :
                         content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefAllowCertificateErrorCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefAllowCertificateErrorCallbackImpl);
};

class CefSelectClientCertificateCallbackImpl :
    public CefSelectClientCertificateCallback {
 public:
  explicit CefSelectClientCertificateCallbackImpl(
      std::unique_ptr<content::ClientCertificateDelegate> delegate)
      : delegate_(std::move(delegate)) {
  }

  ~CefSelectClientCertificateCallbackImpl() {
    // If Select has not been called, call it with NULL to continue without any
    // client certificate.
    if (delegate_)
      DoSelect(NULL);
  }

  void Select(CefRefPtr<CefX509Certificate> cert) override {
    if (delegate_)
      DoSelect(cert);
  }

 private:
  void DoSelect(CefRefPtr<CefX509Certificate> cert) {
    if (CEF_CURRENTLY_ON_UIT()) {
      RunNow(std::move(delegate_), cert);
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefSelectClientCertificateCallbackImpl::RunNow,
                     base::Passed(std::move(delegate_)), cert));
    }
  }

  static void RunNow(
      std::unique_ptr<content::ClientCertificateDelegate> delegate,
      CefRefPtr<CefX509Certificate> cert) {
    CEF_REQUIRE_UIT();

    scoped_refptr<net::X509Certificate> x509cert = NULL;
    if (cert) {
      CefX509CertificateImpl* certImpl =
          static_cast<CefX509CertificateImpl*>(cert.get());
      x509cert = certImpl->GetInternalCertObject();
    }

    delegate->ContinueWithCertificate(x509cert.get());
  }

  std::unique_ptr<content::ClientCertificateDelegate> delegate_;

  IMPLEMENT_REFCOUNTING(CefSelectClientCertificateCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefSelectClientCertificateCallbackImpl);
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
        CefBrowserHostImpl::GetBrowserForFrame(render_process_id,
                                               params.render_frame_id);
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

#if defined(OS_POSIX) && !defined(OS_MACOSX)
breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
    const std::string& process_type) {
  base::FilePath dumps_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
  {
    ANNOTATE_SCOPED_MEMORY_LEAK;
    // Uploads will only occur if a non-empty crash URL is specified in
    // CefMainDelegate::InitCrashReporter.
    breakpad::CrashHandlerHostLinux* crash_handler =
        new breakpad::CrashHandlerHostLinux(
            process_type, dumps_path, true /* upload */);
    crash_handler->StartUploaderThread();
    return crash_handler;
  }
}

int GetCrashSignalFD(const base::CommandLine& command_line) {
  if (!breakpad::IsCrashReporterEnabled())
    return -1;

  // Extensions have the same process type as renderers.
  if (command_line.HasSwitch(extensions::switches::kExtensionProcess)) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost("extension");
    return crash_handler->GetDeathSignalSocket();
  }

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess) {
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

// TODO(cef): We can't currently trust NavigationParams::is_main_frame() because
// it's always set to true in
// InterceptNavigationThrottle::CheckIfShouldIgnoreNavigation. Remove the
// |is_main_frame| argument once this problem is fixed.
bool NavigationOnUIThread(
    bool is_main_frame,
    int64 frame_id,
    int64 parent_frame_id,
    content::WebContents* source,
    const navigation_interception::NavigationParams& params) {
  CEF_REQUIRE_UIT();

  bool ignore_navigation = false;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForContents(source);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
      if (handler.get()) {
        CefRefPtr<CefFrame> frame;
        if (is_main_frame) {
          frame = browser->GetMainFrame();
        } else if (frame_id >= 0) {
          frame = browser->GetFrame(frame_id);
          DCHECK(frame);
        } else {
          // Create a temporary frame object for navigation of sub-frames that
          // don't yet exist.
          frame = new CefFrameHostImpl(browser.get(),
                                       CefFrameHostImpl::kInvalidFrameId,
                                       false,
                                       CefString(),
                                       CefString(),
                                       parent_frame_id);
        }

        CefRefPtr<CefRequestImpl> request = new CefRequestImpl();
        request->Set(params, is_main_frame);
        request->SetReadOnly(true);

        ignore_navigation = handler->OnBeforeBrowse(
            browser.get(), frame, request.get(), params.is_redirect());
      }
    }
  }

  return ignore_navigation;
}

void FindFrameHostForNavigationHandle(
    content::NavigationHandle* navigation_handle,
    content::RenderFrameHost** matching_frame_host,
    content::RenderFrameHost* current_frame_host) {
  content::RenderFrameHostImpl* current_impl =
      static_cast<content::RenderFrameHostImpl*>(current_frame_host);
  if (current_impl->navigation_handle() == navigation_handle)
    *matching_frame_host = current_frame_host;
}

}  // namespace


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
      CefContentClient::Get()->browser());
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
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());

  host->GetChannel()->AddFilter(new CefBrowserMessageFilter(id));
  host->AddFilter(new printing::CefPrintingMessageFilter(id, profile));

  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) {
    host->AddFilter(new SpellCheckMessageFilter(id));
#if defined(OS_MACOSX)
    host->AddFilter(new SpellCheckMessageFilterPlatform(id));
#endif
  }

  host->AddFilter(new CefPluginInfoMessageFilter(id,
      static_cast<CefBrowserContext*>(profile)));

  if (extensions::ExtensionsEnabled()) {
    host->AddFilter(
        new extensions::ExtensionMessageFilter(id, profile));
    host->AddFilter(
        new extensions::IOThreadExtensionMessageFilter(id, profile));
    host->AddFilter(
        new extensions::ExtensionsGuestViewMessageFilter(id, profile));
  }

  // If the renderer process crashes then the host may already have
  // CefBrowserInfoManager as an observer. Try to remove it first before adding
  // to avoid DCHECKs.
  host->RemoveObserver(CefBrowserInfoManager::GetInstance());
  host->AddObserver(CefBrowserInfoManager::GetInstance());

  host->Send(new CefProcessMsg_SetIsIncognitoProcess(
      profile->IsOffTheRecord()));
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

bool CefContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  const std::string& scheme = url.scheme();
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));

  if (scheme::IsInternalHandledScheme(scheme))
    return true;

  return CefContentClient::Get()->HasCustomScheme(scheme);
}

void CefContentBrowserClient::SiteInstanceGotProcess(
    content::SiteInstance* site_instance) {
  if (!extensions::ExtensionsEnabled())
    return;

  // If this isn't an extension renderer there's nothing to do.
  const extensions::Extension* extension = GetExtension(site_instance);
  if (!extension)
    return;

  CefBrowserContext* browser_context =
      static_cast<CefBrowserContext*>(site_instance->GetBrowserContext());

  extensions::ProcessMap::Get(browser_context)
      ->Insert(extension->id(),
               site_instance->GetProcess()->GetID(),
               site_instance->GetId());

  CEF_POST_TASK(CEF_IOT,
      base::Bind(&extensions::InfoMap::RegisterExtensionProcess,
                 browser_context->extension_system()->info_map(),
                 extension->id(),
                 site_instance->GetProcess()->GetID(),
                 site_instance->GetId()));
}

void CefContentBrowserClient::SiteInstanceDeleting(
    content::SiteInstance* site_instance) {
  if (!extensions::ExtensionsEnabled())
    return;

  // May be NULL during shutdown.
  if (!extensions::ExtensionsBrowserClient::Get())
    return;

  // If this isn't an extension renderer there's nothing to do.
  const extensions::Extension* extension = GetExtension(site_instance);
  if (!extension)
    return;

  CefBrowserContext* browser_context =
      static_cast<CefBrowserContext*>(site_instance->GetBrowserContext());

  extensions::ProcessMap::Get(browser_context)
      ->Remove(extension->id(),
               site_instance->GetProcess()->GetID(),
               site_instance->GetId());

  CEF_POST_TASK(CEF_IOT,
      base::Bind(&extensions::InfoMap::UnregisterExtensionProcess,
                 browser_context->extension_system()->info_map(),
                 extension->id(),
                 site_instance->GetProcess()->GetID(),
                 site_instance->GetId()));
}

std::unique_ptr<base::Value>
CefContentBrowserClient::GetServiceManifestOverlay(base::StringPiece name) {
  int id = -1;
  if (name == content::mojom::kBrowserServiceName)
    id = IDR_CEF_BROWSER_MANIFEST_OVERLAY;
  else if (name == content::mojom::kRendererServiceName)
    id = IDR_CEF_RENDERER_MANIFEST_OVERLAY;
  else if (name == content::mojom::kUtilityServiceName)
    id = IDR_CEF_UTILITY_MANIFEST_OVERLAY;
  if (id == -1)
    return nullptr;

  base::StringPiece manifest_contents =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
          id, ui::ScaleFactor::SCALE_FACTOR_NONE);
  return base::JSONReader::Read(manifest_contents);
}

void CefContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line, int child_process_id) {
  const base::CommandLine* browser_cmd =
      base::CommandLine::ForCurrentProcess();

  {
    // Propagate the following switches to all command lines (along with any
    // associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
      switches::kDisablePackLoading,
#if defined(OS_MACOSX)
      switches::kFrameworkDirPath,
#endif
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
      switches::kEnableSystemFlash,
      switches::kPpapiFlashArgs,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
      switches::kUncaughtExceptionStackSize,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames,
                                   arraysize(kSwitchNames));

    if (extensions::ExtensionsEnabled()) {
      // Based on ChromeContentBrowserClientExtensionsPart::
      // AppendExtraRendererCommandLineSwitches
      content::RenderProcessHost* process =
          content::RenderProcessHost::FromID(child_process_id);
      content::BrowserContext* browser_context =
          process ? process->GetBrowserContext() : NULL;
      if (browser_context &&
          extensions::ProcessMap::Get(browser_context)->Contains(
              process->GetID())) {
        command_line->AppendSwitch(extensions::switches::kExtensionProcess);
      }
    }
  }

#if defined(OS_LINUX)
  if (process_type == switches::kZygoteProcess) {
    // Propagate the following switches to the zygote command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames,
                                   arraysize(kSwitchNames));

#if defined(WIDEVINE_CDM_AVAILABLE) && BUILDFLAG(ENABLE_PEPPER_CDMS)
    if (!browser_cmd->HasSwitch(switches::kNoSandbox)) {
      // Pass the Widevine CDM path to the Zygote process. See comments in
      // CefWidevineLoader::AddPepperPlugins.
      const base::FilePath& cdm_path = CefWidevineLoader::GetInstance()->path();
      if (!cdm_path.empty())
        command_line->AppendSwitchPath(switches::kWidevineCdmPath, cdm_path);
    }
#endif

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

void CefContentBrowserClient::GetQuotaSettings(
    content::BrowserContext* context,
    content::StoragePartition* partition,
    const storage::OptionalQuotaSettingsCallback& callback) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&storage::CalculateNominalDynamicSettings,
                 partition->GetPath(), context->IsOffTheRecord()),
      callback);
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
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    content::ResourceType resource_type,
    bool overridable,
    bool strict_enforcement,
    bool expired_previous_decision,
    const base::Callback<
        void(content::CertificateRequestResultType)>& callback) {
  CEF_REQUIRE_UIT();

  if (resource_type != content::ResourceType::RESOURCE_TYPE_MAIN_FRAME) {
    // A sub-resource has a certificate error. The user doesn't really
    // have a context for making the right decision, so block the request
    // hard.
    callback.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
    return;
  }

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForContents(web_contents);
  if (!browser.get())
    return;
  CefRefPtr<CefClient> client = browser->GetClient();
  if (!client.get())
    return;
  CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
  if (!handler.get())
    return;

  CefRefPtr<CefSSLInfo> cef_ssl_info = new CefSSLInfoImpl(ssl_info);

  CefRefPtr<CefAllowCertificateErrorCallbackImpl> callbackImpl(
      new CefAllowCertificateErrorCallbackImpl(callback));

  bool proceed = handler->OnCertificateError(
      browser.get(), static_cast<cef_errorcode_t>(cert_error),
      request_url.spec(), cef_ssl_info, callbackImpl.get());
  if (!proceed) {
    callbackImpl->Disconnect();
    callback.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
  }
}

void CefContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  CEF_REQUIRE_UIT();

  CefRefPtr<CefRequestHandler> handler;
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForContents(web_contents);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get())
      handler = client->GetRequestHandler();
  }

  if (!handler.get()) {
    delegate->ContinueWithCertificate(NULL);
    return;
  }

  CefRequestHandler::X509CertificateList certs;
  for (std::vector<scoped_refptr<net::X509Certificate> >::iterator iter =
           cert_request_info->client_certs.begin();
         iter != cert_request_info->client_certs.end(); iter++) {
    certs.push_back(new CefX509CertificateImpl(*iter));
  }

  CefRefPtr<CefSelectClientCertificateCallbackImpl> callbackImpl(
      new CefSelectClientCertificateCallbackImpl(std::move(delegate)));

  bool proceed = handler->OnSelectClientCertificate(
      browser.get(), cert_request_info->is_proxy,
      cert_request_info->host_and_port.host(),
      cert_request_info->host_and_port.port(),
      certs, callbackImpl.get());

  if (!proceed && !certs.empty()) {
    callbackImpl->Select(certs[0]);
  }
}

bool CefContentBrowserClient::CanCreateWindow(
    int opener_render_process_id,
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
    bool* no_javascript_access) {
  CEF_REQUIRE_IOT();
  *no_javascript_access = false;

  return CefBrowserInfoManager::GetInstance()->CanCreateWindow(
      target_url, referrer, frame_name, disposition, features, user_gesture,
      opener_suppressed, opener_render_process_id, opener_render_frame_id,
      no_javascript_access);
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
  // Using RVH instead of RFH here because rvh->GetMainFrame() may be nullptr
  // when this method is called.
  renderer_prefs::PopulateWebPreferences(rvh, *prefs);

  if (rvh->GetWidget()->GetView()) {
    rvh->GetWidget()->GetView()->SetBackgroundColor(
        prefs->base_background_color);
  }
}

void CefContentBrowserClient::BrowserURLHandlerCreated(
    content::BrowserURLHandler* handler) {
  scheme::BrowserURLHandlerCreated(handler);
}

std::string CefContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

void CefContentBrowserClient::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* browser_host) {
  browser_host->GetPpapiHost()->AddHostFactoryFilter(
      std::unique_ptr<ppapi::host::HostFactory>(
          new CefBrowserPepperHostFactory(browser_host)));
}

content::DevToolsManagerDelegate*
    CefContentBrowserClient::GetDevToolsManagerDelegate() {
  return new CefDevToolsManagerDelegate();
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
CefContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* navigation_handle) {
  CEF_REQUIRE_UIT();

  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;

  const bool is_main_frame = navigation_handle->IsInMainFrame();

  int64 parent_frame_id = CefFrameHostImpl::kUnspecifiedFrameId;
  if (!is_main_frame) {
    // Identify the RenderFrameHostImpl that originated the navigation.
    // TODO(cef): It would be better if NavigationHandle could directly report
    // the owner RenderFrameHostImpl.
    // There is additional complexity here if PlzNavigate is enabled. See
    // comments in content/browser/frame_host/navigation_handle_impl.h.
    content::WebContents* web_contents = navigation_handle->GetWebContents();
    content::RenderFrameHost* parent_frame_host = NULL;
    web_contents->ForEachFrame(
        base::Bind(FindFrameHostForNavigationHandle,
                   navigation_handle, &parent_frame_host));
    DCHECK(parent_frame_host);

    parent_frame_id = parent_frame_host->GetRoutingID();
    if (parent_frame_id < 0)
      parent_frame_id = CefFrameHostImpl::kUnspecifiedFrameId;
  }

  int64 frame_id = CefFrameHostImpl::kInvalidFrameId;
  if (!is_main_frame && navigation_handle->HasCommitted()) {
    frame_id = navigation_handle->GetRenderFrameHost()->GetRoutingID();
    if (frame_id < 0)
      frame_id = CefFrameHostImpl::kInvalidFrameId;
  }

  std::unique_ptr<content::NavigationThrottle> throttle =
      base::MakeUnique<navigation_interception::InterceptNavigationThrottle>(
          navigation_handle,
          base::Bind(&NavigationOnUIThread, is_main_frame, frame_id,
                     parent_frame_id),
          true);
  throttles.push_back(std::move(throttle));

  return throttles;
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

bool CefContentBrowserClient::PreSpawnRenderer(
    sandbox::TargetPolicy* policy) {
  return true;
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

CefBrowserContextImpl* CefContentBrowserClient::browser_context() const {
  return browser_main_parts_->browser_context();
}

CefDevToolsDelegate* CefContentBrowserClient::devtools_delegate() const {
  return browser_main_parts_->devtools_delegate();
}

const extensions::Extension* CefContentBrowserClient::GetExtension(
    content::SiteInstance* site_instance) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(site_instance->GetBrowserContext());
  return registry->enabled_extensions().GetExtensionOrAppByURL(
      site_instance->GetSiteURL());
}
