// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/content_renderer_client.h"

#include "libcef/browser/context.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/content_client.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/values_impl.h"
#include "libcef/renderer/browser_impl.h"
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
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "chrome/renderer/spellchecker/spellcheck_provider.h"
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

class CefPrintWebViewHelperDelegate :
    public printing::PrintWebViewHelper::Delegate {
 public:
  CefPrintWebViewHelperDelegate() {}

  bool CancelPrerender(content::RenderView* render_view,
                      int routing_id) override {
    return false;
  }

  blink::WebElement GetPdfElement(blink::WebLocalFrame* frame) override {
    return blink::WebElement();
  }

  bool IsOutOfProcessPdfEnabled() override {
    return false;
  }

  bool IsPrintPreviewEnabled() override {
    return false;
  }

  bool OverridePrint(blink::WebLocalFrame* frame) override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CefPrintWebViewHelperDelegate);
};

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

  blink::WebRuntimeFeatures::enableMediaPlayer(
      media::IsMediaLibraryInitialized());

  // TODO(cef): Enable these once the implementation supports it.
  blink::WebRuntimeFeatures::enableNotifications(false);

  // TODO(cef): Remove this line once off-screen rendering is fixed to work
  // with the new popup menu implementation (issue #1566).
  blink::WebRuntimeFeatures::enableFeatureFromString("HTMLPopupMenu", false);

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

  render_task_runner_ = base::MessageLoopProxy::current();
  observer_.reset(new CefRenderProcessObserver());
  web_cache_observer_.reset(new web_cache::WebCacheRenderProcessObserver());

  content::RenderThread* thread = content::RenderThread::Get();
  thread->AddObserver(observer_.get());
  thread->AddObserver(web_cache_observer_.get());
  thread->GetChannel()->AddFilter(new CefRenderMessageFilter);
  thread->RegisterExtension(extensions_v8::LoadTimesExtension::Get());

  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) {
    spellcheck_.reset(new SpellCheck());
    thread->AddObserver(spellcheck_.get());
  }

  if (content::RenderProcessHost::run_renderer_in_process()) {
    // When running in single-process mode register as a destruction observer
    // on the render thread's MessageLoop.
    base::MessageLoop::current()->AddDestructionObserver(this);
  }

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  base::FilePath media_path;
  PathService::Get(content::DIR_MEDIA_LIBS, &media_path);
  if (!media_path.empty())
    media::InitializeMediaLibrary(media_path);

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
}

void CefContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  new CefRenderFrameObserver(render_frame);
  new CefPepperHelper(render_frame);
  BrowserCreated(render_frame->GetRenderView(), render_frame);
}

void CefContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  BrowserCreated(render_view, render_view->GetMainRenderFrame());
}

bool CefContentRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  CefRefPtr<CefBrowserImpl> browser =
      CefBrowserImpl::GetBrowserForMainFrame(frame->top());
  if (!browser.get() || !browser->is_windowless())
    return false;

#if defined(ENABLE_PLUGINS)
  if (base::UTF16ToASCII(params.mimeType) == content::kBrowserPluginMimeType)
    return false;

  content::RenderFrameImpl* render_frame_impl =
      static_cast<content::RenderFrameImpl*>(render_frame);

  content::WebPluginInfo info;
  std::string mime_type;
  bool found = false;
  render_frame_impl->Send(
      new FrameHostMsg_GetPluginInfo(
          render_frame_impl->GetRoutingID(),
          params.url,
          frame->top()->document().url(),
          params.mimeType.utf8(),
          &found,
          &info,
          &mime_type));
  if (!found)
    return false;

  bool silverlight = StartsWithASCII(mime_type,
                                     "application/x-silverlight", false);
  if (silverlight) {
    // Force Flash and Silverlight plugins to use windowless mode.
    blink::WebPluginParams params_to_use = params;
    params_to_use.mimeType = blink::WebString::fromUTF8(mime_type);
  
    size_t size = params.attributeNames.size();
    blink::WebVector<blink::WebString> new_names(size+1),
                                       new_values(size+1);

    for (size_t i = 0; i < size; ++i) {
      new_names[i] = params.attributeNames[i];
      new_values[i] = params.attributeValues[i];
    }

    new_names[size] = "windowless";
    new_values[size] = "true";

    params_to_use.attributeNames.swap(new_names);
    params_to_use.attributeValues.swap(new_values);

    *plugin = render_frame_impl->CreatePlugin(
        frame, info, params_to_use, nullptr);
    return true;
  }
#endif  // defined(ENABLE_PLUGINS)

  return false;
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
      DCHECK(browserPtr.get());
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

void CefContentRendererClient::WillDestroyCurrentMessageLoop() {
  base::AutoLock lock_scope(single_process_cleanup_lock_);
  single_process_cleanup_complete_ = true;
}

void CefContentRendererClient::BrowserCreated(
    content::RenderView* render_view,
    content::RenderFrame* render_frame) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Retrieve the browser information synchronously. This will also register
  // the routing ids with the browser info object in the browser process.
  CefProcessHostMsg_GetNewBrowserInfo_Params params;
  content::RenderThread::Get()->Send(
      new CefProcessHostMsg_GetNewBrowserInfo(
          render_view->GetRoutingID(),
          render_frame->GetRoutingID(),
          &params));
  DCHECK_GT(params.browser_id, 0);

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

  new CefPrerendererClient(render_view);
  new printing::PrintWebViewHelper(
      render_view,
      make_scoped_ptr<printing::PrintWebViewHelper::Delegate>(
          new CefPrintWebViewHelperDelegate()));

  if (!command_line->HasSwitch(switches::kDisableSpellChecking)) 
    new SpellCheckProvider(render_view, spellcheck_.get());

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
