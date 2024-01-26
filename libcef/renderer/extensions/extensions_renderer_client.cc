// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/extensions/extensions_renderer_client.h"

#include "libcef/renderer/alloy/alloy_content_renderer_client.h"
#include "libcef/renderer/alloy/alloy_render_thread_observer.h"
#include "libcef/renderer/extensions/extensions_dispatcher_delegate.h"

#include "base/stl_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/resource_request_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_render_frame_observer.h"
#include "extensions/renderer/extensions_renderer_api_provider.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_params.h"

namespace extensions {

namespace {

void IsGuestViewApiAvailableToScriptContext(
    bool* api_is_available,
    extensions::ScriptContext* context) {
  if (context->GetAvailability("guestViewInternal").is_available()) {
    *api_is_available = true;
  }
}

}  // namespace

CefExtensionsRendererClient::CefExtensionsRendererClient(
    AlloyContentRendererClient* alloy_content_renderer_client)
    : alloy_content_renderer_client_(alloy_content_renderer_client) {}

CefExtensionsRendererClient::~CefExtensionsRendererClient() = default;

bool CefExtensionsRendererClient::IsIncognitoProcess() const {
  return alloy_content_renderer_client_->GetAlloyObserver()
      ->IsIncognitoProcess();
}

int CefExtensionsRendererClient::GetLowestIsolatedWorldId() const {
  // CEF doesn't need to reserve world IDs for anything other than extensions,
  // so we always return 1. Note that 0 is reserved for the global world.
  return 1;
}

extensions::Dispatcher* CefExtensionsRendererClient::GetDispatcher() {
  return extension_dispatcher_.get();
}

void CefExtensionsRendererClient::OnExtensionLoaded(
    const extensions::Extension& extension) {
  resource_request_policy_->OnExtensionLoaded(extension);
}

void CefExtensionsRendererClient::OnExtensionUnloaded(
    const extensions::ExtensionId& extension_id) {
  resource_request_policy_->OnExtensionUnloaded(extension_id);
}

bool CefExtensionsRendererClient::ExtensionAPIEnabledForServiceWorkerScript(
    const GURL& scope,
    const GURL& script_url) const {
  // TODO(extensions): Implement to support background sevice worker scripts
  // in extensions
  return false;
}

void CefExtensionsRendererClient::RenderThreadStarted() {
  content::RenderThread* thread = content::RenderThread::Get();

  extension_dispatcher_ = std::make_unique<extensions::Dispatcher>(
      std::make_unique<extensions::CefExtensionsDispatcherDelegate>(),
      std::vector<
          std::unique_ptr<extensions::ExtensionsRendererAPIProvider>>());
  extension_dispatcher_->OnRenderThreadStarted(thread);
  resource_request_policy_ =
      std::make_unique<extensions::ResourceRequestPolicy>(
          extension_dispatcher_.get());

  thread->AddObserver(extension_dispatcher_.get());
}

void CefExtensionsRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry) {
  new extensions::ExtensionsRenderFrameObserver(render_frame, registry);
  new extensions::ExtensionFrameHelper(render_frame,
                                       extension_dispatcher_.get());
  extension_dispatcher_->OnRenderFrameCreated(render_frame);
}

bool CefExtensionsRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  if (params.mime_type.Utf8() != content::kBrowserPluginMimeType) {
    return true;
  }

  bool guest_view_api_available = false;
  extension_dispatcher_->script_context_set_iterator()->ForEach(
      render_frame, base::BindRepeating(&IsGuestViewApiAvailableToScriptContext,
                                        &guest_view_api_available));
  return !guest_view_api_available;
}

void CefExtensionsRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* initiator_origin,
    GURL* new_url) {
  // Check whether the request should be allowed. If not allowed, we reset the
  // URL to something invalid to prevent the request and cause an error.
  if (url.ProtocolIs(extensions::kExtensionScheme) &&
      !resource_request_policy_->CanRequestResource(
          GURL(url), frame, transition_type, initiator_origin)) {
    *new_url = GURL(chrome::kExtensionInvalidRequestURL);
  }
}

void CefExtensionsRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentStart(render_frame);
}

void CefExtensionsRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentEnd(render_frame);
}

void CefExtensionsRendererClient::RunScriptsAtDocumentIdle(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentIdle(render_frame);
}

}  // namespace extensions
