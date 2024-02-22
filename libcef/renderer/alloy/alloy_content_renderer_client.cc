// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/alloy/alloy_content_renderer_client.h"

#include <memory>
#include <utility>

#include "build/build_config.h"

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wdeprecated-declarations"
#else
#pragma warning(push)
#pragma warning(default : 4996)
#endif
#endif

#include "libcef/browser/alloy/alloy_content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/common/alloy/alloy_content_client.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_client.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/features/runtime_checks.h"
#include "libcef/renderer/alloy/alloy_render_thread_observer.h"
#include "libcef/renderer/alloy/url_loader_throttle_provider_impl.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/extensions/extensions_renderer_client.h"
#include "libcef/renderer/extensions/print_render_frame_helper_delegate.h"
#include "libcef/renderer/render_frame_observer.h"
#include "libcef/renderer/render_manager.h"
#include "libcef/renderer/thread_util.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics_action.h"
#include "base/path_service.h"
#include "base/process/current_process.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pdf_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/browser_exposed_renderer_interfaces.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/media/chrome_key_systems.h"
#include "chrome/renderer/plugins/chrome_plugin_placeholder.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/nacl/common/nacl_constants.h"
#include "components/pdf/common/constants.h"
#include "components/pdf/renderer/internal_plugin_renderer_helpers.h"
#include "components/printing/renderer/print_render_frame_helper.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "ipc/ipc_sync_channel.h"
#include "media/base/media.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "printing/print_settings.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/scheduler/web_renderer_process_type.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_controller.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#endif

AlloyContentRendererClient::AlloyContentRendererClient()
    : main_entry_time_(base::TimeTicks::Now()),
      render_manager_(new CefRenderManager) {
  if (extensions::ExtensionsEnabled()) {
    extensions_client_ = std::make_unique<extensions::CefExtensionsClient>();
    extensions::ExtensionsClient::Set(extensions_client_.get());
    extensions_renderer_client_ =
        std::make_unique<extensions::CefExtensionsRendererClient>(this);
    extensions::ExtensionsRendererClient::Set(
        extensions_renderer_client_.get());
  }
}

AlloyContentRendererClient::~AlloyContentRendererClient() = default;

// static
AlloyContentRendererClient* AlloyContentRendererClient::Get() {
  REQUIRE_ALLOY_RUNTIME();
  return static_cast<AlloyContentRendererClient*>(
      CefAppManager::Get()->GetContentClient()->renderer());
}

scoped_refptr<base::SingleThreadTaskRunner>
AlloyContentRendererClient::GetCurrentTaskRunner() {
  // Check if currently on the render thread.
  if (CEF_CURRENTLY_ON_RT()) {
    return render_task_runner_;
  }
  return nullptr;
}

void AlloyContentRendererClient::RunSingleProcessCleanup() {
  DCHECK(content::RenderProcessHost::run_renderer_in_process());

  // Make sure the render thread was actually started.
  if (!render_task_runner_.get()) {
    return;
  }

  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    RunSingleProcessCleanupOnUIThread();
  } else {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(
            &AlloyContentRendererClient::RunSingleProcessCleanupOnUIThread,
            base::Unretained(this)));
  }

  // Wait for the render thread cleanup to complete. Spin instead of using
  // base::WaitableEvent because calling Wait() is not allowed on the UI
  // thread.
  bool complete = false;
  do {
    {
      base::AutoLock lock_scope(single_process_cleanup_lock_);
      complete = single_process_cleanup_complete_;
    }
    if (!complete) {
      base::PlatformThread::YieldCurrentThread();
    }
  } while (!complete);
}

void AlloyContentRendererClient::PostIOThreadCreated(
    base::SingleThreadTaskRunner*) {
  // TODO(cef): Enable these once the implementation supports it.
  blink::WebRuntimeFeatures::EnableNotifications(false);
  blink::WebRuntimeFeatures::EnablePushMessaging(false);
}

