// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/renderer/render_frame_observer.h"

#include "libcef/renderer/content_renderer_client.h"

#include "content/public/renderer/render_frame.h"

CefRenderFrameObserver::CefRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
}

CefRenderFrameObserver::~CefRenderFrameObserver() {
}

void CefRenderFrameObserver::WillReleaseScriptContext(
    v8::Handle<v8::Context> context,
    int world_id) {
  CefContentRendererClient::Get()->WillReleaseScriptContext(
      render_frame()->GetWebFrame(), context, world_id);
}
