// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef LIBCEF_RENDERER_RENDER_FRAME_OBSERVER_H_
#define LIBCEF_RENDERER_RENDER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame_observer.h"

namespace content {
class RenderFrame;
class RenderView;
}  // namespace content

class CefFrameImpl;

class CefRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit CefRenderFrameObserver(content::RenderFrame* render_frame);
  ~CefRenderFrameObserver() override;

  // RenderFrameObserver methods:
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFailProvisionalLoad() override;
  void DidFinishLoad() override;
  void WillDetach() override;
  void FocusedElementChanged(const blink::WebElement& element) override;
  void DraggableRegionsChanged() override;
  void DidCreateScriptContext(v8::Handle<v8::Context> context,
                              int world_id) override;
  void WillReleaseScriptContext(v8::Handle<v8::Context> context,
                                int world_id) override;
  void OnDestruct() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void AttachFrame(CefFrameImpl* frame);

 private:
  void OnLoadStart();
  void OnLoadError();

  CefFrameImpl* frame_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CefRenderFrameObserver);
};

#endif  // LIBCEF_RENDERER_RENDER_FRAME_OBSERVER_H_
