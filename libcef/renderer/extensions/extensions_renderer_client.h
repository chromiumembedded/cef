// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_EXTENSIONS_EXTENSIONS_RENDERER_CLIENT_H_
#define CEF_LIBCEF_RENDERER_EXTENSIONS_EXTENSIONS_RENDERER_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace blink {
class WebFrame;
class WebLocalFrame;
struct WebPluginParams;
class WebURL;
}  // namespace blink

namespace content {
class BrowserPluginDelegate;
class RenderFrame;
struct WebPluginInfo;
}  // namespace content

namespace url {
class Origin;
}

namespace extensions {

class Dispatcher;
class DispatcherDelegate;
class ExtensionsGuestViewContainerDispatcher;
class ResourceRequestPolicy;

class CefExtensionsRendererClient : public ExtensionsRendererClient {
 public:
  CefExtensionsRendererClient();
  ~CefExtensionsRendererClient() override;

  // ExtensionsRendererClient implementation.
  bool IsIncognitoProcess() const override;
  int GetLowestIsolatedWorldId() const override;
  extensions::Dispatcher* GetDispatcher() override;
  void OnExtensionLoaded(const extensions::Extension& extension) override;
  void OnExtensionUnloaded(
      const extensions::ExtensionId& extension_id) override;
  bool ExtensionAPIEnabledForServiceWorkerScript(
      const GURL& scope,
      const GURL& script_url) const override;

  // See CefContentRendererClient methods with the same names.
  void RenderThreadStarted();
  void RenderFrameCreated(content::RenderFrame* render_frame,
                          service_manager::BinderRegistry* registry);
  bool OverrideCreatePlugin(content::RenderFrame* render_frame,
                            const blink::WebPluginParams& params);
  void WillSendRequest(blink::WebLocalFrame* frame,
                       ui::PageTransition transition_type,
                       const blink::WebURL& url,
                       const url::Origin* initiator_origin,
                       GURL* new_url,
                       bool* attach_same_site_cookies);
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame);
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame);
  void RunScriptsAtDocumentIdle(content::RenderFrame* render_frame);

  static bool IsStandaloneExtensionProcess();
  static bool ShouldFork(blink::WebLocalFrame* frame,
                         const GURL& url,
                         bool is_initial_navigation,
                         bool is_server_redirect);
  static content::BrowserPluginDelegate* CreateBrowserPluginDelegate(
      content::RenderFrame* render_frame,
      const content::WebPluginInfo& info,
      const std::string& mime_type,
      const GURL& original_url);

 private:
  std::unique_ptr<extensions::Dispatcher> extension_dispatcher_;
  std::unique_ptr<extensions::ExtensionsGuestViewContainerDispatcher>
      guest_view_container_dispatcher_;
  std::unique_ptr<extensions::ResourceRequestPolicy> resource_request_policy_;

  DISALLOW_COPY_AND_ASSIGN(CefExtensionsRendererClient);
};

}  // namespace extensions

#endif  // CEF_LIBCEF_RENDERER_EXTENSIONS_EXTENSIONS_RENDERER_CLIENT_H_
