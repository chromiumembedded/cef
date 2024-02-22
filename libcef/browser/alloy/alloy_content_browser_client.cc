// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/alloy_content_browser_client.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "include/cef_version.h"
#include "libcef/browser/alloy/alloy_browser_context.h"
#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/alloy/alloy_browser_main.h"
#include "libcef/browser/alloy/alloy_web_contents_view_delegate.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_frame.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_manager.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/certificate_query.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools/devtools_manager_delegate.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/net/chrome_scheme_handler.h"
#include "libcef/browser/net/throttle_handler.h"
#include "libcef/browser/net_service/cookie_manager_impl.h"
#include "libcef/browser/net_service/login_delegate.h"
#include "libcef/browser/net_service/proxy_url_loader_factory.h"
#include "libcef/browser/net_service/resource_request_handler_wrapper.h"
#include "libcef/browser/prefs/renderer_prefs.h"
#include "libcef/browser/speech_recognition_manager_delegate.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/x509_certificate_impl.h"
#include "libcef/common/alloy/alloy_content_client.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/command_line_impl.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/frame_util.h"
#include "libcef/common/net/scheme_registration.h"
#include "libcef/common/request_impl.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "cef/grit/cef_resources.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/pdf/chrome_pdf_stream_delegate.h"
#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"
#include "chrome/browser/plugins/plugin_info_host_impl.h"
#include "chrome/browser/plugins/plugin_response_interceptor_url_loader_throttle.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/predictors/network_hints_handler_impl.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/renderer_updater.h"
#include "chrome/browser/profiles/renderer_updater_factory.h"
#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"
#include "chrome/browser/spellchecker/spell_check_initialization_host_impl.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/pdf/chrome_pdf_document_helper_client.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/google_url_loader_throttle.h"
#include "chrome/common/pdf_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/printing/printing_service.h"
#include "chrome/services/speech/buildflags/buildflags.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/embedder_support/switches.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/pdf/browser/pdf_navigation_throttle.h"
#include "components/pdf/browser/pdf_url_loader_request_interceptor.h"
#include "components/pdf/common/constants.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/version_info/version_info.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "crypto/crypto_buildflags.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/browser/api/mime_handler_private/mime_handler_private.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/extensions_guest_view.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/service_worker/service_worker_host.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/constants.h"
#include "extensions/common/mojom/automation_registry.mojom.h"
#include "extensions/common/mojom/event_router.mojom.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "extensions/common/mojom/renderer_host.mojom.h"
#include "extensions/common/switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "net/base/auth.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "pdf/pdf_features.h"
#include "sandbox/policy/switches.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/badging/badging.mojom.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "net/ssl/client_cert_store_mac.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#elif BUILDFLAG(IS_POSIX)
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/common/content_descriptors.h"
#include "libcef/common/crash_reporting.h"
#endif

#if BUILDFLAG(IS_WIN)
#if BUILDFLAG(ENABLE_SPEECH_SERVICE)
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#endif
#include "net/ssl/client_cert_store_win.h"
#include "sandbox/win/src/sandbox_policy.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(USE_NSS_CERTS)
#include "net/ssl/client_cert_store_nss.h"
#endif

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
#include "chrome/browser/spellchecker/spell_check_panel_host_impl.h"
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
    if (delegate_) {
      DoSelect(nullptr);
    }
  }

  void Select(CefRefPtr<CefX509Certificate> cert) override {
    if (delegate_) {
      DoSelect(cert);
    }
  }

 private:
  void DoSelect(CefRefPtr<CefX509Certificate> cert) {
    if (CEF_CURRENTLY_ON_UIT()) {
      RunNow(std::move(delegate_), cert);
    } else {
      CEF_POST_TASK(
          CEF_UIT,
          base::BindOnce(&CefSelectClientCertificateCallbackImpl::RunNow,
                         std::move(delegate_), cert));
    }
  }

  static void RunNow(
      std::unique_ptr<content::ClientCertificateDelegate> delegate,
      CefRefPtr<CefX509Certificate> cert) {
    CEF_REQUIRE_UIT();

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
};

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
int GetCrashSignalFD() {
  if (!crash_reporting::Enabled()) {
    return -1;
  }

  int fd;
  pid_t pid;
  return crash_reporter::GetHandlerSocket(&fd, &pid) ? fd : -1;
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

// From chrome/browser/plugins/chrome_content_browser_client_plugins_part.cc.
void BindPluginInfoHost(
    int render_process_id,
    mojo::PendingAssociatedReceiver<chrome::mojom::PluginInfoHost> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<PluginInfoHostImpl>(render_process_id, profile),
      std::move(receiver));
}

void BindBadgeService(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {}

void BindBadgeServiceForServiceWorker(
    const content::ServiceWorkerVersionBaseInfo& info,
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_SPEECH_SERVICE)
void BindMediaFoundationRendererNotifierHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererNotifier>
        receiver) {}
#endif

void BindNetworkHintsHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  predictors::NetworkHintsHandlerImpl::Create(frame_host, std::move(receiver));
}

