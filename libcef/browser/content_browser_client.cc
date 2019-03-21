// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/content_browser_client.h"

#include <algorithm>
#include <utility>

#include "include/cef_version.h"
#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_main.h"
#include "libcef/browser/browser_message_filter.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools/devtools_manager_delegate.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/net/chrome_scheme_handler.h"
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
#include "libcef/common/net_service/util.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/service_manifests/cef_content_browser_overlay_manifest.h"
#include "libcef/common/service_manifests/cef_content_gpu_overlay_manifest.h"
#include "libcef/common/service_manifests/cef_content_renderer_overlay_manifest.h"
#include "libcef/common/service_manifests/cef_content_utility_overlay_manifest.h"
#include "libcef/common/service_manifests/cef_packaged_service_manifests.h"
#include "libcef/common/service_manifests/cef_renderer_manifest.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "cef/grit/cef_resources.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_service.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/plugins/plugin_info_host_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/constants.mojom.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/printing/public/mojom/constants.mojom.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "components/services/pdf_compositor/public/interfaces/pdf_compositor.mojom.h"
#include "components/version_info/version_info.h"
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
#include "content/public/common/user_agent.h"
#include "content/public/common/web_preferences.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/extensions_guest_view_message_filter.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/io_thread_extension_message_filter.h"
#include "extensions/common/constants.h"
#include "extensions/common/switches.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ppapi/host/ppapi_host.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/sandbox/switches.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"

#if defined(OS_LINUX)
#include "libcef/common/widevine_loader.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/debug/leak_annotations.h"
#include "chrome/common/chrome_paths.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/content/browser/crash_handler_host_linux.h"
#include "content/public/common/content_descriptors.h"
#endif

#if defined(OS_MACOSX)
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_policy.h"
#endif

namespace {

class CefQuotaCallbackImpl : public CefRequestCallback {
 public:
  explicit CefQuotaCallbackImpl(
      const content::QuotaPermissionContext::PermissionCallback& callback)
      : callback_(callback) {}

