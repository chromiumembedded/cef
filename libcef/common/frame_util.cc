// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/common/frame_util.h"

namespace frame_util {

int64_t MakeFrameId(int32_t render_process_id, int32_t render_routing_id) {
  return (static_cast<uint64_t>(render_process_id) << 32) |
         static_cast<uint64_t>(render_routing_id);
}

}  // namespace frame_util
