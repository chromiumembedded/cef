// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_FRAME_UTIL_H_
#define CEF_LIBCEF_COMMON_FRAME_UTIL_H_

#include <stdint.h>
#include <optional>
#include <string>

#include "base/logging.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class NavigationHandle;
}

namespace frame_util {

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

// Returns an invalid global ID value.
inline content::GlobalRenderFrameHostId InvalidGlobalId() {
  return content::GlobalRenderFrameHostId();
}

// Returns the best match of global ID for |navigation_handle|. For pre-commit
// navigations this will return the current RFH, if any, or an invalid ID.
content::GlobalRenderFrameHostId GetGlobalId(
    content::NavigationHandle* navigation_handle);

// Returns true if |frame_token| is valid.
inline bool IsValidFrameToken(const blink::LocalFrameToken& frame_token) {
  return !frame_token->is_empty();
}

// Returns true if |global_token| is valid.
inline bool IsValidGlobalToken(
    const content::GlobalRenderFrameHostToken& global_token) {
  return IsValidChildId(global_token.child_id) &&
         IsValidFrameToken(global_token.frame_token);
}

// Create a global token from a frame identifier. Returns std::nullopt if
// |identifier| is invalid.
std::optional<content::GlobalRenderFrameHostToken> ParseFrameIdentifier(
    const std::string& identifier);

// Return the frame identifier for a global token. Returns empty if
// |global_token| is invalid.
std::string MakeFrameIdentifier(
    const content::GlobalRenderFrameHostToken& global_token);

// Returns a human-readable version of the ID/token.
std::string GetFrameDebugString(
    const content::GlobalRenderFrameHostId& global_id);
std::string GetFrameDebugString(
    const content::GlobalRenderFrameHostToken& global_token);

}  // namespace frame_util

#endif  // CEF_LIBCEF_COMMON_FRAME_UTIL_H_
