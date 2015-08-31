// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/content_renderer_client.h"

#include "libcef/browser/context.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/content_client.h"
#include "libcef/common/extensions/extensions_client.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/values_impl.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/extensions/extensions_dispatcher_delegate.h"
#include "libcef/renderer/extensions/extensions_renderer_client.h"
#include "libcef/renderer/extensions/print_web_view_helper_delegate.h"
#include "libcef/renderer/media/cef_key_systems.h"
#include "libcef/renderer/pepper/pepper_helper.h"
#include "libcef/renderer/render_frame_observer.h"
#include "libcef/renderer/render_message_filter.h"
#include "libcef/renderer/render_process_observer.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/renderer/v8_impl.h"
#include "libcef/renderer/webkit_glue.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/pepper_permission_util.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/pepper/chrome_pdf_print_client.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "chrome/renderer/spellchecker/spellcheck_provider.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/printing/renderer/print_web_view_helper.h"
#include "components/web_cache/renderer/web_cache_render_process_observer.h"
#include "content/child/worker_task_runner.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/renderer/plugin_instance_throttler.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/renderer/render_frame_impl.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/dispatcher_delegate.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/renderer/guest_view/extensions_guest_view_container.h"
#include "extensions/renderer/guest_view/extensions_guest_view_container_dispatcher.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container.h"
#include "ipc/ipc_sync_channel.h"
#include "media/base/media.h"
#include "third_party/WebKit/public/platform/WebPrerenderingSupport.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebPrerendererClient.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#endif

namespace {

// Stub implementation of blink::WebPrerenderingSupport.
class CefPrerenderingSupport : public blink::WebPrerenderingSupport {
 private:
  void add(const blink::WebPrerender& prerender) override {}
  void cancel(const blink::WebPrerender& prerender) override {}
  void abandon(const blink::WebPrerender& prerender) override {}
};

// Stub implementation of blink::WebPrerendererClient.
class CefPrerendererClient : public content::RenderViewObserver,
                             public blink::WebPrerendererClient {
 public:
  explicit CefPrerendererClient(content::RenderView* render_view)
      : content::RenderViewObserver(render_view) {
    DCHECK(render_view);
    render_view->GetWebView()->setPrerendererClient(this);
  }

 private:
  ~CefPrerendererClient() override {}

  void willAddPrerender(blink::WebPrerender* prerender) override {}
};

// Implementation of SequencedTaskRunner for WebWorker threads.
class CefWebWorkerTaskRunner : public base::SequencedTaskRunner,
                               public content::WorkerTaskRunner::Observer {
 public:
  CefWebWorkerTaskRunner(content::WorkerTaskRunner* runner,
                         int worker_id)
      : runner_(runner),
        worker_id_(worker_id) {
    DCHECK(runner_);
    DCHECK_GT(worker_id_, 0);
    DCHECK(RunsTasksOnCurrentThread());

    // Adds an observer for the current thread.
    runner_->AddStopObserver(this);
  }

  // SequencedTaskRunner methods:
  bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) override {
    return PostDelayedTask(from_here, task, delay);
  }

  // TaskRunner methods:
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    if (delay != base::TimeDelta())
      LOG(WARNING) << "Delayed tasks are not supported on WebWorker threads";
    runner_->PostTask(worker_id_, task);
    return true;
  }

  bool RunsTasksOnCurrentThread() const override {
    return (runner_->CurrentWorkerId() == worker_id_);
  }

  // WorkerTaskRunner::Observer methods:
  void OnWorkerRunLoopStopped() override {
    CefContentRendererClient::Get()->RemoveWorkerTaskRunner(worker_id_);
  }

 private:
  content::WorkerTaskRunner* runner_;
  int worker_id_;
};

void IsGuestViewApiAvailableToScriptContext(
    bool* api_is_available,
    extensions::ScriptContext* context) {
  if (context->GetAvailability("guestViewInternal").is_available()) {
    *api_is_available = true;
  }
}

