// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/chrome_content_browser_client_cef.h"

#include <tuple>

#include "base/command_line.h"
#include "base/path_service.h"
#include "cef/libcef/browser/browser_frame.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/browser_info_manager.h"
#include "cef/libcef/browser/browser_manager.h"
#include "cef/libcef/browser/certificate_query.h"
#include "cef/libcef/browser/chrome/chrome_browser_main_extra_parts_cef.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/net/throttle_handler.h"
#include "cef/libcef/browser/net_service/cookie_manager_impl.h"
#include "cef/libcef/browser/net_service/login_delegate.h"
#include "cef/libcef/browser/net_service/proxy_url_loader_factory.h"
#include "cef/libcef/browser/net_service/resource_request_handler_wrapper.h"
#include "cef/libcef/browser/prefs/browser_prefs.h"
#include "cef/libcef/browser/prefs/renderer_prefs.h"
#include "cef/libcef/browser/x509_certificate_impl.h"
#include "cef/libcef/common/app_manager.h"
#include "cef/libcef/common/cef_switches.h"
#include "cef/libcef/common/command_line_impl.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/common/content_switches.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

#if !BUILDFLAG(IS_MAC)
#include "cef/libcef/browser/chrome/chrome_web_contents_view_delegate_cef.h"
#endif

namespace {

class CefSelectClientCertificateCallbackImpl
    : public CefSelectClientCertificateCallback {
 public:
  explicit CefSelectClientCertificateCallbackImpl(
      std::unique_ptr<content::ClientCertificateDelegate> delegate)
      : delegate_(std::move(delegate)) {}

  CefSelectClientCertificateCallbackImpl(
      const CefSelectClientCertificateCallbackImpl&) = delete;
  CefSelectClientCertificateCallbackImpl& operator=(
      const CefSelectClientCertificateCallbackImpl&) = delete;

  ~CefSelectClientCertificateCallbackImpl() override {
    // If Select has not been called, call it with NULL to continue without any
    // client certificate.
    RunNow(std::move(delegate_), nullptr);
  }

  void Select(CefRefPtr<CefX509Certificate> cert) override {
    if (!CEF_CURRENTLY_ON_UIT()) {
      CEF_POST_TASK(
          CEF_UIT,
          base::BindOnce(&CefSelectClientCertificateCallbackImpl::Select, this,
                         cert));
    } else {
      RunNow(std::move(delegate_), cert);
    }
  }

  [[nodiscard]] std::unique_ptr<content::ClientCertificateDelegate>
  DisconnectDelegate() {
    CEF_REQUIRE_UIT();
    return std::move(delegate_);
  }

 private:
  static void RunNow(
      std::unique_ptr<content::ClientCertificateDelegate> delegate,
      CefRefPtr<CefX509Certificate> cert) {
    CEF_REQUIRE_UIT();

    if (delegate) {
      if (cert) {
        CefX509CertificateImpl* certImpl =
            static_cast<CefX509CertificateImpl*>(cert.get());
        certImpl->AcquirePrivateKey(base::BindOnce(
            &CefSelectClientCertificateCallbackImpl::RunWithPrivateKey,
            std::move(delegate), cert));
        return;
      }

      delegate->ContinueWithCertificate(nullptr, nullptr);
    }
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

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefSelectClientCertificateCallbackImpl);
};

void HandleExternalProtocolHelper(
    ChromeContentBrowserClientCef* self,
    content::WebContents::Getter web_contents_getter,
    content::FrameTreeNodeId frame_tree_node_id,
    content::NavigationUIData* navigation_data,
    bool is_primary_main_frame,
    bool is_in_fenced_frame_tree,
    network::mojom::WebSandboxFlags sandbox_flags,
    const network::ResourceRequest& resource_request,
    const std::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr initiator_document,
    const net::IsolationInfo& isolation_info) {
  CEF_REQUIRE_UIT();

  // May return nullptr if frame has been deleted or a cross-document navigation
  // has committed in the same RenderFrameHost.
  auto initiator_rfh = initiator_document.AsRenderFrameHostIfValid();
  if (!initiator_rfh) {
    return;
  }

  // Match the logic of the original call in
  // NavigationURLLoaderImpl::PrepareForNonInterceptedRequest.
  self->HandleExternalProtocol(
      resource_request.url, web_contents_getter, frame_tree_node_id,
      navigation_data, is_primary_main_frame, is_in_fenced_frame_tree,
      sandbox_flags,
      static_cast<ui::PageTransition>(resource_request.transition_type),
      resource_request.has_user_gesture, initiating_origin, initiator_rfh,
      isolation_info, nullptr);
}

}  // namespace

