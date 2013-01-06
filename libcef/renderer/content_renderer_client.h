// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_CONTENT_RENDERER_CLIENT_H_
#define CEF_LIBCEF_RENDERER_CONTENT_RENDERER_CLIENT_H_
#pragma once

#include <list>
#include <map>
#include <string>
#include <vector>

#include "libcef/renderer/browser_impl.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "content/public/renderer/content_renderer_client.h"

class CefRenderProcessObserver;
class CefWebWorkerScriptObserver;
struct Cef_CrossOriginWhiteListEntry_Params;

class CefContentRendererClient : public content::ContentRendererClient,
                                 public MessageLoop::DestructionObserver {
 public:
  CefContentRendererClient();
  virtual ~CefContentRendererClient();

  // Returns the singleton CefContentRendererClient instance.
  static CefContentRendererClient* Get();

  // Returns the browser associated with the specified RenderView.
  CefRefPtr<CefBrowserImpl> GetBrowserForView(content::RenderView* view);

  // Returns the browser associated with the specified main WebFrame.
  CefRefPtr<CefBrowserImpl> GetBrowserForMainFrame(WebKit::WebFrame* frame);

  // Called from CefBrowserImpl::OnDestruct().
  void OnBrowserDestroyed(CefBrowserImpl* browser);

  // Add a custom scheme registration.
  void AddCustomScheme(const std::string& scheme_name,
                       bool is_local,
                       bool is_display_isolated);

  // Render thread task runner.
  base::SequencedTaskRunner* render_task_runner() const {
    return render_task_runner_.get();
  }

  int uncaught_exception_stack_size() const {
    return uncaught_exception_stack_size_;
  }

  void WebKitInitialized();
  void OnRenderProcessShutdown();

  void DevToolsAgentAttached();
  void DevToolsAgentDetached();

  // Returns the task runner for the current thread. If this is a WebWorker
  // thread and the task runner does not already exist it will be created.
  // Returns NULL if the current thread is not a valid render process thread.
  scoped_refptr<base::SequencedTaskRunner> GetCurrentTaskRunner();

  // Returns the task runner for the specified worker ID or NULL if the
  // specified worker ID is not valid.
  scoped_refptr<base::SequencedTaskRunner> GetWorkerTaskRunner(int worker_id);

  // Remove the task runner associated with the specified worker ID.
  void RemoveWorkerTaskRunner(int worker_id);

  // Used in single-process mode to test when the RenderThread has stopped.
  bool IsRenderThreadShutdownComplete();

 private:
  // ContentRendererClient implementation.
  virtual void RenderThreadStarted() OVERRIDE;
  virtual void RenderViewCreated(content::RenderView* render_view) OVERRIDE;
  virtual bool HandleNavigation(WebKit::WebFrame* frame,
                                const WebKit::WebURLRequest& request,
                                WebKit::WebNavigationType type,
                                WebKit::WebNavigationPolicy default_policy,
                                bool is_redirect) OVERRIDE;
  virtual void DidCreateScriptContext(WebKit::WebFrame* frame,
                                      v8::Handle<v8::Context> context,
                                      int extension_group,
                                      int world_id) OVERRIDE;
  virtual void WillReleaseScriptContext(WebKit::WebFrame* frame,
                                        v8::Handle<v8::Context> context,
                                        int world_id) OVERRIDE;

  // MessageLoop::DestructionObserver implementation.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  scoped_refptr<base::SequencedTaskRunner> render_task_runner_;
  scoped_ptr<CefRenderProcessObserver> observer_;
  scoped_ptr<CefWebWorkerScriptObserver> worker_script_observer_;

  // Map of RenderView pointers to CefBrowserImpl references.
  typedef std::map<content::RenderView*, CefRefPtr<CefBrowserImpl> > BrowserMap;
  BrowserMap browsers_;

  // Custom schemes that need to be registered with WebKit.
  struct SchemeInfo;
  typedef std::list<SchemeInfo> SchemeInfoList;
  SchemeInfoList scheme_info_list_;

  // Cross-origin white list entries that need to be registered with WebKit.
  typedef std::vector<Cef_CrossOriginWhiteListEntry_Params> CrossOriginList;
  CrossOriginList cross_origin_whitelist_entries_;

  int devtools_agent_count_;
  int uncaught_exception_stack_size_;

  // Map of worker thread IDs to task runners. Access must be protected by
  // |worker_task_runner_lock_|.
  typedef std::map<int, scoped_refptr<base::SequencedTaskRunner> >
      WorkerTaskRunnerMap;
  WorkerTaskRunnerMap worker_task_runner_map_;
  base::Lock worker_task_runner_lock_;

  // Used in single-process mode to test when the RenderThread has stopped.
  // Access must be protected by |render_thread_shutdown_lock_|.
  bool render_thread_shutdown_complete_;
  base::Lock render_thread_shutdown_lock_;
};

#endif  // CEF_LIBCEF_RENDERER_CONTENT_RENDERER_CLIENT_H_