// From chrome/browser/chrome_browser_interface_binders.cc
void BindMimeHandlerService(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<extensions::mime_handler::MimeHandlerService>
        receiver) {
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromRenderFrameHost(frame_host);
  if (!guest_view) {
    return;
  }
  extensions::MimeHandlerServiceImpl::Create(guest_view->GetStreamWeakPtr(),
                                             std::move(receiver));
}

// From chrome/browser/chrome_browser_interface_binders.cc
void BindBeforeUnloadControl(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<extensions::mime_handler::BeforeUnloadControl>
        receiver) {
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromRenderFrameHost(frame_host);
  if (!guest_view) {
    return;
  }
  guest_view->FuseBeforeUnloadControl(std::move(receiver));
}

base::FilePath GetUserDataPath() {
  base::FilePath user_data_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_path);
  DCHECK(!user_data_path.empty());
  return user_data_path;
}

const extensions::Extension* GetEnabledExtensionFromSiteURL(
    content::BrowserContext* context,
    const GURL& site_url) {
  if (!site_url.SchemeIs(extensions::kExtensionScheme)) {
    return nullptr;
  }

  auto registry = extensions::ExtensionRegistry::Get(context);
  if (!registry) {
    return nullptr;
  }

  return registry->enabled_extensions().GetByID(site_url.host());
}

std::unique_ptr<blink::URLLoaderThrottle> CreateGoogleURLLoaderThrottle(
    Profile* profile) {
  chrome::mojom::DynamicParamsPtr dynamic_params =
      chrome::mojom::DynamicParams::New(
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
          /*bound_session_params=*/nullptr,
#endif
          profile->GetPrefs()->GetBoolean(
              policy::policy_prefs::kForceGoogleSafeSearch),
          profile->GetPrefs()->GetInteger(
              policy::policy_prefs::kForceYouTubeRestrict),
          profile->GetPrefs()->GetString(prefs::kAllowedDomainsForApps));
  return std::make_unique<GoogleURLLoaderThrottle>(
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      /*bound_session_request_throttled_listener=*/nullptr,
#endif
      std::move(dynamic_params));
}

}  // namespace

AlloyContentBrowserClient::AlloyContentBrowserClient() = default;

AlloyContentBrowserClient::~AlloyContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
AlloyContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  auto browser_main_parts = std::make_unique<AlloyBrowserMainParts>();
  browser_main_parts_ = browser_main_parts.get();
  return browser_main_parts;
}

void AlloyContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());

  // If the renderer process crashes then the host may already have
  // CefBrowserInfoManager as an observer. Try to remove it first before adding
  // to avoid DCHECKs.
  host->RemoveObserver(CefBrowserInfoManager::GetInstance());
  host->AddObserver(CefBrowserInfoManager::GetInstance());

  // Forwards dynamic parameters to CefRenderThreadObserver.
  Profile* original_profile = profile->GetOriginalProfile();
  RendererUpdaterFactory::GetForProfile(original_profile)
      ->InitializeRenderer(host);
}

bool AlloyContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  if (extensions::ExtensionsEnabled()) {
    if (auto profile = Profile::FromBrowserContext(browser_context)) {
      return extensions::ChromeContentBrowserClientExtensionsPart::
          ShouldUseProcessPerSite(profile, site_url);
    }
  }

  return content::ContentBrowserClient::ShouldUseProcessPerSite(browser_context,
                                                                site_url);
}

bool AlloyContentBrowserClient::ShouldUseSpareRenderProcessHost(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  if (extensions::ExtensionsEnabled()) {
    if (auto profile = Profile::FromBrowserContext(browser_context)) {
      return extensions::ChromeContentBrowserClientExtensionsPart::
          ShouldUseSpareRenderProcessHost(profile, site_url);
    }
  }

  return content::ContentBrowserClient::ShouldUseSpareRenderProcessHost(
      browser_context, site_url);
}

bool AlloyContentBrowserClient::DoesSiteRequireDedicatedProcess(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
  if (extensions::ExtensionsEnabled()) {
    return extensions::ChromeContentBrowserClientExtensionsPart::
        DoesSiteRequireDedicatedProcess(browser_context, effective_site_url);
  }

  return content::ContentBrowserClient::DoesSiteRequireDedicatedProcess(
      browser_context, effective_site_url);
}

bool AlloyContentBrowserClient::ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
    base::StringPiece scheme,
    bool is_embedded_origin_secure) {
  // This is needed to bypass the normal SameSite rules for any chrome:// page
  // embedding a secure origin, regardless of the registrable domains of any
  // intervening frames. For example, this is needed for browser UI to interact
  // with SameSite cookies on accounts.google.com, which are used for logging
  // into Cloud Print from chrome://print, for displaying a list of available
  // accounts on the NTP (chrome://new-tab-page), etc.
  if (is_embedded_origin_secure && scheme == content::kChromeUIScheme) {
    return true;
  }

  if (extensions::ExtensionsEnabled()) {
    return scheme == extensions::kExtensionScheme;
  }

  return false;
}

bool AlloyContentBrowserClient::
    ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
        base::StringPiece scheme,
        bool is_embedded_origin_secure) {
  return is_embedded_origin_secure && scheme == content::kChromeUIScheme;
}

