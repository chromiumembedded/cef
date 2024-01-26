// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_RENDER_FRAME_UTIL_H_
#define CEF_LIBCEF_RENDERER_RENDER_FRAME_UTIL_H_

#include <optional>
#include <string>

#include "third_party/blink/public/common/tokens/tokens.h"

namespace blink {
class WebLocalFrame;
}  // namespace blink

namespace render_frame_util {

std::string GetIdentifier(blink::WebLocalFrame* frame);
std::string GetName(blink::WebLocalFrame* frame);

// Parses |identifier| and returns a frame token appropriate to this renderer
// process, or std::nullopt.
std::optional<blink::LocalFrameToken> ParseFrameTokenFromIdentifier(
    const std::string& identifier);

}  // namespace render_frame_util

#endif  // CEF_LIBCEF_RENDERER_RENDER_FRAME_UTIL_H_
