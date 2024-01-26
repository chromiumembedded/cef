// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_manager.h"

#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/origin_whitelist_impl.h"
#include "libcef/common/frame_util.h"

#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"

CefBrowserManager::CefBrowserManager(int render_process_id)
    : render_process_id_(render_process_id) {}

CefBrowserManager::~CefBrowserManager() = default;

// static
void CefBrowserManager::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* host) {
  registry->AddInterface<cef::mojom::BrowserManager>(base::BindRepeating(
      [](int render_process_id,
         mojo::PendingReceiver<cef::mojom::BrowserManager> receiver) {
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<CefBrowserManager>(render_process_id),
            std::move(receiver));
      },
      host->GetID()));
}

// static
mojo::Remote<cef::mojom::RenderManager>
CefBrowserManager::GetRenderManagerForProcess(
    content::RenderProcessHost* host) {
  mojo::Remote<cef::mojom::RenderManager> client;
  host->BindReceiver(client.BindNewPipeAndPassReceiver());
  return client;
}

void CefBrowserManager::GetNewRenderThreadInfo(
    cef::mojom::BrowserManager::GetNewRenderThreadInfoCallback callback) {
  auto info = cef::mojom::NewRenderThreadInfo::New();
  GetCrossOriginWhitelistEntries(&info->cross_origin_whitelist_entries);
  std::move(callback).Run(std::move(info));
}

void CefBrowserManager::GetNewBrowserInfo(
    const blink::LocalFrameToken& render_frame_token,
    cef::mojom::BrowserManager::GetNewBrowserInfoCallback callback) {
  CefBrowserInfoManager::GetInstance()->OnGetNewBrowserInfo(
      content::GlobalRenderFrameHostToken(render_process_id_,
                                          render_frame_token),
      std::move(callback));
}
