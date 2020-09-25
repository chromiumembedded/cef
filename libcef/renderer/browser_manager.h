// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_BROWSER_MANAGER_H_
#define CEF_LIBCEF_RENDERER_BROWSER_MANAGER_H_
#pragma once

#include <map>
#include <memory>

#include "include/internal/cef_ptr.h"

#include "base/optional.h"

namespace blink {
class WebFrame;
}

namespace content {
class RenderFrame;
class RenderView;
}  // namespace content

struct Cef_CrossOriginWhiteListEntry_Params;
class CefBrowserImpl;
class CefGuestView;
class CefRenderFrameObserver;

// Singleton object for managing BrowserImpl instances. Only accessed on the
// main renderer thread.
class CefBrowserManager {
 public:
  CefBrowserManager();
  ~CefBrowserManager();

  // Returns this singleton instance of this class.
  static CefBrowserManager* Get();

  // Called from ContentRendererClient methods of the same name.
  void RenderThreadConnected();
  void RenderFrameCreated(content::RenderFrame* render_frame,
                          CefRenderFrameObserver* render_frame_observer,
                          bool& browser_created,
                          base::Optional<bool>& is_windowless);
  void RenderViewCreated(content::RenderView* render_view,
                         bool& browser_created,
                         base::Optional<bool>& is_windowless);
  void DevToolsAgentAttached();
  void DevToolsAgentDetached();

  // Returns the browser associated with the specified RenderView.
  CefRefPtr<CefBrowserImpl> GetBrowserForView(content::RenderView* view);

  // Returns the browser associated with the specified main WebFrame.
  CefRefPtr<CefBrowserImpl> GetBrowserForMainFrame(blink::WebFrame* frame);

 private:
  friend class CefBrowserImpl;
  friend class CefGuestView;

  void WebKitInitialized();

  // Maybe create a new browser object, return the existing one, or return
  // nullptr for guest views.
  CefRefPtr<CefBrowserImpl> MaybeCreateBrowser(
      content::RenderView* render_view,
      content::RenderFrame* render_frame,
      bool* browser_created,
      base::Optional<bool>* is_windowless);

  // Called from CefBrowserImpl::OnDestruct().
  void OnBrowserDestroyed(CefBrowserImpl* browser);

  // Returns the guest view associated with the specified RenderView if any.
  CefGuestView* GetGuestViewForView(content::RenderView* view);

  // Called from CefGuestView::OnDestruct().
  void OnGuestViewDestroyed(CefGuestView* guest_view);

  // Map of RenderView pointers to CefBrowserImpl references.
  typedef std::map<content::RenderView*, CefRefPtr<CefBrowserImpl>> BrowserMap;
  BrowserMap browsers_;

  // Map of RenderView poiners to CefGuestView implementations.
  typedef std::map<content::RenderView*, std::unique_ptr<CefGuestView>>
      GuestViewMap;
  GuestViewMap guest_views_;

  // Cross-origin white list entries that need to be registered with WebKit.
  typedef std::vector<Cef_CrossOriginWhiteListEntry_Params> CrossOriginList;
  CrossOriginList cross_origin_whitelist_entries_;

  int devtools_agent_count_ = 0;
  int uncaught_exception_stack_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserManager);
};

#endif  // CEF_LIBCEF_RENDERER_BROWSER_MANAGER_H_
