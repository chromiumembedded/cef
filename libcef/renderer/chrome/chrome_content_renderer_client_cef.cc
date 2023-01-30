// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/chrome/chrome_content_renderer_client_cef.h"

#include "libcef/renderer/render_frame_observer.h"
#include "libcef/renderer/render_manager.h"
#include "libcef/renderer/thread_util.h"

#include "content/public/renderer/render_thread.h"

ChromeContentRendererClientCef::ChromeContentRendererClientCef()
    : render_manager_(new CefRenderManager) {}

ChromeContentRendererClientCef::~ChromeContentRendererClientCef() = default;

scoped_refptr<base::SingleThreadTaskRunner>
ChromeContentRendererClientCef::GetCurrentTaskRunner() {
  // Check if currently on the render thread.
  if (CEF_CURRENTLY_ON_RT()) {
    return render_task_runner_;
  }
  return nullptr;
}

void ChromeContentRendererClientCef::RenderThreadStarted() {
  ChromeContentRendererClient::RenderThreadStarted();

  render_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

void ChromeContentRendererClientCef::RenderThreadConnected() {
  ChromeContentRendererClient::RenderThreadConnected();

  render_manager_->RenderThreadConnected();
}

void ChromeContentRendererClientCef::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  ChromeContentRendererClient::RenderFrameCreated(render_frame);

  // Will delete itself when no longer needed.
  CefRenderFrameObserver* render_frame_observer =
      new CefRenderFrameObserver(render_frame);

  bool browser_created;
  absl::optional<bool> is_windowless;
  render_manager_->RenderFrameCreated(render_frame, render_frame_observer,
                                      browser_created, is_windowless);
  if (is_windowless.has_value() && *is_windowless) {
    LOG(ERROR) << "The chrome runtime does not support windowless browsers";
  }
}

void ChromeContentRendererClientCef::WebViewCreated(
    blink::WebView* web_view,
    bool was_created_by_renderer,
    const url::Origin* outermost_origin) {
  ChromeContentRendererClient::WebViewCreated(web_view, was_created_by_renderer,
                                              outermost_origin);

  bool browser_created;
  absl::optional<bool> is_windowless;
  render_manager_->WebViewCreated(web_view, browser_created, is_windowless);
  if (is_windowless.has_value() && *is_windowless) {
    LOG(ERROR) << "The chrome runtime does not support windowless browsers";
  }
}

void ChromeContentRendererClientCef::DevToolsAgentAttached() {
  // WebWorkers may be creating agents on a different thread.
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromeContentRendererClientCef::DevToolsAgentAttached,
                       base::Unretained(this)));
    return;
  }

  render_manager_->DevToolsAgentAttached();
}

void ChromeContentRendererClientCef::DevToolsAgentDetached() {
  // WebWorkers may be creating agents on a different thread.
  if (!render_task_runner_->BelongsToCurrentThread()) {
    render_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromeContentRendererClientCef::DevToolsAgentDetached,
                       base::Unretained(this)));
    return;
  }

  render_manager_->DevToolsAgentDetached();
}

void ChromeContentRendererClientCef::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  ChromeContentRendererClient::ExposeInterfacesToBrowser(binders);

  render_manager_->ExposeInterfacesToBrowser(binders);
}