/// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/render_thread_observer.h"

#include "libcef/common/cef_messages.h"
#include "libcef/common/net/net_resource_provider.h"
#include "libcef/renderer/blink_glue.h"
#include "libcef/renderer/content_renderer_client.h"

#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "content/public/child/child_thread.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/net_module.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_security_policy.h"

namespace {

chrome::mojom::DynamicParams* GetDynamicConfigParams() {
  static base::NoDestructor<chrome::mojom::DynamicParams> dynamic_params;
  return dynamic_params.get();
}

}  // namespace

bool CefRenderThreadObserver::is_incognito_process_ = false;

CefRenderThreadObserver::CefRenderThreadObserver() {
  net::NetModule::SetResourceProvider(NetResourceProvider);
}

CefRenderThreadObserver::~CefRenderThreadObserver() {}

// static
const chrome::mojom::DynamicParams&
CefRenderThreadObserver::GetDynamicParams() {
  return *GetDynamicConfigParams();
}

bool CefRenderThreadObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefRenderThreadObserver, message)
    IPC_MESSAGE_HANDLER(CefProcessMsg_ModifyCrossOriginWhitelistEntry,
                        OnModifyCrossOriginWhitelistEntry)
    IPC_MESSAGE_HANDLER(CefProcessMsg_ClearCrossOriginWhitelist,
                        OnClearCrossOriginWhitelist)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CefRenderThreadObserver::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface(base::Bind(
      &CefRenderThreadObserver::OnRendererConfigurationAssociatedRequest,
      base::Unretained(this)));
}

void CefRenderThreadObserver::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(
      chrome::mojom::RendererConfiguration::Name_);
}

void CefRenderThreadObserver::SetInitialConfiguration(
    bool is_incognito_process,
    mojo::PendingReceiver<chrome::mojom::ChromeOSListener> chromeos_listener) {
  is_incognito_process_ = is_incognito_process;
}

void CefRenderThreadObserver::SetConfiguration(
    chrome::mojom::DynamicParamsPtr params) {
  *GetDynamicConfigParams() = std::move(*params);
}

void CefRenderThreadObserver::SetContentSettingRules(
    const RendererContentSettingRules& rules) {}

void CefRenderThreadObserver::OnRendererConfigurationAssociatedRequest(
    mojo::PendingAssociatedReceiver<chrome::mojom::RendererConfiguration>
        receiver) {
  renderer_configuration_receivers_.Add(this, std::move(receiver));
}

void CefRenderThreadObserver::OnModifyCrossOriginWhitelistEntry(
    bool add,
    const Cef_CrossOriginWhiteListEntry_Params& params) {
  GURL gurl = GURL(params.source_origin);
  if (add) {
    blink::WebSecurityPolicy::AddOriginAccessAllowListEntry(
        gurl, blink::WebString::FromUTF8(params.target_protocol),
        blink::WebString::FromUTF8(params.target_domain),
        /*destination_port=*/0,
        params.allow_target_subdomains
            ? network::mojom::CorsDomainMatchMode::kAllowSubdomains
            : network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
        network::mojom::CorsPortMatchMode::kAllowAnyPort,
        network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  } else {
    blink::WebSecurityPolicy::ClearOriginAccessListForOrigin(gurl);
  }
}

void CefRenderThreadObserver::OnClearCrossOriginWhitelist() {
  blink::WebSecurityPolicy::ClearOriginAccessList();
}