  ~CefQuotaCallbackImpl() {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_IOT()) {
        RunNow(callback_, false);
      } else {
        CEF_POST_TASK(CEF_IOT, base::Bind(&CefQuotaCallbackImpl::RunNow,
                                          callback_, false));
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

  void Cancel() override { Continue(false); }

  void Disconnect() { callback_.Reset(); }

 private:
  static void RunNow(
      const content::QuotaPermissionContext::PermissionCallback& callback,
      bool allow) {
    CEF_REQUIRE_IOT();
    callback.Run(
        allow ? content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW
              : content::QuotaPermissionContext::
                    QUOTA_PERMISSION_RESPONSE_DISALLOW);
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
      : callback_(callback) {}

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

  void Cancel() override { Continue(false); }

  void Disconnect() { callback_.Reset(); }

 private:
  static void RunNow(const CallbackType& callback, bool allow) {
    CEF_REQUIRE_UIT();
    callback.Run(allow ? content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE
                       : content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
  }

  CallbackType callback_;

  IMPLEMENT_REFCOUNTING(CefAllowCertificateErrorCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefAllowCertificateErrorCallbackImpl);
};

class CefSelectClientCertificateCallbackImpl
    : public CefSelectClientCertificateCallback {
 public:
  explicit CefSelectClientCertificateCallbackImpl(
      std::unique_ptr<content::ClientCertificateDelegate> delegate)
      : delegate_(std::move(delegate)) {}

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

    if (cert) {
      CefX509CertificateImpl* certImpl =
          static_cast<CefX509CertificateImpl*>(cert.get());
      certImpl->AcquirePrivateKey(
          base::Bind(&CefSelectClientCertificateCallbackImpl::RunWithPrivateKey,
                     base::Passed(std::move(delegate)), cert));
      return;
    }

    delegate->ContinueWithCertificate(nullptr, nullptr);
  }

  static void RunWithPrivateKey(
      std::unique_ptr<content::ClientCertificateDelegate> delegate,
      CefRefPtr<CefX509Certificate> cert,
      scoped_refptr<net::SSLPrivateKey> key) {
    CEF_REQUIRE_UIT();
    DCHECK(cert);

    if (key) {
      CefX509CertificateImpl* certImpl =
          static_cast<CefX509CertificateImpl*>(cert.get());
      delegate->ContinueWithCertificate(certImpl->GetInternalCertObject(), key);
    } else {
      delegate->ContinueWithCertificate(nullptr, nullptr);
    }
  }

  std::unique_ptr<content::ClientCertificateDelegate> delegate_;

  IMPLEMENT_REFCOUNTING(CefSelectClientCertificateCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefSelectClientCertificateCallbackImpl);
};

class CefQuotaPermissionContext : public content::QuotaPermissionContext {
 public:
  CefQuotaPermissionContext() {}

  // The callback will be dispatched on the IO thread.
  void RequestQuotaPermission(const content::StorageQuotaParams& params,
                              int render_process_id,
                              const PermissionCallback& callback) override {
    if (params.storage_type != blink::mojom::StorageType::kPersistent) {
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
          handled = handler->OnQuotaRequest(
              browser.get(), params.origin_url.spec(), params.requested_size,
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
  base::PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
  {
    ANNOTATE_SCOPED_MEMORY_LEAK;
    // Uploads will only occur if a non-empty crash URL is specified in
    // CefMainDelegate::InitCrashReporter.
    breakpad::CrashHandlerHostLinux* crash_handler =
        new breakpad::CrashHandlerHostLinux(process_type, dumps_path,
                                            true /* upload */);
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
          frame = new CefFrameHostImpl(
              browser.get(), CefFrameHostImpl::kInvalidFrameId, false,
              CefString(), CefString(), parent_frame_id);
        }

        CefRefPtr<CefRequestImpl> request = new CefRequestImpl();
        request->Set(params, is_main_frame);
        request->SetReadOnly(true);

        // Initiating a new navigation in OnBeforeBrowse will delete the
        // InterceptNavigationThrottle that currently owns this callback,
        // resulting in a crash. Use the lock to prevent that.
        std::unique_ptr<CefBrowserHostImpl::NavigationLock> navigation_lock =
            browser->CreateNavigationLock();
        ignore_navigation = handler->OnBeforeBrowse(
            browser.get(), frame, request.get(), params.has_user_gesture(),
            params.is_redirect());
      }
    }
  }

  return ignore_navigation;
}

}  // namespace

CefContentBrowserClient::CefContentBrowserClient() : browser_main_parts_(NULL) {
  plugin_service_filter_.reset(new CefPluginServiceFilter);
  content::PluginServiceImpl::GetInstance()->SetFilter(
      plugin_service_filter_.get());
}

CefContentBrowserClient::~CefContentBrowserClient() {}

// static
CefContentBrowserClient* CefContentBrowserClient::Get() {
  if (!CefContentClient::Get())
    return nullptr;
  return static_cast<CefContentBrowserClient*>(
      CefContentClient::Get()->browser());
}

content::BrowserMainParts* CefContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  browser_main_parts_ = new CefBrowserMainParts(parameters);
  browser_main_parts_->AddParts(
      ChromeService::GetInstance()->CreateExtraParts());
  return browser_main_parts_;
}

void CefContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host,
    service_manager::mojom::ServiceRequest* service_request) {
  const int id = host->GetID();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());

  host->AddFilter(new CefBrowserMessageFilter(id));
  host->AddFilter(new printing::CefPrintingMessageFilter(id, profile));

  if (extensions::ExtensionsEnabled()) {
    host->AddFilter(new extensions::ExtensionMessageFilter(id, profile));
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

  host->Send(
      new CefProcessMsg_SetIsIncognitoProcess(profile->IsOffTheRecord()));

  service_manager::mojom::ServicePtr service;
  *service_request = mojo::MakeRequest(&service);
  service_manager::mojom::PIDReceiverPtr pid_receiver;
  service_manager::Identity renderer_identity = host->GetChildIdentity();
  ChromeService::GetInstance()->connector()->RegisterServiceInstance(
      service_manager::Identity(chrome::mojom::kRendererServiceName,
                                renderer_identity.instance_group(),
                                renderer_identity.instance_id(),
                                base::Token::CreateRandom()),
      std::move(service), mojo::MakeRequest(&pid_receiver));
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

// Based on
// ChromeContentBrowserClientExtensionsPart::DoesSiteRequireDedicatedProcess.
bool CefContentBrowserClient::DoesSiteRequireDedicatedProcess(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
  if (!extensions::ExtensionsEnabled())
    return false;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context);
  const extensions::Extension* extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(effective_site_url);
  if (!extension)
    return false;

  // Always isolate Chrome Web Store.
  if (extension->id() == extensions::kWebStoreAppId)
    return true;

  // Extensions should be isolated, except for hosted apps. Isolating hosted
  // apps is a good idea, but ought to be a separate knob.
  if (extension->is_hosted_app())
    return false;

  // Isolate all extensions.
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
      ->Insert(extension->id(), site_instance->GetProcess()->GetID(),
               site_instance->GetId());

  CEF_POST_TASK(
      CEF_IOT, base::Bind(&extensions::InfoMap::RegisterExtensionProcess,
                          browser_context->extension_system()->info_map(),
                          extension->id(), site_instance->GetProcess()->GetID(),
                          site_instance->GetId()));
}

