// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_ALLOY_ALLOY_CONTENT_RENDERER_CLIENT_H_
#define CEF_LIBCEF_RENDERER_ALLOY_ALLOY_CONTENT_RENDERER_CLIENT_H_
#pragma once

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "libcef/renderer/browser_impl.h"

#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/common/plugin.mojom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"

namespace extensions {
class CefExtensionsRendererClient;
class Dispatcher;
class DispatcherDelegate;
class ExtensionsClient;
class ExtensionsRendererClient;
class ResourceRequestPolicy;
}  // namespace extensions

namespace visitedlink {
class VisitedLinkReader;
}

namespace web_cache {
class WebCacheImpl;
}

class AlloyRenderThreadObserver;
class CefRenderManager;
class SpellCheck;

class AlloyContentRendererClient
    : public content::ContentRendererClient,
      public service_manager::LocalInterfaceProvider,
      public base::CurrentThread::DestructionObserver {
 public:
  AlloyContentRendererClient();

  AlloyContentRendererClient(const AlloyContentRendererClient&) = delete;
  AlloyContentRendererClient& operator=(const AlloyContentRendererClient&) =
      delete;

  ~AlloyContentRendererClient() override;

  // Returns the singleton AlloyContentRendererClient instance.
  // This method is deprecated and should not be used in new callsites.
  static AlloyContentRendererClient* Get();

  // Render thread task runner.
  base::SingleThreadTaskRunner* render_task_runner() const {
    return render_task_runner_.get();
  }

  // Returns the task runner for the current thread. Returns NULL if the current
  // thread is not the main render process thread.
  scoped_refptr<base::SingleThreadTaskRunner> GetCurrentTaskRunner();

  // Perform cleanup work that needs to occur before shutdown when running in
  // single-process mode. Blocks until cleanup is complete.
  void RunSingleProcessCleanup();

  // ContentRendererClient implementation.
  void PostIOThreadCreated(
      base::SingleThreadTaskRunner* io_thread_task_runner) override;
  void RenderThreadStarted() override;
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;
  void RenderThreadConnected() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void WebViewCreated(blink::WebView* web_view,
                      bool was_created_by_renderer,
                      const url::Origin* outermost_origin) override;
  bool IsPluginHandledExternally(content::RenderFrame* render_frame,
                                 const blink::WebElement& plugin_element,
                                 const GURL& original_url,
                                 const std::string& mime_type) override;
  bool OverrideCreatePlugin(content::RenderFrame* render_frame,
                            const blink::WebPluginParams& params,
                            blink::WebPlugin** plugin) override;
  void WillSendRequest(blink::WebLocalFrame* frame,
                       ui::PageTransition transition_type,
                       const blink::WebURL& url,
                       const net::SiteForCookies& site_for_cookies,
                       const url::Origin* initiator_origin,
                       GURL* new_url) override;
  uint64_t VisitedLinkHash(std::string_view canonical_url) override;
  bool IsLinkVisited(uint64_t link_hash) override;
  bool IsOriginIsolatedPepperPlugin(const base::FilePath& plugin_path) override;
  void GetSupportedKeySystems(media::GetSupportedKeySystemsCB cb) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentIdle(content::RenderFrame* render_frame) override;
  void DevToolsAgentAttached() override;
  void DevToolsAgentDetached() override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType provider_type) override;
  void AppendContentSecurityPolicy(
      const blink::WebURL& url,
      blink::WebVector<blink::WebContentSecurityPolicyHeader>* csp) override;

  // service_manager::LocalInterfaceProvider implementation.
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle) override;

  // MessageLoopCurrent::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override;

  AlloyRenderThreadObserver* GetAlloyObserver() const {
    return observer_.get();
  }

 private:
  void OnBrowserCreated(blink::WebView* web_view,
                        absl::optional<bool> is_windowless);

  // Perform cleanup work for single-process mode.
  void RunSingleProcessCleanupOnUIThread();

  // Time at which this object was created. This is very close to the time at
  // which the RendererMain function was entered.
  base::TimeTicks main_entry_time_;

  std::unique_ptr<CefRenderManager> render_manager_;

  scoped_refptr<base::SingleThreadTaskRunner> render_task_runner_;
  std::unique_ptr<AlloyRenderThreadObserver> observer_;
  std::unique_ptr<web_cache::WebCacheImpl> web_cache_impl_;
  std::unique_ptr<SpellCheck> spellcheck_;
  std::unique_ptr<visitedlink::VisitedLinkReader> visited_link_slave_;

  std::unique_ptr<extensions::ExtensionsClient> extensions_client_;
  std::unique_ptr<extensions::CefExtensionsRendererClient>
      extensions_renderer_client_;

  // Used in single-process mode to test when cleanup is complete.
  // Access must be protected by |single_process_cleanup_lock_|.
  bool single_process_cleanup_complete_ = false;
  base::Lock single_process_cleanup_lock_;
};

#endif  // CEF_LIBCEF_RENDERER_ALLOY_ALLOY_CONTENT_RENDERER_CLIENT_H_