ChromeContentBrowserClientCef::ChromeContentBrowserClientCef() = default;
ChromeContentBrowserClientCef::~ChromeContentBrowserClientCef() = default;

void ChromeContentBrowserClientCef::CleanupOnUIThread() {
  browser_main_parts_ = nullptr;
  ChromeContentBrowserClient::CleanupOnUIThread();
}

std::unique_ptr<content::BrowserMainParts>
ChromeContentBrowserClientCef::CreateBrowserMainParts(
    bool is_integration_test) {
  auto main_parts =
      ChromeContentBrowserClient::CreateBrowserMainParts(is_integration_test);
  auto browser_main_parts = std::make_unique<ChromeBrowserMainExtraPartsCef>();
  browser_main_parts_ = browser_main_parts.get();
  static_cast<ChromeBrowserMainParts*>(main_parts.get())
      ->AddParts(std::move(browser_main_parts));
  return main_parts;
}

void ChromeContentBrowserClientCef::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  ChromeContentBrowserClient::AppendExtraCommandLineSwitches(command_line,
                                                             child_process_id);

  // Necessary to populate DIR_USER_DATA in sub-processes.
  // See resource_util.cc GetUserDataPath.
  base::FilePath user_data_dir;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  }

  const base::CommandLine* browser_cmd = base::CommandLine::ForCurrentProcess();

  {
    // Propagate the following switches to all command lines (along with any
    // associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
#if BUILDFLAG(IS_MAC)
        switches::kFrameworkDirPath,
        switches::kMainBundlePath,
#endif
        switches::kLocalesDirPath,
        switches::kLogItems,
        switches::kLogSeverity,
        switches::kResourcesDirPath,
        switches::kUserAgentProductAndVersion,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames);
  }

  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  if (process_type == switches::kZygoteProcess &&
      browser_cmd->HasSwitch(switches::kBrowserSubprocessPath)) {
    // Force use of the sub-process executable path for the zygote process.
    const base::FilePath& subprocess_path =
        browser_cmd->GetSwitchValuePath(switches::kBrowserSubprocessPath);
    if (!subprocess_path.empty()) {
      command_line->SetProgram(subprocess_path);
    }
  }
#endif

  if (process_type == switches::kRendererProcess) {
    // Propagate the following switches to the renderer command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
        switches::kUncaughtExceptionStackSize,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames);
  }

  CefRefPtr<CefApp> app = CefAppManager::Get()->GetApplication();
  if (app.get()) {
    CefRefPtr<CefBrowserProcessHandler> handler =
        app->GetBrowserProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefCommandLineImpl> commandLinePtr(
          new CefCommandLineImpl(command_line, false, false));
      handler->OnBeforeChildProcessLaunch(commandLinePtr.get());
      std::ignore = commandLinePtr->Detach(nullptr);
    }
  }
}

void ChromeContentBrowserClientCef::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  ChromeContentBrowserClient::RenderProcessWillLaunch(host);

  // If the renderer process crashes then the host may already have
  // CefBrowserInfoManager as an observer. Try to remove it first before adding
  // to avoid DCHECKs.
  host->RemoveObserver(CefBrowserInfoManager::GetInstance());
  host->AddObserver(CefBrowserInfoManager::GetInstance());
}

void ChromeContentBrowserClientCef::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    base::OnceCallback<void(content::CertificateRequestResultType)> callback) {
  auto returned_callback = certificate_query::AllowCertificateError(
      web_contents, cert_error, ssl_info, request_url, is_main_frame_request,
      strict_enforcement, std::move(callback), /*default_disallow=*/false);
  if (returned_callback.is_null()) {
    // The error was handled.
    return;
  }

  // Proceed with default handling.
  ChromeContentBrowserClient::AllowCertificateError(
      web_contents, cert_error, ssl_info, request_url, is_main_frame_request,
      strict_enforcement, std::move(returned_callback));
}