void CefContentBrowserClient::SiteInstanceDeleting(
    content::SiteInstance* site_instance) {
  if (!extensions::ExtensionsEnabled())
    return;

  // May be NULL during shutdown.
  if (!extensions::ExtensionsBrowserClient::Get())
    return;

  // May be NULL during shutdown.
  if (!site_instance->HasProcess())
    return;

  // If this isn't an extension renderer there's nothing to do.
  const extensions::Extension* extension = GetExtension(site_instance);
  if (!extension)
    return;

  CefBrowserContext* browser_context =
      static_cast<CefBrowserContext*>(site_instance->GetBrowserContext());

  extensions::ProcessMap::Get(browser_context)
      ->Remove(extension->id(), site_instance->GetProcess()->GetID(),
               site_instance->GetId());

  CEF_POST_TASK(
      CEF_IOT, base::Bind(&extensions::InfoMap::UnregisterExtensionProcess,
                          browser_context->extension_system()->info_map(),
                          extension->id(), site_instance->GetProcess()->GetID(),
                          site_instance->GetId()));
}

void CefContentBrowserClient::RegisterIOThreadServiceHandlers(
    content::ServiceManagerConnection* connection) {
  // For spell checking.
  connection->AddServiceRequestHandler(
      chrome::mojom::kServiceName,
      ChromeService::GetInstance()->CreateChromeServiceRequestHandler());
}

void CefContentBrowserClient::RegisterOutOfProcessServices(
    OutOfProcessServiceMap* services) {
  (*services)[printing::mojom::kServiceName] =
      base::BindRepeating(&base::ASCIIToUTF16, "PDF Compositor Service");
  (*services)[printing::mojom::kChromePrintingServiceName] =
      base::BindRepeating(&base::ASCIIToUTF16, "Printing Service");
  (*services)[proxy_resolver::mojom::kProxyResolverServiceName] =
      base::BindRepeating(&l10n_util::GetStringUTF16,
                          IDS_UTILITY_PROCESS_PROXY_RESOLVER_NAME);
}