void AlloyContentRendererClient::RenderThreadStarted() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  render_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  observer_ = std::make_unique<AlloyRenderThreadObserver>();
  web_cache_impl_ = std::make_unique<web_cache::WebCacheImpl>();
  visited_link_slave_ = std::make_unique<visitedlink::VisitedLinkReader>();

  content::RenderThread* thread = content::RenderThread::Get();

  const bool is_extension = CefRenderManager::IsExtensionProcess();

  thread->SetRendererProcessType(
      is_extension
          ? blink::scheduler::WebRendererProcessType::kExtensionRenderer
          : blink::scheduler::WebRendererProcessType::kRenderer);

  if (is_extension) {
    // The process name was set to "Renderer" in RendererMain(). Update it to
    // "Extension Renderer" to highlight that it's hosting an extension.
    base::CurrentProcess::GetInstance().SetProcessType(
        base::CurrentProcessType::PROCESS_RENDERER_EXTENSION);
  }

  thread->AddObserver(observer_.get());

  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) {
    spellcheck_ = std::make_unique<SpellCheck>(this);
  }

  if (content::RenderProcessHost::run_renderer_in_process()) {
    // When running in single-process mode register as a destruction observer
    // on the render thread's MessageLoop.
    base::CurrentThread::Get()->AddDestructionObserver(this);
  }

#if BUILDFLAG(IS_MAC)
  {
    base::apple::ScopedCFTypeRef<CFStringRef> key(
        base::SysUTF8ToCFStringRef("NSScrollViewRubberbanding"));
    base::apple::ScopedCFTypeRef<CFStringRef> value;

    // If the command-line switch is specified then set the value that will be
    // checked in RenderThreadImpl::Init(). Otherwise, remove the application-
    // level value.
    if (command_line->HasSwitch(switches::kDisableScrollBounce)) {
      value.reset(base::SysUTF8ToCFStringRef("false"));
    }

    CFPreferencesSetAppValue(key.get(), value.get(),
                             kCFPreferencesCurrentApplication);
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
  }
#endif  // BUILDFLAG(IS_MAC)

  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->RenderThreadStarted();
  }
}

void AlloyContentRendererClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();

  binders->Add<web_cache::mojom::WebCache>(
      base::BindRepeating(&web_cache::WebCacheImpl::BindReceiver,
                          base::Unretained(web_cache_impl_.get())),
      task_runner);

  binders->Add<visitedlink::mojom::VisitedLinkNotificationSink>(
      visited_link_slave_->GetBindCallback(), task_runner);

  if (spellcheck_) {
    binders->Add<spellcheck::mojom::SpellChecker>(
        base::BindRepeating(
            [](SpellCheck* spellcheck,
               mojo::PendingReceiver<spellcheck::mojom::SpellChecker>
                   receiver) { spellcheck->BindReceiver(std::move(receiver)); },
            base::Unretained(spellcheck_.get())),
        task_runner);
  }

  render_manager_->ExposeInterfacesToBrowser(binders);
}

void AlloyContentRendererClient::RenderThreadConnected() {
  // Register extensions last because it will trigger WebKit initialization.
  blink::WebScriptController::RegisterExtension(
      extensions_v8::LoadTimesExtension::Get());

  render_manager_->RenderThreadConnected();
}

void AlloyContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  auto render_frame_observer = new CefRenderFrameObserver(render_frame);

  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->RenderFrameCreated(
        render_frame, render_frame_observer->registry());

    render_frame_observer->associated_interfaces()
        ->AddInterface<extensions::mojom::MimeHandlerViewContainerManager>(
            base::BindRepeating(
                &extensions::MimeHandlerViewContainerManager::BindReceiver,
                base::Unretained(render_frame)));
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) {
    new SpellCheckProvider(render_frame, spellcheck_.get());
  }

  bool browser_created;
  absl::optional<bool> is_windowless;
  render_manager_->RenderFrameCreated(render_frame, render_frame_observer,
                                      browser_created, is_windowless);
  if (browser_created) {
    OnBrowserCreated(render_frame->GetWebView(), is_windowless);
  }

  if (is_windowless.has_value()) {
    new printing::PrintRenderFrameHelper(
        render_frame,
        base::WrapUnique(
            new extensions::CefPrintRenderFrameHelperDelegate(*is_windowless)));
  }
}

void AlloyContentRendererClient::WebViewCreated(
    blink::WebView* web_view,
    bool was_created_by_renderer,
    const url::Origin* outermost_origin) {
  bool browser_created;
  absl::optional<bool> is_windowless;
  render_manager_->WebViewCreated(web_view, browser_created, is_windowless);
  if (browser_created) {
    OnBrowserCreated(web_view, is_windowless);
  }
}