void AlloyContentBrowserClient::OverrideURLLoaderFactoryParams(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params) {
  if (extensions::ExtensionsEnabled()) {
    extensions::URLLoaderFactoryManager::OverrideURLLoaderFactoryParams(
        browser_context, origin, is_for_isolated_world, factory_params);
  }
}

void AlloyContentBrowserClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  // Any schemes listed here are treated as WebUI schemes but do not get WebUI
  // bindings. Also, view-source is allowed for these schemes. WebUI schemes
  // will not be passed to HandleExternalProtocol.
}

void AlloyContentBrowserClient::GetAdditionalViewSourceSchemes(
    std::vector<std::string>* additional_schemes) {
  GetAdditionalWebUISchemes(additional_schemes);

  additional_schemes->push_back(extensions::kExtensionScheme);
}

std::unique_ptr<ui::SelectFilePolicy>
AlloyContentBrowserClient::CreateSelectFilePolicy(
    content::WebContents* web_contents) {
  return std::make_unique<ChromeSelectFilePolicy>(web_contents);
}

void AlloyContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
    std::vector<std::string>* additional_allowed_schemes) {
  ContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
      additional_allowed_schemes);
  additional_allowed_schemes->push_back(content::kChromeDevToolsScheme);
  additional_allowed_schemes->push_back(content::kChromeUIScheme);
  additional_allowed_schemes->push_back(content::kChromeUIUntrustedScheme);
}

bool AlloyContentBrowserClient::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  return scheme::IsWebUIAllowedToMakeNetworkRequests(origin);
}

bool AlloyContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  const std::string& scheme = url.scheme();
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));

  if (scheme::IsInternalHandledScheme(scheme)) {
    return true;
  }

  return CefAppManager::Get()->HasCustomScheme(scheme);
}

void AlloyContentBrowserClient::SiteInstanceGotProcessAndSite(
    content::SiteInstance* site_instance) {
  if (!extensions::ExtensionsEnabled()) {
    return;
  }

  CHECK(site_instance->HasProcess());

  auto context = site_instance->GetBrowserContext();

  // Only add the process to the map if the SiteInstance's site URL is already
  // a chrome-extension:// URL. This includes hosted apps, except in rare cases
  // that a URL in the hosted app's extent is not treated as a hosted app (e.g.,
  // for isolated origins or cross-site iframes). For that case, don't look up
  // the hosted app's Extension from the site URL using GetExtensionOrAppByURL,
  // since it isn't treated as a hosted app.
  const auto extension =
      GetEnabledExtensionFromSiteURL(context, site_instance->GetSiteURL());
  if (!extension) {
    return;
  }

  extensions::ProcessMap::Get(context)->Insert(
      extension->id(), site_instance->GetProcess()->GetID());
}

void AlloyContentBrowserClient::BindHostReceiverForRenderer(
    content::RenderProcessHost* render_process_host,
    mojo::GenericPendingReceiver receiver) {
  if (auto host_receiver =
          receiver.As<spellcheck::mojom::SpellCheckInitializationHost>()) {
    SpellCheckInitializationHostImpl::Create(render_process_host->GetID(),
                                             std::move(host_receiver));
    return;
  }

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
  if (auto panel_host_receiver =
          receiver.As<spellcheck::mojom::SpellCheckPanelHost>()) {
    SpellCheckPanelHostImpl::Create(render_process_host->GetID(),
                                    std::move(panel_host_receiver));
    return;
  }
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)
}

void AlloyContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  const base::CommandLine* browser_cmd = base::CommandLine::ForCurrentProcess();

  {
    // Propagate the following switches to all command lines (along with any
    // associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
        switches::kDisablePackLoading,
#if BUILDFLAG(IS_MAC)
        switches::kFrameworkDirPath,   switches::kMainBundlePath,
#endif
        switches::kLocalesDirPath,     switches::kLogItems,
        switches::kLogSeverity,        switches::kResourcesDirPath,
        embedder_support::kUserAgent,  switches::kUserAgentProductAndVersion,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames);
  }

  const std::string& process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type == switches::kRendererProcess) {
    // Propagate the following switches to the renderer command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
        switches::kDisableExtensions,
        switches::kDisablePdfExtension,
        switches::kDisablePrintPreview,
        switches::kDisableScrollBounce,
        switches::kDisableSpellChecking,
        switches::kEnableSpeechInput,
        switches::kUncaughtExceptionStackSize,
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames);

    if (extensions::ExtensionsEnabled()) {
      content::RenderProcessHost* process =
          content::RenderProcessHost::FromID(child_process_id);
      auto browser_context = process->GetBrowserContext();
      CefBrowserContext* cef_browser_context =
          process ? CefBrowserContext::FromBrowserContext(browser_context)
                  : nullptr;
      if (cef_browser_context) {
        if (cef_browser_context->IsPrintPreviewSupported()) {
          command_line->AppendSwitch(switches::kEnablePrintPreview);
        }

        // Based on ChromeContentBrowserClientExtensionsPart::
        // AppendExtraRendererCommandLineSwitches
        if (extensions::ProcessMap::Get(browser_context)
                ->Contains(process->GetID())) {
          command_line->AppendSwitch(extensions::switches::kExtensionProcess);
        }
      }
    }
  } else {
    // Propagate the following switches to non-renderer command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
        switches::kLang,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames);
  }

  // Necessary to populate DIR_USER_DATA in sub-processes.
  // See resource_util.cc GetUserDataPath.
  base::FilePath user_data_dir;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  }

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  if (process_type == switches::kZygoteProcess) {
    if (browser_cmd->HasSwitch(switches::kBrowserSubprocessPath)) {
      // Force use of the sub-process executable path for the zygote process.
      const base::FilePath& subprocess_path =
          browser_cmd->GetSwitchValuePath(switches::kBrowserSubprocessPath);
      if (!subprocess_path.empty()) {
        command_line->SetProgram(subprocess_path);
      }
    }

    // Propagate the following switches to the zygote command line (along with
    // any associated values) if present in the browser command line.
    static const char* const kSwitchNames[] = {
        switches::kLogFile,
    };
    command_line->CopySwitchesFrom(*browser_cmd, kSwitchNames);
  }

  if (crash_reporting::Enabled()) {
    int fd;
    pid_t pid;
    if (crash_reporter::GetHandlerSocket(&fd, &pid)) {
      command_line->AppendSwitchASCII(
          crash_reporter::switches::kCrashpadHandlerPid,
          base::NumberToString(pid));
    }
  }
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

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

