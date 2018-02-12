// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/content_renderer_client.h"

#include <utility>

#include "base/compiler_specific.h"

// Enable deprecation warnings for MSVC. See http://crbug.com/585142.
#if defined(OS_WIN)
#pragma warning(push)
#pragma warning(default : 4996)
#endif

#include "libcef/browser/context.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/content_client.h"
#include "libcef/common/extensions/extensions_client.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/values_impl.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/extensions/extensions_renderer_client.h"
#include "libcef/renderer/extensions/print_render_frame_helper_delegate.h"
#include "libcef/renderer/render_frame_observer.h"
#include "libcef/renderer/render_message_filter.h"
#include "libcef/renderer/render_thread_observer.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/renderer/v8_impl.h"
#include "libcef/renderer/webkit_glue.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics_action.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/constants.mojom.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
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
#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/plugin_instance_throttler.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/renderer/render_widget.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "ipc/ipc_sync_channel.h"
#include "media/base/media.h"
#include "printing/print_settings.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebPrerenderingSupport.h"
#include "third_party/WebKit/public/platform/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/scheduler/renderer_process_type.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPrerendererClient.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"
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
  explicit CefGuestView(content::RenderView* render_view)
      : content::RenderViewObserver(render_view) {}

 private:
  // RenderViewObserver methods.
  void OnDestruct() override {
    CefContentRendererClient::Get()->OnGuestViewDestroyed(this);
  }
};

CefContentRendererClient::CefContentRendererClient()
    : devtools_agent_count_(0),
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

  printing::SetAgent(CefContentClient::Get()->GetUserAgent());
}

CefContentRendererClient::~CefContentRendererClient() {}

// static
CefContentRendererClient* CefContentRendererClient::Get() {
  return static_cast<CefContentRendererClient*>(
      CefContentClient::Get()->renderer());
}

CefRefPtr<CefBrowserImpl> CefContentRendererClient::GetBrowserForView(
    content::RenderView* view) {
  CEF_REQUIRE_RT_RETURN(NULL);

  BrowserMap::const_iterator it = browsers_.find(view);
  if (it != browsers_.end())
    return it->second;
  return NULL;
}

CefRefPtr<CefBrowserImpl> CefContentRendererClient::GetBrowserForMainFrame(
    blink::WebFrame* frame) {
  CEF_REQUIRE_RT_RETURN(NULL);

  BrowserMap::const_iterator it = browsers_.begin();
  for (; it != browsers_.end(); ++it) {
    content::RenderView* render_view = it->second->render_view();
    if (render_view && render_view->GetWebView() &&
        render_view->GetWebView()->MainFrame() == frame) {
      return it->second;
    }
  }

  return NULL;
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

bool CefContentRendererClient::HasGuestViewForView(content::RenderView* view) {
  CEF_REQUIRE_RT_RETURN(false);

  GuestViewMap::const_iterator it = guest_views_.find(view);
  return it != guest_views_.end();
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
        webkit_glue::RegisterURLSchemeAsLocal(scheme);
      if (info.is_display_isolated)
        blink::WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(scheme);
      if (info.is_secure)
        webkit_glue::RegisterURLSchemeAsSecure(scheme);
    }
  }

  if (!cross_origin_whitelist_entries_.empty()) {
    // Add the cross-origin white list entries.
    for (size_t i = 0; i < cross_origin_whitelist_entries_.size(); ++i) {
      const Cef_CrossOriginWhiteListEntry_Params& entry =
          cross_origin_whitelist_entries_[i];
      GURL gurl = GURL(entry.source_origin);
      blink::WebSecurityPolicy::AddOriginAccessWhitelistEntry(
          gurl, blink::WebString::FromUTF8(entry.target_protocol),
          blink::WebString::FromUTF8(entry.target_domain),
          entry.allow_target_subdomains);
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

  url_loader_factory_ =
      blink::Platform::Current()->CreateDefaultURLLoaderFactory();

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
  return NULL;
}

void CefContentRendererClient::RunSingleProcessCleanup() {
  DCHECK(content::RenderProcessHost::run_renderer_in_process());

  // Make sure the render thread was actually started.
  if (!render_task_runner_.get())
    return;

  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    RunSingleProcessCleanupOnUIThread();
  } else {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
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
  observer_.reset(new CefRenderThreadObserver());
  web_cache_impl_.reset(new web_cache::WebCacheImpl());

  content::RenderThread* thread = content::RenderThread::Get();

  thread->SetRendererProcessType(
      IsStandaloneExtensionProcess()
          ? blink::scheduler::RendererProcessType::kExtensionRenderer
          : blink::scheduler::RendererProcessType::kRenderer);

  thread->AddObserver(observer_.get());
  thread->GetChannel()->AddFilter(new CefRenderMessageFilter);

  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) {
    spellcheck_.reset(new SpellCheck(this));
    thread->AddObserver(spellcheck_.get());
  }

  if (content::RenderProcessHost::run_renderer_in_process()) {
    // When running in single-process mode register as a destruction observer
    // on the render thread's MessageLoop.
    base::MessageLoop::current()->AddDestructionObserver(this);
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

  if (extensions::ExtensionsEnabled())
    extensions_renderer_client_->RenderThreadStarted();
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
      listValuePtr->Detach(NULL);
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
  new printing::PrintRenderFrameHelper(
      render_frame,
      base::WrapUnique(new extensions::CefPrintRenderFrameHelperDelegate()));

  if (extensions::ExtensionsEnabled())
    extensions_renderer_client_->RenderFrameCreated(render_frame, registry);

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) {
    SpellCheckProvider* spell_check_provider =
        new SpellCheckProvider(render_frame, spellcheck_.get(), this);
    // TODO(xiaochengh): Design better way to sync between Chrome-side and
    // Blink-side spellcheck enabled states.  See crbug.com/710097.
    //
    // TODO(alexmos): Do this for all frames so that this works properly for
    // OOPIFs.  See https://crbug.com/789273.
    if (render_frame->IsMainFrame())
      spell_check_provider->EnableSpellcheck(
          spellcheck_->IsSpellcheckEnabled());
  }

  BrowserCreated(render_frame->GetRenderView(), render_frame);
}

void CefContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  new CefPrerendererClient(render_view);

  BrowserCreated(render_view, render_view->GetMainRenderFrame());
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

bool CefContentRendererClient::ShouldFork(blink::WebLocalFrame* frame,
                                          const GURL& url,
                                          const std::string& http_method,
                                          bool is_initial_navigation,
                                          bool is_server_redirect,
                                          bool* send_referrer) {
  DCHECK(!frame->Parent());

  // For now, we skip the rest for POST submissions.  This is because
  // http://crbug.com/101395 is more likely to cause compatibility issues
  // with hosted apps and extensions than WebUI pages.  We will remove this
  // check when cross-process POST submissions are supported.
  if (http_method != "GET")
    return false;

  if (extensions::ExtensionsEnabled()) {
    return extensions::CefExtensionsRendererClient::ShouldFork(
        frame, url, is_initial_navigation, is_server_redirect, send_referrer);
  }

  return false;
}

bool CefContentRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& url,
    std::vector<std::unique_ptr<content::URLLoaderThrottle>>* throttles,
    GURL* new_url) {
  if (extensions::ExtensionsEnabled()) {
    return extensions_renderer_client_->WillSendRequest(frame, transition_type,
                                                        url, new_url);
  }

  return false;
}

unsigned long long CefContentRendererClient::VisitedLinkHash(
    const char* canonical_url,
    size_t length) {
  return observer_->visited_link_slave()->ComputeURLFingerprint(canonical_url,
                                                                length);
}

bool CefContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return observer_->visited_link_slave()->IsVisited(link_hash);
}

content::BrowserPluginDelegate*
CefContentRendererClient::CreateBrowserPluginDelegate(
    content::RenderFrame* render_frame,
    const std::string& mime_type,
    const GURL& original_url) {
  DCHECK(extensions::ExtensionsEnabled());
  return extensions::CefExtensionsRendererClient::CreateBrowserPluginDelegate(
      render_frame, mime_type, original_url);
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

void CefContentRendererClient::DevToolsAgentAttached(
    content::RenderFrame* render_frame,
    int session_id) {
  CEF_REQUIRE_RT();
  ++devtools_agent_count_;
}

void CefContentRendererClient::DevToolsAgentDetached(
    content::RenderFrame* render_frame,
    int session_id) {
  CEF_REQUIRE_RT();
  --devtools_agent_count_;
  if (devtools_agent_count_ == 0 && uncaught_exception_stack_size_ > 0) {
    // When the last DevToolsAgent is detached the stack size is set to 0.
    // Restore the user-specified stack size here.
    CefV8SetUncaughtExceptionStackSize(uncaught_exception_stack_size_);
  }
}

void CefContentRendererClient::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  content::RenderThread::Get()->GetConnector()->BindInterface(
      service_manager::Identity(chrome::mojom::kServiceName), interface_name,
      std::move(interface_pipe));
}

void CefContentRendererClient::WillDestroyCurrentMessageLoop() {
  base::AutoLock lock_scope(single_process_cleanup_lock_);
  single_process_cleanup_complete_ = true;
}

void CefContentRendererClient::BrowserCreated(
    content::RenderView* render_view,
    content::RenderFrame* render_frame) {
  if (!render_view || !render_frame)
    return;

  // Don't create another browser or guest view object if one already exists for
  // the view.
  if (GetBrowserForView(render_view).get() || HasGuestViewForView(render_view))
    return;

  const int render_frame_routing_id = render_frame->GetRoutingID();

  // Retrieve the browser information synchronously. This will also register
  // the routing ids with the browser info object in the browser process.
  CefProcessHostMsg_GetNewBrowserInfo_Params params;
  content::RenderThread::Get()->Send(new CefProcessHostMsg_GetNewBrowserInfo(
      render_frame_routing_id, &params));
  if (params.browser_id == 0) {
    // The popup may have been canceled during creation.
    return;
  }

  if (params.is_guest_view) {
    // Don't create a CefBrowser for guest views.
    guest_views_.insert(std::make_pair(
        render_view, base::MakeUnique<CefGuestView>(render_view)));
    return;
  }

#if defined(OS_MACOSX)
  // FIXME: It would be better if this API would be a callback from the
  // WebKit layer, or if it would be exposed as an WebView instance method; the
  // current implementation uses a static variable, and WebKit needs to be
  // patched in order to make it work for each WebView instance
  render_view->GetWebView()->SetUseExternalPopupMenusThisInstance(
      !params.is_windowless);
#endif

  CefRefPtr<CefBrowserImpl> browser = new CefBrowserImpl(
      render_view, params.browser_id, params.is_popup, params.is_windowless);
  browsers_.insert(std::make_pair(render_view, browser));

  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get())
      handler->OnBrowserCreated(browser.get());
  }
}

void CefContentRendererClient::RunSingleProcessCleanupOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Clean up the single existing RenderProcessHost.
  content::RenderProcessHost* host = NULL;
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

// Enable deprecation warnings for MSVC. See http://crbug.com/585142.
#if defined(OS_WIN)
#pragma warning(pop)
#endif
