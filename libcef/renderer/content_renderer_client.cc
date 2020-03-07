// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/content_renderer_client.h"

#include <utility>

#include "base/compiler_specific.h"

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if defined(OS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wdeprecated-declarations"
#else
#pragma warning(push)
#pragma warning(default : 4996)
#endif
#endif

#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/content_client.h"
#include "libcef/common/extensions/extensions_client.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/values_impl.h"
#include "libcef/renderer/blink_glue.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/extensions/extensions_renderer_client.h"
#include "libcef/renderer/extensions/print_render_frame_helper_delegate.h"
#include "libcef/renderer/render_frame_observer.h"
#include "libcef/renderer/render_thread_observer.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/renderer/url_loader_throttle_provider_impl.h"
#include "libcef/renderer/v8_impl.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics_action.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/browser_exposed_renderer_interfaces.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/media/chrome_key_systems.h"
#include "chrome/renderer/pepper/chrome_pdf_print_client.h"
#include "chrome/renderer/pepper/pepper_helper.h"
#include "chrome/renderer/plugins/chrome_plugin_placeholder.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/nacl/common/nacl_constants.h"
#include "components/printing/renderer/print_render_frame_helper.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/plugin_instance_throttler.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/renderer/render_widget.h"
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
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_renderer_process_type.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_prerendering_support.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_prerenderer_client.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#endif

namespace {

// Stub implementation of blink::WebPrerenderingSupport.
class CefPrerenderingSupport : public blink::WebPrerenderingSupport {
 private:
  void Add(const blink::WebPrerender& prerender) override {}
  void Cancel(const blink::WebPrerender& prerender) override {}
  void Abandon(const blink::WebPrerender& prerender) override {}
  void PrefetchFinished() override {}
};

// Stub implementation of blink::WebPrerendererClient.
class CefPrerendererClient : public content::RenderViewObserver,
                             public blink::WebPrerendererClient {
 public:
  explicit CefPrerendererClient(content::RenderView* render_view)
      : content::RenderViewObserver(render_view) {
    DCHECK(render_view);
    render_view->GetWebView()->SetPrerendererClient(this);
  }

 private:
  ~CefPrerendererClient() override {}

  // RenderViewObserver methods:
  void OnDestruct() override { delete this; }

  // WebPrerendererClient methods:
  void WillAddPrerender(blink::WebPrerender* prerender) override {}
  bool IsPrefetchOnly() override { return false; }
};

bool IsStandaloneExtensionProcess() {
  return extensions::ExtensionsEnabled() &&
         extensions::CefExtensionsRendererClient::
             IsStandaloneExtensionProcess();
}

}  // namespace

// Placeholder object for guest views.
class CefGuestView : public content::RenderViewObserver {
 public:
  CefGuestView(content::RenderView* render_view, bool is_windowless)
      : content::RenderViewObserver(render_view),
        is_windowless_(is_windowless) {}

  bool is_windowless() const { return is_windowless_; }

 private:
  // RenderViewObserver methods.
  void OnDestruct() override {
    CefContentRendererClient::Get()->OnGuestViewDestroyed(this);
  }

  const bool is_windowless_;
};

CefContentRendererClient::CefContentRendererClient()
    : main_entry_time_(base::TimeTicks::Now()),
      devtools_agent_count_(0),
      uncaught_exception_stack_size_(0),
      single_process_cleanup_complete_(false) {
  if (extensions::ExtensionsEnabled()) {
    extensions_client_.reset(new extensions::CefExtensionsClient);
    extensions::ExtensionsClient::Set(extensions_client_.get());
    extensions_renderer_client_.reset(
        new extensions::CefExtensionsRendererClient);
    extensions::ExtensionsRendererClient::Set(
        extensions_renderer_client_.get());
  }
}

CefContentRendererClient::~CefContentRendererClient() {}

// static
CefContentRendererClient* CefContentRendererClient::Get() {
  return static_cast<CefContentRendererClient*>(
      CefContentClient::Get()->renderer());
}

CefRefPtr<CefBrowserImpl> CefContentRendererClient::GetBrowserForView(
    content::RenderView* view) {
  CEF_REQUIRE_RT_RETURN(nullptr);

  BrowserMap::const_iterator it = browsers_.find(view);
  if (it != browsers_.end())
    return it->second;
  return nullptr;
}

CefRefPtr<CefBrowserImpl> CefContentRendererClient::GetBrowserForMainFrame(
    blink::WebFrame* frame) {
  CEF_REQUIRE_RT_RETURN(nullptr);

  BrowserMap::const_iterator it = browsers_.begin();
  for (; it != browsers_.end(); ++it) {
    content::RenderView* render_view = it->second->render_view();
    if (render_view && render_view->GetWebView() &&
        render_view->GetWebView()->MainFrame() == frame) {
      return it->second;
    }
  }

  return nullptr;
}

void CefContentRendererClient::OnBrowserDestroyed(CefBrowserImpl* browser) {
  BrowserMap::iterator it = browsers_.begin();
  for (; it != browsers_.end(); ++it) {
    if (it->second.get() == browser) {
      browsers_.erase(it);
      return;
    }
  }

  // No browser was found in the map.
  NOTREACHED();
}

CefGuestView* CefContentRendererClient::GetGuestViewForView(
    content::RenderView* view) {
  CEF_REQUIRE_RT_RETURN(nullptr);

  GuestViewMap::const_iterator it = guest_views_.find(view);
  if (it != guest_views_.end())
    return it->second.get();
  return nullptr;
}

void CefContentRendererClient::OnGuestViewDestroyed(CefGuestView* guest_view) {
  GuestViewMap::iterator it = guest_views_.begin();
  for (; it != guest_views_.end(); ++it) {
    if (it->second.get() == guest_view) {
      guest_views_.erase(it);
      return;
    }
  }

  // No guest view was found in the map.
  NOTREACHED();
}

blink::WebURLLoaderFactory*
CefContentRendererClient::GetDefaultURLLoaderFactory() {
  if (!default_url_loader_factory_) {
    default_url_loader_factory_ =
        blink::Platform::Current()->CreateDefaultURLLoaderFactory();
  }
  return default_url_loader_factory_.get();
}

void CefContentRendererClient::WebKitInitialized() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Create global objects associated with the default Isolate.
  CefV8IsolateCreated();

  // TODO(cef): Enable these once the implementation supports it.
  blink::WebRuntimeFeatures::EnableNotifications(false);

  const CefContentClient::SchemeInfoList* schemes =
      CefContentClient::Get()->GetCustomSchemes();
  if (!schemes->empty()) {
    // Register the custom schemes. The |is_standard| value is excluded here
    // because it's not explicitly registered with Blink.
    CefContentClient::SchemeInfoList::const_iterator it = schemes->begin();
    for (; it != schemes->end(); ++it) {
      const CefContentClient::SchemeInfo& info = *it;
      const blink::WebString& scheme =
          blink::WebString::FromUTF8(info.scheme_name);
      if (info.is_local)
        blink_glue::RegisterURLSchemeAsLocal(scheme);
      if (info.is_display_isolated)
        blink::WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(scheme);
      if (info.is_secure)
        blink_glue::RegisterURLSchemeAsSecure(scheme);
      if (info.is_fetch_enabled)
        blink_glue::RegisterURLSchemeAsSupportingFetchAPI(scheme);
    }
  }

  if (!cross_origin_whitelist_entries_.empty()) {
    // Add the cross-origin white list entries.
    for (size_t i = 0; i < cross_origin_whitelist_entries_.size(); ++i) {
      const Cef_CrossOriginWhiteListEntry_Params& entry =
          cross_origin_whitelist_entries_[i];
      GURL gurl = GURL(entry.source_origin);
      blink::WebSecurityPolicy::AddOriginAccessAllowListEntry(
          gurl, blink::WebString::FromUTF8(entry.target_protocol),
          blink::WebString::FromUTF8(entry.target_domain),
          /*destination_port=*/0,
          entry.allow_target_subdomains
              ? network::mojom::CorsDomainMatchMode::kAllowSubdomains
              : network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
          network::mojom::CorsPortMatchMode::kAllowAnyPort,
          network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    }
    cross_origin_whitelist_entries_.clear();
  }

  // The number of stack trace frames to capture for uncaught exceptions.
  if (command_line->HasSwitch(switches::kUncaughtExceptionStackSize)) {
    int uncaught_exception_stack_size = 0;
    base::StringToInt(command_line->GetSwitchValueASCII(
                          switches::kUncaughtExceptionStackSize),
                      &uncaught_exception_stack_size);

    if (uncaught_exception_stack_size > 0) {
      uncaught_exception_stack_size_ = uncaught_exception_stack_size;
      CefV8SetUncaughtExceptionStackSize(uncaught_exception_stack_size_);
    }
  }

  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get())
      handler->OnWebKitInitialized();
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
CefContentRendererClient::GetCurrentTaskRunner() {
  // Check if currently on the render thread.
  if (CEF_CURRENTLY_ON_RT())
    return render_task_runner_;
  return nullptr;
}

void CefContentRendererClient::RunSingleProcessCleanup() {
  DCHECK(content::RenderProcessHost::run_renderer_in_process());

  // Make sure the render thread was actually started.
  if (!render_task_runner_.get())
    return;

  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    RunSingleProcessCleanupOnUIThread();
  } else {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::Bind(&CefContentRendererClient::RunSingleProcessCleanupOnUIThread,
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
    if (!complete)
      base::PlatformThread::YieldCurrentThread();
  } while (!complete);
}

void CefContentRendererClient::RenderThreadStarted() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  render_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  observer_ = std::make_unique<CefRenderThreadObserver>();
  web_cache_impl_ = std::make_unique<web_cache::WebCacheImpl>();
  visited_link_slave_ = std::make_unique<visitedlink::VisitedLinkReader>();

  content::RenderThread* thread = content::RenderThread::Get();

  thread->SetRendererProcessType(
      IsStandaloneExtensionProcess()
          ? blink::scheduler::WebRendererProcessType::kExtensionRenderer
          : blink::scheduler::WebRendererProcessType::kRenderer);

  thread->AddObserver(observer_.get());

  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) {
    spellcheck_ = std::make_unique<SpellCheck>(this);
  }

  if (content::RenderProcessHost::run_renderer_in_process()) {
    // When running in single-process mode register as a destruction observer
    // on the render thread's MessageLoop.
    base::MessageLoopCurrent::Get()->AddDestructionObserver(this);
  }

  blink::WebPrerenderingSupport::Initialize(new CefPrerenderingSupport());