std::string AlloyContentBrowserClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

scoped_refptr<network::SharedURLLoaderFactory>
AlloyContentBrowserClient::GetSystemSharedURLLoaderFactory() {
  DCHECK(
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) ||
      !content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));

  if (!SystemNetworkContextManager::GetInstance()) {
    return nullptr;
  }

  return SystemNetworkContextManager::GetInstance()
      ->GetSharedURLLoaderFactory();
}

network::mojom::NetworkContext*
AlloyContentBrowserClient::GetSystemNetworkContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(SystemNetworkContextManager::GetInstance());
  return SystemNetworkContextManager::GetInstance()->GetContext();
}

content::MediaObserver* AlloyContentBrowserClient::GetMediaObserver() {
  return CefMediaCaptureDevicesDispatcher::GetInstance();
}

content::SpeechRecognitionManagerDelegate*
AlloyContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSpeechInput)) {
    return new CefSpeechRecognitionManagerDelegate();
  }

  return nullptr;
}

content::GeneratedCodeCacheSettings
AlloyContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  const base::FilePath& cache_path = context->GetPath();
  return content::GeneratedCodeCacheSettings(!cache_path.empty() /* enabled */,
                                             0 /* size */, cache_path);
}

void AlloyContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    base::OnceCallback<void(content::CertificateRequestResultType)> callback) {
  auto returned_callback = certificate_query::AllowCertificateError(
      web_contents, cert_error, ssl_info, request_url, is_main_frame_request,
      strict_enforcement, std::move(callback), /*default_disallow=*/true);
  // Callback should not be returned.
  DCHECK(returned_callback.is_null());
}

base::OnceClosure AlloyContentBrowserClient::SelectClientCertificate(
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  CEF_REQUIRE_UIT();

  CefRefPtr<CefRequestHandler> handler;
  CefRefPtr<AlloyBrowserHostImpl> browser =
      AlloyBrowserHostImpl::GetBrowserForContents(web_contents);
  if (browser.get()) {
    CefRefPtr<CefClient> client = browser->GetClient();
    if (client.get()) {
      handler = client->GetRequestHandler();
    }
  }

  if (!handler.get()) {
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return base::OnceClosure();
  }

  CefRequestHandler::X509CertificateList certs;
  for (auto& client_cert : client_certs) {
    certs.push_back(new CefX509CertificateImpl(std::move(client_cert)));
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
  return base::OnceClosure();
}

bool AlloyContentBrowserClient::CanCreateWindow(
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

void AlloyContentBrowserClient::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
  auto rvh = web_contents->GetRenderViewHost();

  // Using RVH instead of RFH here because rvh->GetMainFrame() may be nullptr
  // when this method is called.
  SkColor base_background_color;
  renderer_prefs::PopulateWebPreferences(rvh, *prefs, base_background_color);

  web_contents->SetPageBaseBackgroundColor(base_background_color);
}

bool AlloyContentBrowserClient::OverrideWebPreferencesAfterNavigation(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
  return renderer_prefs::PopulateWebPreferencesAfterNavigation(web_contents,
                                                               *prefs);
}

void AlloyContentBrowserClient::BrowserURLHandlerCreated(
    content::BrowserURLHandler* handler) {
  scheme::BrowserURLHandlerCreated(handler);
}

std::string AlloyContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

std::unique_ptr<content::DevToolsManagerDelegate>
AlloyContentBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<CefDevToolsManagerDelegate>();
}

void AlloyContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  CefBrowserManager::ExposeInterfacesToRenderer(registry, associated_registry,
                                                render_process_host);

  if (extensions::ExtensionsEnabled()) {
    // From ChromeContentBrowserClientExtensionsPart::
    // ExposeInterfacesToRenderer
    associated_registry->AddInterface<extensions::mojom::RendererHost>(
        base::BindRepeating(&extensions::RendererStartupHelper::BindForRenderer,
                            render_process_host->GetID()));
  }
}