base::OnceClosure ChromeContentBrowserClientCef::SelectClientCertificate(
    content::BrowserContext* browser_context,
    int process_id,
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  CEF_REQUIRE_UIT();

  CefRefPtr<CefRequestHandler> handler;
  CefRefPtr<CefBrowserHostBase> browser =
      CefBrowserHostBase::GetBrowserForContents(web_contents);
  if (browser) {
    if (auto client = browser->GetClient()) {
      handler = client->GetRequestHandler();
    }
  }

  if (!handler) {
    return ChromeContentBrowserClient::SelectClientCertificate(
        browser_context, process_id, web_contents, cert_request_info,
        std::move(client_certs), std::move(delegate));
  }

  CefRequestHandler::X509CertificateList certs;
  for (auto& client_cert : client_certs) {
    certs.push_back(new CefX509CertificateImpl(std::move(client_cert)));
  }

  CefRefPtr<CefSelectClientCertificateCallbackImpl> callbackImpl(
      new CefSelectClientCertificateCallbackImpl(std::move(delegate)));

  bool handled = handler->OnSelectClientCertificate(
      browser.get(), cert_request_info->is_proxy,
      cert_request_info->host_and_port.host(),
      cert_request_info->host_and_port.port(), certs, callbackImpl.get());

  if (!handled) {
    delegate = callbackImpl->DisconnectDelegate();
    if (delegate) {
      client_certs.clear();
      for (auto& cert : certs) {
        CefX509CertificateImpl* certImpl =
            static_cast<CefX509CertificateImpl*>(cert.get());
        client_certs.push_back(certImpl->DisconnectIdentity());
      }
      return ChromeContentBrowserClient::SelectClientCertificate(
          browser_context, process_id, web_contents, cert_request_info,
          std::move(client_certs), std::move(delegate));
    } else {
      LOG(ERROR) << "Should return true from OnSelectClientCertificate when "
                    "executing the callback";
    }
  }

  return base::OnceClosure();
}

bool ChromeContentBrowserClientCef::CanCreateWindow(
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
  // The chrome layer has popup blocker, extensions, etc.
  if (!ChromeContentBrowserClient::CanCreateWindow(
          opener, opener_url, opener_top_level_frame_url, source_origin,
          container_type, target_url, referrer, frame_name, disposition,
          features, user_gesture, opener_suppressed, no_javascript_access)) {
    return false;
  }

  return CefBrowserInfoManager::GetInstance()->CanCreateWindow(
      opener, target_url, referrer, frame_name, disposition, features,
      user_gesture, opener_suppressed, no_javascript_access);
}

void ChromeContentBrowserClientCef::CreateWindowResult(
    content::RenderFrameHost* opener,
    bool success) {
  CefBrowserInfoManager::GetInstance()->CreateWindowResult(opener, success);
}

void ChromeContentBrowserClientCef::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
  renderer_prefs::SetDefaultPrefs(*prefs);

  ChromeContentBrowserClient::OverrideWebkitPrefs(web_contents, prefs);

  SkColor base_background_color;
  auto browser = CefBrowserHostBase::GetBrowserForContents(web_contents);
  if (browser) {
    renderer_prefs::SetCefPrefs(browser->settings(), *prefs);

    // Set the background color for the WebView.
    base_background_color = browser->GetBackgroundColor();
  } else {
    // We don't know for sure that the browser will be windowless but assume
    // that the global windowless state is likely to be accurate.
    base_background_color =
        CefContext::Get()->GetBackgroundColor(nullptr, STATE_DEFAULT);
  }

  web_contents->SetPageBaseBackgroundColor(base_background_color);
}