#if defined(OS_MACOSX)
  {
    base::ScopedCFTypeRef<CFStringRef> key(
        base::SysUTF8ToCFStringRef("NSScrollViewRubberbanding"));
    base::ScopedCFTypeRef<CFStringRef> value;

    // If the command-line switch is specified then set the value that will be
    // checked in RenderThreadImpl::Init(). Otherwise, remove the application-
    // level value.
    if (command_line->HasSwitch(switches::kDisableScrollBounce))
      value.reset(base::SysUTF8ToCFStringRef("false"));

    CFPreferencesSetAppValue(key, value, kCFPreferencesCurrentApplication);
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
  }
#endif  // defined(OS_MACOSX)

  if (extensions::PdfExtensionEnabled()) {
    pdf_print_client_.reset(new ChromePDFPrintClient());
    pdf::PepperPDFHost::SetPrintClient(pdf_print_client_.get());
  }

  for (auto& origin_or_hostname_pattern :
       network::SecureOriginAllowlist::GetInstance().GetCurrentAllowlist()) {
    blink::WebSecurityPolicy::AddOriginToTrustworthySafelist(
        blink::WebString::FromUTF8(origin_or_hostname_pattern));
  }

  if (extensions::ExtensionsEnabled())
    extensions_renderer_client_->RenderThreadStarted();
}

void CefContentRendererClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  auto task_runner = base::SequencedTaskRunnerHandle::Get();

  binders->Add(base::BindRepeating(&web_cache::WebCacheImpl::BindReceiver,
                                   base::Unretained(web_cache_impl_.get())),
               task_runner);

  binders->Add(visited_link_slave_->GetBindCallback(), task_runner);

  if (spellcheck_) {
    binders->Add(
        base::BindRepeating(
            [](SpellCheck* spellcheck,
               mojo::PendingReceiver<spellcheck::mojom::SpellChecker>
                   receiver) { spellcheck->BindReceiver(std::move(receiver)); },
            base::Unretained(spellcheck_.get())),
        task_runner);
  }
}

void CefContentRendererClient::RenderThreadConnected() {
  content::RenderThread* thread = content::RenderThread::Get();

  // Retrieve the new render thread information synchronously.
  CefProcessHostMsg_GetNewRenderThreadInfo_Params params;
  thread->Send(new CefProcessHostMsg_GetNewRenderThreadInfo(&params));

  // Cross-origin entries need to be added after WebKit is initialized.
  cross_origin_whitelist_entries_ = params.cross_origin_whitelist_entries;

  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefListValueImpl> listValuePtr(
          new CefListValueImpl(&params.extra_info, false, true));
      handler->OnRenderThreadCreated(listValuePtr.get());
      listValuePtr->Detach(nullptr);
    }
  }

  // Register extensions last because it will trigger WebKit initialization.
  thread->RegisterExtension(extensions_v8::LoadTimesExtension::Get());

  WebKitInitialized();
}

void CefContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  CefRenderFrameObserver* render_frame_observer =
      new CefRenderFrameObserver(render_frame);
  service_manager::BinderRegistry* registry = render_frame_observer->registry();

  new PepperHelper(render_frame);

  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->RenderFrameCreated(render_frame, registry);

    blink::AssociatedInterfaceRegistry* associated_interfaces =
        render_frame_observer->associated_interfaces();
    associated_interfaces->AddInterface(base::BindRepeating(
        &extensions::MimeHandlerViewContainerManager::BindReceiver,
        render_frame->GetRoutingID()));
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) {
    new SpellCheckProvider(render_frame, spellcheck_.get(), this);
  }

  base::Optional<bool> is_windowless;

  auto browser = MaybeCreateBrowser(render_frame->GetRenderView(), render_frame,
                                    &is_windowless);
  if (browser) {
    // Attach the frame to the observer for message routing purposes.
    render_frame_observer->AttachFrame(
        browser->GetWebFrameImpl(render_frame->GetWebFrame()).get());
  }

  if (is_windowless.has_value()) {
    new printing::PrintRenderFrameHelper(
        render_frame,
        base::WrapUnique(
            new extensions::CefPrintRenderFrameHelperDelegate(*is_windowless)));
  }
}

void CefContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  new CefPrerendererClient(render_view);

  MaybeCreateBrowser(render_view, render_view->GetMainRenderFrame(), nullptr);
}

bool CefContentRendererClient::IsPluginHandledExternally(
    content::RenderFrame* render_frame,
    const blink::WebElement& plugin_element,
    const GURL& original_url,
    const std::string& mime_type) {
  if (!extensions::ExtensionsEnabled())
    return false;

  DCHECK(plugin_element.HasHTMLTagName("object") ||
         plugin_element.HasHTMLTagName("embed"));
  // Blink will next try to load a WebPlugin which would end up in
  // OverrideCreatePlugin, sending another IPC only to find out the plugin is
  // not supported. Here it suffices to return false but there should perhaps be
  // a more unified approach to avoid sending the IPC twice.
  chrome::mojom::PluginInfoPtr plugin_info = chrome::mojom::PluginInfo::New();
  ChromeContentRendererClient::GetPluginInfoHost()->GetPluginInfo(
      render_frame->GetRoutingID(), original_url,
      render_frame->GetWebFrame()->Top()->GetSecurityOrigin(), mime_type,
      &plugin_info);
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
  return ChromeExtensionsRendererClient::MaybeCreateMimeHandlerView(
      plugin_element, original_url, plugin_info->actual_mime_type,
      plugin_info->plugin);
}

bool CefContentRendererClient::OverrideCreatePlugin(
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
  ChromeContentRendererClient::GetPluginInfoHost()->GetPluginInfo(
      render_frame->GetRoutingID(), url,
      render_frame->GetWebFrame()->Top()->GetSecurityOrigin(), orig_mime_type,
      &plugin_info);
  *plugin = ChromeContentRendererClient::CreatePlugin(render_frame, params,
                                                      *plugin_info);
  return true;
}

void CefContentRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* initiator_origin,
    GURL* new_url,
    bool* attach_same_site_cookies) {
  if (extensions::ExtensionsEnabled()) {
    extensions_renderer_client_->WillSendRequest(
        frame, transition_type, url, site_for_cookies, initiator_origin,
        new_url, attach_same_site_cookies);
    if (!new_url->is_empty())
      return;
  }
}

uint64_t CefContentRendererClient::VisitedLinkHash(const char* canonical_url,
                                                   size_t length) {
  return visited_link_slave_->ComputeURLFingerprint(canonical_url, length);
}

bool CefContentRendererClient::IsLinkVisited(uint64_t link_hash) {
  return visited_link_slave_->IsVisited(link_hash);
}

bool CefContentRendererClient::IsOriginIsolatedPepperPlugin(
    const base::FilePath& plugin_path) {
  return plugin_path ==
         base::FilePath::FromUTF8Unsafe(CefContentClient::kPDFPluginPath);
}

content::BrowserPluginDelegate*
CefContentRendererClient::CreateBrowserPluginDelegate(
    content::RenderFrame* render_frame,
    const content::WebPluginInfo& info,
    const std::string& mime_type,
    const GURL& original_url) {
  DCHECK(extensions::ExtensionsEnabled());
  return extensions::CefExtensionsRendererClient::CreateBrowserPluginDelegate(
      render_frame, info, mime_type, original_url);
}

void CefContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>* key_systems) {
  AddChromeKeySystems(key_systems);
}

void CefContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  if (extensions::ExtensionsEnabled())
    extensions_renderer_client_->RunScriptsAtDocumentStart(render_frame);
}

void CefContentRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  if (extensions::ExtensionsEnabled())
    extensions_renderer_client_->RunScriptsAtDocumentEnd(render_frame);
}

