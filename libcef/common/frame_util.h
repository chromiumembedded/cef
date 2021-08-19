// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_FRAME_UTIL_H_
#define CEF_LIBCEF_COMMON_FRAME_UTIL_H_

#include <stdint.h>
#include <string>

#include "base/logging.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/child_process_host.h"

namespace content {
class NavigationHandle;
}

namespace frame_util {

// Create a frame ID in the format exposed by the CEF API.
inline int64_t MakeFrameId(int child_id, int frame_routing_id) {
  return (static_cast<uint64_t>(child_id) << 32) |
         static_cast<uint64_t>(frame_routing_id);
}

// Create a frame ID in the format exposed by the CEF API.
inline int64_t MakeFrameId(const content::GlobalRenderFrameHostId& global_id) {
  return MakeFrameId(global_id.child_id, global_id.frame_routing_id);
}

// Returns true if |child_id| is valid.
inline bool IsValidChildId(int child_id) {
  // See comments in ChildProcessHostImpl::GenerateChildProcessUniqueId().
  return child_id != content::ChildProcessHost::kInvalidUniqueID &&
         child_id != 0;
}

// Returns true if |frame_routing_id| is valid.
inline bool IsValidRoutingId(int frame_routing_id) {
  return frame_routing_id != MSG_ROUTING_NONE;
}

// Returns true if |global_id| is valid.
inline bool IsValidGlobalId(const content::GlobalRenderFrameHostId& global_id) {
  return IsValidChildId(global_id.child_id) &&
         IsValidRoutingId(global_id.frame_routing_id);
}

// Create a global ID from components.
inline content::GlobalRenderFrameHostId MakeGlobalId(
    int child_id,
    int frame_routing_id,
    bool allow_invalid_frame_id = false) {
  DCHECK(IsValidChildId(child_id));
  DCHECK(allow_invalid_frame_id || IsValidRoutingId(frame_routing_id));
  return content::GlobalRenderFrameHostId(child_id, frame_routing_id);
}

// Create a global ID from a frame ID.
inline content::GlobalRenderFrameHostId MakeGlobalId(int64_t frame_id) {
  uint32_t child_id = frame_id >> 32;
  uint32_t frame_routing_id = std::numeric_limits<uint32_t>::max() & frame_id;
  return MakeGlobalId(child_id, frame_routing_id);
}

// Returns an invalid global ID value.
inline content::GlobalRenderFrameHostId InvalidGlobalId() {
  return content::GlobalRenderFrameHostId();
}

// Returns the best match of global ID for |navigation_handle|. For pre-commit
// navigations this will return the current RFH, if any, or an invalid ID.
content::GlobalRenderFrameHostId GetGlobalId(
    content::NavigationHandle* navigation_handle);

// Returns a human-readable version of the ID.
std::string GetFrameDebugString(int64_t frame_id);
std::string GetFrameDebugString(
    const content::GlobalRenderFrameHostId& global_id);

}  // namespace frame_util

#endif  // CEF_LIBCEF_COMMON_FRAME_UTIL_H_
