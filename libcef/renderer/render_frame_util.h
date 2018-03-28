// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_RENDER_FRAME_UTIL_H_
#define CEF_LIBCEF_RENDERER_RENDER_FRAME_UTIL_H_

#include <stdint.h>

#include <string>

namespace blink {
class WebLocalFrame;
}

namespace render_frame_util {

int64_t GetIdentifier(blink::WebLocalFrame* frame);
std::string GetName(blink::WebLocalFrame* frame);

}  // namespace render_frame_util

#endif  // CEF_LIBCEF_RENDERER_RENDER_FRAME_UTIL_H_