void AppendParams(const std::vector<base::string16>& additional_names,
                  const std::vector<base::string16>& additional_values,
                  blink::WebVector<blink::WebString>* existing_names,
                  blink::WebVector<blink::WebString>* existing_values) {
  DCHECK(additional_names.size() == additional_values.size());
  DCHECK(existing_names->size() == existing_values->size());

  size_t existing_size = existing_names->size();
  size_t total_size = existing_size + additional_names.size();

  blink::WebVector<blink::WebString> names(total_size);
  blink::WebVector<blink::WebString> values(total_size);

  for (size_t i = 0; i < existing_size; ++i) {
    names[i] = (*existing_names)[i];
    values[i] = (*existing_values)[i];
  }

  for (size_t i = 0; i < additional_names.size(); ++i) {
    names[existing_size + i] = additional_names[i];
    values[existing_size + i] = additional_values[i];
  }

  existing_names->swap(names);
  existing_values->swap(values);
}

}  // namespace

CefContentRendererClient::CefContentRendererClient()
    : devtools_agent_count_(0),
      uncaught_exception_stack_size_(0),
      single_process_cleanup_complete_(false) {
}

CefContentRendererClient::~CefContentRendererClient() {
}

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
        render_view->GetWebView()->mainFrame() == frame) {
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

void CefContentRendererClient::WebKitInitialized() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Create global objects associated with the default Isolate.
  CefV8IsolateCreated();

  // TODO(cef): Enable these once the implementation supports it.
  blink::WebRuntimeFeatures::enableNotifications(false);

  const CefContentClient::SchemeInfoList* schemes =
      CefContentClient::Get()->GetCustomSchemes();
  if (!schemes->empty()) {
    // Register the custom schemes.
    CefContentClient::SchemeInfoList::const_iterator it = schemes->begin();
    for (; it != schemes->end(); ++it) {
      const CefContentClient::SchemeInfo& info = *it;
      const blink::WebString& scheme =
          blink::WebString::fromUTF8(info.scheme_name);
      if (info.is_standard) {
        // Standard schemes must also be registered as CORS enabled to support
        // CORS-restricted requests (for example, XMLHttpRequest redirects).
        blink::WebSecurityPolicy::registerURLSchemeAsCORSEnabled(scheme);
      }
      if (info.is_local)
        blink::WebSecurityPolicy::registerURLSchemeAsLocal(scheme);
      if (info.is_display_isolated)
        blink::WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(scheme);
    }
  }

  if (!cross_origin_whitelist_entries_.empty()) {
    // Add the cross-origin white list entries.
    for (size_t i = 0; i < cross_origin_whitelist_entries_.size(); ++i) {
      const Cef_CrossOriginWhiteListEntry_Params& entry =
          cross_origin_whitelist_entries_[i];
      GURL gurl = GURL(entry.source_origin);
      blink::WebSecurityPolicy::addOriginAccessWhitelistEntry(
          gurl,
          blink::WebString::fromUTF8(entry.target_protocol),
          blink::WebString::fromUTF8(entry.target_domain),
          entry.allow_target_subdomains);
    }
    cross_origin_whitelist_entries_.clear();
  }

  // The number of stack trace frames to capture for uncaught exceptions.
  if (command_line->HasSwitch(switches::kUncaughtExceptionStackSize)) {
    int uncaught_exception_stack_size = 0;
    base::StringToInt(
        command_line->GetSwitchValueASCII(
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

void CefContentRendererClient::OnRenderProcessShutdown() {
  // Destroy global objects associated with the default Isolate.
  CefV8IsolateDestroyed();
}

void CefContentRendererClient::DevToolsAgentAttached() {
  CEF_REQUIRE_RT();
  ++devtools_agent_count_;
}

void CefContentRendererClient::DevToolsAgentDetached() {
  CEF_REQUIRE_RT();
  --devtools_agent_count_;
  if (devtools_agent_count_ == 0 && uncaught_exception_stack_size_ > 0) {
    // When the last DevToolsAgent is detached the stack size is set to 0.
    // Restore the user-specified stack size here.
    CefV8SetUncaughtExceptionStackSize(uncaught_exception_stack_size_);

    // And do the same for any WebWorker threads.
    WorkerTaskRunnerMap map_copy;

    {
      base::AutoLock lock_scope(worker_task_runner_lock_);
      map_copy = worker_task_runner_map_;
    }

    WorkerTaskRunnerMap::const_iterator it = map_copy.begin();
    for (; it != map_copy.end(); ++it) {
      it->second->PostTask(FROM_HERE,
          base::Bind(CefV8SetUncaughtExceptionStackSize,
                     uncaught_exception_stack_size_));
    }
  }
}

scoped_refptr<base::SequencedTaskRunner>
    CefContentRendererClient::GetCurrentTaskRunner() {
  // Check if currently on the render thread.
  if (CEF_CURRENTLY_ON_RT())
    return render_task_runner_;

  // Check if a WebWorker exists for the current thread.
  content::WorkerTaskRunner* worker_runner =
      content::WorkerTaskRunner::Instance();
  int worker_id = worker_runner->CurrentWorkerId();
  if (worker_id > 0) {
    base::AutoLock lock_scope(worker_task_runner_lock_);
    WorkerTaskRunnerMap::const_iterator it =
        worker_task_runner_map_.find(worker_id);
    if (it != worker_task_runner_map_.end())
      return it->second;

    scoped_refptr<base::SequencedTaskRunner> task_runner =
        new CefWebWorkerTaskRunner(worker_runner, worker_id);
    worker_task_runner_map_[worker_id] = task_runner;
    return task_runner;
  }

  return NULL;
}

scoped_refptr<base::SequencedTaskRunner>
    CefContentRendererClient::GetWorkerTaskRunner(int worker_id) {
  base::AutoLock lock_scope(worker_task_runner_lock_);
  WorkerTaskRunnerMap::const_iterator it =
      worker_task_runner_map_.find(worker_id);
  if (it != worker_task_runner_map_.end())
    return it->second;

  return NULL;
}

void CefContentRendererClient::RemoveWorkerTaskRunner(int worker_id) {
  base::AutoLock lock_scope(worker_task_runner_lock_);
  WorkerTaskRunnerMap::iterator it = worker_task_runner_map_.find(worker_id);
  if (it != worker_task_runner_map_.end())
    worker_task_runner_map_.erase(it);
}

void CefContentRendererClient::RunSingleProcessCleanup() {
  DCHECK(content::RenderProcessHost::run_renderer_in_process());

  // Make sure the render thread was actually started.
  if (!render_task_runner_.get())
    return;

  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    RunSingleProcessCleanupOnUIThread();
  } else {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
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
  observer_.reset(new CefRenderProcessObserver());
  web_cache_observer_.reset(new web_cache::WebCacheRenderProcessObserver());

  content::RenderThread* thread = content::RenderThread::Get();
  thread->AddObserver(observer_.get());
  thread->AddObserver(web_cache_observer_.get());
  thread->GetChannel()->AddFilter(new CefRenderMessageFilter);

  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) {
    spellcheck_.reset(new SpellCheck());
    thread->AddObserver(spellcheck_.get());
  }

  if (content::RenderProcessHost::run_renderer_in_process()) {
    // When running in single-process mode register as a destruction observer
    // on the render thread's MessageLoop.
    base::MessageLoop::current()->AddDestructionObserver(this);
  }

  blink::WebPrerenderingSupport::initialize(new CefPrerenderingSupport());

  // Retrieve the new render thread information synchronously.
  CefProcessHostMsg_GetNewRenderThreadInfo_Params params;
  thread->Send(new CefProcessHostMsg_GetNewRenderThreadInfo(&params));

  // Cross-origin entries need to be added after WebKit is initialized.
  cross_origin_whitelist_entries_ = params.cross_origin_whitelist_entries;

#if defined(OS_MACOSX)
  if (base::mac::IsOSLionOrLater()) {
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

  if (extensions::ExtensionsEnabled()) {
    extensions_client_.reset(new extensions::CefExtensionsClient);
    extensions::ExtensionsClient::Set(extensions_client_.get());

    extensions_renderer_client_.reset(
        new extensions::CefExtensionsRendererClient);
    extensions::ExtensionsRendererClient::Set(
        extensions_renderer_client_.get());

    extension_dispatcher_delegate_.reset(
        new extensions::CefExtensionsDispatcherDelegate());

    // Must be initialized after ExtensionsRendererClient.
    extension_dispatcher_.reset(
        new extensions::Dispatcher(extension_dispatcher_delegate_.get()));
    thread->AddObserver(extension_dispatcher_.get());

    guest_view_container_dispatcher_.reset(
        new extensions::ExtensionsGuestViewContainerDispatcher());
    thread->AddObserver(guest_view_container_dispatcher_.get());
  }

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
}

void CefContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  new CefRenderFrameObserver(render_frame);
  new CefPepperHelper(render_frame);

  if (extensions::ExtensionsEnabled()) {
    new extensions::ExtensionFrameHelper(render_frame,
                                         extension_dispatcher_.get());
    extension_dispatcher_->OnRenderFrameCreated(render_frame);
  }

  BrowserCreated(render_frame->GetRenderView(), render_frame);
}

void CefContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  new CefPrerendererClient(render_view);
  new printing::PrintWebViewHelper(
      render_view,
      make_scoped_ptr<printing::PrintWebViewHelper::Delegate>(
          new extensions::CefPrintWebViewHelperDelegate()));

  if (extensions::ExtensionsEnabled()) {
    new extensions::ExtensionHelper(render_view, extension_dispatcher_.get());
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableSpellChecking))
    new SpellCheckProvider(render_view, spellcheck_.get());

  BrowserCreated(render_view, render_view->GetMainRenderFrame());
}

bool CefContentRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  if (!extensions::ExtensionsEnabled())
    return false;

  // Based on ChromeContentRendererClient::OverrideCreatePlugin.
  std::string orig_mime_type = params.mimeType.utf8();
  if (orig_mime_type == content::kBrowserPluginMimeType) {
    bool guest_view_api_available = false;
    extension_dispatcher_->script_context_set().ForEach(
        render_frame,
        base::Bind(&IsGuestViewApiAvailableToScriptContext,
                   &guest_view_api_available));
    if (guest_view_api_available)
      return false;
  }

  GURL url(params.url);
  CefViewHostMsg_GetPluginInfo_Output output;
  render_frame->Send(new CefViewHostMsg_GetPluginInfo(
      render_frame->GetRoutingID(), url, frame->top()->document().url(),
      orig_mime_type, &output));

  *plugin = CreatePlugin(render_frame, frame, params, output);
  return true;
}