base::Optional<service_manager::Manifest>
CefContentBrowserClient::GetServiceManifestOverlay(base::StringPiece name) {
  if (name == content::mojom::kBrowserServiceName) {
    return GetCefContentBrowserOverlayManifest();
  } else if (name == content::mojom::kGpuServiceName) {
    return GetCefContentGpuOverlayManifest();
  } else if (name == content::mojom::kPackagedServicesServiceName) {
    service_manager::Manifest overlay;
    overlay.packaged_services = GetCefPackagedServiceManifests();
    return overlay;
  } else if (name == content::mojom::kRendererServiceName) {
    return GetCefContentRendererOverlayManifest();
  } else if (name == content::mojom::kUtilityServiceName) {
    return GetCefContentUtilityOverlayManifest();
  }

  return base::nullopt;
}

std::vector<service_manager::Manifest>
CefContentBrowserClient::GetExtraServiceManifests() {
  return std::vector<service_manager::Manifest>{GetCefRendererManifest()};
}

bool CefContentBrowserClient::IsSameBrowserContext(
    content::BrowserContext* context1,
    content::BrowserContext* context2) {
  return CefBrowserContextImpl::GetForContext(context1) ==
         CefBrowserContextImpl::GetForContext(context2);
}

void CefContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  const base::CommandLine* browser_cmd = base::CommandLine::ForCurrentProcess();

  {
    // Propagate the following switches to all command lines (along with any
    // associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
      switches::kDisablePackLoading,
#if defined(OS_MACOSX)
      switches::kFrameworkDirPath,
#endif
      switches::kLocalesDirPath,
      switches::kLogFile,
      switches::kLogSeverity,
      switches::kProductVersion,
      switches::kResourcesDirPath,
      switches::kUserAgent,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames,
                                   base::size(kSwitchNames));
  }

  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type == switches::kRendererProcess) {
    // Propagate the following switches to the renderer command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
        switches::kDisableExtensions,
        switches::kDisablePdfExtension,
        switches::kDisablePlugins,
        switches::kDisableScrollBounce,
        switches::kDisableSpellChecking,
        switches::kEnableSpeechInput,
        switches::kEnableSystemFlash,
        switches::kPpapiFlashArgs,
        switches::kPpapiFlashPath,
        switches::kPpapiFlashVersion,
        switches::kUncaughtExceptionStackSize,
        switches::kUnsafelyTreatInsecureOriginAsSecure,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames,
                                   base::size(kSwitchNames));

    if (extensions::ExtensionsEnabled()) {
      // Based on ChromeContentBrowserClientExtensionsPart::
      // AppendExtraRendererCommandLineSwitches
      content::RenderProcessHost* process =
          content::RenderProcessHost::FromID(child_process_id);
      content::BrowserContext* browser_context =
          process ? process->GetBrowserContext() : NULL;
      if (browser_context && extensions::ProcessMap::Get(browser_context)
                                 ->Contains(process->GetID())) {
        command_line->AppendSwitch(extensions::switches::kExtensionProcess);
      }
    }
  } else {
    // Propagate the following switches to non-renderer command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
        switches::kLang,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames,
                                   base::size(kSwitchNames));
  }

#if defined(OS_LINUX)
  if (process_type == service_manager::switches::kZygoteProcess) {
    // Propagate the following switches to the zygote command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
        switches::kPpapiFlashPath,
        switches::kPpapiFlashVersion,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames,
                                   base::size(kSwitchNames));

