/// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/render_thread_observer.h"

#include "libcef/common/cef_messages.h"
#include "libcef/common/net/net_resource_provider.h"
#include "libcef/renderer/blink_glue.h"
#include "libcef/renderer/content_renderer_client.h"

#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/net_module.h"
#include "services/service_manager/public/cpp/binder_registry.h"
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

CefRenderThreadObserver::CefRenderThreadObserver()
    : visited_link_slave_(new visitedlink::VisitedLinkSlave) {
  net::NetModule::SetResourceProvider(NetResourceProvider);

  auto registry = std::make_unique<service_manager::BinderRegistry>();
  registry->AddInterface(visited_link_slave_->GetBindCallback(),
                         base::ThreadTaskRunnerHandle::Get());
  if (content::ChildThread::Get()) {
    content::ChildThread::Get()
        ->GetServiceManagerConnection()
        ->AddConnectionFilter(std::make_unique<content::SimpleConnectionFilter>(
            std::move(registry)));
  }
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
    chrome::mojom::ChromeOSListenerRequest chromeos_listener_request) {
  is_incognito_process_ = is_incognito_process;
}

void CefRenderThreadObserver::SetConfiguration(
    chrome::mojom::DynamicParamsPtr params) {
  *GetDynamicConfigParams() = std::move(*params);
}

void CefRenderThreadObserver::SetContentSettingRules(
    const RendererContentSettingRules& rules) {}

void CefRenderThreadObserver::SetFieldTrialGroup(
    const std::string& trial_name,
    const std::string& group_name) {
  content::RenderThread::Get()->SetFieldTrialGroup(trial_name, group_name);
}

void CefRenderThreadObserver::OnRendererConfigurationAssociatedRequest(
    chrome::mojom::RendererConfigurationAssociatedRequest request) {
  renderer_configuration_bindings_.AddBinding(this, std::move(request));
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