bool CefContentRendererClient::HandleNavigation(
    content::RenderFrame* render_frame,
    content::DocumentState* document_state,
    int opener_id,
    blink::WebFrame* frame,
    const blink::WebURLRequest& request,
    blink::WebNavigationType type,
    blink::WebNavigationPolicy default_policy,
    bool is_redirect) {
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefBrowserImpl> browserPtr =
          CefBrowserImpl::GetBrowserForMainFrame(frame->top());
      if (browserPtr.get()) {
        CefRefPtr<CefFrameImpl> framePtr = browserPtr->GetWebFrameImpl(frame);
        CefRefPtr<CefRequest> requestPtr(CefRequest::Create());
        CefRequestImpl* requestImpl =
            static_cast<CefRequestImpl*>(requestPtr.get());
        requestImpl->Set(request);
        requestImpl->SetReadOnly(true);

        cef_navigation_type_t navigation_type = NAVIGATION_OTHER;
        switch (type) {
          case blink::WebNavigationTypeLinkClicked:
            navigation_type = NAVIGATION_LINK_CLICKED;
            break;
          case blink::WebNavigationTypeFormSubmitted:
            navigation_type = NAVIGATION_FORM_SUBMITTED;
            break;
          case blink::WebNavigationTypeBackForward:
            navigation_type = NAVIGATION_BACK_FORWARD;
            break;
          case blink::WebNavigationTypeReload:
            navigation_type = NAVIGATION_RELOAD;
            break;
          case blink::WebNavigationTypeFormResubmitted:
            navigation_type = NAVIGATION_FORM_RESUBMITTED;
            break;
          case blink::WebNavigationTypeOther:
            navigation_type = NAVIGATION_OTHER;
            break;
        }

        if (handler->OnBeforeNavigation(browserPtr.get(), framePtr.get(),
                                        requestPtr.get(), navigation_type,
                                        is_redirect)) {
          return true;
        }
      }
    }
  }

  return false;
}

