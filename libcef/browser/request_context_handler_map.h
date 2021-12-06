// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_REQUEST_CONTEXT_HANDLER_MAP_
#define CEF_LIBCEF_BROWSER_REQUEST_CONTEXT_HANDLER_MAP_
#pragma once

#include <map>

#include "include/cef_request_context.h"
#include "include/cef_request_context_handler.h"

#include "content/public/browser/global_routing_id.h"

// Tracks CefRequestContextHandler associations on a single thread.
class CefRequestContextHandlerMap {
 public:
  CefRequestContextHandlerMap();

  CefRequestContextHandlerMap(const CefRequestContextHandlerMap&) = delete;
  CefRequestContextHandlerMap& operator=(const CefRequestContextHandlerMap&) =
      delete;

  ~CefRequestContextHandlerMap();

  // Keep track of handlers associated with specific frames. This information
  // originates from frame create/delete notifications in
  // CefBrowserContentsDelegate or CefMimeHandlerViewGuestDelegate which are
  // forwarded via CefRequestContextImpl and CefBrowserContext.
  void AddHandler(const content::GlobalRenderFrameHostId& global_id,
                  CefRefPtr<CefRequestContextHandler> handler);
  void RemoveHandler(const content::GlobalRenderFrameHostId& global_id);

  // Returns the handler that matches the specified IDs. If
  // |require_frame_match| is true only exact matches will be returned. If
  // |require_frame_match| is false, and there is not an exact match, then the
  // first handler for the same |global_id.child_id| will be returned.
  CefRefPtr<CefRequestContextHandler> GetHandler(
      const content::GlobalRenderFrameHostId& global_id,
      bool require_frame_match) const;

 private:
  // Map of global ID to handler. These IDs are guaranteed to uniquely
  // identify a RFH for its complete lifespan. See documentation on
  // RenderFrameHost::GetFrameTreeNodeId() for background.
  using RenderIdHandlerMap = std::map<content::GlobalRenderFrameHostId,
                                      CefRefPtr<CefRequestContextHandler>>;
  RenderIdHandlerMap render_id_handler_map_;
};

#endif  // CEF_LIBCEF_BROWSER_REQUEST_CONTEXT_HANDLER_MAP_