bool AlloyContentRendererClient::IsPluginHandledExternally(
    content::RenderFrame* render_frame,
    const blink::WebElement& plugin_element,
    const GURL& original_url,
    const std::string& mime_type) {
  if (!extensions::ExtensionsEnabled()) {
    return false;
  }

  DCHECK(plugin_element.HasHTMLTagName("object") ||
         plugin_element.HasHTMLTagName("embed"));
  // Blink will next try to load a WebPlugin which would end up in
  // OverrideCreatePlugin, sending another IPC only to find out the plugin is
  // not supported. Here it suffices to return false but there should perhaps be
  // a more unified approach to avoid sending the IPC twice.
  chrome::mojom::PluginInfoPtr plugin_info = chrome::mojom::PluginInfo::New();
  mojo::AssociatedRemote<chrome::mojom::PluginInfoHost> plugin_info_host;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
      &plugin_info_host);
  plugin_info_host->GetPluginInfo(
      original_url, render_frame->GetWebFrame()->Top()->GetSecurityOrigin(),
      mime_type, &plugin_info);
  // TODO(ekaramad): Not continuing here due to a disallowed status should take
  // us to CreatePlugin. See if more in depths investigation of |status| is
  // necessary here (see https://crbug.com/965747). For now, returning false
  // should take us to CreatePlugin after HTMLPlugInElement which is called
  // through HTMLPlugInElement::LoadPlugin code path.
  if (plugin_info->status != chrome::mojom::PluginStatus::kAllowed &&
      plugin_info->status !=
          chrome::mojom::PluginStatus::kPlayImportantContent) {
    // We could get here when a MimeHandlerView is loaded inside a <webview>
    // which is using permissions API (see WebViewPluginTests).
    ChromeExtensionsRendererClient::DidBlockMimeHandlerViewForDisallowedPlugin(
        plugin_element);
    return false;
  }
  if (plugin_info->actual_mime_type == pdf::kInternalPluginMimeType) {
    // Only actually treat the internal PDF plugin as externally handled if
    // used within an origin allowed to create the internal PDF plugin;
    // otherwise, let Blink try to create the in-process PDF plugin.
    if (IsPdfInternalPluginAllowedOrigin(
            render_frame->GetWebFrame()->GetSecurityOrigin())) {
      return true;
    }
  }
  return ChromeExtensionsRendererClient::MaybeCreateMimeHandlerView(
      plugin_element, original_url, plugin_info->actual_mime_type,
      plugin_info->plugin);
}

bool AlloyContentRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  std::string orig_mime_type = params.mime_type.Utf8();
  if (extensions::ExtensionsEnabled() &&
      !extensions_renderer_client_->OverrideCreatePlugin(render_frame,
                                                         params)) {
    return false;
  }

  GURL url(params.url);
  chrome::mojom::PluginInfoPtr plugin_info = chrome::mojom::PluginInfo::New();
  mojo::AssociatedRemote<chrome::mojom::PluginInfoHost> plugin_info_host;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
      &plugin_info_host);
  plugin_info_host->GetPluginInfo(
      url, render_frame->GetWebFrame()->Top()->GetSecurityOrigin(),
      orig_mime_type, &plugin_info);
  *plugin = ChromeContentRendererClient::CreatePlugin(render_frame, params,
                                                      *plugin_info);
  return true;
}

void AlloyContentRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* initiator_origin,
    GURL* new_url) {
  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->WillSendRequest(frame, transition_type, url,
                                                 site_for_cookies,
                                                 initiator_origin, new_url);
    if (!new_url->is_empty()) {
      return;
    }
  }
}

uint64_t AlloyContentRendererClient::VisitedLinkHash(
    std::string_view canonical_url) {
  return visited_link_slave_->ComputeURLFingerprint(canonical_url);
}

bool AlloyContentRendererClient::IsLinkVisited(uint64_t link_hash) {
  return visited_link_slave_->IsVisited(link_hash);
}

bool AlloyContentRendererClient::IsOriginIsolatedPepperPlugin(
    const base::FilePath& plugin_path) {
  // Isolate all the plugins (including the PDF plugin).
  return true;
}

void AlloyContentRendererClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  GetChromeKeySystems(std::move(cb));
}

void AlloyContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->RunScriptsAtDocumentStart(render_frame);
  }
}

void AlloyContentRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->RunScriptsAtDocumentEnd(render_frame);
  }
}

void AlloyContentRendererClient::RunScriptsAtDocumentIdle(
    content::RenderFrame* render_frame) {
  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->RunScriptsAtDocumentIdle(render_frame);
  }
}