void AlloyContentBrowserClient::
    RegisterAssociatedInterfaceBindersForServiceWorker(
        const content::ServiceWorkerVersionBaseInfo&
            service_worker_version_info,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  if (extensions::ExtensionsEnabled()) {
    // From ChromeContentBrowserClientExtensionsPart::
    // ExposeInterfacesToRendererForServiceWorker
    CHECK(service_worker_version_info.process_id !=
          content::ChildProcessHost::kInvalidUniqueID);
    associated_registry.AddInterface<extensions::mojom::RendererHost>(
        base::BindRepeating(&extensions::RendererStartupHelper::BindForRenderer,
                            service_worker_version_info.process_id));
    associated_registry.AddInterface<extensions::mojom::ServiceWorkerHost>(
        base::BindRepeating(&extensions::ServiceWorkerHost::BindReceiver,
                            service_worker_version_info.process_id));
    associated_registry
        .AddInterface<extensions::mojom::RendererAutomationRegistry>(
            base::BindRepeating(
                &extensions::AutomationEventRouter::BindForRenderer,
                service_worker_version_info.process_id));
    associated_registry.AddInterface<extensions::mojom::EventRouter>(
        base::BindRepeating(&extensions::EventRouter::BindForRenderer,
                            service_worker_version_info.process_id));
  }
}

void AlloyContentBrowserClient::
    RegisterAssociatedInterfaceBindersForRenderFrameHost(
        content::RenderFrameHost& render_frame_host,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  associated_registry.AddInterface<printing::mojom::PrintManagerHost>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost>
                 receiver) {
            printing::PrintViewManager::BindPrintManagerHost(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));

  associated_registry.AddInterface<pdf::mojom::PdfService>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<pdf::mojom::PdfService> receiver) {
        pdf::PDFDocumentHelper::BindPdfService(
            std::move(receiver), render_frame_host,
            std::make_unique<ChromePDFDocumentHelperClient>());
      },
      &render_frame_host));

  associated_registry.AddInterface<chrome::mojom::PluginInfoHost>(
      base::BindRepeating(&BindPluginInfoHost,
                          render_frame_host.GetProcess()->GetID()));

  if (extensions::ExtensionsEnabled()) {
    // From ChromeContentBrowserClientExtensionsPart::
    // ExposeInterfacesToRendererForRenderFrameHost
    int render_process_id = render_frame_host.GetProcess()->GetID();
    associated_registry.AddInterface<extensions::mojom::RendererHost>(
        base::BindRepeating(&extensions::RendererStartupHelper::BindForRenderer,
                            render_process_id));
    associated_registry
        .AddInterface<extensions::mojom::RendererAutomationRegistry>(
            base::BindRepeating(
                &extensions::AutomationEventRouter::BindForRenderer,
                render_process_id));
    associated_registry.AddInterface<extensions::mojom::EventRouter>(
        base::BindRepeating(&extensions::EventRouter::BindForRenderer,
                            render_process_id));
    associated_registry.AddInterface<guest_view::mojom::GuestViewHost>(
        base::BindRepeating(
            &extensions::ExtensionsGuestView::CreateForComponents,
            render_frame_host.GetGlobalId()));
    associated_registry.AddInterface<extensions::mojom::GuestView>(
        base::BindRepeating(
            &extensions::ExtensionsGuestView::CreateForExtensions,
            render_frame_host.GetGlobalId()));

    // From ChromeContentBrowserClient::
    // RegisterAssociatedInterfaceBindersForRenderFrameHost
    associated_registry.AddInterface<extensions::mojom::LocalFrameHost>(
        base::BindRepeating(
            [](content::RenderFrameHost* render_frame_host,
               mojo::PendingAssociatedReceiver<
                   extensions::mojom::LocalFrameHost> receiver) {
              extensions::ExtensionWebContentsObserver::BindLocalFrameHost(
                  std::move(receiver), render_frame_host);
            },
            &render_frame_host));
  }
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
AlloyContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* navigation_handle) {
  throttle::NavigationThrottleList throttles;

  if (extensions::ExtensionsEnabled()) {
    auto pdf_iframe_throttle =
        PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(navigation_handle);
    if (pdf_iframe_throttle) {
      throttles.push_back(std::move(pdf_iframe_throttle));
    }

    auto pdf_throttle = pdf::PdfNavigationThrottle::MaybeCreateThrottleFor(
        navigation_handle, std::make_unique<ChromePdfStreamDelegate>());
    if (pdf_throttle) {
      throttles.push_back(std::move(pdf_throttle));
    }
  }

  throttle::CreateThrottlesForNavigation(navigation_handle, throttles);

  return throttles;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
AlloyContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id,
    absl::optional<int64_t> navigation_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  // Used to substitute View ID for PDF contents when using the PDF plugin.
  result.push_back(std::make_unique<PluginResponseInterceptorURLLoaderThrottle>(
      request.destination, frame_tree_node_id));

  Profile* profile = Profile::FromBrowserContext(browser_context);

  if (auto google_throttle = CreateGoogleURLLoaderThrottle(profile)) {
    result.push_back(std::move(google_throttle));
  }

  return result;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
AlloyContentBrowserClient::CreateURLLoaderThrottlesForKeepAlive(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  Profile* profile = Profile::FromBrowserContext(browser_context);

  if (auto google_throttle = CreateGoogleURLLoaderThrottle(profile)) {
    result.push_back(std::move(google_throttle));
  }

  return result;
}

std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
AlloyContentBrowserClient::WillCreateURLLoaderRequestInterceptors(
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id,
    int64_t navigation_id,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner) {
  std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
      interceptors;

  if (extensions::ExtensionsEnabled()) {
    auto pdf_interceptor =
        pdf::PdfURLLoaderRequestInterceptor::MaybeCreateInterceptor(
            frame_tree_node_id, std::make_unique<ChromePdfStreamDelegate>());
    if (pdf_interceptor) {
      interceptors.push_back(std::move(pdf_interceptor));
    }
  }

  return interceptors;
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
void AlloyContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
  int crash_signal_fd = GetCrashSignalFD();
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  //  BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

std::unique_ptr<net::ClientCertStore>
AlloyContentBrowserClient::CreateClientCertStore(
    content::BrowserContext* browser_context) {
  // Match the logic in ProfileNetworkContextService::CreateClientCertStore.
#if BUILDFLAG(USE_NSS_CERTS)
  // TODO: Add support for client implementation of crypto password dialog.
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreNSS(
      net::ClientCertStoreNSS::PasswordDelegateFactory()));
#elif BUILDFLAG(IS_WIN)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreWin());
#elif BUILDFLAG(IS_MAC)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreMac());
#else
#error Unknown platform.
#endif
}

std::unique_ptr<content::LoginDelegate>
AlloyContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const content::GlobalRequestID& request_id,
    bool is_request_for_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return std::make_unique<net_service::LoginDelegate>(
      auth_info, web_contents, request_id, url,
      std::move(auth_required_callback));
}

