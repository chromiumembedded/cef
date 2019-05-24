// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_CONTENT_RENDERER_CLIENT_H_
#define CEF_LIBCEF_RENDERER_CONTENT_RENDERER_CLIENT_H_
#pragma once

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "libcef/renderer/browser_impl.h"

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "chrome/common/plugin.mojom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"

namespace blink {
class WebURLLoaderFactory;
}

namespace extensions {
class CefExtensionsRendererClient;
class Dispatcher;
class DispatcherDelegate;
class ExtensionsClient;
class ExtensionsGuestViewContainerDispatcher;
class ExtensionsRendererClient;
class ResourceRequestPolicy;
}  // namespace extensions

namespace web_cache {
class WebCacheImpl;
}

class CefGuestView;
class CefRenderThreadObserver;
struct Cef_CrossOriginWhiteListEntry_Params;
class ChromePDFPrintClient;
class SpellCheck;

class CefContentRendererClient
    : public content::ContentRendererClient,
      public service_manager::Service,
      public service_manager::LocalInterfaceProvider,
      public base::MessageLoopCurrent::DestructionObserver {
 public:
  CefContentRendererClient();
  ~CefContentRendererClient() override;

  // Returns the singleton CefContentRendererClient instance.
  static CefContentRendererClient* Get();

  // Returns the browser associated with the specified RenderView.
  CefRefPtr<CefBrowserImpl> GetBrowserForView(content::RenderView* view);

  // Returns the browser associated with the specified main WebFrame.
  CefRefPtr<CefBrowserImpl> GetBrowserForMainFrame(blink::WebFrame* frame);

  // Called from CefBrowserImpl::OnDestruct().
  void OnBrowserDestroyed(CefBrowserImpl* browser);

  // Returns true if a guest view associated with the specified RenderView.
  bool HasGuestViewForView(content::RenderView* view);

  // Called from CefGuestView::OnDestruct().
  void OnGuestViewDestroyed(CefGuestView* guest_view);

  // Render thread task runner.
  base::SingleThreadTaskRunner* render_task_runner() const {
    return render_task_runner_.get();
  }

  int uncaught_exception_stack_size() const {
    return uncaught_exception_stack_size_;
  }

  // Returns a factory that only supports unintercepted http(s) and blob
  // requests. Used by CefRenderURLRequest.
  blink::WebURLLoaderFactory* GetDefaultURLLoaderFactory();

  void WebKitInitialized();

  // Returns the task runner for the current thread. Returns NULL if the current
  // thread is not the main render process thread.
  scoped_refptr<base::SingleThreadTaskRunner> GetCurrentTaskRunner();

  // Perform cleanup work that needs to occur before shutdown when running in
  // single-process mode. Blocks until cleanup is complete.
  void RunSingleProcessCleanup();

  // ContentRendererClient implementation.
  void RenderThreadStarted() override;
  void RenderThreadConnected() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void RenderViewCreated(content::RenderView* render_view) override;
  bool OverrideCreatePlugin(content::RenderFrame* render_frame,
                            const blink::WebPluginParams& params,
                            blink::WebPlugin** plugin) override;
  bool ShouldFork(blink::WebLocalFrame* frame,
                  const GURL& url,
                  const std::string& http_method,
                  bool is_initial_navigation,
                  bool is_server_redirect) override;
  void WillSendRequest(blink::WebLocalFrame* frame,
                       ui::PageTransition transition_type,
                       const blink::WebURL& url,
                       const url::Origin* initiator_origin,
                       GURL* new_url,
                       bool* attach_same_site_cookies) override;
  uint64_t VisitedLinkHash(const char* canonical_url, size_t length) override;
  bool IsLinkVisited(uint64_t link_hash) override;
  bool IsOriginIsolatedPepperPlugin(const base::FilePath& plugin_path) override;
  content::BrowserPluginDelegate* CreateBrowserPluginDelegate(
      content::RenderFrame* render_frame,
      const content::WebPluginInfo& info,
      const std::string& mime_type,
      const GURL& original_url) override;
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<::media::KeySystemProperties>>* key_systems)
      override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentIdle(content::RenderFrame* render_frame) override;
  void DevToolsAgentAttached() override;
  void DevToolsAgentDetached() override;
  void CreateRendererService(
      service_manager::mojom::ServiceRequest service_request) override;

  // service_manager::Service implementation.
  void OnBindInterface(const service_manager::BindSourceInfo& remote_info,
                       const std::string& name,
                       mojo::ScopedMessagePipeHandle handle) override;

  // service_manager::LocalInterfaceProvider implementation.
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle) override;

  // MessageLoopCurrent::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override;

 private:
  // Maybe create a new browser object, return the existing one, or return
  // nullptr for guest views.
  CefRefPtr<CefBrowserImpl> MaybeCreateBrowser(
      content::RenderView* render_view,
      content::RenderFrame* render_frame);

  // Perform cleanup work for single-process mode.
  void RunSingleProcessCleanupOnUIThread();

  service_manager::Connector* GetConnector();

  // Time at which this object was created. This is very close to the time at
  // which the RendererMain function was entered.
  base::TimeTicks main_entry_time_;

  scoped_refptr<base::SingleThreadTaskRunner> render_task_runner_;
  std::unique_ptr<CefRenderThreadObserver> observer_;
  std::unique_ptr<web_cache::WebCacheImpl> web_cache_impl_;
  std::unique_ptr<SpellCheck> spellcheck_;

  std::unique_ptr<blink::WebURLLoaderFactory> default_url_loader_factory_;

  // Map of RenderView pointers to CefBrowserImpl references.
  typedef std::map<content::RenderView*, CefRefPtr<CefBrowserImpl>> BrowserMap;
  BrowserMap browsers_;

  // Map of RenderView poiners to CefGuestView implementations.
  typedef std::map<content::RenderView*, std::unique_ptr<CefGuestView>>
      GuestViewMap;
  GuestViewMap guest_views_;

  // Cross-origin white list entries that need to be registered with WebKit.
  typedef std::vector<Cef_CrossOriginWhiteListEntry_Params> CrossOriginList;
  CrossOriginList cross_origin_whitelist_entries_;

  std::unique_ptr<ChromePDFPrintClient> pdf_print_client_;

  std::unique_ptr<extensions::ExtensionsClient> extensions_client_;
  std::unique_ptr<extensions::CefExtensionsRendererClient>
      extensions_renderer_client_;

  int devtools_agent_count_;
  int uncaught_exception_stack_size_;

  // Used in single-process mode to test when cleanup is complete.
  // Access must be protected by |single_process_cleanup_lock_|.
  bool single_process_cleanup_complete_;
  base::Lock single_process_cleanup_lock_;

  service_manager::ServiceBinding service_binding_{this};
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(CefContentRendererClient);
};

#endif  // CEF_LIBCEF_RENDERER_CONTENT_RENDERER_CLIENT_H_
