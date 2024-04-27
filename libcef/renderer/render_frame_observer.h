// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef LIBCEF_RENDERER_RENDER_FRAME_OBSERVER_H_
#define LIBCEF_RENDERER_RENDER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame_observer.h"

class CefFrameImpl;

class CefRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit CefRenderFrameObserver(content::RenderFrame* render_frame);

  CefRenderFrameObserver(const CefRenderFrameObserver&) = delete;
  CefRenderFrameObserver& operator=(const CefRenderFrameObserver&) = delete;

  ~CefRenderFrameObserver() override;

  // RenderFrameObserver methods:
  void WasShown() override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFailProvisionalLoad() override;
  void DidFinishLoad() override;
  void WillDetach(blink::DetachReason detach_reason) override;
  void FocusedElementChanged(const blink::WebElement& element) override;
  void DidCreateScriptContext(v8::Handle<v8::Context> context,
                              int world_id) override;
  void WillReleaseScriptContext(v8::Handle<v8::Context> context,
                                int world_id) override;
  void OnDestruct() override;

  void AttachFrame(CefFrameImpl* frame);

 private:
  void OnLoadStart();
  void OnLoadError();

  CefFrameImpl* frame_ = nullptr;
};

#endif  // LIBCEF_RENDERER_RENDER_FRAME_OBSERVER_H_