void AlloyContentBrowserClient::RegisterNonNetworkNavigationURLLoaderFactories(
    int frame_tree_node_id,
    NonNetworkURLLoaderFactoryMap* factories) {
  if (!extensions::ExtensionsEnabled()) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionNavigationURLLoaderFactory(
          web_contents->GetBrowserContext(),
          !!extensions::WebViewGuest::FromWebContents(web_contents)));
}

void AlloyContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    const std::optional<url::Origin>& request_initiator_origin,
    NonNetworkURLLoaderFactoryMap* factories) {
  if (!extensions::ExtensionsEnabled()) {
    return;
  }

  auto factory = extensions::CreateExtensionURLLoaderFactory(render_process_id,
                                                             render_frame_id);
  if (factory) {
    factories->emplace(extensions::kExtensionScheme, std::move(factory));
  }

  content::RenderFrameHost* frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents) {
    return;
  }

  extensions::CefExtensionWebContentsObserver* web_observer =
      extensions::CefExtensionWebContentsObserver::FromWebContents(
          web_contents);

  // There is nothing to do if no CefExtensionWebContentsObserver is attached
  // to the |web_contents|.
  if (!web_observer) {
    return;
  }

  const extensions::Extension* extension =
      web_observer->GetExtensionFromFrame(frame_host, false);
  if (!extension) {
    return;
  }

  std::vector<std::string> allowed_webui_hosts;
  // Support for chrome:// scheme if appropriate.
  if ((extension->is_extension() || extension->is_platform_app()) &&
      extensions::Manifest::IsComponentLocation(extension->location())) {
    // Components of chrome that are implemented as extensions or platform apps
    // are allowed to use chrome://resources/ and chrome://theme/ URLs.
    // See also HasCrossOriginWhitelistEntry.
    allowed_webui_hosts.emplace_back(content::kChromeUIResourcesHost);
    allowed_webui_hosts.emplace_back(chrome::kChromeUIThemeHost);
  }
  if (!allowed_webui_hosts.empty()) {
    factories->emplace(content::kChromeUIScheme,
                       content::CreateWebUIURLLoaderFactory(
                           frame_host, content::kChromeUIScheme,
                           std::move(allowed_webui_hosts)));
  }
}

void AlloyContentBrowserClient::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    std::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner) {
  auto request_handler = net_service::CreateInterceptedRequestHandler(
      browser_context, frame, render_process_id,
      type == URLLoaderFactoryType::kNavigation,
      type == URLLoaderFactoryType::kDownload, request_initiator);

  net_service::ProxyURLLoaderFactory::CreateProxy(
      browser_context, factory_builder, header_client,
      std::move(request_handler));
}

void AlloyContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  DCHECK(g_browser_process);
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  // Need to set up global NetworkService state before anything else uses it.
  DCHECK(SystemNetworkContextManager::GetInstance());
  SystemNetworkContextManager::GetInstance()->OnNetworkServiceCreated(
      network_service);
}