content::BrowserPluginDelegate*
CefContentRendererClient::CreateBrowserPluginDelegate(
    content::RenderFrame* render_frame,
    const std::string& mime_type,
    const GURL& original_url) {
  DCHECK(extensions::ExtensionsEnabled());
  if (mime_type == content::kBrowserPluginMimeType) {
    return new extensions::ExtensionsGuestViewContainer(render_frame);
  } else {
    return new extensions::MimeHandlerViewContainer(
        render_frame, mime_type, original_url);
  }
}

void CefContentRendererClient::AddKeySystems(
    std::vector<media::KeySystemInfo>* key_systems) {
  AddCefKeySystems(key_systems);
}

void CefContentRendererClient::WillDestroyCurrentMessageLoop() {
  base::AutoLock lock_scope(single_process_cleanup_lock_);
  single_process_cleanup_complete_ = true;
}

bool CefContentRendererClient::IsExtensionOrSharedModuleWhitelisted(
    const GURL& url, const std::set<std::string>& whitelist) {
  DCHECK(extensions::ExtensionsEnabled());
  const extensions::ExtensionSet* extension_set =
      extensions::RendererExtensionRegistry::Get()->GetMainThreadExtensionSet();
  return chrome::IsExtensionOrSharedModuleWhitelisted(url, extension_set,
      whitelist);
}

