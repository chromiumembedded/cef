// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/request_context_handler_map.h"

CefRequestContextHandlerMap::CefRequestContextHandlerMap() = default;
CefRequestContextHandlerMap::~CefRequestContextHandlerMap() = default;

void CefRequestContextHandlerMap::AddHandler(
    int render_process_id,
    int render_frame_id,
    int frame_tree_node_id,
    CefRefPtr<CefRequestContextHandler> handler) {
  DCHECK_GE(render_process_id, 0);
  DCHECK_GE(render_frame_id, 0);
  DCHECK_GE(frame_tree_node_id, 0);
  DCHECK(handler);

  render_id_handler_map_.insert(std::make_pair(
      std::make_pair(render_process_id, render_frame_id), handler));
  node_id_handler_map_.insert(std::make_pair(frame_tree_node_id, handler));
}

void CefRequestContextHandlerMap::RemoveHandler(int render_process_id,
                                                int render_frame_id,
                                                int frame_tree_node_id) {
  DCHECK_GE(render_process_id, 0);
  DCHECK_GE(render_frame_id, 0);
  DCHECK_GE(frame_tree_node_id, 0);

  auto it1 = render_id_handler_map_.find(
      std::make_pair(render_process_id, render_frame_id));
  if (it1 != render_id_handler_map_.end())
    render_id_handler_map_.erase(it1);

  auto it2 = node_id_handler_map_.find(frame_tree_node_id);
  if (it2 != node_id_handler_map_.end())
    node_id_handler_map_.erase(it2);
}

CefRefPtr<CefRequestContextHandler> CefRequestContextHandlerMap::GetHandler(
    int render_process_id,
    int render_frame_id,
    int frame_tree_node_id,
    bool require_frame_match) const {
  if (render_process_id >= 0 && render_frame_id >= 0) {
    const auto it1 = render_id_handler_map_.find(
        std::make_pair(render_process_id, render_frame_id));
    if (it1 != render_id_handler_map_.end())
      return it1->second;
  }

  if (frame_tree_node_id >= 0) {
    const auto it2 = node_id_handler_map_.find(frame_tree_node_id);
    if (it2 != node_id_handler_map_.end())
      return it2->second;
  }

  if (render_process_id >= 0 && !require_frame_match) {
    // Choose an arbitrary handler for the same process.
    for (auto& kv : render_id_handler_map_) {
      if (kv.first.first == render_process_id)
        return kv.second;
    }
  }

  return nullptr;
}
