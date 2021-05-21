// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/common/frame_util.h"

#include <limits>
#include <sstream>

namespace frame_util {

int64_t MakeFrameId(int32_t render_process_id, int32_t render_routing_id) {
  return (static_cast<uint64_t>(render_process_id) << 32) |
         static_cast<uint64_t>(render_routing_id);
}

std::string GetFrameDebugString(int64_t frame_id) {
  uint32_t process_id = frame_id >> 32;
  uint32_t routing_id = std::numeric_limits<uint32_t>::max() & frame_id;

  std::stringstream ss;
  ss << frame_id << " [" << process_id << "," << routing_id << "]";
  return ss.str();
}

}  // namespace frame_util
