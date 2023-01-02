// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/request_context_handler_map.h"

#include "libcef/common/frame_util.h"

CefRequestContextHandlerMap::CefRequestContextHandlerMap() = default;
CefRequestContextHandlerMap::~CefRequestContextHandlerMap() = default;

void CefRequestContextHandlerMap::AddHandler(
    const content::GlobalRenderFrameHostId& global_id,
    CefRefPtr<CefRequestContextHandler> handler) {
  DCHECK(frame_util::IsValidGlobalId(global_id));
  DCHECK(handler);

  render_id_handler_map_.insert(std::make_pair(global_id, handler));
}

void CefRequestContextHandlerMap::RemoveHandler(
    const content::GlobalRenderFrameHostId& global_id) {
  DCHECK(frame_util::IsValidGlobalId(global_id));

  auto it1 = render_id_handler_map_.find(global_id);
  if (it1 != render_id_handler_map_.end()) {
    render_id_handler_map_.erase(it1);
  }
}

CefRefPtr<CefRequestContextHandler> CefRequestContextHandlerMap::GetHandler(
    const content::GlobalRenderFrameHostId& global_id,
    bool require_frame_match) const {
  if (frame_util::IsValidGlobalId(global_id)) {
    const auto it1 = render_id_handler_map_.find(global_id);
    if (it1 != render_id_handler_map_.end()) {
      return it1->second;
    }
  }

  if (frame_util::IsValidChildId(global_id.child_id) && !require_frame_match) {
    // Choose an arbitrary handler for the same process.
    for (auto& kv : render_id_handler_map_) {
      if (kv.first.child_id == global_id.child_id) {
        return kv.second;
      }
    }
  }

  return nullptr;
}