#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_LIBRARY_CDMS)
    if (!browser_cmd->HasSwitch(service_manager::switches::kNoSandbox)) {
      // Pass the Widevine CDM path to the Zygote process. See comments in
      // CefWidevineLoader::AddContentDecryptionModules.
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

void CefContentBrowserClient::AdjustUtilityServiceProcessCommandLine(
    const service_manager::Identity& identity,
    base::CommandLine* command_line) {
#if defined(OS_MACOSX)
  // On Mac, the video-capture and audio services require a CFRunLoop, provided
  // by a UI message loop, to run AVFoundation and CoreAudio code.
  // See https://crbug.com/834581
  if (identity.name() == video_capture::mojom::kServiceName ||
      identity.name() == audio::mojom::kServiceName)
    command_line->AppendSwitch(switches::kMessageLoopTypeUi);
#endif
}

bool CefContentBrowserClient::ShouldEnableStrictSiteIsolation() {
  // TODO(cef): Enable this mode once we figure out why it breaks ceftests that
  // rely on command-line arguments passed to the renderer process. It looks
  // like the first renderer process is getting all of the callbacks despite
  // multiple renderer processes being launched.
  // For example, V8RendererTest::OnBrowserCreated appears to get the same
  // kV8TestCmdArg value twice when running with:
  // --gtest_filter=V8Test.ContextEvalCspBypassUnsafeEval:V8Test.ContextEntered
  return false;
}

std::string CefContentBrowserClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

scoped_refptr<network::SharedURLLoaderFactory>
CefContentBrowserClient::GetSystemSharedURLLoaderFactory() {
  DCHECK(
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) ||
      !content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));

  if (!SystemNetworkContextManager::GetInstance())
    return nullptr;

  return SystemNetworkContextManager::GetInstance()
      ->GetSharedURLLoaderFactory();
}

network::mojom::NetworkContext*
CefContentBrowserClient::GetSystemNetworkContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(SystemNetworkContextManager::GetInstance());
  return SystemNetworkContextManager::GetInstance()->GetContext();
}

content::QuotaPermissionContext*
CefContentBrowserClient::CreateQuotaPermissionContext() {
  return new CefQuotaPermissionContext();
}

void CefContentBrowserClient::GetQuotaSettings(
    content::BrowserContext* context,
    content::StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  const base::FilePath& cache_path = partition->GetPath();
  storage::GetNominalDynamicSettings(
      cache_path, cache_path.empty() /* is_incognito */,
      storage::GetDefaultDiskInfoHelper(), std::move(callback));
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

content::GeneratedCodeCacheSettings
CefContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  const base::FilePath& cache_path = context->GetPath();
  return content::GeneratedCodeCacheSettings(!cache_path.empty() /* enabled */,
                                             0 /* size */, cache_path);
}

void CefContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    content::ResourceType resource_type,
    bool strict_enforcement,
    bool expired_previous_decision,
    const base::Callback<void(content::CertificateRequestResultType)>&
        callback) {
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
    net::ClientCertIdentityList client_certs,
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
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return;
  }

  CefRequestHandler::X509CertificateList certs;
  for (net::ClientCertIdentityList::iterator iter = client_certs.begin();
       iter != client_certs.end(); iter++) {
    certs.push_back(new CefX509CertificateImpl(std::move(*iter)));
  }

  CefRefPtr<CefSelectClientCertificateCallbackImpl> callbackImpl(
      new CefSelectClientCertificateCallbackImpl(std::move(delegate)));

  bool proceed = handler->OnSelectClientCertificate(
      browser.get(), cert_request_info->is_proxy,
      cert_request_info->host_and_port.host(),
      cert_request_info->host_and_port.port(), certs, callbackImpl.get());

  if (!proceed && !certs.empty()) {
    callbackImpl->Select(certs[0]);
  }
}

bool CefContentBrowserClient::CanCreateWindow(
    content::RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const url::Origin& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  CEF_REQUIRE_UIT();
  *no_javascript_access = false;

  return CefBrowserInfoManager::GetInstance()->CanCreateWindow(
      opener, target_url, referrer, frame_name, disposition, features,
      user_gesture, opener_suppressed, no_javascript_access);
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
          new ChromeBrowserPepperHostFactory(browser_host)));
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
    // Identify the RenderFrameHost that originated the navigation.
    content::RenderFrameHost* parent_frame_host =
        navigation_handle->GetParentFrame();
    DCHECK(parent_frame_host);
    if (parent_frame_host)
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

  // Must use SynchronyMode::kSync to ensure that OnBeforeBrowse is always
  // called before OnBeforeResourceLoad.
  std::unique_ptr<content::NavigationThrottle> throttle =
      std::make_unique<navigation_interception::InterceptNavigationThrottle>(
          navigation_handle,
          base::Bind(&NavigationOnUIThread, is_main_frame, frame_id,
                     parent_frame_id),
          navigation_interception::SynchronyMode::kSync);
  throttles.push_back(std::move(throttle));

  return throttles;
}

