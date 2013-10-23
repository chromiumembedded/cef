// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"

#include "config.h"
MSVC_PUSH_WARNING_LEVEL(0);
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8RecursionScope.h"
#include "bindings/v8/V8Utilities.h"
MSVC_POP_WARNING();
#undef ceil
#undef LOG

#include "libcef/renderer/content_renderer_client.h"

#include "libcef/browser/context.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/content_client.h"
#include "libcef/common/values_impl.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/chrome_bindings.h"
#include "libcef/renderer/render_message_filter.h"
#include "libcef/renderer/render_process_observer.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/renderer/v8_impl.h"
#include "libcef/renderer/webkit_glue.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/printing/print_web_view_helper.h"
#include "content/child/child_thread.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_sync_channel.h"
#include "media/base/media.h"
#include "third_party/WebKit/public/platform/WebPrerenderingSupport.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebWorkerRunLoop.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebPrerendererClient.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWorkerInfo.h"
#include "v8/include/v8.h"
#include "webkit/child/worker_task_runner.h"

namespace {

// Stub implementation of WebKit::WebPrerenderingSupport.
class CefPrerenderingSupport : public WebKit::WebPrerenderingSupport {
 public:
  virtual ~CefPrerenderingSupport() {}

 private:
  virtual void add(const WebKit::WebPrerender& prerender) OVERRIDE {}
  virtual void cancel(const WebKit::WebPrerender& prerender) OVERRIDE {}
  virtual void abandon(const WebKit::WebPrerender& prerender) OVERRIDE {}
};

// Stub implementation of WebKit::WebPrerendererClient.
class CefPrerendererClient : public content::RenderViewObserver,
                             public WebKit::WebPrerendererClient {
 public:
  explicit CefPrerendererClient(content::RenderView* render_view)
      : content::RenderViewObserver(render_view) {
    DCHECK(render_view);
    render_view->GetWebView()->setPrerendererClient(this);
  }

 private:
  virtual ~CefPrerendererClient() {}

  virtual void willAddPrerender(WebKit::WebPrerender* prerender) OVERRIDE {}
};

// Implementation of SequencedTaskRunner for WebWorker threads.
class CefWebWorkerTaskRunner : public base::SequencedTaskRunner,
                               public webkit_glue::WorkerTaskRunner::Observer {
 public:
  CefWebWorkerTaskRunner(webkit_glue::WorkerTaskRunner* runner,
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
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE {
    return PostDelayedTask(from_here, task, delay);
  }

  // TaskRunner methods:
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE {
    if (delay != base::TimeDelta())
      LOG(WARNING) << "Delayed tasks are not supported on WebWorker threads";
    runner_->PostTask(worker_id_, task);
    return true;
  }

  virtual bool RunsTasksOnCurrentThread() const OVERRIDE {
    return (runner_->CurrentWorkerId() == worker_id_);
  }

  // WorkerTaskRunner::Observer methods:
  virtual void OnWorkerRunLoopStopped() OVERRIDE {
    CefContentRendererClient::Get()->RemoveWorkerTaskRunner(worker_id_);
  }

 private:
  webkit_glue::WorkerTaskRunner* runner_;
  int worker_id_;
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
    WebKit::WebFrame* frame) {
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
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  WebKit::WebRuntimeFeatures::enableMediaPlayer(
      media::IsMediaLibraryInitialized());

  // TODO(cef): Enable these once the implementation supports it.
  WebKit::WebRuntimeFeatures::enableNotifications(false);

  WebKit::WebRuntimeFeatures::enableSpeechInput(
      command_line.HasSwitch(switches::kEnableSpeechInput));
  WebKit::WebRuntimeFeatures::enableMediaStream(
      command_line.HasSwitch(switches::kEnableMediaStream));

  const CefContentClient::SchemeInfoList* schemes =
      CefContentClient::Get()->GetCustomSchemes();
  if (!schemes->empty()) {
    // Register the custom schemes.
    CefContentClient::SchemeInfoList::const_iterator it = schemes->begin();
    for (; it != schemes->end(); ++it) {
      const CefContentClient::SchemeInfo& info = *it;
      const WebKit::WebString& scheme =
          WebKit::WebString::fromUTF8(info.scheme_name);
      if (info.is_standard) {
        // Standard schemes must also be registered as CORS enabled to support
        // CORS-restricted requests (for example, XMLHttpRequest redirects).
        WebKit::WebSecurityPolicy::registerURLSchemeAsCORSEnabled(scheme);
      }
      if (info.is_local)
        WebKit::WebSecurityPolicy::registerURLSchemeAsLocal(scheme);
      if (info.is_display_isolated)
        WebKit::WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(scheme);
    }
  }

  if (!cross_origin_whitelist_entries_.empty()) {
    // Add the cross-origin white list entries.
    for (size_t i = 0; i < cross_origin_whitelist_entries_.size(); ++i) {
      const Cef_CrossOriginWhiteListEntry_Params& entry =
          cross_origin_whitelist_entries_[i];
      GURL gurl = GURL(entry.source_origin);
      WebKit::WebSecurityPolicy::addOriginAccessWhitelistEntry(
          gurl,
          WebKit::WebString::fromUTF8(entry.target_protocol),
          WebKit::WebString::fromUTF8(entry.target_domain),
          entry.allow_target_subdomains);
    }
    cross_origin_whitelist_entries_.clear();
  }

  // The number of stack trace frames to capture for uncaught exceptions.
  if (command_line.HasSwitch(switches::kUncaughtExceptionStackSize)) {
    int uncaught_exception_stack_size = 0;
    base::StringToInt(
        command_line.GetSwitchValueASCII(switches::kUncaughtExceptionStackSize),
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
  webkit_glue::WorkerTaskRunner* worker_runner =
      webkit_glue::WorkerTaskRunner::Instance();
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
  if (!render_task_runner_)
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
  render_task_runner_ = base::MessageLoopProxy::current();
  observer_.reset(new CefRenderProcessObserver());

  content::RenderThread* thread = content::RenderThread::Get();
  thread->AddObserver(observer_.get());
  thread->GetChannel()->AddFilter(new CefRenderMessageFilter);
  thread->RegisterExtension(extensions_v8::LoadTimesExtension::Get());

  if (content::RenderProcessHost::run_renderer_in_process()) {
    // When running in single-process mode register as a destruction observer
    // on the render thread's MessageLoop.
    base::MessageLoop::current()->AddDestructionObserver(this);
  }

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  base::FilePath media_path;
  PathService::Get(base::DIR_MODULE, &media_path);
  if (!media_path.empty())
    media::InitializeMediaLibrary(media_path);

  WebKit::WebPrerenderingSupport::initialize(new CefPrerenderingSupport());

  // Create global objects associated with the default Isolate.
  CefV8IsolateCreated();

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
}

void CefContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  // Retrieve the new browser information synchronously.
  CefProcessHostMsg_GetNewBrowserInfo_Params params;
  content::RenderThread::Get()->Send(
      new CefProcessHostMsg_GetNewBrowserInfo(render_view->GetRoutingID(),
                                              &params));
  DCHECK_GT(params.browser_id, 0);

#if defined(OS_MACOSX)
  // FIXME: It would be better if this API would be a callback from the
  // WebKit layer, or if it would be exposed as an WebView instance method; the
  // current implementation uses a static variable, and WebKit needs to be
  // patched in order to make it work for each WebView instance
  render_view->GetWebView()->setUseExternalPopupMenusThisInstance(
      !params.is_window_rendering_disabled);
#endif

  CefRefPtr<CefBrowserImpl> browser =
      new CefBrowserImpl(render_view, params.browser_id, params.is_popup,
                         params.is_window_rendering_disabled);
  browsers_.insert(std::make_pair(render_view, browser));

  new CefPrerendererClient(render_view);
  new printing::PrintWebViewHelper(render_view);

  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get())
      handler->OnBrowserCreated(browser.get());
  }
}

bool CefContentRendererClient::OverrideCreatePlugin(
    content::RenderView* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params,
    WebKit::WebPlugin** plugin) {
  CefRefPtr<CefBrowserImpl> browser =
      CefBrowserImpl::GetBrowserForMainFrame(frame->top());
  if (!browser || !browser->is_window_rendering_disabled())
    return false;

#if defined(ENABLE_PLUGINS)
  if (UTF16ToASCII(params.mimeType) == content::kBrowserPluginMimeType)
    return false;

  content::RenderViewImpl* render_view_impl =
      static_cast<content::RenderViewImpl*>(render_view);

  content::WebPluginInfo info;
  std::string mime_type;
  bool found = render_view_impl->GetPluginInfo(params.url,
                                               frame->top()->document().url(),
                                               params.mimeType.utf8(),
                                               &info,
                                               &mime_type);
  if (!found)
    return false;

  bool flash = LowerCaseEqualsASCII(mime_type,
                                    "application/x-shockwave-flash");
  bool silverlight = StartsWithASCII(mime_type,
                                     "application/x-silverlight", false);

  if (flash) {
    // "wmode" values of "opaque" or "transparent" are allowed.
    size_t size = params.attributeNames.size();
    for (size_t i = 0; i < size; ++i) {
      std::string name = params.attributeNames[i].utf8();
      if (name == "wmode") {
        std::string value = params.attributeValues[i].utf8();
        if (value == "opaque" || value == "transparent")
          flash = false;
        break;
      }
    }
  }

  if (flash || silverlight) {
    // Force Flash and Silverlight plugins to use windowless mode.
    WebKit::WebPluginParams params_to_use = params;
    params_to_use.mimeType = WebKit::WebString::fromUTF8(mime_type);
  
    size_t size = params.attributeNames.size();
    WebKit::WebVector<WebKit::WebString> new_names(size+1),
                                         new_values(size+1);

    for (size_t i = 0; i < size; ++i) {
      new_names[i] = params.attributeNames[i];
      new_values[i] = params.attributeValues[i];
    }

    if (flash) {
      new_names[size] = "wmode";
      new_values[size] = "opaque";
    } else if (silverlight) {
      new_names[size] = "windowless";
      new_values[size] = "true";
    }

    params_to_use.attributeNames.swap(new_names);
    params_to_use.attributeValues.swap(new_values);

    *plugin = render_view_impl->CreatePlugin(frame, info, params_to_use);
    return true;
  }
#endif  // defined(ENABLE_PLUGINS)

  return false;
}

void CefContentRendererClient::DidCreateScriptContext(
    WebKit::WebFrame* frame, v8::Handle<v8::Context> context,
    int extension_group, int world_id) {
  CefRefPtr<CefBrowserImpl> browserPtr =
      CefBrowserImpl::GetBrowserForMainFrame(frame->top());
  DCHECK(browserPtr.get());
  if (!browserPtr.get())
    return;

  CefRefPtr<CefFrameImpl> framePtr = browserPtr->GetWebFrameImpl(frame);

  v8::HandleScope handle_scope(webkit_glue::GetV8Isolate(frame));
  v8::Context::Scope scope(context);
  WebCore::V8RecursionScope recursion_scope(
      WebCore::toExecutionContext(context));

  CefRefPtr<CefV8Context> contextPtr(new CefV8ContextImpl(context));

  scheme::OnContextCreated(browserPtr, framePtr, contextPtr);

  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get())
      handler->OnContextCreated(browserPtr.get(), framePtr.get(), contextPtr);
  }
}

void CefContentRendererClient::WillReleaseScriptContext(
    WebKit::WebFrame* frame, v8::Handle<v8::Context> context, int world_id) {
  // Notify the render process handler.
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

        v8::HandleScope handle_scope(webkit_glue::GetV8Isolate(frame));
        v8::Context::Scope scope(context);
        WebCore::V8RecursionScope recursion_scope(
            WebCore::toExecutionContext(context));

        CefRefPtr<CefV8Context> contextPtr(new CefV8ContextImpl(context));

        handler->OnContextReleased(browserPtr.get(), framePtr.get(),
                                   contextPtr);
      }
    }
  }

  CefV8ReleaseContext(context);
}

void CefContentRendererClient::WillDestroyCurrentMessageLoop() {
  base::AutoLock lock_scope(single_process_cleanup_lock_);
  single_process_cleanup_complete_ = true;
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
