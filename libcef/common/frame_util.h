// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_FRAME_UTIL_H_
#define CEF_LIBCEF_COMMON_FRAME_UTIL_H_

#include <stdint.h>

namespace frame_util {

// Returns the frame ID, which is a 64-bit combination of |render_process_id|
// and |render_routing_id|.
int64_t MakeFrameId(int32_t render_process_id, int32_t render_routing_id);

}  // namespace frame_util

#endif  // CEF_LIBCEF_COMMON_FRAME_UTIL_H_
