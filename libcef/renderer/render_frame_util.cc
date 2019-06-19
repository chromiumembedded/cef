// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/render_frame_util.h"

#include "libcef/common/frame_util.h"
#include "libcef/renderer/blink_glue.h"

#include "base/logging.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace render_frame_util {

int64_t GetIdentifier(blink::WebLocalFrame* frame) {
  // Each WebFrame will have an associated RenderFrame. The RenderFrame
  // routing IDs are unique within a given renderer process.
  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(frame);
  return frame_util::MakeFrameId(content::RenderThread::Get()->GetClientId(),
                                 render_frame->GetRoutingID());
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
  if (render_frame)
    return render_frame->unique_name();
  return std::string();
}

}  // namespace render_frame_util
