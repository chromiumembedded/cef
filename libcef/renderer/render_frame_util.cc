// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/render_frame_util.h"

#include "libcef/renderer/webkit_glue.h"

#include "base/logging.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace render_frame_util {

int64_t GetIdentifier(blink::WebLocalFrame* frame) {
  // Each WebFrame will have an associated RenderFrame. The RenderFrame
  // routing IDs are unique within a given renderer process.
  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(frame);
  DCHECK(render_frame);
  if (render_frame)
    return render_frame->GetRoutingID();
  return webkit_glue::kInvalidFrameId;
}

std::string GetUniqueName(blink::WebLocalFrame* frame) {
  content::RenderFrameImpl* render_frame =
      content::RenderFrameImpl::FromWebFrame(frame);
  DCHECK(render_frame);
  if (render_frame)
    return render_frame->unique_name();
  return std::string();
}

}  // namespace render_frame_util
