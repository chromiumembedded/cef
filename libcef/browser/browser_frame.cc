// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_frame.h"

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/thread_util.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

CefBrowserFrame::CefBrowserFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<cef::mojom::BrowserFrame> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)) {}

CefBrowserFrame::~CefBrowserFrame() = default;

// static
void CefBrowserFrame::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<cef::mojom::BrowserFrame>(base::BindRepeating(
      [](content::RenderFrameHost* frame_host,
         mojo::PendingReceiver<cef::mojom::BrowserFrame> receiver) {
        // This object is bound to the lifetime of |frame_host| and the mojo
        // connection. See DocumentServiceBase for details.
        new CefBrowserFrame(frame_host, std::move(receiver));
      }));
}

void CefBrowserFrame::SendMessage(const std::string& name,
                                  base::Value::List arguments) {
  // Always send to the newly created RFH, which may be speculative when
  // navigating cross-origin.
  if (auto host = GetFrameHost(/*prefer_speculative=*/true)) {
    host->SendMessage(name, std::move(arguments));
  }
}

void CefBrowserFrame::SendSharedMemoryRegion(
    const std::string& name,
    base::WritableSharedMemoryRegion region) {
  // Always send to the newly created RFH, which may be speculative when
  // navigating cross-origin.
  if (auto host = GetFrameHost(/*prefer_speculative=*/true)) {
    host->SendSharedMemoryRegion(name, std::move(region));
  }
}

void CefBrowserFrame::FrameAttached(
    mojo::PendingRemote<cef::mojom::RenderFrame> render_frame,
    bool reattached) {
  // Always send to the newly created RFH, which may be speculative when
  // navigating cross-origin.
  if (auto host = GetFrameHost(/*prefer_speculative=*/true)) {
    host->FrameAttached(std::move(render_frame), reattached);
  }
}

void CefBrowserFrame::UpdateDraggableRegions(
    absl::optional<std::vector<cef::mojom::DraggableRegionEntryPtr>> regions) {
  if (auto host = GetFrameHost()) {
    host->UpdateDraggableRegions(std::move(regions));
  }
}

CefRefPtr<CefFrameHostImpl> CefBrowserFrame::GetFrameHost(
    bool prefer_speculative) const {
  CEF_REQUIRE_UIT();
  auto rfh = render_frame_host();
  if (auto browser = CefBrowserHostBase::GetBrowserForHost(rfh)) {
    return browser->browser_info()->GetFrameForHost(rfh, nullptr,
                                                    prefer_speculative);
  }
  DCHECK(false);
  return nullptr;
}