bool AlloyContentBrowserClient::ConfigureNetworkContextParams(
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

  auto cef_context = CefBrowserContext::FromBrowserContext(context);

  Profile* profile = cef_context->AsProfile();
  ProfileNetworkContextService* service =
      ProfileNetworkContextServiceFactory::GetForContext(profile);
  if (service) {
    service->ConfigureNetworkContextParams(in_memory, relative_partition_path,
                                           network_context_params,
                                           cert_verifier_creation_params);
  } else {
    // Set default params.
    network_context_params->user_agent = GetUserAgent();
    network_context_params->accept_language = GetApplicationLocale();
  }

  network_context_params->cookieable_schemes =
      cef_context->GetCookieableSchemes();

  // TODO(cef): Remove this and add required NetworkAnonymizationKey,
  // this is currently not the case and this was not required pre M84.
  network_context_params->require_network_anonymization_key = false;

  return true;
}

// The sandbox may block read/write access from the NetworkService to
// directories that are not returned by this method.
std::vector<base::FilePath>
AlloyContentBrowserClient::GetNetworkContextsParentDirectory() {
  return {GetUserDataPath()};
}

bool AlloyContentBrowserClient::HandleExternalProtocol(
    const GURL& url,
    content::WebContents::Getter web_contents_getter,
    int frame_tree_node_id,
    content::NavigationUIData* navigation_data,
    bool is_primary_main_frame,
    bool is_in_fenced_frame_tree,
    network::mojom::WebSandboxFlags sandbox_flags,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const std::optional<url::Origin>& initiating_origin,
    content::RenderFrameHost* initiator_document,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  // Call the other HandleExternalProtocol variant.
  return false;
}

bool AlloyContentBrowserClient::HandleExternalProtocol(
    content::WebContents::Getter web_contents_getter,
    int frame_tree_node_id,
    content::NavigationUIData* navigation_data,
    bool is_primary_main_frame,
    bool is_in_fenced_frame_tree,
    network::mojom::WebSandboxFlags sandbox_flags,
    const network::ResourceRequest& resource_request,
    const std::optional<url::Origin>& initiating_origin,
    content::RenderFrameHost* initiator_document,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver =
      out_factory->InitWithNewPipeAndPassReceiver();

  // CefBrowserPlatformDelegate::HandleExternalProtocol may be called if
  // nothing handles the request.
  auto request_handler = net_service::CreateInterceptedRequestHandler(
      web_contents_getter, frame_tree_node_id, resource_request,
      base::BindRepeating(CefBrowserPlatformDelegate::HandleExternalProtocol,
                          resource_request.url));

  net_service::ProxyURLLoaderFactory::CreateProxy(
      web_contents_getter, std::move(receiver), std::move(request_handler));
  return true;
}

std::unique_ptr<content::VideoOverlayWindow>
AlloyContentBrowserClient::CreateWindowForVideoPictureInPicture(
    content::VideoPictureInPictureWindowController* controller) {
  // Note: content::VideoOverlayWindow::Create() is defined by platform-specific
  // implementation in chrome/browser/ui/views. This layering hack, which goes
  // through //content and ContentBrowserClient, allows us to work around the
  // dependency constraints that disallow directly calling
  // chrome/browser/ui/views code either from here or from other code in
  // chrome/browser.
  return content::VideoOverlayWindow::Create(controller);
}

void AlloyContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  CefBrowserFrame::RegisterBrowserInterfaceBindersForFrame(render_frame_host,
                                                           map);

  map->Add<blink::mojom::BadgeService>(base::BindRepeating(&BindBadgeService));
#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_SPEECH_SERVICE)
  map->Add<media::mojom::MediaFoundationRendererNotifier>(
      base::BindRepeating(&BindMediaFoundationRendererNotifierHandler));
#endif
  map->Add<network_hints::mojom::NetworkHintsHandler>(
      base::BindRepeating(&BindNetworkHintsHandler));

  map->Add<spellcheck::mojom::SpellCheckHost>(base::BindRepeating(
      [](content::RenderFrameHost* frame_host,
         mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver) {
        SpellCheckHostChromeImpl::Create(frame_host->GetProcess()->GetID(),
                                         std::move(receiver));
      }));

  if (!extensions::ExtensionsEnabled()) {
    return;
  }

  map->Add<extensions::mime_handler::MimeHandlerService>(
      base::BindRepeating(&BindMimeHandlerService));
  map->Add<extensions::mime_handler::BeforeUnloadControl>(
      base::BindRepeating(&BindBeforeUnloadControl));

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return;
  }

  const GURL& site = render_frame_host->GetSiteInstance()->GetSiteURL();
  if (!site.SchemeIs(extensions::kExtensionScheme)) {
    return;
  }

  content::BrowserContext* browser_context =
      render_frame_host->GetProcess()->GetBrowserContext();
  auto* extension = extensions::ExtensionRegistry::Get(browser_context)
                        ->enabled_extensions()
                        .GetByID(site.host());
  if (!extension) {
    return;
  }
  extensions::ExtensionsBrowserClient::Get()
      ->RegisterBrowserInterfaceBindersForFrame(map, render_frame_host,
                                                extension);
}

void AlloyContentBrowserClient::RegisterBrowserInterfaceBindersForServiceWorker(
    content::BrowserContext* browser_context,
    const content::ServiceWorkerVersionBaseInfo& service_worker_version_info,
    mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>*
        map) {
  map->Add<blink::mojom::BadgeService>(
      base::BindRepeating(&BindBadgeServiceForServiceWorker));
}