#if defined(OS_LINUX)
void CefContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(service_manager::kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
const wchar_t* CefContentBrowserClient::GetResourceDllName() {
  static wchar_t file_path[MAX_PATH + 1] = {0};

  if (file_path[0] == 0) {
    // Retrieve the module path (usually libcef.dll).
    base::FilePath module;
    base::PathService::Get(base::FILE_MODULE, &module);
    const std::wstring wstr = module.value();
    size_t count = std::min(static_cast<size_t>(MAX_PATH), wstr.size());
    wcsncpy(file_path, wstr.c_str(), count);
    file_path[count] = 0;
  }

  return file_path;
}

bool CefContentBrowserClient::PreSpawnRenderer(sandbox::TargetPolicy* policy) {
  return true;
}
#endif  // defined(OS_WIN)

void CefContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* host) {
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  host->GetChannel()->AddAssociatedInterfaceForIOThread(base::Bind(
      &PluginInfoHostImpl::OnPluginInfoHostRequest,
      base::MakeRefCounted<PluginInfoHostImpl>(host->GetID(), profile)));
}

std::unique_ptr<net::ClientCertStore>
CefContentBrowserClient::CreateClientCertStore(
    content::ResourceContext* resource_context) {
  if (!resource_context)
    return nullptr;
  return static_cast<CefResourceContext*>(resource_context)
      ->CreateClientCertStore();
}

void CefContentBrowserClient::RegisterNonNetworkNavigationURLLoaderFactories(
    int frame_tree_node_id,
    NonNetworkURLLoaderFactoryMap* factories) {
  if (!extensions::ExtensionsEnabled())
    return;

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionNavigationURLLoaderFactory(
          web_contents->GetBrowserContext(),
          !!extensions::WebViewGuest::FromWebContents(web_contents)));
}

void CefContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    NonNetworkURLLoaderFactoryMap* factories) {
  if (!extensions::ExtensionsEnabled())
    return;

  auto factory = extensions::CreateExtensionURLLoaderFactory(render_process_id,
                                                             render_frame_id);
  if (factory)
    factories->emplace(extensions::kExtensionScheme, std::move(factory));
}

bool CefContentBrowserClient::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    bool is_navigation,
    bool is_download,
    const url::Origin& request_initiator,
    network::mojom::URLLoaderFactoryRequest* factory_request,
    network::mojom::TrustedURLLoaderHeaderClientPtrInfo* header_client,
    bool* bypass_redirect_checks) {
  if (!extensions::ExtensionsEnabled())
    return false;

  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);
  bool use_proxy = web_request_api->MaybeProxyURLLoaderFactory(
      browser_context, frame, render_process_id, is_navigation, is_download,
      factory_request, header_client);
  if (bypass_redirect_checks)
    *bypass_redirect_checks = use_proxy;
  return use_proxy;
}

void CefContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  if (!net_service::IsEnabled())
    return;

  DCHECK(g_browser_process);
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  if (!SystemNetworkContextManager::GetInstance()) {
    SystemNetworkContextManager::CreateInstance(local_state);
  }
  // Need to set up global NetworkService state before anything else uses it.
  SystemNetworkContextManager::GetInstance()->OnNetworkServiceCreated(
      network_service);
}

network::mojom::NetworkContextPtr CefContentBrowserClient::CreateNetworkContext(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  Profile* profile = Profile::FromBrowserContext(context);
  return profile->CreateNetworkContext(in_memory, relative_partition_path);
}

