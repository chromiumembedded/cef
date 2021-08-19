// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/common/frame_util.h"

#include "libcef/browser/thread_util.h"

#include <limits>
#include <sstream>

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"

namespace frame_util {

content::GlobalRenderFrameHostId GetGlobalId(
    content::NavigationHandle* navigation_handle) {
  CEF_REQUIRE_UIT();
  return navigation_handle->HasCommitted()
             ? navigation_handle->GetRenderFrameHost()->GetGlobalId()
             : navigation_handle->GetPreviousRenderFrameHostId();
}

std::string GetFrameDebugString(int64_t frame_id) {
  uint32_t process_id = frame_id >> 32;
  uint32_t routing_id = std::numeric_limits<uint32_t>::max() & frame_id;

  std::stringstream ss;
  ss << frame_id << " [" << process_id << "," << routing_id << "]";
  return ss.str();
}

std::string GetFrameDebugString(
    const content::GlobalRenderFrameHostId& global_id) {
  return GetFrameDebugString(MakeFrameId(global_id));
}

}  // namespace frame_util