base::FilePath
AlloyContentBrowserClient::GetSandboxedStorageServiceDataDirectory() {
  return GetUserDataPath();
}

base::FilePath AlloyContentBrowserClient::GetShaderDiskCacheDirectory() {
  return GetUserDataPath().Append(FILE_PATH_LITERAL("ShaderCache"));
}

base::FilePath AlloyContentBrowserClient::GetGrShaderDiskCacheDirectory() {
  return GetUserDataPath().Append(FILE_PATH_LITERAL("GrShaderCache"));
}

base::FilePath AlloyContentBrowserClient::GetGraphiteDawnDiskCacheDirectory() {
  return GetUserDataPath().Append(FILE_PATH_LITERAL("GraphiteDawnCache"));
}

base::FilePath AlloyContentBrowserClient::GetNetLogDefaultDirectory() {
  return GetUserDataPath();
}

base::FilePath AlloyContentBrowserClient::GetFirstPartySetsDirectory() {
  return GetUserDataPath();
}

std::optional<base::FilePath>
AlloyContentBrowserClient::GetLocalTracesDirectory() {
  return GetUserDataPath();
}

std::string AlloyContentBrowserClient::GetProduct() {
  return GetChromeProduct();
}

std::string AlloyContentBrowserClient::GetChromeProduct() {
  return std::string(version_info::GetProductNameAndVersionForUserAgent());
}

std::string AlloyContentBrowserClient::GetUserAgent() {
  return embedder_support::GetUserAgent();
}

std::unique_ptr<content::WebContentsViewDelegate>
AlloyContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<AlloyWebContentsViewDelegate>(web_contents);
}

blink::UserAgentMetadata AlloyContentBrowserClient::GetUserAgentMetadata() {
  return embedder_support::GetUserAgentMetadata(
      g_browser_process->local_state());
}

base::flat_set<std::string>
AlloyContentBrowserClient::GetPluginMimeTypesWithExternalHandlers(
    content::BrowserContext* browser_context) {
  base::flat_set<std::string> mime_types;
  auto map = PluginUtils::GetMimeTypeToExtensionIdMap(browser_context);
  for (const auto& pair : map) {
    mime_types.insert(pair.first);
  }
  mime_types.insert(pdf::kInternalPluginMimeType);
  return mime_types;
}

void AlloyContentBrowserClient::GetMediaDeviceIDSalt(
    content::RenderFrameHost* rfh,
    const net::SiteForCookies& site_for_cookies,
    const blink::StorageKey& storage_key,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  GURL url = rfh->GetLastCommittedURL();
  url::Origin top_frame_origin = rfh->GetMainFrame()->GetLastCommittedOrigin();
  content::BrowserContext* browser_context = rfh->GetBrowserContext();

  // Persistent MediaDevice IDs are allowed if cookies are allowed.
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  bool allowed = cookie_settings->IsFullCookieAccessAllowed(
      url, site_for_cookies, top_frame_origin,
      cookie_settings->SettingOverridesForStorage());
  media_device_salt::MediaDeviceSaltService* salt_service =
      MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
          browser_context);
  if (!salt_service) {
    std::move(callback).Run(allowed, browser_context->UniqueId());
    return;
  }

  salt_service->GetSalt(rfh->GetStorageKey(),
                        base::BindOnce(std::move(callback), allowed));
}

void AlloyContentBrowserClient::OnWebContentsCreated(
    content::WebContents* web_contents) {
  // Attach universal WebContentsObservers. These are quite rare, and in most
  // cases CefBrowserPlatformDelegateAlloy::BrowserCreated and/or
  // CefExtensionsAPIClient::AttachWebContentsHelpers should be used instead.

  if (extensions::ExtensionsEnabled()) {
    extensions::CefExtensionWebContentsObserver::CreateForWebContents(
        web_contents);
  }
}

bool AlloyContentBrowserClient::IsFindInPageDisabledForOrigin(
    const url::Origin& origin) {
  // For PDF viewing with the PPAPI-free PDF Viewer, find-in-page should only
  // display results from the PDF content, and not from the UI.
  return IsPdfExtensionOrigin(origin);
}

CefRefPtr<CefRequestContextImpl> AlloyContentBrowserClient::request_context()
    const {
  return browser_main_parts_->request_context();
}

CefDevToolsDelegate* AlloyContentBrowserClient::devtools_delegate() const {
  return browser_main_parts_->devtools_delegate();
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyContentBrowserClient::background_task_runner() const {
  return browser_main_parts_->background_task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyContentBrowserClient::user_visible_task_runner() const {
  return browser_main_parts_->user_visible_task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyContentBrowserClient::user_blocking_task_runner() const {
  return browser_main_parts_->user_blocking_task_runner();
}

const extensions::Extension* AlloyContentBrowserClient::GetExtension(
    content::SiteInstance* site_instance) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(site_instance->GetBrowserContext());
  if (!registry) {
    return nullptr;
  }
  return registry->enabled_extensions().GetExtensionOrAppByURL(
      site_instance->GetSiteURL());
}