std::vector<base::FilePath>
CefContentBrowserClient::GetNetworkContextsParentDirectory() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());

  // TODO(cef): Do we really want to create a cache directory underneath
  // DIR_USER_DATA?
  base::FilePath cache_dir;
  chrome::GetUserCacheDirectory(user_data_dir, &cache_dir);
  DCHECK(!cache_dir.empty());

  // On some platforms, the cache is a child of the user_data_dir so only
  // return the one path.
  if (user_data_dir.IsParent(cache_dir))
    return {user_data_dir};

  return {user_data_dir, cache_dir};
}

bool CefContentBrowserClient::HandleExternalProtocol(
    const GURL& url,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    int child_id,
    content::NavigationUIData* navigation_data,
    bool is_main_frame,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const std::string& method,
    const net::HttpRequestHeaders& headers) {
  CEF_POST_TASK(
      CEF_UIT,
      base::Bind(
          base::IgnoreResult(
              &CefContentBrowserClient::HandleExternalProtocolOnUIThread),
          url, web_contents_getter));
  return false;
}

std::string CefContentBrowserClient::GetProduct() const {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kProductVersion))
    return command_line->GetSwitchValueASCII(switches::kProductVersion);

  return GetChromeProduct();
}

std::string CefContentBrowserClient::GetChromeProduct() const {
  return base::StringPrintf("Chrome/%d.%d.%d.%d", CHROME_VERSION_MAJOR,
                            CHROME_VERSION_MINOR, CHROME_VERSION_BUILD,
                            CHROME_VERSION_PATCH);
}

std::string CefContentBrowserClient::GetUserAgent() const {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserAgent))
    return command_line->GetSwitchValueASCII(switches::kUserAgent);

  return content::BuildUserAgentFromProduct(GetProduct());
}

blink::UserAgentMetadata CefContentBrowserClient::GetUserAgentMetadata() const {
  blink::UserAgentMetadata metadata;

  metadata.brand = version_info::GetProductName();
  metadata.full_version = version_info::GetVersionNumber();
  metadata.platform = version_info::GetOSType();

  // TODO(mkwst): Poke at BuildUserAgentFromProduct to split out these pieces.
  metadata.architecture = "";
  metadata.model = "";

  return metadata;
}

void CefContentBrowserClient::RegisterCustomScheme(const std::string& scheme) {
  // Register as a Web-safe scheme so that requests for the scheme from a
  // render process will be allowed in resource_dispatcher_host_impl.cc
  // ShouldServiceRequest.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  if (!policy->IsWebSafeScheme(scheme))
    policy->RegisterWebSafeScheme(scheme);
}

CefRefPtr<CefRequestContextImpl> CefContentBrowserClient::request_context()
    const {
  return browser_main_parts_->request_context();
}

CefDevToolsDelegate* CefContentBrowserClient::devtools_delegate() const {
  return browser_main_parts_->devtools_delegate();
}

scoped_refptr<base::SingleThreadTaskRunner>
CefContentBrowserClient::background_task_runner() const {
  return browser_main_parts_->background_task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
CefContentBrowserClient::user_visible_task_runner() const {
  return browser_main_parts_->user_visible_task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
CefContentBrowserClient::user_blocking_task_runner() const {
  return browser_main_parts_->user_blocking_task_runner();
}

const extensions::Extension* CefContentBrowserClient::GetExtension(
    content::SiteInstance* site_instance) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(site_instance->GetBrowserContext());
  if (!registry)
    return nullptr;
  return registry->enabled_extensions().GetExtensionOrAppByURL(
      site_instance->GetSiteURL());
}

// static
void CefContentBrowserClient::HandleExternalProtocolOnUIThread(
    const GURL& url,
    const content::ResourceRequestInfo::WebContentsGetter&
        web_contents_getter) {
  CEF_REQUIRE_UIT();
  content::WebContents* web_contents = web_contents_getter.Run();
  if (web_contents) {
    CefRefPtr<CefBrowserHostImpl> browser =
        CefBrowserHostImpl::GetBrowserForContents(web_contents);
    if (browser.get())
      browser->HandleExternalProtocol(url);
  }
}
