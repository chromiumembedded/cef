// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_MANAGER_H_
#define CEF_LIBCEF_BROWSER_BROWSER_MANAGER_H_

#include "cef/libcef/common/mojom/cef.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace blink {
class AssociatedInterfaceRegistry;
}

namespace content {
class RenderProcessHost;
}

class CefBrowserManager : public cef::mojom::BrowserManager {
 public:
  explicit CefBrowserManager(int render_process_id);

  CefBrowserManager(const CefBrowserManager&) = delete;
  CefBrowserManager& operator=(const CefBrowserManager&) = delete;

  ~CefBrowserManager() override;

  // Called from the ContentBrowserClient method of the same name.
  // |associated_registry| is used for interfaces which must be associated with
  // some IPC::ChannelProxy, meaning that messages on the interface retain FIFO
  // with respect to legacy Chrome IPC messages sent or dispatched on the
  // channel.
  static void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* host);

  // Connects to CefRenderManager in the render process.
  static mojo::Remote<cef::mojom::RenderManager> GetRenderManagerForProcess(
      content::RenderProcessHost* host);

 private:
  // cef::mojom::BrowserManager methods:
  void GetNewRenderThreadInfo(
      cef::mojom::BrowserManager::GetNewRenderThreadInfoCallback callback)
      override;
  void GetNewBrowserInfo(
      const blink::LocalFrameToken& render_frame_token,
      cef::mojom::BrowserManager::GetNewBrowserInfoCallback callback) override;

  // The process ID of the renderer.
  const int render_process_id_;
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_MANAGER_H_