void AlloyContentRendererClient::DevToolsAgentAttached() {
  // WebWorkers may be creating agents on a different thread.
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AlloyContentRendererClient::DevToolsAgentAttached,
                       base::Unretained(this)));
    return;
  }

  render_manager_->DevToolsAgentAttached();
}

void AlloyContentRendererClient::DevToolsAgentDetached() {
  // WebWorkers may be creating agents on a different thread.
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AlloyContentRendererClient::DevToolsAgentDetached,
                       base::Unretained(this)));
    return;
  }

  render_manager_->DevToolsAgentDetached();
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
AlloyContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType provider_type) {
  return std::make_unique<CefURLLoaderThrottleProviderImpl>(provider_type,
                                                            this);
}

void AlloyContentRendererClient::AppendContentSecurityPolicy(
    const blink::WebURL& url,
    blink::WebVector<blink::WebContentSecurityPolicyHeader>* csp) {
  if (!extensions::ExtensionsEnabled()) {
    return;
  }

  // Don't apply default CSP to PDF renderers.
  // TODO(crbug.com/1252096): Lock down the CSP once style and script are no
  // longer injected inline by `pdf::PluginResponseWriter`. That class may be a
  // better place to define such CSP, or we may continue doing so here.
  if (pdf::IsPdfRenderer()) {
    return;
  }

  DCHECK(csp);
  GURL gurl(url);
  const extensions::Extension* extension =
      extensions::RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(
          gurl);
  if (!extension) {
    return;
  }

  // Append a minimum CSP to ensure the extension can't relax the default
  // applied CSP through means like Service Worker.
  const std::string* default_csp =
      extensions::CSPInfo::GetMinimumCSPToAppend(*extension, gurl.path());
  if (!default_csp) {
    return;
  }

  csp->push_back({blink::WebString::FromUTF8(*default_csp),
                  network::mojom::ContentSecurityPolicyType::kEnforce,
                  network::mojom::ContentSecurityPolicySource::kHTTP});
}

void AlloyContentRendererClient::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // TODO(crbug.com/977637): Get rid of the use of this implementation of
  // |service_manager::LocalInterfaceProvider|. This was done only to avoid
  // churning spellcheck code while eliminating the "chrome" and
  // "chrome_renderer" services. Spellcheck is (and should remain) the only
  // consumer of this implementation.
  content::RenderThread::Get()->BindHostReceiver(
      mojo::GenericPendingReceiver(interface_name, std::move(interface_pipe)));
}

void AlloyContentRendererClient::WillDestroyCurrentMessageLoop() {
  base::AutoLock lock_scope(single_process_cleanup_lock_);
  single_process_cleanup_complete_ = true;
}

void AlloyContentRendererClient::OnBrowserCreated(
    blink::WebView* web_view,
    absl::optional<bool> is_windowless) {
#if BUILDFLAG(IS_MAC)
  const bool windowless = is_windowless.has_value() && *is_windowless;

  // FIXME: It would be better if this API would be a callback from the
  // WebKit layer, or if it would be exposed as an WebView instance method; the
  // current implementation uses a static variable, and WebKit needs to be
  // patched in order to make it work for each WebView instance
  web_view->SetUseExternalPopupMenusThisInstance(!windowless);
#endif
}

void AlloyContentRendererClient::RunSingleProcessCleanupOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Clean up the single existing RenderProcessHost.
  content::RenderProcessHost* host = nullptr;
  content::RenderProcessHost::iterator iterator(
      content::RenderProcessHost::AllHostsIterator());
  if (!iterator.IsAtEnd()) {
    host = iterator.GetCurrentValue();
    host->Cleanup();
    iterator.Advance();
    DCHECK(iterator.IsAtEnd());
  }
  DCHECK(host);

  // Clear the run_renderer_in_process() flag to avoid a DCHECK in the
  // RenderProcessHost destructor.
  content::RenderProcessHost::SetRunRendererInProcess(false);

  // Deletion of the RenderProcessHost object will stop the render thread and
  // result in a call to WillDestroyCurrentMessageLoop.
  // Cleanup() will cause deletion to be posted as a task on the UI thread but
  // this task will only execute when running in multi-threaded message loop
  // mode (because otherwise the UI message loop has already stopped). Therefore
  // we need to explicitly delete the object when not running in this mode.
  if (!CefContext::Get()->settings().multi_threaded_message_loop) {
    delete host;
  }
}

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif
#endif