void ChromeContentBrowserClientCef::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    const net::IsolationInfo& isolation_info,
    std::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner) {
  // Don't intercept requests for Profiles that were not created by CEF.
  // For example, the User Manager profile created via
  // profiles::CreateSystemProfileForUserManager.
  auto profile = Profile::FromBrowserContext(browser_context);
  if (!CefBrowserContext::FromProfile(profile)) {
    ChromeContentBrowserClient::WillCreateURLLoaderFactory(
        browser_context, frame, render_process_id, type, request_initiator,
        isolation_info, navigation_id, ukm_source_id, factory_builder,
        header_client, bypass_redirect_checks, disable_secure_dns,
        factory_override, navigation_response_task_runner);
    return;
  }

  // Based on content/browser/devtools/devtools_instrumentation.cc
  // WillCreateURLLoaderFactoryParams::Run.
  network::mojom::URLLoaderFactoryOverridePtr cef_override(
      network::mojom::URLLoaderFactoryOverride::New());
  // If caller passed some existing overrides, use those.
  // Otherwise, use our local var, then if handlers actually
  // decide to intercept, move it to |factory_override|.
  network::mojom::URLLoaderFactoryOverridePtr* handler_override =
      factory_override && *factory_override ? factory_override : &cef_override;
  network::mojom::URLLoaderFactoryOverride* intercepting_factory =
      handler_override->get();

  // If we're the first interceptor to install an override, make a
  // remote/receiver pair, then handle this similarly to appending
  // a proxy to existing override.
  if (!intercepting_factory->overriding_factory) {
    DCHECK(!intercepting_factory->overridden_factory_receiver);
    intercepting_factory->overridden_factory_receiver =
        intercepting_factory->overriding_factory
            .InitWithNewPipeAndPassReceiver();
  }

  // TODO(chrome): Is it necessary to proxy |header_client| callbacks?
  ChromeContentBrowserClient::WillCreateURLLoaderFactory(
      browser_context, frame, render_process_id, type, request_initiator,
      isolation_info, navigation_id, ukm_source_id, factory_builder,
      /*header_client=*/nullptr, bypass_redirect_checks, disable_secure_dns,
      handler_override, navigation_response_task_runner);

  DCHECK(intercepting_factory->overriding_factory);
  DCHECK(intercepting_factory->overridden_factory_receiver);
  if (!factory_override) {
    // Not a subresource navigation, so just override the target receiver.
    auto [receiver, remote] = factory_builder.Append();
    mojo::FusePipes(std::move(receiver),
                    std::move(cef_override->overriding_factory));
    mojo::FusePipes(std::move(cef_override->overridden_factory_receiver),
                    std::move(remote));
  } else if (!*factory_override) {
    // No other overrides, so just returns ours as is.
    *factory_override = network::mojom::URLLoaderFactoryOverride::New(
        std::move(cef_override->overriding_factory),
        std::move(cef_override->overridden_factory_receiver), false);
  }
  // ... else things are already taken care of, as handler_override was
  // pointing to factory override and we've done all magic in-place.
  DCHECK(!cef_override->overriding_factory);
  DCHECK(!cef_override->overridden_factory_receiver);

  auto request_handler = net_service::CreateInterceptedRequestHandler(
      browser_context, frame, render_process_id,
      type == URLLoaderFactoryType::kNavigation,
      type == URLLoaderFactoryType::kDownload, request_initiator);

  net_service::ProxyURLLoaderFactory::CreateProxy(
      browser_context, factory_builder, header_client,
      std::move(request_handler));
}

bool ChromeContentBrowserClientCef::HandleExternalProtocol(
    const GURL& url,
    content::WebContents::Getter web_contents_getter,
    content::FrameTreeNodeId frame_tree_node_id,
    content::NavigationUIData* navigation_data,
    bool is_primary_main_frame,
    bool is_in_fenced_frame_tree,
    network::mojom::WebSandboxFlags sandbox_flags,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const std::optional<url::Origin>& initiating_origin,
    content::RenderFrameHost* initiator_document,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  // |out_factory| will be non-nullptr when this method is initially called
  // from NavigationURLLoaderImpl::PrepareForNonInterceptedRequest.
  if (out_factory) {
    // Let the other HandleExternalProtocol variant handle the request.
    return false;
  }

  // The request was unhandled and we've received a callback from
  // HandleExternalProtocolHelper. Forward to the chrome layer for default
  // handling.
  return ChromeContentBrowserClient::HandleExternalProtocol(
      url, web_contents_getter, frame_tree_node_id, navigation_data,
      is_primary_main_frame, is_in_fenced_frame_tree, sandbox_flags,
      page_transition, has_user_gesture, initiating_origin, initiator_document,
      isolation_info, nullptr);
}