void CefContentRendererClient::BrowserCreated(
    content::RenderView* render_view,
    content::RenderFrame* render_frame) {
  // Retrieve the browser information synchronously. This will also register
  // the routing ids with the browser info object in the browser process.
  CefProcessHostMsg_GetNewBrowserInfo_Params params;
  content::RenderThread::Get()->Send(
      new CefProcessHostMsg_GetNewBrowserInfo(
          render_view->GetRoutingID(),
          render_frame->GetRoutingID(),
          &params));
  DCHECK_GT(params.browser_id, 0);

  if (params.is_mime_handler_view) {
    // Don't create a CefBrowser for mime handler views.
    return;
  }

  // Don't create another browser object if one already exists for the view.
  if (GetBrowserForView(render_view).get())
    return;

#if defined(OS_MACOSX)
  // FIXME: It would be better if this API would be a callback from the
  // WebKit layer, or if it would be exposed as an WebView instance method; the
  // current implementation uses a static variable, and WebKit needs to be
  // patched in order to make it work for each WebView instance
  render_view->GetWebView()->setUseExternalPopupMenusThisInstance(
      !params.is_windowless);
#endif

  CefRefPtr<CefBrowserImpl> browser =
      new CefBrowserImpl(render_view, params.browser_id, params.is_popup,
                         params.is_windowless);
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

blink::WebPlugin* CefContentRendererClient::CreatePlugin(
    content::RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& original_params,
    const CefViewHostMsg_GetPluginInfo_Output& output) {
  const content::WebPluginInfo& info = output.plugin;
  const std::string& actual_mime_type = output.actual_mime_type;
  CefViewHostMsg_GetPluginInfo_Status status = output.status;
  GURL url(original_params.url);
  std::string orig_mime_type = original_params.mimeType.utf8();

  // If the browser plugin is to be enabled, this should be handled by the
  // renderer, so the code won't reach here due to the early exit in
  // OverrideCreatePlugin.
  if (status == CefViewHostMsg_GetPluginInfo_Status::kNotFound ||
      orig_mime_type == content::kBrowserPluginMimeType) {
    return NULL;
  } else {
    // TODO(bauerb): This should be in content/.
    blink::WebPluginParams params(original_params);
    for (size_t i = 0; i < info.mime_types.size(); ++i) {
      if (info.mime_types[i].mime_type == actual_mime_type) {
        AppendParams(info.mime_types[i].additional_param_names,
                     info.mime_types[i].additional_param_values,
                     &params.attributeNames, &params.attributeValues);
        break;
      }
    }
    if (params.mimeType.isNull() && (actual_mime_type.size() > 0)) {
      // Webkit might say that mime type is null while we already know the
      // actual mime type via CefViewHostMsg_GetPluginInfo. In that case
      // we should use what we know since WebpluginDelegateProxy does some
      // specific initializations based on this information.
      params.mimeType = blink::WebString::fromUTF8(actual_mime_type.c_str());
    }

    switch (status) {
      case CefViewHostMsg_GetPluginInfo_Status::kNotFound: {
        NOTREACHED();
        break;
      }
      case CefViewHostMsg_GetPluginInfo_Status::kAllowed:
      case CefViewHostMsg_GetPluginInfo_Status::kPlayImportantContent: {
        // TODO(cef): Maybe supply a throttler based on power settings.
        return render_frame->CreatePlugin(frame, info, params, nullptr);
      }
      default:
        // TODO(cef): Provide a placeholder for the various failure conditions.
        break;
    }
  }
  return NULL;
}
