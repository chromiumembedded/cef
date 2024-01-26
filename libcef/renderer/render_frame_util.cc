// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/render_frame_util.h"

#include "libcef/common/frame_util.h"
#include "libcef/renderer/blink_glue.h"

#include "base/logging.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace render_frame_util {

std::string GetIdentifier(blink::WebLocalFrame* frame) {
  // Each WebFrame will have an associated RenderFrame. The RenderFrame
  // routing IDs are unique within a given renderer process.
  return frame_util::MakeFrameIdentifier(content::GlobalRenderFrameHostToken(
      content::RenderThread::Get()->GetClientId(),
      frame->GetLocalFrameToken()));
}

std::string GetName(blink::WebLocalFrame* frame) {
  DCHECK(frame);
  // Return the assigned name if it is non-empty. This represents the name
  // property on the frame DOM element. If the assigned name is empty, revert to
  // the internal unique name. This matches the logic in
  // CefFrameHostImpl::RefreshAttributes.
  if (frame->AssignedName().length() > 0) {
    return frame->AssignedName().Utf8();
  }
  content::RenderFrameImpl* render_frame =
      content::RenderFrameImpl::FromWebFrame(frame);
  DCHECK(render_frame);
  if (render_frame) {
    return render_frame->unique_name();
  }
  return std::string();
}

std::optional<blink::LocalFrameToken> ParseFrameTokenFromIdentifier(
    const std::string& identifier) {
  const auto& global_token = frame_util::ParseFrameIdentifier(identifier);
  if (!global_token ||
      global_token->child_id != content::RenderThread::Get()->GetClientId()) {
    return std::nullopt;
  }
  return global_token->frame_token;
}

}  // namespace render_frame_util