bool ChromeContentBrowserClientCef::HandleExternalProtocol(
    content::WebContents::Getter web_contents_getter,
    content::FrameTreeNodeId frame_tree_node_id,
    content::NavigationUIData* navigation_data,
    bool is_primary_main_frame,
    bool is_in_fenced_frame_tree,
    network::mojom::WebSandboxFlags sandbox_flags,
    const network::ResourceRequest& request,
    const std::optional<url::Origin>& initiating_origin,
    content::RenderFrameHost* initiator_document,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver =
      out_factory->InitWithNewPipeAndPassReceiver();

  auto weak_initiator_document = initiator_document
                                     ? initiator_document->GetWeakDocumentPtr()
                                     : content::WeakDocumentPtr();

  // HandleExternalProtocolHelper may be called if nothing handles the request.
  auto request_handler = net_service::CreateInterceptedRequestHandler(
      web_contents_getter, frame_tree_node_id, request,
      base::BindRepeating(HandleExternalProtocolHelper, base::Unretained(this),
                          web_contents_getter, frame_tree_node_id,
                          navigation_data, is_primary_main_frame,
                          is_in_fenced_frame_tree, sandbox_flags, request,
                          initiating_origin, std::move(weak_initiator_document),
                          isolation_info));

  net_service::ProxyURLLoaderFactory::CreateProxy(
      web_contents_getter, std::move(receiver), std::move(request_handler));
  return true;
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
ChromeContentBrowserClientCef::CreateThrottlesForNavigation(
    content::NavigationHandle* navigation_handle) {
  auto throttles = ChromeContentBrowserClient::CreateThrottlesForNavigation(
      navigation_handle);
  throttle::CreateThrottlesForNavigation(navigation_handle, throttles);
  return throttles;
}

bool ChromeContentBrowserClientCef::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  // This method may be called during shutdown when using multi-threaded
  // message loop mode. In that case exit early to avoid crashes.
  if (!SystemNetworkContextManager::GetInstance()) {
    // Cancel NetworkContext creation in
    // StoragePartitionImpl::InitNetworkContext.
    return false;
  }

  ChromeContentBrowserClient::ConfigureNetworkContextParams(
      context, in_memory, relative_partition_path, network_context_params,
      cert_verifier_creation_params);

  auto cef_context = CefBrowserContext::FromBrowserContext(context);
  network_context_params->cookieable_schemes =
      cef_context ? cef_context->GetCookieableSchemes()
                  : CefBrowserContext::GetGlobalCookieableSchemes();

  return true;
}

std::unique_ptr<content::LoginDelegate>
ChromeContentBrowserClientCef::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const content::GlobalRequestID& request_id,
    bool is_request_for_primary_main_frame_navigation,
    bool is_request_for_navigation,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  // |web_contents| is nullptr for CefURLRequests without an associated frame.
  if (!web_contents || base::CommandLine::ForCurrentProcess()->HasSwitch(
                           switches::kDisableChromeLoginPrompt)) {
    // Delegate auth callbacks to GetAuthCredentials.
    return std::make_unique<net_service::LoginDelegate>(
        auth_info, web_contents, request_id, url,
        std::move(auth_required_callback));
  }

  return ChromeContentBrowserClient::CreateLoginDelegate(
      auth_info, web_contents, browser_context, request_id,
      is_request_for_primary_main_frame_navigation, is_request_for_navigation,
      url, response_headers, first_auth_attempt,
      std::move(auth_required_callback));
}

void ChromeContentBrowserClientCef::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* host) {
  ChromeContentBrowserClient::ExposeInterfacesToRenderer(
      registry, associated_registry, host);

  CefBrowserManager::ExposeInterfacesToRenderer(registry, associated_registry,
                                                host);
}

void ChromeContentBrowserClientCef::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
      render_frame_host, map);

  CefBrowserFrame::RegisterBrowserInterfaceBindersForFrame(render_frame_host,
                                                           map);
}

std::unique_ptr<content::WebContentsViewDelegate>
ChromeContentBrowserClientCef::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  // From ChromeContentBrowserClient::GetWebContentsViewDelegate. Windowless
  // browsers don't call this method and use
  // CefBrowserPlatformDelegateAlloy::AttachHelpers instead.
  if (auto* registry =
          performance_manager::PerformanceManagerRegistry::GetInstance()) {
    registry->MaybeCreatePageNodeForWebContents(web_contents);
  }

  // Used to customize context menu behavior for Alloy style. Called during
  // WebContents::Create() so we don't yet have an associated BrowserHost.
  return CreateWebContentsViewDelegate(web_contents);
}

CefRefPtr<CefRequestContextImpl>
ChromeContentBrowserClientCef::request_context() const {
  return browser_main_parts_->request_context();
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeContentBrowserClientCef::background_task_runner() const {
  return browser_main_parts_->background_task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeContentBrowserClientCef::user_visible_task_runner() const {
  return browser_main_parts_->user_visible_task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeContentBrowserClientCef::user_blocking_task_runner() const {
  return browser_main_parts_->user_blocking_task_runner();
}

#if !BUILDFLAG(IS_MAC)
// Defined in a separate .mm file on MacOS to work around
// ChromeWebContentsViewDelegateViewsMac containing ObjC references.

// static
std::unique_ptr<content::WebContentsViewDelegate>
ChromeContentBrowserClientCef::CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<ChromeWebContentsViewDelegateCef>(web_contents);
}
#endif
