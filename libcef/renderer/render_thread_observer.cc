/// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/render_thread_observer.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/net/net_resource_provider.h"
#include "libcef/renderer/content_renderer_client.h"

#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "content/public/renderer/render_thread.h"
#include "net/base/net_module.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"

bool CefRenderThreadObserver::is_incognito_process_ = false;

CefRenderThreadObserver::CefRenderThreadObserver()
  : visited_link_slave_(new visitedlink::VisitedLinkSlave) {
  net::NetModule::SetResourceProvider(NetResourceProvider);

  content::RenderThread* thread = content::RenderThread::Get();
  thread->GetInterfaceRegistry()->AddInterface(
      visited_link_slave_->GetBindCallback());
}

CefRenderThreadObserver::~CefRenderThreadObserver() {
}

bool CefRenderThreadObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefRenderThreadObserver, message)
    IPC_MESSAGE_HANDLER(CefProcessMsg_SetIsIncognitoProcess,
                        OnSetIsIncognitoProcess)
    IPC_MESSAGE_HANDLER(CefProcessMsg_ModifyCrossOriginWhitelistEntry,
                        OnModifyCrossOriginWhitelistEntry)
    IPC_MESSAGE_HANDLER(CefProcessMsg_ClearCrossOriginWhitelist,
                        OnClearCrossOriginWhitelist)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CefRenderThreadObserver::OnRenderProcessShutdown() {
  CefContentRendererClient::Get()->OnRenderProcessShutdown();
  visited_link_slave_.reset();
}

void CefRenderThreadObserver::OnSetIsIncognitoProcess(
    bool is_incognito_process) {
  is_incognito_process_ = is_incognito_process;
}

void CefRenderThreadObserver::OnModifyCrossOriginWhitelistEntry(
    bool add,
    const Cef_CrossOriginWhiteListEntry_Params& params) {
  GURL gurl = GURL(params.source_origin);
  if (add) {
    blink::WebSecurityPolicy::AddOriginAccessWhitelistEntry(
        gurl,
        blink::WebString::FromUTF8(params.target_protocol),
        blink::WebString::FromUTF8(params.target_domain),
        params.allow_target_subdomains);
  } else {
    blink::WebSecurityPolicy::RemoveOriginAccessWhitelistEntry(
        gurl,
        blink::WebString::FromUTF8(params.target_protocol),
        blink::WebString::FromUTF8(params.target_domain),
        params.allow_target_subdomains);
  }
}

void CefRenderThreadObserver::OnClearCrossOriginWhitelist() {
  blink::WebSecurityPolicy::ResetOriginAccessWhitelists();
}
