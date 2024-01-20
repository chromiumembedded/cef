// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_RENDER_MANAGER_H_
#define CEF_LIBCEF_RENDERER_RENDER_MANAGER_H_
#pragma once

#include <map>
#include <memory>

#include "include/internal/cef_ptr.h"

#include "cef/libcef/common/mojom/cef.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink {
class WebFrame;
class WebView;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace mojo {
class BinderMap;
}  // namespace mojo

class CefBrowserImpl;
class CefGuestView;
class CefRenderFrameObserver;

// Singleton object for managing BrowserImpl instances. Only accessed on the
// main renderer thread.
class CefRenderManager : public cef::mojom::RenderManager {
 public:
  CefRenderManager();

  CefRenderManager(const CefRenderManager&) = delete;
  CefRenderManager& operator=(const CefRenderManager&) = delete;

  ~CefRenderManager() override;

  // Returns this singleton instance of this class.
  static CefRenderManager* Get();

  // Called from ContentRendererClient methods of the same name.
  void RenderThreadConnected();
  void RenderFrameCreated(content::RenderFrame* render_frame,
                          CefRenderFrameObserver* render_frame_observer,
                          bool& browser_created,
                          absl::optional<bool>& is_windowless);
  void WebViewCreated(blink::WebView* web_view,
                      bool& browser_created,
                      absl::optional<bool>& is_windowless);
  void DevToolsAgentAttached();
  void DevToolsAgentDetached();
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders);

  // Returns the browser associated with the specified RenderView.
  CefRefPtr<CefBrowserImpl> GetBrowserForView(blink::WebView* view);

  // Returns the browser associated with the specified main WebFrame.
  CefRefPtr<CefBrowserImpl> GetBrowserForMainFrame(blink::WebFrame* frame);

  // Connects to CefBrowserManager in the browser process.
  mojo::Remote<cef::mojom::BrowserManager>& GetBrowserManager();

  // Returns true if this renderer process is hosting an extension.
  static bool IsExtensionProcess();

  // Returns true if this renderer process is hosting a PDF.
  static bool IsPdfProcess();

 private:
  friend class CefBrowserImpl;
  friend class CefGuestView;

  // Binds receivers for the RenderManager interface.
  void BindReceiver(mojo::PendingReceiver<cef::mojom::RenderManager> receiver);

  // cef::mojom::RenderManager methods:
  void ModifyCrossOriginWhitelistEntry(
      bool add,
      cef::mojom::CrossOriginWhiteListEntryPtr entry) override;
  void ClearCrossOriginWhitelist() override;

  void WebKitInitialized();

  // Maybe create a new browser object, return the existing one, or return
  // nullptr for guest views.
  CefRefPtr<CefBrowserImpl> MaybeCreateBrowser(
      blink::WebView* web_view,
      content::RenderFrame* render_frame,
      bool* browser_created,
      absl::optional<bool>* is_windowless);

  // Called from CefBrowserImpl::OnDestruct().
  void OnBrowserDestroyed(CefBrowserImpl* browser);

  // Returns the guest view associated with the specified RenderView if any.
  CefGuestView* GetGuestViewForView(blink::WebView* view);

  // Called from CefGuestView::OnDestruct().
  void OnGuestViewDestroyed(CefGuestView* guest_view);

  // Map of RenderView pointers to CefBrowserImpl references.
  using BrowserMap = std::map<blink::WebView*, CefRefPtr<CefBrowserImpl>>;
  BrowserMap browsers_;

  // Map of RenderView poiners to CefGuestView implementations.
  using GuestViewMap = std::map<blink::WebView*, std::unique_ptr<CefGuestView>>;
  GuestViewMap guest_views_;

  // Cross-origin white list entries that need to be registered with WebKit.
  std::vector<cef::mojom::CrossOriginWhiteListEntryPtr>
      cross_origin_whitelist_entries_;

  int devtools_agent_count_ = 0;
  int uncaught_exception_stack_size_ = 0;

  mojo::ReceiverSet<cef::mojom::RenderManager> receivers_;

  mojo::Remote<cef::mojom::BrowserManager> browser_manager_;
};

#endif  // CEF_LIBCEF_RENDERER_RENDER_MANAGER_H_