void CefContentRendererClient::RunScriptsAtDocumentIdle(
    content::RenderFrame* render_frame) {
  if (extensions::ExtensionsEnabled())
    extensions_renderer_client_->RunScriptsAtDocumentIdle(render_frame);
}

void CefContentRendererClient::DevToolsAgentAttached() {
  // WebWorkers may be creating agents on a different thread.
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CefContentRendererClient::DevToolsAgentAttached,
                       base::Unretained(this)));
    return;
  }

  ++devtools_agent_count_;
}

void CefContentRendererClient::DevToolsAgentDetached() {
  // WebWorkers may be creating agents on a different thread.
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CefContentRendererClient::DevToolsAgentDetached,
                       base::Unretained(this)));
    return;
  }

  --devtools_agent_count_;
  if (devtools_agent_count_ == 0 && uncaught_exception_stack_size_ > 0) {
    // When the last DevToolsAgent is detached the stack size is set to 0.
    // Restore the user-specified stack size here.
    CefV8SetUncaughtExceptionStackSize(uncaught_exception_stack_size_);
  }
}

std::unique_ptr<content::URLLoaderThrottleProvider>
CefContentRendererClient::CreateURLLoaderThrottleProvider(
    content::URLLoaderThrottleProviderType provider_type) {
  return std::make_unique<CefURLLoaderThrottleProviderImpl>(provider_type);
}

bool CefContentRendererClient::RequiresWebComponentsV0(const GURL& url) {
  // TODO(1025782): For now, file:// URLs are allowed to access Web Components
  // v0 features. This will be removed once origin trials support file:// URLs
  // for this purpose.
  return url.SchemeIs(content::kChromeUIScheme) || url.SchemeIs("file");
}

void CefContentRendererClient::GetInterface(
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

void CefContentRendererClient::WillDestroyCurrentMessageLoop() {
  base::AutoLock lock_scope(single_process_cleanup_lock_);
  single_process_cleanup_complete_ = true;
}

CefRefPtr<CefBrowserImpl> CefContentRendererClient::MaybeCreateBrowser(
    content::RenderView* render_view,
    content::RenderFrame* render_frame,
    base::Optional<bool>* is_windowless) {
  if (!render_view || !render_frame)
    return nullptr;

  // Don't create another browser or guest view object if one already exists for
  // the view.
  auto browser = GetBrowserForView(render_view);
  if (browser) {
    if (is_windowless) {
      *is_windowless = browser->is_windowless();
    }
    return browser;
  }

  auto guest_view = GetGuestViewForView(render_view);
  if (guest_view) {
    if (is_windowless) {
      *is_windowless = guest_view->is_windowless();
    }
    return nullptr;
  }

  const int render_frame_routing_id = render_frame->GetRoutingID();

  // Retrieve the browser information synchronously. This will also register
  // the routing ids with the browser info object in the browser process.
  CefProcessHostMsg_GetNewBrowserInfo_Params params;
  content::RenderThread::Get()->Send(new CefProcessHostMsg_GetNewBrowserInfo(
      render_frame_routing_id, &params));

  if (is_windowless) {
    *is_windowless = params.is_windowless;
  }

  if (params.browser_id == 0) {
    // The popup may have been canceled during creation.
    return nullptr;
  }

  if (params.is_guest_view || params.browser_id < 0) {
    // Don't create a CefBrowser for guest views, or if the new browser info
    // response has timed out.
    guest_views_.insert(std::make_pair(
        render_view,
        std::make_unique<CefGuestView>(render_view, params.is_windowless)));
    return nullptr;
  }

#if defined(OS_MACOSX)
  // FIXME: It would be better if this API would be a callback from the
  // WebKit layer, or if it would be exposed as an WebView instance method; the
  // current implementation uses a static variable, and WebKit needs to be
  // patched in order to make it work for each WebView instance
  render_view->GetWebView()->SetUseExternalPopupMenusThisInstance(
      !params.is_windowless);
#endif

  browser = new CefBrowserImpl(render_view, params.browser_id, params.is_popup,
                               params.is_windowless);
  browsers_.insert(std::make_pair(render_view, browser));

  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefDictionaryValueImpl> dictValuePtr(
          new CefDictionaryValueImpl(&params.extra_info, false, true));
      handler->OnBrowserCreated(browser.get(), dictValuePtr.get());
      dictValuePtr->Detach(nullptr);
    }
  }

  return browser;
}

void CefContentRendererClient::RunSingleProcessCleanupOnUIThread() {
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
  if (!CefContext::Get()->settings().multi_threaded_message_loop)
    delete host;
}

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if defined(OS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif
#endif
