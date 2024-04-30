// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/renderer/chrome/chrome_content_renderer_client_cef.h"

#include "cef/libcef/renderer/render_frame_observer.h"
#include "cef/libcef/renderer/render_manager.h"
#include "cef/libcef/renderer/thread_util.h"
#include "chrome/renderer/printing/chrome_print_render_frame_helper_delegate.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/web/web_view.h"

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
  // Will delete itself when no longer needed.
  CefRenderFrameObserver* render_frame_observer =
      new CefRenderFrameObserver(render_frame);

  bool browser_created;
  std::optional<bool> is_windowless;
  std::optional<bool> print_preview_enabled;
  render_manager_->RenderFrameCreated(render_frame, render_frame_observer,
                                      browser_created, is_windowless,
                                      print_preview_enabled);
  if (browser_created) {
    OnBrowserCreated(render_frame->GetWebView(), is_windowless);
  }

  if (print_preview_enabled.has_value()) {
    // This value will be used when the when ChromeContentRendererClient
    // creates the new ChromePrintRenderFrameHelperDelegate below.
    ChromePrintRenderFrameHelperDelegate::SetNextPrintPreviewEnabled(
        *print_preview_enabled);
  }

  ChromeContentRendererClient::RenderFrameCreated(render_frame);
}

void ChromeContentRendererClientCef::WebViewCreated(
    blink::WebView* web_view,
    bool was_created_by_renderer,
    const url::Origin* outermost_origin) {
  ChromeContentRendererClient::WebViewCreated(web_view, was_created_by_renderer,
                                              outermost_origin);

  bool browser_created;
  std::optional<bool> is_windowless;
  std::optional<bool> print_preview_enabled;
  render_manager_->WebViewCreated(web_view, browser_created, is_windowless,
                                  print_preview_enabled);
  if (browser_created) {
    OnBrowserCreated(web_view, is_windowless);
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

void ChromeContentRendererClientCef::OnBrowserCreated(
    blink::WebView* web_view,
    std::optional<bool> is_windowless) {
#if BUILDFLAG(IS_MAC)
  const bool windowless = is_windowless.has_value() && *is_windowless;

  // FIXME: It would be better if this API would be a callback from the
  // WebKit layer, or if it would be exposed as an WebView instance method; the
  // current implementation uses a static variable, and WebKit needs to be
  // patched in order to make it work for each WebView instance
  web_view->SetUseExternalPopupMenusThisInstance(!windowless);
#endif
}
