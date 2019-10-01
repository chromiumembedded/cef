// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_REQUEST_CONTEXT_HANDLER_MAP_
#define CEF_LIBCEF_BROWSER_REQUEST_CONTEXT_HANDLER_MAP_
#pragma once

#include <map>

#include "include/cef_request_context.h"
#include "include/cef_request_context_handler.h"

#include "base/macros.h"

// Tracks CefRequestContextHandler associations on a single thread.
class CefRequestContextHandlerMap {
 public:
  CefRequestContextHandlerMap();
  ~CefRequestContextHandlerMap();

  // Keep track of handlers associated with specific frames. This information
  // originates from frame create/delete notifications in CefBrowserHostImpl or
  // CefMimeHandlerViewGuestDelegate which are forwarded via
  // CefRequestContextImpl and CefBrowserContext.
  void AddHandler(int render_process_id,
                  int render_frame_id,
                  int frame_tree_node_id,
                  CefRefPtr<CefRequestContextHandler> handler);
  void RemoveHandler(int render_process_id,
                     int render_frame_id,
                     int frame_tree_node_id);

  // Returns the handler that matches the specified IDs. Pass -1 for unknown
  // values. If |require_frame_match| is true only exact matches will be
  // returned. If |require_frame_match| is false, and there is not an exact
  // match, then the first handler for the same |render_process_id| will be
  // returned.
  CefRefPtr<CefRequestContextHandler> GetHandler(
      int render_process_id,
      int render_frame_id,
      int frame_tree_node_id,
      bool require_frame_match) const;

 private:
  // Map of (render_process_id, render_frame_id) to handler.
  typedef std::map<std::pair<int, int>, CefRefPtr<CefRequestContextHandler>>
      RenderIdHandlerMap;
  RenderIdHandlerMap render_id_handler_map_;

  // Map of frame_tree_node_id to handler. Keeping this map is necessary
  // because, when navigating the main frame, a new (pre-commit) network request
  // will be created before the RenderFrameHost. Consequently we can't rely
  // on valid render IDs. See https://crbug.com/776884 for background.
  typedef std::map<int, CefRefPtr<CefRequestContextHandler>> NodeIdHandlerMap;
  NodeIdHandlerMap node_id_handler_map_;

  DISALLOW_COPY_AND_ASSIGN(CefRequestContextHandlerMap);
};

#endif  // CEF_LIBCEF_BROWSER_REQUEST_CONTEXT_HANDLER_MAP_
