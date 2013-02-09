// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"

#include "third_party/WebKit/Source/WebCore/config.h"
MSVC_PUSH_WARNING_LEVEL(0);
#include "V8RecursionScope.h"  // NOLINT(build/include)
#include "V8Utilities.h"  // NOLINT(build/include)
MSVC_POP_WARNING();
#undef LOG

#include "libcef/renderer/content_renderer_client.h"

#include "libcef/browser/context.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/content_client.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/values_impl.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/chrome_bindings.h"
#include "libcef/renderer/render_message_filter.h"
#include "libcef/renderer/render_process_observer.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/renderer/v8_impl.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "content/common/child_thread.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_sync_channel.h"
#include "media/base/media.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPrerenderingSupport.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPrerendererClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorkerInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorkerRunLoop.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorkerScriptObserver.h"
#include "v8/include/v8.h"
#include "webkit/glue/worker_task_runner.h"

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


class CefWebWorkerScriptObserver : public WebKit::WebWorkerScriptObserver {
 public:
  CefWebWorkerScriptObserver() {
  }

  virtual void didCreateWorkerScriptContext(
      const WebKit::WebWorkerRunLoop& run_loop,
      const WebKit::WebURL& url,
      v8::Handle<v8::Context> context) OVERRIDE {
    // Create global objects associated with the WebWorker Isolate.
    CefV8IsolateCreated();

    int stack_size =
        CefContentRendererClient::Get()->uncaught_exception_stack_size();
    if (stack_size > 0)
      CefV8SetUncaughtExceptionStackSize(stack_size);

    webkit_glue::WorkerTaskRunner* worker_runner =
        webkit_glue::WorkerTaskRunner::Instance();
    int worker_id = worker_runner->CurrentWorkerId();
    DCHECK_GT(worker_id, 0);
    GURL gurl = GURL(url);

    CefV8SetWorkerAttributes(worker_id, gurl);

    // Notify the render process handler.
    CefRefPtr<CefApp> application = CefContentClient::Get()->application();
    if (application.get()) {
      CefRefPtr<CefRenderProcessHandler> handler =
          application->GetRenderProcessHandler();
      if (handler.get()) {
        v8::HandleScope handle_scope;
        v8::Context::Scope scope(context);

        CefRefPtr<CefV8Context> contextPtr(new CefV8ContextImpl(context));
        handler->OnWorkerContextCreated(worker_id, gurl.spec(), contextPtr);
      }
    }
  }

  virtual void willReleaseWorkerScriptContext(
      const WebKit::WebWorkerRunLoop& run_loop,
      const WebKit::WebURL& url,
      v8::Handle<v8::Context> context) OVERRIDE {
    v8::HandleScope handle_scope;
    v8::Context::Scope scope(context);

    // Notify the render process handler.
    CefRefPtr<CefApp> application = CefContentClient::Get()->application();
    if (application.get()) {
      CefRefPtr<CefRenderProcessHandler> handler =
          application->GetRenderProcessHandler();
      if (handler.get()) {
        webkit_glue::WorkerTaskRunner* worker_runner =
            webkit_glue::WorkerTaskRunner::Instance();
        int worker_id = worker_runner->CurrentWorkerId();
        DCHECK_GT(worker_id, 0);

        CefRefPtr<CefV8Context> contextPtr(new CefV8ContextImpl(context));
        GURL gurl = GURL(url);
        handler->OnWorkerContextReleased(worker_id, gurl.spec(), contextPtr);
      }
    }

    CefV8ReleaseContext(context);

    // Destroy global objects associated with the WebWorker Isolate.
    CefV8IsolateDestroyed();
  }
};


struct CefContentRendererClient::SchemeInfo {
  std::string scheme_name;
  bool is_local;
  bool is_display_isolated;
};

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
      content::GetContentClient()->renderer());
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

void CefContentRendererClient::AddCustomScheme(
    const std::string& scheme_name,
    bool is_local,
    bool is_display_isolated) {
  SchemeInfo info = {scheme_name, is_local, is_display_isolated};
  scheme_info_list_.push_back(info);
}

void CefContentRendererClient::WebKitInitialized() {
  WebKit::WebRuntimeFeatures::enableMediaPlayer(
      media::IsMediaLibraryInitialized());

  // TODO(cef): Enable these once the implementation supports it.
  WebKit::WebRuntimeFeatures::enableNotifications(false);

  worker_script_observer_.reset(new CefWebWorkerScriptObserver());
  WebKit::WebWorkerInfo::addScriptObserver(worker_script_observer_.get());

  if (!scheme_info_list_.empty()) {
    // Register the custom schemes.
    SchemeInfoList::const_iterator it = scheme_info_list_.begin();
    for (; it != scheme_info_list_.end(); ++it) {
      const SchemeInfo& info = *it;
      if (info.is_local) {
        WebKit::WebSecurityPolicy::registerURLSchemeAsLocal(
            WebKit::WebString::fromUTF8(info.scheme_name));
      }
      if (info.is_display_isolated) {
        WebKit::WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(
            WebKit::WebString::fromUTF8(info.scheme_name));
      }
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
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
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

  if (content::RenderProcessHost::run_renderer_in_process()) {
    // When running in single-process mode register as a destruction observer
    // on the render thread's MessageLoop.
    MessageLoop::current()->AddDestructionObserver(this);
  }

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  FilePath media_path;
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

  CefRefPtr<CefBrowserImpl> browser =
      new CefBrowserImpl(render_view, params.browser_id, params.is_popup);
  browsers_.insert(std::make_pair(render_view, browser));

  new CefPrerendererClient(render_view);

  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get())
      handler->OnBrowserCreated(browser.get());
  }
}

bool CefContentRendererClient::HandleNavigation(
    WebKit::WebFrame* frame,
    const WebKit::WebURLRequest& request,
    WebKit::WebNavigationType type,
    WebKit::WebNavigationPolicy default_policy,
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
          case WebKit::WebNavigationTypeLinkClicked:
            navigation_type = NAVIGATION_LINK_CLICKED;
            break;
          case WebKit::WebNavigationTypeFormSubmitted:
            navigation_type = NAVIGATION_FORM_SUBMITTED;
            break;
          case WebKit::WebNavigationTypeBackForward:
            navigation_type = NAVIGATION_BACK_FORWARD;
            break;
          case WebKit::WebNavigationTypeReload:
            navigation_type = NAVIGATION_RELOAD;
            break;
          case WebKit::WebNavigationTypeFormResubmitted:
            navigation_type = NAVIGATION_FORM_RESUBMITTED;
            break;
          case WebKit::WebNavigationTypeOther:
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

void CefContentRendererClient::DidCreateScriptContext(
    WebKit::WebFrame* frame, v8::Handle<v8::Context> context,
    int extension_group, int world_id) {
  CefRefPtr<CefBrowserImpl> browserPtr =
      CefBrowserImpl::GetBrowserForMainFrame(frame->top());
  DCHECK(browserPtr.get());
  if (!browserPtr.get())
    return;

  CefRefPtr<CefFrameImpl> framePtr = browserPtr->GetWebFrameImpl(frame);

  v8::HandleScope handle_scope;
  v8::Context::Scope scope(context);
  WebCore::V8RecursionScope recursion_scope(
      WebCore::getScriptExecutionContext());

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

        v8::HandleScope handle_scope;
        v8::Context::Scope scope(context);
        WebCore::V8RecursionScope recursion_scope(
            WebCore::getScriptExecutionContext());

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
  if (!_Context->settings().multi_threaded_message_loop)
    delete host;
}
